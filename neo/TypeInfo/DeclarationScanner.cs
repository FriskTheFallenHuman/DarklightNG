using System.Text.RegularExpressions;

namespace Darklight.TypeInfo;

/// <summary>
/// Scans UnrealHeaderTool-style annotations on real C++ declarations. Class
/// names, base classes, and attached member names come from the declarations;
/// they are deliberately not repeated in the annotation arguments.
/// </summary>
internal static class SourceScanner
{
    private static readonly HashSet<string> TagNames = new(StringComparer.Ordinal)
    {
        "D3_CLASS",
        "D3_ENUM",
        "D3_EVENT",
        "D3_NODE",
    };

    private static readonly Regex ClassAfterTag = new(
        @"\G\s*class\s+(?<name>[A-Za-z_]\w*)\s*(?:\:\s*public\s+(?<base>[A-Za-z_]\w*))?\s*\{",
        RegexOptions.CultureInvariant);

    private static readonly Regex LegacyEventDefinition = new(
        @"(?m)^\s*(?:static\s+)?const\s+idEventDef\s+\w+\s*\(",
        RegexOptions.CultureInvariant);

    private static readonly Regex LegacyClassDeclaration = new(
        @"(?m)^\s*(?:CLASS_DECLARATION|ABSTRACT_DECLARATION)\s*\(",
        RegexOptions.CultureInvariant);

    private static readonly Regex ImplementationTag = new(
        @"\bD3_(?:CLASS|ENUM(?:_VALUE)?|EVENT|NODE)\s*\(",
        RegexOptions.CultureInvariant);

    private static readonly Regex EnumAfterTag = new(
        @"\G\s*(?:(?<typedef>typedef)\s+)?enum\s*(?<name>[A-Za-z_]\w*)?\s*(?:\:\s*[^\{]+)?\s*\{",
        RegexOptions.CultureInvariant);

    private sealed record ParsedClass(
        ClassDefinition Definition,
        SourceDocument Document,
        int BodyStart,
        int BodyEnd);

    private sealed record ParsedFunction(
        string Name,
        string Format,
        IReadOnlyList<string> ParameterNames,
        IReadOnlyList<string> ParameterEnums);

    public static SourceModel Scan(ToolOptions options)
    {
        List<SourceDocument> documents = LoadDocuments(options);

        foreach (SourceDocument implementation in documents.Where(IsCpp))
        {
            RejectImplementationMetadata(implementation);
        }

        List<EventDefinition> eventDefinitions = new();
        List<ParsedClass> parsedClasses = new();
        List<EnumDefinition> enums = new();

        foreach (SourceDocument header in documents.Where(IsHeader))
        {
            ParseEnums(header, enums);
        }

        Dictionary<string, EnumDefinition> enumsByName = new(StringComparer.Ordinal);
        foreach (EnumDefinition definition in enums)
        {
            if (!enumsByName.TryAdd(definition.Name, definition))
            {
                throw new ToolException(
                    $"{definition.Location}: duplicate D3_ENUM annotation for '{definition.Name}'");
            }
        }

        foreach (SourceDocument header in documents.Where(IsHeader))
        {
            ParseHeader(header, eventDefinitions, parsedClasses, enumsByName);
        }

        IReadOnlyList<EventDefinition> events = CoalesceEventDefinitions(eventDefinitions);
        IReadOnlyList<ClassDefinition> classes = parsedClasses.Select(item => item.Definition).ToArray();
        Validate(events, classes);

        return new SourceModel
        {
            Events = events,
            Classes = classes,
            Enums = enums,
        };
    }

