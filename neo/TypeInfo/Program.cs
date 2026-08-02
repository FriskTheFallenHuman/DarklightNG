using System.Text;
using System.Text.RegularExpressions;

namespace Darklight.TypeInfo;

internal static class Program
{
    public static int Main(string[] args)
    {
        try
        {
            ToolOptions options = ToolOptions.Parse(args);

            SourceModel model = SourceScanner.Scan(options);
            GeneratedFiles generated = TypeInfoGenerator.Generate(model);

            if (options.Command == ToolCommand.Verify)
            {
                List<string> stale = new();
                GeneratedFileWriter.Verify(options.HeaderPath!, generated.Header, stale);
                GeneratedFileWriter.Verify(options.ScriptPath!, generated.Script, stale);

                if (stale.Count != 0)
                {
                    Console.Error.WriteLine("Generated TypeInfo output is stale:");
                    foreach (string path in stale)
                    {
                        Console.Error.WriteLine($"  {path}");
                    }

                    return 2;
                }
            }
            else
            {
                int changed = 0;
                changed += GeneratedFileWriter.WriteIfChanged(options.HeaderPath!, generated.Header) ? 1 : 0;
                changed += GeneratedFileWriter.WriteIfChanged(options.ScriptPath!, generated.Script) ? 1 : 0;

                if (options.StampPath is not null)
                {
                    GeneratedFileWriter.TouchStamp(options.StampPath);
                }

                Console.WriteLine(
                    $"TypeInfo: {model.Events.Count} events, {model.Classes.Count} classes, " +
                    $"{model.Classes.Sum(type => type.Bindings.Count)} bindings; {changed} output file(s) changed.");
            }

            return 0;
        }
        catch (ToolException exception)
        {
            Console.Error.WriteLine($"TypeInfo error: {exception.Message}");
            return 1;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 1;
        }
    }
}

internal enum ToolCommand
{
    Generate,
    Verify,
}

internal sealed class ToolOptions
{
    public required ToolCommand Command { get; init; }
    public required string SourceRoot { get; init; }
    public string? HeaderPath { get; init; }
    public string? ScriptPath { get; init; }
    public string? StampPath { get; init; }
    public required IReadOnlySet<string> Excludes { get; init; }

    public static ToolOptions Parse(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help")
        {
            throw new ToolException(
                "usage: DoomTypeInfo <generate|verify> --source <game-dir> " +
                "--header <file> --script <file> [--stamp <file>] [--exclude <relative-file>]");
        }

        ToolCommand command = args[0] switch
        {
            "generate" => ToolCommand.Generate,
            "verify" => ToolCommand.Verify,
            _ => throw new ToolException($"unknown command '{args[0]}'"),
        };

        string? source = null;
        string? header = null;
        string? script = null;
        string? stamp = null;
        HashSet<string> excludes = new(StringComparer.OrdinalIgnoreCase);

        for (int index = 1; index < args.Length; index++)
        {
            string option = args[index];
            string Value()
            {
                if (++index >= args.Length)
                {
                    throw new ToolException($"missing value for '{option}'");
                }

                return args[index];
            }

            switch (option)
            {
                case "--source":
                    source = Value();
                    break;
                case "--header":
                    header = Value();
                    break;
                case "--script":
                    script = Value();
                    break;
                case "--stamp":
                    stamp = Value();
                    break;
                case "--exclude":
                    excludes.Add(NormalizeRelativePath(Value()));
                    break;
                default:
                    throw new ToolException($"unknown option '{option}'");
            }
        }

        if (source is null)
        {
            throw new ToolException("--source is required");
        }

        source = Path.GetFullPath(source);
        if (!Directory.Exists(source))
        {
            throw new ToolException($"source directory does not exist: {source}");
        }

        if (command is ToolCommand.Generate or ToolCommand.Verify &&
            (header is null || script is null))
        {
            throw new ToolException("generate and verify require --header and --script");
        }

        return new ToolOptions
        {
            Command = command,
            SourceRoot = source,
            HeaderPath = header is null ? null : Path.GetFullPath(header),
            ScriptPath = script is null ? null : Path.GetFullPath(script),
            StampPath = stamp is null ? null : Path.GetFullPath(stamp),
            Excludes = excludes,
        };
    }

    public bool IsExcluded(string relativePath)
    {
        string normalized = NormalizeRelativePath(relativePath);
        return Excludes.Contains(normalized) ||
            Excludes.Any(exclude =>
                !exclude.Contains('/') &&
                string.Equals(Path.GetFileName(normalized), exclude, StringComparison.OrdinalIgnoreCase));
    }

    public static string NormalizeRelativePath(string path) => path.Replace('\\', '/').TrimStart('/');
}

internal sealed class ToolException : Exception
{
    public ToolException(string message)
        : base(message)
    {
    }
}

internal readonly record struct SourceLocation(string File, int Line)
{
    public override string ToString() => $"{File}:{Line}";
}

internal sealed record ConditionalContext(IReadOnlyList<IReadOnlyList<string>> Frames)
{
    public static readonly ConditionalContext Empty = new(Array.Empty<IReadOnlyList<string>>());
}

internal sealed record EventDefinition(
    string Symbol,
    IReadOnlyList<string> ConstructorArguments,
    IReadOnlyList<string> ParameterNames,
    SourceLocation Location,
    ConditionalContext Condition);

internal sealed record EventBinding(
    string EventSymbol,
    string Callback,
    string? InferredFormat,
    SourceLocation Location);

internal sealed class ClassDefinition
{
    public required string SuperClass { get; init; }
    public required string Name { get; init; }
    public required bool IsAbstract { get; init; }
    public required SourceLocation Location { get; init; }
    public required ConditionalContext Condition { get; init; }
    public List<EventBinding> Bindings { get; } = new();
}

internal sealed record SourceDocument(string RelativePath, string Text, string MaskedText);

internal sealed class SourceModel
{
    public required IReadOnlyList<EventDefinition> Events { get; init; }
    public required IReadOnlyList<ClassDefinition> Classes { get; init; }
}

internal sealed record MacroInvocation(
    string Name,
    IReadOnlyList<string> Arguments,
    int Start,
    int End,
    int Line);

internal sealed record EventSignature(string Command, string Format, string ReturnType)
{
    public static EventSignature From(EventDefinition definition)
    {
        string command = CppText.ParseCppString(definition.ConstructorArguments[0], definition.Location);
        string format = definition.ConstructorArguments.Count >= 2
            ? CppText.ParseNullableCppString(definition.ConstructorArguments[1], definition.Location)
            : string.Empty;
        string returnType = definition.ConstructorArguments.Count >= 3
            ? CppText.ParseCppCharacter(definition.ConstructorArguments[2], definition.Location)
            : "void";
        return new EventSignature(command, format, returnType);
    }
}

internal static class TypeInfoGenerator
{
    private static readonly HashSet<string> ScriptKeywords = new(StringComparer.Ordinal)
    {
        "boolean", "break", "continue", "else", "entity", "float", "for", "if",
        "namespace", "object", "return", "scriptEvent", "string", "sys", "thread",
        "vector", "void", "while",
    };

    public static GeneratedFiles Generate(SourceModel model)
    {
        return new GeneratedFiles(
            GenerateHeader(model),
            GenerateScript(model));
    }