    private static void ParseEnums(SourceDocument document, ICollection<EnumDefinition> definitions)
    {
        IReadOnlyDictionary<int, ConditionalContext> conditions =
            CppText.BuildConditionalContexts(document.Text);
        string? includeGuard = FindIncludeGuard(document.MaskedText);
        List<MacroInvocation> macros = CppText.FindMacros(document.Text, TagNames)
            .Where(macro => !IsPreprocessorDirective(document.Text, macro.Start))
            .ToList();

        foreach (MacroInvocation tag in macros.Where(macro => macro.Name == "D3_ENUM"))
        {
            if (tag.Arguments.Count > 1 ||
                (tag.Arguments.Count == 1 && tag.Arguments[0] != "BlueprintType"))
            {
                throw ErrorAt(document, tag.Start, "D3_ENUM accepts only the optional marker 'BlueprintType'");
            }

            Match declaration = EnumAfterTag.Match(document.MaskedText, tag.End);
            if (!declaration.Success)
            {
                throw ErrorAt(document, tag.Start, "D3_ENUM must immediately precede an enum declaration");
            }

            int openBrace = declaration.Index + declaration.Length - 1;
            int closeBrace = CppText.FindMatchingDelimiter(document.Text, openBrace, '{', '}');
            if (closeBrace < 0)
            {
                throw ErrorAt(document, openBrace, "reflected enum has no closing brace");
            }

            bool isTypedef = declaration.Groups["typedef"].Success;
            string name = declaration.Groups["name"].Value;
            if (isTypedef)
            {
                Match alias = Regex.Match(
                    document.MaskedText[(closeBrace + 1)..],
                    @"^\s*(?<name>[A-Za-z_]\w*)\s*;",
                    RegexOptions.CultureInvariant);
                if (!alias.Success)
                {
                    throw ErrorAt(document, closeBrace, "a reflected typedef enum must end with its type name");
                }
                name = alias.Groups["name"].Value;
            }
            else if (name.Length == 0)
            {
                throw ErrorAt(document, tag.Start, "a reflected enum must have a type name");
            }

            string body = document.MaskedText[(openBrace + 1)..closeBrace];
            List<EnumValueDefinition> values = new();
            foreach (string rawValue in CppText.SplitTopLevel(body, ','))
            {
                string value = rawValue.Trim();
                if (value.Length == 0)
                {
                    continue;
                }
                Match identifier = Regex.Match(value, @"^(?<name>[A-Za-z_]\w*)\b", RegexOptions.CultureInvariant);
                if (!identifier.Success)
                {
                    throw ErrorAt(document, openBrace, $"cannot parse reflected enum value '{value}'");
                }
                bool hidden = Regex.IsMatch(
                    value,
                    @"\bD3_ENUM_VALUE\s*\(\s*Hidden\s*\)",
                    RegexOptions.CultureInvariant);
                int valueOffset = openBrace + 1 + body.IndexOf(rawValue, StringComparison.Ordinal);
                values.Add(new EnumValueDefinition(
                    identifier.Groups["name"].Value,
                    hidden,
                    new SourceLocation(document.RelativePath, CppText.GetLineNumber(document.Text, valueOffset))));
            }
            if (values.Count == 0 || values.All(value => value.Hidden))
            {
                throw ErrorAt(document, tag.Start, $"reflected enum '{name}' has no script-visible values");
            }
            HashSet<string> valueNames = new(StringComparer.Ordinal);
            foreach (EnumValueDefinition value in values)
            {
                if (!valueNames.Add(value.Name))
                {
                    throw new ToolException($"{value.Location}: duplicate value '{value.Name}' in enum '{name}'");
                }
            }

            definitions.Add(new EnumDefinition(
                name,
                values,
                new SourceLocation(document.RelativePath, tag.Line),
                ConditionAt(conditions, tag.Line, includeGuard)));
        }
    }

    private static List<SourceDocument> LoadDocuments(ToolOptions options)
    {
        List<SourceDocument> documents = new();
        IEnumerable<string> files = Directory
            .EnumerateFiles(options.SourceRoot, "*.*", SearchOption.AllDirectories)
            .Where(path => path.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase) ||
                           path.EndsWith(".h", StringComparison.OrdinalIgnoreCase))
            .OrderBy(
                path => ToolOptions.NormalizeRelativePath(Path.GetRelativePath(options.SourceRoot, path)),
                StringComparer.OrdinalIgnoreCase);

        foreach (string path in files)
        {
            string relative = ToolOptions.NormalizeRelativePath(Path.GetRelativePath(options.SourceRoot, path));
            if (relative.StartsWith("generated/", StringComparison.OrdinalIgnoreCase) ||
                options.IsExcluded(relative))
            {
                continue;
            }

            string text = File.ReadAllText(path);
            documents.Add(new SourceDocument(
                relative,
                text,
                CppText.MaskCommentsAndStrings(text, preserveStrings: true)));
        }