    private static string GenerateHeader(SourceModel model)
    {
        StringBuilder output = BeginGeneratedFile("C++ TypeInfo metadata");
        output.AppendLine("// This file is intentionally includable twice: Event.h exposes the event");
        output.AppendLine("// declarations, while gamesys/Class.cpp defines D3_TYPEINFO_IMPLEMENTATION");
        output.AppendLine("// for the one and only implementation include.");
        output.AppendLine();
        output.AppendLine("#ifndef __DOOM_TYPEINFO_GENERATED_DECLARATIONS_H__");
        output.AppendLine("#define __DOOM_TYPEINFO_GENERATED_DECLARATIONS_H__");
        output.AppendLine();
        output.AppendLine("class idEventDef;");
        output.AppendLine();

        string? currentFile = null;
        foreach (EventDefinition eventDefinition in model.Events)
        {
            if (currentFile != eventDefinition.Location.File)
            {
                currentFile = eventDefinition.Location.File;
                output.AppendLine($"// {currentFile}");
            }

            AppendConditionStart(output, eventDefinition.Condition);
            output.AppendLine($"extern const idEventDef {eventDefinition.Symbol};");
            AppendConditionEnd(output, eventDefinition.Condition);
        }

        output.AppendLine();
        output.AppendLine("#endif // __DOOM_TYPEINFO_GENERATED_DECLARATIONS_H__");
        output.AppendLine();
        output.AppendLine("#if defined( D3_TYPEINFO_IMPLEMENTATION ) && !defined( __DOOM_TYPEINFO_GENERATED_IMPLEMENTATION_H__ )");
        output.AppendLine("#define __DOOM_TYPEINFO_GENERATED_IMPLEMENTATION_H__");
        output.AppendLine();
        output.AppendLine("/* idEventDef instances */");
        output.AppendLine();

        currentFile = null;
        foreach (EventDefinition eventDefinition in model.Events)
        {
            if (currentFile != eventDefinition.Location.File)
            {
                currentFile = eventDefinition.Location.File;
                output.AppendLine($"// {currentFile}");
            }

            AppendConditionStart(output, eventDefinition.Condition);
            output.Append("const idEventDef ");
            output.Append(eventDefinition.Symbol);
            output.Append("( ");
            output.Append(string.Join(", ", eventDefinition.ConstructorArguments));
            output.AppendLine(" );");
            AppendConditionEnd(output, eventDefinition.Condition);
        }

        output.AppendLine();
        output.AppendLine("/* idTypeInfo instances and callback tables */");
        output.AppendLine();
        currentFile = null;
        foreach (ClassDefinition classDefinition in model.Classes)
        {
            if (currentFile != classDefinition.Location.File)
            {
                currentFile = classDefinition.Location.File;
                output.AppendLine($"// {currentFile}");
            }

            AppendConditionStart(output, classDefinition.Condition);
            output.Append(classDefinition.IsAbstract ? "ABSTRACT_DECLARATION" : "CLASS_DECLARATION");
            output.Append("( ");
            output.Append(classDefinition.SuperClass);
            output.Append(", ");
            output.Append(classDefinition.Name);
            output.AppendLine(" )");
            foreach (EventBinding binding in classDefinition.Bindings)
            {
                output.Append("\tEVENT( ");
                output.Append(binding.EventSymbol);
                output.Append(", ");
                output.Append(binding.Callback);
                output.AppendLine(" )");
            }
            output.AppendLine("END_CLASS");
            AppendConditionEnd(output, classDefinition.Condition);
            output.AppendLine();
        }

        output.AppendLine("#endif // D3_TYPEINFO_IMPLEMENTATION");
        return output.ToString();
    }

    private static string GenerateScript(SourceModel model)
    {
        Dictionary<string, EventDefinition> events = model.Events.ToDictionary(item => item.Symbol, StringComparer.Ordinal);
        StringBuilder output = BeginGeneratedFile("Doom script events");
        output.AppendLine("#ifndef __DOOM_EVENTS_SCRIPT__");
        output.AppendLine("#define __DOOM_EVENTS_SCRIPT__");
        output.AppendLine();
        output.AppendLine("/*");
        output.AppendLine("================================================");
        output.AppendLine();
        output.AppendLine("\tC++ class hierarchy");
        output.AppendLine();
        output.AppendLine("================================================");

        foreach (string line in BuildHierarchy(model.Classes))
        {
            output.AppendLine(line);
        }

        output.AppendLine("*/");
        output.AppendLine();

        HashSet<string> emittedCommands = new(StringComparer.Ordinal);

        void AppendScriptEvent(EventDefinition definition, EventSignature signature)
        {
            output.Append("scriptEvent ");
            output.Append(MapScriptType(signature.ReturnType));
            output.Append(' ');
            output.Append(signature.Command);
            output.Append('(');

            if (signature.Format.Length != 0)
            {
                output.Append(' ');
            }

            for (int index = 0; index < signature.Format.Length; index++)
            {
                if (index != 0)
                {
                    output.Append(", ");
                }

                output.Append(MapScriptType(signature.Format[index].ToString()));
                output.Append(' ');
                output.Append(index < definition.ParameterNames.Count
                    ? SanitizeScriptName(definition.ParameterNames[index], index)
                    : $"arg{index + 1}");
            }

            if (signature.Format.Length != 0)
            {
                output.Append(' ');
            }

            output.AppendLine(");");
        }

        HashSet<string> boundSymbols = model.Classes
            .SelectMany(item => item.Bindings)
            .Select(item => item.EventSymbol)
            .ToHashSet(StringComparer.Ordinal);
        List<(EventDefinition Definition, EventSignature Signature)> unbound = model.Events
            .Where(item => !boundSymbols.Contains(item.Symbol))
            .Select(item => (Definition: item, Signature: EventSignature.From(item)))
            .Where(item => IsScriptVisible(item.Signature) && emittedCommands.Add(item.Signature.Command))
            .ToList();
        if (unbound.Count != 0)
        {
            output.AppendLine("/* Namespace event signatures */");
            foreach (var item in unbound)
            {
                AppendScriptEvent(item.Definition, item.Signature);
            }
            output.AppendLine();
        }

        foreach (ClassDefinition classDefinition in OrderByHierarchy(model.Classes))
        {
            List<(EventBinding Binding, EventDefinition Definition, EventSignature Signature)> visible = new();
            HashSet<string> emittedSignatures = new(StringComparer.Ordinal);

            foreach (EventBinding binding in classDefinition.Bindings)
            {
                EventDefinition definition = events[binding.EventSymbol];
                EventSignature signature = EventSignature.From(definition);
                if (!IsScriptVisible(signature))
                {
                    continue;
                }

                string key = $"{signature.Command}\0{signature.Format}\0{signature.ReturnType}";
                if (emittedSignatures.Add(key) && emittedCommands.Add(signature.Command))
                {
                    visible.Add((binding, definition, signature));
                }
            }

            if (visible.Count == 0)
            {
                continue;
            }

            output.AppendLine("/*");
            output.AppendLine("================================================");
            output.AppendLine($"\t{classDefinition.Name}");
            output.AppendLine("================================================");
            output.AppendLine("*/");
            output.AppendLine();

            foreach (var item in visible)
            {
                AppendScriptEvent(item.Definition, item.Signature);
            }

            output.AppendLine();
        }

        output.AppendLine("#endif // __DOOM_EVENTS_SCRIPT__");
        return output.ToString();
    }

    private static bool IsScriptVisible(EventSignature signature)
    {
        return signature.Command.Length != 0 &&
            signature.Command[0] is not '<' and not '_' &&
            !signature.Format.Contains('t') &&
            signature.ReturnType != "t";
    }

    private static string MapScriptType(string eventType)
    {
        return eventType switch
        {
            "void" or "\\0" => "void",
            "d" or "f" => "float",
            "v" => "vector",
            "s" => "string",
            "e" or "E" => "entity",
            _ => throw new ToolException($"event type '{eventType}' cannot be represented in Doom script"),
        };
    }

    private static string SanitizeScriptName(string name, int index)
    {
        string value = name.Trim();
        return Regex.IsMatch(value, @"^[A-Za-z_]\w*$", RegexOptions.CultureInvariant) &&
            !ScriptKeywords.Contains(value)
            ? value
            : $"arg{index + 1}";
    }

    private static IReadOnlyList<ClassDefinition> OrderByHierarchy(IReadOnlyList<ClassDefinition> classes)
    {
        Dictionary<string, List<ClassDefinition>> children = classes
            .GroupBy(item => item.SuperClass, StringComparer.Ordinal)
            .ToDictionary(
                group => group.Key,
                group => group.OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase).ToList(),
                StringComparer.Ordinal);
        List<ClassDefinition> ordered = new();