        return documents;
    }

    private static void RejectImplementationMetadata(SourceDocument document)
    {
        Match tag = ImplementationTag.Match(document.MaskedText);
        if (tag.Success)
        {
            throw ErrorAt(document, tag.Index, "D3 metadata belongs on declarations in headers, not in a .cpp file");
        }

        Match eventDefinition = LegacyEventDefinition.Match(document.MaskedText);
        if (eventDefinition.Success)
        {
            throw ErrorAt(document, eventDefinition.Index, "idEventDef objects are generated; annotate a declaration with D3_EVENT");
        }

        Match classDeclaration = LegacyClassDeclaration.Match(document.MaskedText);
        if (classDeclaration.Success)
        {
            throw ErrorAt(document, classDeclaration.Index, "class registration is generated; annotate the class declaration with D3_CLASS");
        }
    }

    private static void ParseHeader(
        SourceDocument document,
        ICollection<EventDefinition> eventDefinitions,
        ICollection<ParsedClass> parsedClasses,
        IReadOnlyDictionary<string, EnumDefinition> enums)
    {
        IReadOnlyDictionary<int, ConditionalContext> conditions =
            CppText.BuildConditionalContexts(document.Text);
        string? includeGuard = FindIncludeGuard(document.MaskedText);
        List<MacroInvocation> macros = CppText.FindMacros(document.Text, TagNames)
            .Where(macro => !IsPreprocessorDirective(document.Text, macro.Start))
            .ToList();

        List<ParsedClass> localClasses = new();
        foreach (MacroInvocation tag in macros.Where(macro => macro.Name == "D3_CLASS"))
        {
            if (tag.Arguments.Count > 1 ||
                (tag.Arguments.Count == 1 && tag.Arguments[0] != "Abstract"))
            {
                throw ErrorAt(document, tag.Start, "D3_CLASS accepts only the optional marker 'Abstract'");
            }

            Match declaration = ClassAfterTag.Match(document.MaskedText, tag.End);
            if (!declaration.Success)
            {
                throw ErrorAt(document, tag.Start, "D3_CLASS must immediately precede a class definition");
            }

            string name = declaration.Groups["name"].Value;
            string superClass = declaration.Groups["base"].Success
                ? declaration.Groups["base"].Value
                : "NULL";
            int openBrace = declaration.Index + declaration.Length - 1;
            int closeBrace = CppText.FindMatchingDelimiter(document.Text, openBrace, '{', '}');
            if (closeBrace < 0)
            {
                throw ErrorAt(document, openBrace, $"class '{name}' has no closing brace");
            }

            bool isAbstract = tag.Arguments.Count == 1;
            string body = document.MaskedText[(openBrace + 1)..closeBrace];
            string expectedPrototype = isAbstract ? "ABSTRACT_PROTOTYPE" : "CLASS_PROTOTYPE";
            if (!Regex.IsMatch(
                    body,
                    $@"\b{expectedPrototype}\s*\(\s*{Regex.Escape(name)}\s*\)",
                    RegexOptions.CultureInvariant))
            {
                throw ErrorAt(
                    document,
                    tag.Start,
                    $"D3_CLASS annotation for '{name}' does not match its {expectedPrototype}");
            }

            ConditionalContext condition = ConditionAt(conditions, tag.Line, includeGuard);
            ClassDefinition definition = new()
            {
                Name = name,
                SuperClass = superClass,
                IsAbstract = isAbstract,
                Location = new SourceLocation(document.RelativePath, tag.Line),
                Condition = condition,
            };
            ParsedClass parsed = new(definition, document, openBrace + 1, closeBrace);
            localClasses.Add(parsed);
            parsedClasses.Add(parsed);
        }

        Dictionary<int, NodeMetadata> nodeMetadata = new();
        foreach (MacroInvocation tag in macros.Where(macro => macro.Name == "D3_NODE"))
        {
            MacroInvocation? eventTag = macros
                .Where(item => item.Start >= tag.End)
                .OrderBy(item => item.Start)
                .FirstOrDefault();
            if (eventTag is null || eventTag.Name != "D3_EVENT" ||
                !CppText.ContainsOnlyTrivia(document.Text, (tag.End, eventTag.Start)))
            {
                throw ErrorAt(document, tag.Start, "D3_NODE must immediately precede D3_EVENT");
            }
            if (!nodeMetadata.TryAdd(eventTag.Start, ParseNodeMetadata(document, tag)))
            {
                throw ErrorAt(document, tag.Start, "only one D3_NODE may annotate an event");
            }
        }

        foreach (MacroInvocation tag in macros.Where(macro => macro.Name == "D3_EVENT"))
        {
            if (tag.Arguments.Count == 0)
            {
                throw ErrorAt(document, tag.Start, "D3_EVENT requires an event symbol");
            }

            ParsedClass? owner = localClasses.SingleOrDefault(
                item => tag.Start > item.BodyStart && tag.Start < item.BodyEnd);
            bool explicitCallback = owner is not null &&
                tag.Arguments.Count == 2 &&
                Regex.IsMatch(
                    tag.Arguments[1],
                    @"^[A-Za-z_]\w*\s*::\s*[A-Za-z_]\w*$",
                    RegexOptions.CultureInvariant);
            bool definesEvent = !explicitCallback && tag.Arguments.Count == 3;
            bool referencesEvent = tag.Arguments.Count == 1 || explicitCallback;
            if (!definesEvent && !referencesEvent)
            {
                throw ErrorAt(
                    document,
                    tag.Start,
                    "D3_EVENT expects (Symbol), (Symbol, Class::Callback), or " +
                    "(Symbol, \"command\", returnType)");
            }
            if (owner is null && !definesEvent)
            {
                throw ErrorAt(document, tag.Start, "a namespace D3_EVENT must define an event signature");
            }

            SourceLocation location = new(document.RelativePath, tag.Line);
            ConditionalContext condition = ConditionAt(conditions, tag.Line, includeGuard);
            ParsedFunction? attached = explicitCallback
                ? null
                : FindAttachedFunction(document, tag, macros, owner?.BodyEnd ?? document.Text.Length, enums);
            if (definesEvent)
            {
                string returnType = ParseReturnType(tag.Arguments[2], location);
                List<string> constructorArguments = new() { tag.Arguments[1] };
                if (attached!.Format.Length != 0 || returnType != "void")
                {
                    constructorArguments.Add(attached.Format.Length == 0 ? "NULL" : $"\"{attached.Format}\"");
                }
                if (returnType != "void")
                {
                    constructorArguments.Add($"'{returnType}'");
                }

                NodeMetadata metadata = nodeMetadata.TryGetValue(tag.Start, out NodeMetadata? annotated)
                    ? annotated
                    : NodeMetadata.Default(ExtractLeadingComment(document.Text, tag.Start));
                eventDefinitions.Add(new EventDefinition(
                    tag.Arguments[0],
                    constructorArguments,
                    attached.ParameterNames,
                    attached.ParameterEnums,
                    location,
                    condition,
                    metadata));
            }

            if (owner is null)
            {
                continue;
            }

            string callback = explicitCallback
                ? Regex.Replace(tag.Arguments[1], @"\s+", string.Empty, RegexOptions.CultureInvariant)
                : $"{owner.Definition.Name}::{attached!.Name}";
            owner.Definition.Bindings.Add(new EventBinding(
                tag.Arguments[0],
                callback,
                attached?.Format,
                location));
        }
    }

    private static NodeMetadata ParseNodeMetadata(SourceDocument document, MacroInvocation tag)
    {
        string title = "";
        string category = "";
        string description = ExtractLeadingComment(document.Text, tag.Start);
        string keywords = "";
        string receiver = "auto";
        bool pure = false;
        bool latent = false;
        bool hidden = false;
        bool deprecated = false;

        foreach (string raw in tag.Arguments)
        {
            string argument = raw.Trim();
            switch (argument)
            {
                case "Pure": pure = true; continue;
                case "Latent": latent = true; continue;
                case "Hidden": hidden = true; continue;
                case "Deprecated": deprecated = true; continue;
            }

            Match named = Regex.Match(argument, @"^(Title|Category|Description|Keywords|Receiver)\s*=\s*(.+)$", RegexOptions.CultureInvariant);
            if (!named.Success)
            {
                throw ErrorAt(document, tag.Start, $"unknown D3_NODE option '{argument}'");
            }
            string value = CppText.ParseCppString(named.Groups[2].Value, new SourceLocation(document.RelativePath, tag.Line));
            switch (named.Groups[1].Value)
            {
                case "Title": title = value; break;
                case "Category": category = value; break;
                case "Description": description = value; break;
                case "Keywords": keywords = value; break;
                case "Receiver": receiver = value; break;
            }
        }

        if (receiver is not ("auto" or "sys" or "global" or "object"))
        {
            throw ErrorAt(document, tag.Start, "D3_NODE Receiver must be auto, sys, global, or object");
        }
        if (pure && latent)
        {
            throw ErrorAt(document, tag.Start, "a D3_NODE cannot be both Pure and Latent");
        }
        return new NodeMetadata(title, category, description, keywords, receiver, pure, latent, hidden, deprecated);
    }

    private static string ExtractLeadingComment(string text, int position)
    {
        int lineStart = text.LastIndexOf('\n', Math.Max(0, position - 1));
        int cursor = lineStart < 0 ? 0 : lineStart;
        List<string> lines = new();
        while (cursor > 0)
        {
            int previousEnd = cursor;
            int previousStart = text.LastIndexOf('\n', Math.Max(0, previousEnd - 1));
            previousStart = previousStart < 0 ? 0 : previousStart + 1;
            string line = text[previousStart..previousEnd].Trim();
            if (!line.StartsWith("//", StringComparison.Ordinal))
            {
                break;
            }
            string value = line[2..].Trim();
            if (value.Length != 0)
            {
                lines.Insert(0, value);
            }
            cursor = previousStart - 1;
        }
        return string.Join(" ", lines);
    }

    private static ParsedFunction FindAttachedFunction(
        SourceDocument document,
        MacroInvocation tag,
        IReadOnlyList<MacroInvocation> macros,
        int classEnd,
        IReadOnlyDictionary<string, EnumDefinition> enums)
    {
        int cursor = tag.End;
        foreach (MacroInvocation following in macros
                     .Where(item => (item.Name is "D3_EVENT" or "D3_NODE") && item.Start >= cursor && item.Start < classEnd)
                     .OrderBy(item => item.Start))
        {
            if (!CppText.ContainsOnlyTrivia(document.Text, (cursor, following.Start)))
            {
                break;
            }
            cursor = following.End;
        }

        int semicolon = document.MaskedText.IndexOf(';', cursor);
        if (semicolon < 0 || semicolon >= classEnd)
        {
            throw ErrorAt(document, tag.Start, "D3_EVENT must immediately precede a function declaration");
        }

        string declaration = document.MaskedText[cursor..semicolon];
        Match member = Regex.Match(
            declaration,
            @"\b(?<name>[A-Za-z_]\w*)\s*\(",
            RegexOptions.CultureInvariant);
        if (!member.Success)
        {
            throw ErrorAt(document, tag.Start, "D3_EVENT must immediately precede a function declaration");
        }

        int openParen = cursor + member.Index + member.Length - 1;
        int closeParen = CppText.FindMatchingDelimiter(document.Text, openParen, '(', ')');
        if (closeParen < 0 || closeParen > semicolon)
        {
            throw ErrorAt(document, tag.Start, "annotated function has an unterminated parameter list");
        }

        string parameters = document.Text[(openParen + 1)..closeParen];
        IReadOnlyList<string> rawParameters = string.IsNullOrWhiteSpace(parameters) || parameters.Trim() == "void"
            ? Array.Empty<string>()
            : CppText.SplitTopLevel(parameters, ',').Select(item => item.Trim()).ToArray();
        string format = string.Concat(rawParameters.Select(parameter => InferParameterType(parameter, document, tag, enums)));
        return new ParsedFunction(
            member.Groups["name"].Value,
            format,
            CppText.ExtractParameterNames(parameters),
            rawParameters.Select(parameter => InferParameterEnum(parameter, enums)).ToArray());
    }

    private static char InferParameterType(
        string parameter,
        SourceDocument document,
        MacroInvocation tag,
        IReadOnlyDictionary<string, EnumDefinition> enums)
    {
        string type = CppText.SplitTopLevel(parameter, '=')[0];
        bool nullable = Regex.IsMatch(type, @"\bD3_NULLABLE\b", RegexOptions.CultureInvariant);
        type = Regex.Replace(type, @"\bD3_NULLABLE\b", string.Empty, RegexOptions.CultureInvariant);
        type = Regex.Replace(type, @"\s+", " ", RegexOptions.CultureInvariant).Trim();

        if (InferParameterEnum(type, enums).Length != 0)
        {
            if (nullable)
            {
                throw ErrorAt(document, tag.Start, "D3_NULLABLE is valid only on entity pointer parameters");
            }
            return 'd';
        }

        if (Regex.IsMatch(type, @"\btrace_t\s*\*", RegexOptions.CultureInvariant))
        {
            return 't';
        }
        if (Regex.IsMatch(type, @"\b(?:const\s+)?char\s*(?:const\s*)?\*", RegexOptions.CultureInvariant))
        {
            return 's';
        }
        if (Regex.IsMatch(type, @"\b(?:idVec3|idAngles)\b", RegexOptions.CultureInvariant))
        {
            return 'v';
        }
        if (Regex.IsMatch(type, @"\bfloat\b", RegexOptions.CultureInvariant))
        {
            return 'f';
        }
        if (Regex.IsMatch(type, @"\bid[A-Za-z_]\w*\s*\*", RegexOptions.CultureInvariant))
        {
            return nullable ? 'E' : 'e';
        }
        if (Regex.IsMatch(
                type,
                @"\b(?:bool|int|jointHandle_t|jointModTransform_t|moverState_t)\b",
                RegexOptions.CultureInvariant))
        {
            if (nullable)
            {
                throw ErrorAt(document, tag.Start, "D3_NULLABLE is valid only on entity pointer parameters");
            }
            return 'd';
        }

        throw ErrorAt(document, tag.Start, $"cannot infer a Doom event type for parameter '{parameter.Trim()}'");
    }

    private static string InferParameterEnum(
        string parameter,
        IReadOnlyDictionary<string, EnumDefinition> enums)
    {
        string type = CppText.SplitTopLevel(parameter, '=')[0];
        foreach (string enumName in enums.Keys.OrderByDescending(name => name.Length))
        {
            if (Regex.IsMatch(type, $@"\b{Regex.Escape(enumName)}\b", RegexOptions.CultureInvariant))
            {
                return enumName;
            }
        }
        return string.Empty;
    }

    private static string ParseReturnType(string expression, SourceLocation location) =>
        expression.Trim() switch
        {
            "void" => "void",
            "integer" => "d",
            "float" => "f",
            "vector" => "v",
            "string" => "s",
            "entity" => "e",
            _ => throw new ToolException(
                $"{location}: return type must be void, integer, float, vector, string, or entity"),
        };

    private static IReadOnlyList<EventDefinition> CoalesceEventDefinitions(
        IReadOnlyList<EventDefinition> definitions)
    {
        List<EventDefinition> unique = new();
        Dictionary<string, EventDefinition> bySymbol = new(StringComparer.Ordinal);
        foreach (EventDefinition definition in definitions)
        {
            if (!bySymbol.TryGetValue(definition.Symbol, out EventDefinition? previous))
            {
                bySymbol.Add(definition.Symbol, definition);
                unique.Add(definition);
                continue;
            }

            if (EventSignature.From(previous) != EventSignature.From(definition))
            {
                throw new ToolException(
                    $"{definition.Location}: event symbol '{definition.Symbol}' conflicts with {previous.Location}");
            }
            if (!previous.ParameterEnums.SequenceEqual(definition.ParameterEnums, StringComparer.Ordinal))
            {
                throw new ToolException(
                    $"{definition.Location}: enum metadata for event symbol '{definition.Symbol}' conflicts with {previous.Location}");
            }
        }
        return unique;
    }

    private static void Validate(
        IReadOnlyList<EventDefinition> events,
        IReadOnlyList<ClassDefinition> classes)
    {
        Dictionary<string, EventDefinition> eventsBySymbol = new(StringComparer.Ordinal);
        foreach (EventDefinition definition in events)
        {
            if (!Regex.IsMatch(definition.Symbol, @"^[A-Za-z_]\w*$", RegexOptions.CultureInvariant))
            {
                throw new ToolException($"{definition.Location}: invalid event symbol '{definition.Symbol}'");
            }
            eventsBySymbol.Add(definition.Symbol, definition);
        }

        Dictionary<string, ClassDefinition> classesByName = new(StringComparer.Ordinal);
        foreach (ClassDefinition definition in classes)
        {
            if (!classesByName.TryAdd(definition.Name, definition))
            {
                throw new ToolException($"{definition.Location}: duplicate D3_CLASS annotation for '{definition.Name}'");
            }

            foreach (EventBinding binding in definition.Bindings)
            {
                if (!eventsBySymbol.TryGetValue(binding.EventSymbol, out EventDefinition? eventDefinition))
                {
                    throw new ToolException(
                        $"{binding.Location}: D3_EVENT references undefined event '{binding.EventSymbol}'");
                }
                if (binding.InferredFormat is not null)
                {
                    EventSignature signature = EventSignature.From(eventDefinition);
                    if (binding.InferredFormat != signature.Format)
                    {
                        throw new ToolException(
                            $"{binding.Location}: callback '{binding.Callback}' infers format " +
                            $"'{binding.InferredFormat}', but event '{signature.Command}' is '{signature.Format}'");
                    }
                }
            }
        }

        foreach (ClassDefinition definition in classes)
        {
            if (definition.SuperClass != "NULL" && !classesByName.ContainsKey(definition.SuperClass))
            {
                throw new ToolException(
                    $"{definition.Location}: superclass '{definition.SuperClass}' has no D3_CLASS annotation");
            }
        }

        ValidateEventCommands(events);
        ValidateHierarchy(classesByName);
    }

    private static void ValidateEventCommands(IReadOnlyList<EventDefinition> events)
    {
        Dictionary<string, (EventSignature Signature, EventDefinition Definition)> commands =
            new(StringComparer.Ordinal);
        foreach (EventDefinition definition in events)
        {
            EventSignature signature = EventSignature.From(definition);
            if (signature.Format.Length > 8)
            {
                throw new ToolException($"{definition.Location}: event '{signature.Command}' has more than eight arguments");
            }
            foreach (char type in signature.Format)
            {
                if (!"dfvseEt".Contains(type))
                {
                    throw new ToolException($"{definition.Location}: event '{signature.Command}' has invalid argument type '{type}'");
                }
            }
            if (signature.ReturnType != "void" &&
                (signature.ReturnType.Length != 1 || !"dfvseEt".Contains(signature.ReturnType[0])))
            {
                throw new ToolException($"{definition.Location}: event '{signature.Command}' has invalid return type '{signature.ReturnType}'");
            }

            if (commands.TryGetValue(signature.Command, out var previous) && previous.Signature != signature)
            {
                throw new ToolException(
                    $"{definition.Location}: event command '{signature.Command}' conflicts with {previous.Definition.Location}");
            }
            commands.TryAdd(signature.Command, (signature, definition));
        }
    }

    private static void ValidateHierarchy(IReadOnlyDictionary<string, ClassDefinition> classes)
    {
        foreach (ClassDefinition start in classes.Values)
        {
            HashSet<string> visited = new(StringComparer.Ordinal);
            ClassDefinition current = start;
            while (current.SuperClass != "NULL")
            {
                if (!visited.Add(current.Name))
                {
                    throw new ToolException($"{start.Location}: class hierarchy contains a cycle at '{current.Name}'");
                }
                current = classes[current.SuperClass];
            }
        }
    }

    private static ConditionalContext ConditionAt(
        IReadOnlyDictionary<int, ConditionalContext> conditions,
        int line,
        string? includeGuard)
    {
        if (!conditions.TryGetValue(line, out ConditionalContext? value) || value.Frames.Count == 0)
        {
            return ConditionalContext.Empty;
        }

        IReadOnlyList<IReadOnlyList<string>> frames = value.Frames;
        if (includeGuard is not null &&
            Regex.IsMatch(
                frames[0][0],
                $@"^#\s*ifndef\s+{Regex.Escape(includeGuard)}\s*$",
                RegexOptions.CultureInvariant))
        {
            frames = frames.Skip(1).ToArray();
        }

        return frames.Count == 0 ? ConditionalContext.Empty : new ConditionalContext(frames);
    }

    private static string? FindIncludeGuard(string maskedText)
    {
        Match guard = Regex.Match(
            maskedText,
            @"(?m)^\s*#\s*ifndef\s+(?<name>[A-Za-z_]\w*)\s*$\s*#\s*define\s+\k<name>\b",
            RegexOptions.CultureInvariant);
        return guard.Success ? guard.Groups["name"].Value : null;
    }

    private static bool IsPreprocessorDirective(string text, int index)
    {
        int lineStart = text.LastIndexOf('\n', Math.Max(0, index - 1));
        lineStart = lineStart < 0 ? 0 : lineStart + 1;
        return text[lineStart..index].TrimStart().StartsWith('#');
    }

    private static bool IsCpp(SourceDocument document) =>
        document.RelativePath.EndsWith(".cpp", StringComparison.OrdinalIgnoreCase);

    private static bool IsHeader(SourceDocument document) =>
        document.RelativePath.EndsWith(".h", StringComparison.OrdinalIgnoreCase);

    private static ToolException ErrorAt(SourceDocument document, int index, string message) =>
        new($"{document.RelativePath}:{CppText.GetLineNumber(document.Text, index)}: {message}");
}