        void Visit(ClassDefinition item)
        {
            ordered.Add(item);
            if (children.TryGetValue(item.Name, out List<ClassDefinition>? descendants))
            {
                foreach (ClassDefinition child in descendants)
                {
                    Visit(child);
                }
            }
        }

        foreach (ClassDefinition root in classes
            .Where(item => item.SuperClass == "NULL")
            .OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase))
        {
            Visit(root);
        }

        return ordered;
    }

    private static IReadOnlyList<string> BuildHierarchy(IReadOnlyList<ClassDefinition> classes)
    {
        Dictionary<string, List<ClassDefinition>> children = classes
            .GroupBy(item => item.SuperClass, StringComparer.Ordinal)
            .ToDictionary(
                group => group.Key,
                group => group.OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase).ToList(),
                StringComparer.Ordinal);
        List<string> lines = new();

        void Visit(ClassDefinition item, string prefix, bool isLast, bool isRoot)
        {
            lines.Add(isRoot ? $"+- {item.Name}" : $"{prefix}{(isLast ? "+-" : "|-")} {item.Name}");
            if (!children.TryGetValue(item.Name, out List<ClassDefinition>? descendants))
            {
                return;
            }

            string childPrefix = isRoot ? "   " : prefix + (isLast ? "   " : "|  ");
            for (int index = 0; index < descendants.Count; index++)
            {
                Visit(descendants[index], childPrefix, index == descendants.Count - 1, isRoot: false);
            }
        }

        List<ClassDefinition> roots = classes
            .Where(item => item.SuperClass == "NULL")
            .OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase)
            .ToList();
        for (int index = 0; index < roots.Count; index++)
        {
            Visit(roots[index], string.Empty, index == roots.Count - 1, isRoot: true);
        }

        return lines;
    }

    private static StringBuilder BeginGeneratedFile(string purpose)
    {
        StringBuilder output = new();
        output.AppendLine("/*");
        output.AppendLine("===============================================================================" );
        output.AppendLine();
        output.AppendLine($"\tAuto-generated {purpose}.");
        output.AppendLine("\tGenerated by neo/TypeInfo. Do not edit by hand.");
        output.AppendLine();
        output.AppendLine("===============================================================================" );
        output.AppendLine("*/");
        output.AppendLine();
        return output;
    }

    private static void AppendConditionStart(StringBuilder output, ConditionalContext context)
    {
        foreach (IReadOnlyList<string> frame in context.Frames)
        {
            foreach (string directive in frame)
            {
                output.AppendLine(directive);
            }
        }
    }

    private static void AppendConditionEnd(StringBuilder output, ConditionalContext context)
    {
        for (int index = context.Frames.Count - 1; index >= 0; index--)
        {
            output.AppendLine("#endif");
        }
    }
}

internal sealed record GeneratedFiles(string Header, string Script);

internal static class GeneratedFileWriter
{
    public static bool WriteIfChanged(string path, string contents)
    {
        if (File.Exists(path) && File.ReadAllText(path) == contents)
        {
            return false;
        }

        string? directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        string temporary = $"{path}.{Environment.ProcessId}.tmp";
        File.WriteAllText(temporary, contents, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
        File.Move(temporary, path, overwrite: true);
        return true;
    }

    public static void Verify(string path, string expected, ICollection<string> stale)
    {
        if (!File.Exists(path) || File.ReadAllText(path) != expected)
        {
            stale.Add(path);
        }
    }

    public static void TouchStamp(string path)
    {
        string? directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrEmpty(directory))
        {
            Directory.CreateDirectory(directory);
        }

        File.WriteAllText(path, DateTime.UtcNow.ToString("O") + Environment.NewLine);
    }
}

internal static class CppText
{
    public static IReadOnlyList<MacroInvocation> FindMacros(string text, IReadOnlySet<string> names)
    {
        List<MacroInvocation> result = new();
        int index = 0;
        State state = State.Normal;

        while (index < text.Length)
        {
            char current = text[index];
            char next = index + 1 < text.Length ? text[index + 1] : '\0';

            switch (state)
            {
                case State.LineComment:
                    if (current is '\r' or '\n')
                    {
                        state = State.Normal;
                    }
                    index++;
                    continue;
                case State.BlockComment:
                    if (current == '*' && next == '/')
                    {
                        state = State.Normal;
                        index += 2;
                    }
                    else
                    {
                        index++;
                    }
                    continue;
                case State.String:
                    index = AdvanceQuoted(text, index, '"', ref state);
                    continue;
                case State.Character:
                    index = AdvanceQuoted(text, index, '\'', ref state);
                    continue;
            }

            if (current == '/' && next == '/')
            {
                state = State.LineComment;
                index += 2;
                continue;
            }
            if (current == '/' && next == '*')
            {
                state = State.BlockComment;
                index += 2;
                continue;
            }
            if (current == '"')
            {
                state = State.String;
                index++;
                continue;
            }
            if (current == '\'')
            {
                state = State.Character;
                index++;
                continue;
            }

            if (!IsIdentifierStart(current))
            {
                index++;
                continue;
            }

            int start = index++;
            while (index < text.Length && IsIdentifierPart(text[index]))
            {
                index++;
            }

            string name = text[start..index];
            if (!names.Contains(name))
            {
                continue;
            }

            int openParen = index;
            while (openParen < text.Length && char.IsWhiteSpace(text[openParen]))
            {
                openParen++;
            }

            if (openParen >= text.Length || text[openParen] != '(')
            {
                continue;
            }

            int closeParen = FindMatchingDelimiter(text, openParen, '(', ')');
            if (closeParen < 0)
            {
                throw new ToolException($"line {GetLineNumber(text, start)}: unterminated {name} tag");
            }

            string argumentText = text[(openParen + 1)..closeParen];
            IReadOnlyList<string> arguments = SplitTopLevel(argumentText, ',')
                .Select(argument => argument.Trim())
                .Where(argument => argument.Length != 0)
                .ToArray();
            result.Add(new MacroInvocation(name, arguments, start, closeParen + 1, GetLineNumber(text, start)));
            index = closeParen + 1;
        }

        return result;
    }

    public static IReadOnlyDictionary<int, ConditionalContext> BuildConditionalContexts(string text)
    {
        Dictionary<int, ConditionalContext> result = new();
        List<List<string>> stack = new();
        string[] lines = Regex.Split(text, "\r\n|\n|\r", RegexOptions.CultureInvariant);
        Regex directive = new(@"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b", RegexOptions.CultureInvariant);

        for (int index = 0; index < lines.Length; index++)
        {
            int lineNumber = index + 1;
            result[lineNumber] = Snapshot(stack);
            Match match = directive.Match(lines[index]);
            if (!match.Success)
            {
                continue;
            }

            string kind = match.Groups[1].Value;
            string normalized = lines[index].Trim();
            switch (kind)
            {
                case "if":
                case "ifdef":
                case "ifndef":
                    stack.Add(new List<string> { normalized });
                    break;
                case "elif":
                case "else":
                    if (stack.Count != 0)
                    {
                        stack[^1].Add(normalized);
                    }
                    break;
                case "endif":
                    if (stack.Count != 0)
                    {
                        stack.RemoveAt(stack.Count - 1);
                    }
                    break;
            }
        }

        return result;
    }

    public static string MaskCommentsAndStrings(string text, bool preserveStrings)
    {
        char[] result = text.ToCharArray();
        State state = State.Normal;

        for (int index = 0; index < text.Length; index++)
        {
            char current = text[index];
            char next = index + 1 < text.Length ? text[index + 1] : '\0';
            switch (state)
            {
                case State.Normal:
                    if (current == '/' && next == '/')
                    {
                        result[index] = result[index + 1] = ' ';
                        state = State.LineComment;
                        index++;
                    }
                    else if (current == '/' && next == '*')
                    {
                        result[index] = result[index + 1] = ' ';
                        state = State.BlockComment;
                        index++;
                    }
                    else if (current == '"')
                    {
                        if (!preserveStrings)
                        {
                            result[index] = ' ';
                        }
                        state = State.String;
                    }
                    else if (current == '\'')
                    {
                        result[index] = ' ';
                        state = State.Character;
                    }
                    break;
                case State.LineComment:
                    if (current is '\r' or '\n')
                    {
                        state = State.Normal;
                    }
                    else
                    {
                        result[index] = ' ';
                    }
                    break;
                case State.BlockComment:
                    if (current == '*' && next == '/')
                    {
                        result[index] = result[index + 1] = ' ';
                        state = State.Normal;
                        index++;
                    }
                    else if (current is not '\r' and not '\n')
                    {
                        result[index] = ' ';
                    }
                    break;
                case State.String:
                case State.Character:
                    bool preserve = state == State.String && preserveStrings;
                    if (current == '\\' && index + 1 < text.Length)
                    {
                        if (!preserve)
                        {
                            result[index] = result[index + 1] = ' ';
                        }
                        index++;
                    }
                    else if ((state == State.String && current == '"') ||
                             (state == State.Character && current == '\''))
                    {
                        if (!preserve)
                        {
                            result[index] = ' ';
                        }
                        state = State.Normal;
                    }
                    else if (!preserve && current is not '\r' and not '\n')
                    {
                        result[index] = ' ';
                    }
                    break;
            }
        }

        return new string(result);
    }

    public static int FindMatchingDelimiter(string text, int openIndex, char open, char close)
    {
        int depth = 0;
        State state = State.Normal;
        for (int index = openIndex; index < text.Length; index++)
        {
            char current = text[index];
            char next = index + 1 < text.Length ? text[index + 1] : '\0';
            switch (state)
            {
                case State.LineComment:
                    if (current is '\r' or '\n')
                    {
                        state = State.Normal;
                    }
                    continue;
                case State.BlockComment:
                    if (current == '*' && next == '/')
                    {
                        state = State.Normal;
                        index++;
                    }
                    continue;
                case State.String:
                    if (current == '\\')
                    {
                        index++;
                    }
                    else if (current == '"')
                    {
                        state = State.Normal;
                    }
                    continue;
                case State.Character:
                    if (current == '\\')
                    {
                        index++;
                    }
                    else if (current == '\'')
                    {
                        state = State.Normal;
                    }
                    continue;
            }

            if (current == '/' && next == '/')
            {
                state = State.LineComment;
                index++;
            }
            else if (current == '/' && next == '*')
            {
                state = State.BlockComment;
                index++;
            }
            else if (current == '"')
            {
                state = State.String;
            }
            else if (current == '\'')
            {
                state = State.Character;
            }
            else if (current == open)
            {
                depth++;
            }
            else if (current == close && --depth == 0)
            {
                return index;
            }
        }

        return -1;
    }

    public static IReadOnlyList<string> SplitTopLevel(string text, char delimiter)
    {
        List<string> result = new();
        int start = 0;
        int parentheses = 0;
        int brackets = 0;
        int braces = 0;
        int angles = 0;
        State state = State.Normal;

        for (int index = 0; index < text.Length; index++)
        {
            char current = text[index];
            char next = index + 1 < text.Length ? text[index + 1] : '\0';
            switch (state)
            {
                case State.String:
                    if (current == '\\')
                    {
                        index++;
                    }
                    else if (current == '"')
                    {
                        state = State.Normal;
                    }
                    continue;
                case State.Character:
                    if (current == '\\')
                    {
                        index++;
                    }
                    else if (current == '\'')
                    {
                        state = State.Normal;
                    }
                    continue;
                case State.LineComment:
                    if (current is '\r' or '\n')
                    {
                        state = State.Normal;
                    }
                    continue;
                case State.BlockComment:
                    if (current == '*' && next == '/')
                    {
                        state = State.Normal;
                        index++;
                    }
                    continue;
            }

            if (current == '"') state = State.String;
            else if (current == '\'') state = State.Character;
            else if (current == '/' && next == '/') { state = State.LineComment; index++; }
            else if (current == '/' && next == '*') { state = State.BlockComment; index++; }
            else if (current == '(') parentheses++;
            else if (current == ')') parentheses--;
            else if (current == '[') brackets++;
            else if (current == ']') brackets--;
            else if (current == '{') braces++;
            else if (current == '}') braces--;
            else if (current == '<') angles++;
            else if (current == '>' && angles > 0) angles--;
            else if (current == delimiter && parentheses == 0 && brackets == 0 && braces == 0 && angles == 0)
            {
                result.Add(text[start..index]);
                start = index + 1;
            }
        }

        result.Add(text[start..]);
        return result;
    }

    public static IReadOnlyList<string> ExtractParameterNames(string parameters)
    {
        if (string.IsNullOrWhiteSpace(parameters) || parameters.Trim() == "void")
        {
            return Array.Empty<string>();
        }

        List<string> names = new();
        Regex identifier = new(@"[A-Za-z_]\w*", RegexOptions.CultureInvariant);
        foreach (string rawParameter in SplitTopLevel(parameters, ','))
        {
            string parameter = SplitTopLevel(rawParameter, '=')[0];
            MatchCollection matches = identifier.Matches(parameter);
            string? name = matches
                .Cast<Match>()
                .Select(match => match.Value)
                .LastOrDefault(value => value is not "const" and not "class" and not "struct" and not "volatile");
            names.Add(name ?? $"arg{names.Count + 1}");
        }

        return names;
    }

    public static bool ContainsOnlyTrivia(string text, (int Start, int End) range)
    {
        if (range.Start > range.End || range.Start < 0 || range.End > text.Length)
        {
            return false;
        }

        string between = text[range.Start..range.End];
        string masked = MaskCommentsAndStrings(between, preserveStrings: false);
        return masked.All(char.IsWhiteSpace);
    }

    public static string ParseCppString(string expression, SourceLocation location)
    {
        string value = expression.Trim();
        if (value.Length < 2 || value[0] != '"' || value[^1] != '"')
        {
            throw new ToolException($"{location}: expected a string literal, got '{expression}'");
        }

        try
        {
            return Regex.Unescape(value[1..^1]);
        }
        catch (ArgumentException exception)
        {
            throw new ToolException($"{location}: invalid C++ string literal '{expression}': {exception.Message}");
        }
    }

    public static string ParseNullableCppString(string expression, SourceLocation location)
    {
        string value = expression.Trim();
        return value is "NULL" or "nullptr" ? string.Empty : ParseCppString(value, location);
    }

    public static string ParseCppCharacter(string expression, SourceLocation location)
    {
        string value = expression.Trim();
        if (value is "D_EVENT_VOID" or "0" or "NULL")
        {
            return "void";
        }

        if (value.Length == 3 && value[0] == '\'' && value[2] == '\'')
        {
            return value[1].ToString();
        }

        throw new ToolException($"{location}: expected an event return character, got '{expression}'");
    }

    public static int GetLineNumber(string text, int index)
    {
        int line = 1;
        for (int position = 0; position < index && position < text.Length; position++)
        {
            if (text[position] == '\n')
            {
                line++;
            }
        }

        return line;
    }

    private static ConditionalContext Snapshot(IReadOnlyList<List<string>> stack)
    {
        if (stack.Count == 0)
        {
            return ConditionalContext.Empty;
        }

        return new ConditionalContext(stack.Select(frame => (IReadOnlyList<string>)frame.ToArray()).ToArray());
    }

    private static int AdvanceQuoted(string text, int index, char quote, ref State state)
    {
        char current = text[index];
        if (current == '\\' && index + 1 < text.Length)
        {
            return index + 2;
        }

        if (current == quote)
        {
            state = State.Normal;
        }

        return index + 1;
    }

    private static bool IsIdentifierStart(char value) => value == '_' || char.IsLetter(value);
    private static bool IsIdentifierPart(char value) => value == '_' || char.IsLetterOrDigit(value);

    private enum State
    {
        Normal,
        LineComment,
        BlockComment,
        String,
        Character,
    }
}
