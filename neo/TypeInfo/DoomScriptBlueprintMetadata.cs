using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace Darklight.TypeInfo;

internal static class DoomScriptBlueprintMetadata
{
    public const string BeginMarker = "//@doomscript-blueprint begin 6";
    private const string BeginPrefix = "//@doomscript-blueprint begin ";
    public const string EndMarker = "//@doomscript-blueprint end";

    private sealed record LayoutNode(string Id, int X, int Y);

    public static int Migrate(string[] args)
    {
        string? root = null;
        bool verify = false;
        for (int index = 0; index < args.Length; index++)
        {
            switch (args[index])
            {
                case "--scripts":
                    if (++index == args.Length)
                    {
                        throw new ToolException("missing value for --scripts");
                    }
                    root = Path.GetFullPath(args[index]);
                    break;
                case "--verify":
                    verify = true;
                    break;
                default:
                    throw new ToolException($"unknown migrate-blueprints option '{args[index]}'");
            }
        }

        if (root is null || !Directory.Exists(root))
        {
            throw new ToolException("migrate-blueprints requires an existing --scripts directory");
        }

        int changed = 0;
        int nodes = 0;
        string catalogPath = Path.Combine(Directory.GetParent(root)!.FullName, "editors", "doomscript_nodes.def");
        string catalogChecksum = File.Exists(catalogPath) ? Checksum(File.ReadAllText(catalogPath)) : "missing";
        string[] files = Directory.GetFiles(root, "*.script", SearchOption.AllDirectories);
        Array.Sort(files, StringComparer.OrdinalIgnoreCase);
        List<string> sharedVariables = DiscoverSharedVariables(files);
        foreach (string path in files)
        {
            string current = File.ReadAllText(path);
            string expected = Append(current, catalogChecksum, sharedVariables);
            nodes += Scan(Remove(current), sharedVariables).Count;
            if (current == expected)
            {
                continue;
            }

            changed++;
            if (!verify)
            {
                GeneratedFileWriter.WriteIfChanged(path, expected);
            }
        }

        Console.WriteLine($"DoomScript Blueprint metadata: {files.Length} scripts, {nodes} nodes, {changed} {(verify ? "stale" : "updated")}.");
        return verify && changed != 0 ? 2 : 0;
    }

    public static string Append(string input, string catalogChecksum, IReadOnlyList<string>? sharedVariables = null)
    {
        string source = Remove(input).TrimEnd('\r', '\n') + Environment.NewLine;
        List<LayoutNode> generated = Scan(source, sharedVariables ?? Array.Empty<string>());
        Dictionary<string, LayoutNode> existing = ReadLayout(input);

        StringBuilder result = new(source);
        result.AppendLine();
        result.AppendLine(BeginMarker);
        result.Append("//@source ");
        result.AppendLine(Hash(source).ToString("X8", CultureInfo.InvariantCulture));
        result.Append("//@catalog ");
        result.AppendLine(catalogChecksum);
        foreach (LayoutNode node in generated)
        {
            LayoutNode position = existing.TryGetValue(node.Id, out LayoutNode? old) ? old : node;
            result.Append("//@node ");
            result.Append(node.Id);
            result.Append(' ');
            result.Append(position.X.ToString(CultureInfo.InvariantCulture));
            result.Append(' ');
            result.AppendLine(position.Y.ToString(CultureInfo.InvariantCulture));
        }
        result.AppendLine(EndMarker);
        return result.ToString();
    }

    public static string Remove(string input)
    {
        int marker = input.LastIndexOf(BeginPrefix, StringComparison.Ordinal);
        if (marker < 0)
        {
            return input;
        }

        int end = input.IndexOf(EndMarker, marker, StringComparison.Ordinal);
        if (end < 0 || input[(end + EndMarker.Length)..].Trim().Length != 0)
        {
            return input;
        }

        return input[..marker].TrimEnd('\r', '\n');
    }

    private static Dictionary<string, LayoutNode> ReadLayout(string input)
    {
        Dictionary<string, LayoutNode> result = new(StringComparer.Ordinal);
        int marker = input.LastIndexOf(BeginPrefix, StringComparison.Ordinal);
        if (marker < 0)
        {
            return result;
        }

        foreach (string line in input[marker..].Split('\n'))
        {
            Match match = Regex.Match(line, @"^//@node ([0-9A-F]{8}-\d+) (-?\d+) (-?\d+)\r?$", RegexOptions.CultureInvariant);
            if (match.Success &&
                int.TryParse(match.Groups[2].Value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int x) &&
                int.TryParse(match.Groups[3].Value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int y))
            {
                result[match.Groups[1].Value] = new LayoutNode(match.Groups[1].Value, x, y);
            }
        }
        return result;
    }

    private static List<string> DiscoverSharedVariables(IEnumerable<string> files)
    {
        List<string> result = new();
        foreach (string path in files)
        {
            string source = Remove(File.ReadAllText(path)).Replace("\r\n", "\n", StringComparison.Ordinal);
            int braceDepth = 0;
            int objectDepth = -1;
            bool blockComment = false;
            foreach (string raw in source.Split('\n'))
            {
                string code = StripComments(raw, ref blockComment).Trim();
                if (objectDepth < 0 && Regex.IsMatch(code, @"^object\b", RegexOptions.CultureInvariant) && code.Contains('{'))
                {
                    objectDepth = braceDepth + Count(code, '{') - Count(code, '}');
                }
                bool declarationScope = objectDepth >= 0 ? braceDepth == objectDepth : braceDepth == 0;
                if (declarationScope && TryParseVariableNames(NormalizeStatement(code), out List<string> declarations))
                {
                    foreach (string name in declarations)
                    {
                        if (!result.Contains(name, StringComparer.Ordinal)) result.Add(name);
                    }
                }
                braceDepth += Count(code, '{') - Count(code, '}');
                if (objectDepth >= 0 && braceDepth < objectDepth) objectDepth = -1;
            }
        }
        return result;
    }

    private static List<LayoutNode> Scan(string source, IReadOnlyList<string> sharedVariables)
    {
        List<LayoutNode> result = new();
        Dictionary<uint, int> occurrences = new();
        string scope = "document";
        int braceDepth = 0;
        int functionDepth = -1;
        int logicColumn = 0;
        string? pendingHeader = null;
        bool blockComment = false;
        List<string> globalVariables = new(sharedVariables);
        List<string> localVariables = new();
        List<string> parameters = new();
        string[] lines = source.Replace("\r\n", "\n", StringComparison.Ordinal).Split('\n');

        for (int lineIndex = 0; lineIndex < lines.Length; lineIndex++)
        {
            string code = StripComments(lines[lineIndex], ref blockComment).Trim();
            if (code.Length == 0 || code[0] == '#')
            {
                continue;
            }

            if (functionDepth < 0)
            {
                if (pendingHeader is null && LooksLikeFunctionHeader(code))
                {
                    pendingHeader = code;
                }
                else if (pendingHeader is not null && !pendingHeader.Contains('{'))
                {
                    pendingHeader += " " + code;
                }

                if (pendingHeader is not null && pendingHeader.Contains('{'))
                {
                    scope = FunctionName(pendingHeader);
                    AddNode(result, occurrences, "entry|" + scope, 40, 40);
                    parameters = FunctionParameters(pendingHeader);
                    localVariables.Clear();
                    logicColumn = 0;
                    functionDepth = braceDepth + Count(pendingHeader, '{') - Count(pendingHeader, '}');
                    braceDepth = functionDepth;
                    pendingHeader = null;
                    continue;
                }

                braceDepth += Count(code, '{') - Count(code, '}');
                if (pendingHeader is not null && code.EndsWith(';'))
                {
                    pendingHeader = null;
                }
                else if (pendingHeader is null && TryParseVariableNames(NormalizeStatement(code), out List<string> declaredGlobals))
                {
                    foreach (string name in declaredGlobals)
                    {
                        if (!globalVariables.Contains(name, StringComparer.Ordinal)) globalVariables.Add(name);
                    }
                }
                continue;
            }

            int closes = Count(code, '}');
            int opens = Count(code, '{');
            int nextDepth = braceDepth + opens - closes;
            string normalized = NormalizeStatement(code);
            int statementDepth = Math.Max(functionDepth, braceDepth - LeadingClosingBraces(code));
            if (Regex.IsMatch(normalized, @"^else\s*$", RegexOptions.CultureInvariant))
            {
                // Else is the False execution output of its owning Branch, not
                // a standalone graph node.
                normalized = "";
            }
            if (TryParseVariableNames(normalized, out List<string> declaredLocals))
            {
                foreach (string name in declaredLocals)
                {
                    if (!localVariables.Contains(name, StringComparer.Ordinal)) localVariables.Add(name);
                }
                normalized = "";
            }
            if (normalized.Length != 0 && normalized != "{" && normalized != "}")
            {
                bool assignment = TryParseAssignment(normalized, out string assignmentTarget, out string assignmentValue);
                string kind = assignment ? "setvar" : NodeKind(normalized);
                int depth = Math.Max(0, statementDepth - functionDepth);
                int primaryX = 330 + logicColumn * 280;
                int primaryY = 40 + depth * 160;
                LayoutNode primary = AddNode(result, occurrences, kind + "|" + scope + "|" + normalized, primaryX, primaryY);
                string dataExpression = assignment ? assignmentValue : ExtractControlCondition(normalized);
                bool structuredFunctionExpression = false;
                if (dataExpression.Length != 0 && (kind is "setvar" or "branch" or "loop") &&
                    TryParseNestedFunctionExpression(dataExpression, out string nestedCommand,
                        out List<string> nestedArguments, out bool negateFunction))
                {
                    int callX = primaryX - (negateFunction ? 440 : 225);
                    int callY = primaryY + 105;
                    LayoutNode callNode = AddNode(result, occurrences,
                        "functionexpr|" + scope + "|" + primary.Id + "|" + nestedCommand, callX, callY);
                    AddCallInputLayouts(result, occurrences, scope, callNode, callX, callY, nestedArguments,
                        localVariables, parameters, globalVariables);
                    if (negateFunction)
                    {
                        AddNode(result, occurrences,
                            "operator:not|" + scope + "|" + primary.Id + "|" + nestedCommand,
                            primaryX - 210, primaryY + 105);
                    }
                    structuredFunctionExpression = true;
                }
                if (dataExpression.Length != 0 && (kind is "setvar" or "branch" or "loop") && !structuredFunctionExpression)
                {
                    List<string> references = new();
                    AppendReferences(references, localVariables, dataExpression);
                    AppendReferences(references, parameters, dataExpression);
                    AppendReferences(references, globalVariables, dataExpression);
                    bool directVariable = references.Count == 1 && Normalize(dataExpression) == references[0];
                    if (!directVariable)
                    {
                        string dataKind = references.Count == 0 && IsLiteral(dataExpression) ? "literal" : "expression";
                        AddNode(result, occurrences, dataKind + "|" + scope + "|" + primary.Id + "|" + Normalize(dataExpression),
                            primaryX - 205, primaryY + 105);
                    }
                    for (int referenceIndex = 0; referenceIndex < references.Count; referenceIndex++)
                    {
                        bool parameterReference = parameters.Contains(references[referenceIndex], StringComparer.Ordinal) &&
                            !localVariables.Contains(references[referenceIndex], StringComparer.Ordinal);
                        if (parameterReference) continue;
                        AddNode(result, occurrences, "getvar|" + scope + "|" + primary.Id + "|" + references[referenceIndex],
                            primaryX - (directVariable ? 205 : 410), primaryY + 105 + referenceIndex * 84);
                    }
                }
                if (kind == "call" && TryParseCallArguments(normalized, out List<string> callArguments))
                {
                    AddCallInputLayouts(result, occurrences, scope, primary, primaryX, primaryY, callArguments,
                        localVariables, parameters, globalVariables);
                }
                logicColumn++;
            }

            braceDepth = nextDepth;
            if (braceDepth < functionDepth)
            {
                functionDepth = -1;
                scope = "document";
            }
        }
        return result;
    }

    private static LayoutNode AddNode(List<LayoutNode> output, Dictionary<uint, int> occurrences, string key, int x, int y)
    {
        uint hash = Hash(key);
        occurrences.TryGetValue(hash, out int occurrence);
        occurrences[hash] = occurrence + 1;
        LayoutNode node = new($"{hash:X8}-{occurrence}", x, y);
        output.Add(node);
        return node;
    }

    private static void AddCallInputLayouts(List<LayoutNode> output, Dictionary<uint, int> occurrences,
        string scope, LayoutNode callNode, int callX, int callY, IReadOnlyList<string> arguments,
        IReadOnlyList<string> localVariables, IReadOnlyList<string> parameters, IReadOnlyList<string> globalVariables)
    {
        for (int inputIndex = 0; inputIndex < arguments.Count; inputIndex++)
        {
            string argument = Normalize(arguments[inputIndex]);
            if (argument.Length == 0) continue;
            List<string> references = new();
            AppendReferences(references, localVariables, argument);
            AppendReferences(references, parameters, argument);
            AppendReferences(references, globalVariables, argument);
            bool directVariable = references.Count == 1 && argument == references[0];
            if (!directVariable && references.Count != 0)
            {
                AddNode(output, occurrences,
                    "expression|" + scope + "|" + callNode.Id + $"|input:{inputIndex}|" + argument,
                    callX - 205, callY + 100 + inputIndex * 92);
            }
            for (int referenceIndex = 0; referenceIndex < references.Count; referenceIndex++)
            {
                bool parameterReference = parameters.Contains(references[referenceIndex], StringComparer.Ordinal) &&
                    !localVariables.Contains(references[referenceIndex], StringComparer.Ordinal);
                if (parameterReference) continue;
                AddNode(output, occurrences,
                    "getvar|" + scope + "|" + callNode.Id + $"|input:{inputIndex}|" + references[referenceIndex],
                    callX - (directVariable ? 205 : 410),
                    callY + 100 + inputIndex * 92 + referenceIndex * 78);
            }
        }
    }

    private static bool LooksLikeFunctionHeader(string code)
    {
        if (code.StartsWith("if", StringComparison.Ordinal) ||
            code.StartsWith("while", StringComparison.Ordinal) ||
            code.StartsWith("for", StringComparison.Ordinal) ||
            code.StartsWith("object", StringComparison.Ordinal) ||
            code.StartsWith("namespace", StringComparison.Ordinal) ||
            code.StartsWith("scriptEvent", StringComparison.Ordinal))
        {
            return false;
        }
        return Regex.IsMatch(code, @"^[A-Za-z_]\w*\s+[A-Za-z_]\w*(?:::\w+)?\s*\(", RegexOptions.CultureInvariant);
    }

    private static string FunctionName(string header)
    {
        Match match = Regex.Match(header, @"^[A-Za-z_]\w*\s+([A-Za-z_]\w*(?:::\w+)?)\s*\(", RegexOptions.CultureInvariant);
        return match.Success ? match.Groups[1].Value : "function";
    }

    private static List<string> FunctionParameters(string header)
    {
        List<string> result = new();
        int open = header.IndexOf('(');
        int close = header.LastIndexOf(')');
        if (open < 0 || close <= open) return result;
        foreach (string parameter in header[(open + 1)..close].Split(','))
        {
            Match match = Regex.Match(parameter.Trim(), @"^[A-Za-z_]\w*\s+(?<name>[A-Za-z_]\w*)$", RegexOptions.CultureInvariant);
            if (match.Success) result.Add(match.Groups["name"].Value);
        }
        return result;
    }

    private static string NodeKind(string code)
    {
        if (Regex.IsMatch(code, @"^Set\b", RegexOptions.CultureInvariant)) return "setlocal";
        if (Regex.IsMatch(code, @"^(if|else)\b", RegexOptions.CultureInvariant)) return "branch";
        if (Regex.IsMatch(code, @"^(while|for|do)\b", RegexOptions.CultureInvariant)) return "loop";
        if (Regex.IsMatch(code, @"^return\b", RegexOptions.CultureInvariant)) return "return";
        if (Regex.IsMatch(code, @"^thread\b", RegexOptions.CultureInvariant)) return "thread";
        if (Regex.IsMatch(code, @"^break\b", RegexOptions.CultureInvariant)) return "break";
        if (Regex.IsMatch(code, @"^continue\b", RegexOptions.CultureInvariant)) return "continue";
        if (code.Contains('(')) return "call";
        return "statement";
    }

    private static bool TryParseVariableNames(string code, out List<string> names)
    {
        names = new List<string>();
        Match match = Regex.Match(code, @"^(?<type>[A-Za-z_]\w*)\s+(?<declarators>.+);\s*$", RegexOptions.CultureInvariant);
        if (!match.Success || match.Groups["type"].Value is "return" or "thread" or "break" or "continue" or "scriptEvent")
        {
            return false;
        }

        foreach (string declarator in SplitDeclarators(match.Groups["declarators"].Value))
        {
            Match declaratorMatch = Regex.Match(declarator.Trim(), @"^(?<name>[A-Za-z_]\w*)\s*(?:=\s*.+)?$", RegexOptions.CultureInvariant);
            if (!declaratorMatch.Success)
            {
                names.Clear();
                return false;
            }
            names.Add(declaratorMatch.Groups["name"].Value);
        }
        return names.Count != 0;
    }

    private static bool TryParseAssignment(string code, out string target, out string value)
    {
        Match increment = Regex.Match(code, @"^(?<target>[A-Za-z_]\w*)\s*(?<operator>\+\+|--);$", RegexOptions.CultureInvariant);
        if (increment.Success)
        {
            target = increment.Groups["target"].Value;
            value = target + (increment.Groups["operator"].Value == "++" ? " + 1" : " - 1");
            return true;
        }
        Match match = Regex.Match(code, @"^(?<target>[A-Za-z_]\w*)\s*(?<operator>[+\-*/]?=)(?!=)\s*(?<value>.+);$", RegexOptions.CultureInvariant);
        target = match.Success ? match.Groups["target"].Value : "";
        value = match.Success ? match.Groups["value"].Value.Trim() : "";
        if (!match.Success || value.Length == 0) return false;
        string operation = match.Groups["operator"].Value;
        if (operation.Length == 2) value = $"{target} {operation[0]} ( {value} )";
        return true;
    }

    private static string ExtractControlCondition(string code)
    {
        int open = code.IndexOf('(');
        int close = code.LastIndexOf(')');
        return open >= 0 && close > open ? Normalize(code[(open + 1)..close]) : "";
    }

    private static bool TryParseCallArguments(string code, out List<string> arguments)
    {
        arguments = new List<string>();
        int open = code.IndexOf('(');
        if (open < 0) return false;
        int start = open + 1;
        int nesting = 1;
        char quote = '\0';
        for (int index = start; index < code.Length; index++)
        {
            char current = code[index];
            if (quote != '\0')
            {
                if (current == quote && (index == 0 || code[index - 1] != '\\')) quote = '\0';
                continue;
            }
            if (current is '"' or '\'') { quote = current; continue; }
            if (current is '(' or '[') { nesting++; continue; }
            if (current is ')' or ']')
            {
                nesting--;
                if (nesting == 0)
                {
                    string value = Normalize(code[start..index]);
                    if (value.Length != 0 || arguments.Count != 0) arguments.Add(value);
                    return true;
                }
                continue;
            }
            if (current == ',' && nesting == 1)
            {
                arguments.Add(Normalize(code[start..index]));
                start = index + 1;
            }
        }
        arguments.Clear();
        return false;
    }

    private static bool TryParseNestedFunctionExpression(string expression, out string command,
        out List<string> arguments, out bool negate)
    {
        string value = StripExpressionParentheses(expression);
        negate = value.StartsWith('!') && !value.StartsWith("!=", StringComparison.Ordinal);
        if (negate) value = StripExpressionParentheses(value[1..]);
        int open = value.IndexOf('(');
        command = "";
        arguments = new List<string>();
        if (open <= 0) return false;
        int close = MatchingCloseParenthesis(value, open);
        if (close < 0 || value[(close + 1)..].Trim().Length != 0) return false;
        Match commandMatch = Regex.Match(value[..open].TrimEnd(), @"(?<command>[A-Za-z_]\w*)$", RegexOptions.CultureInvariant);
        if (!commandMatch.Success || !TryParseCallArguments(value, out arguments)) return false;
        command = commandMatch.Groups["command"].Value;
        return true;
    }

    private static string StripExpressionParentheses(string expression)
    {
        string value = Normalize(expression);
        while (value.Length >= 2 && value[0] == '(')
        {
            int close = MatchingCloseParenthesis(value, 0);
            if (close != value.Length - 1) break;
            value = Normalize(value[1..^1]);
        }
        return value;
    }

    private static int MatchingCloseParenthesis(string value, int open)
    {
        int nesting = 0;
        char quote = '\0';
        for (int index = open; index < value.Length; index++)
        {
            char current = value[index];
            if (quote != '\0')
            {
                if (current == quote && (index == 0 || value[index - 1] != '\\')) quote = '\0';
                continue;
            }
            if (current is '"' or '\'') { quote = current; continue; }
            if (current == '(') nesting++;
            else if (current == ')' && --nesting == 0) return index;
        }
        return -1;
    }

    private static void AppendReferences(List<string> output, IEnumerable<string> candidates, string expression)
    {
        foreach (string candidate in candidates)
        {
            if (output.Contains(candidate, StringComparer.Ordinal)) continue;
            if (Regex.IsMatch(expression, $@"(?<![A-Za-z0-9_]){Regex.Escape(candidate)}(?![A-Za-z0-9_])", RegexOptions.CultureInvariant))
            {
                output.Add(candidate);
            }
        }
    }

    private static bool IsLiteral(string expression)
    {
        string value = Normalize(expression);
        if (value.Length == 0) return false;
        if (value is "true" or "false" || value[0] is '"' or '\'' or '$') return true;
        return double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out _);
    }

    private static IEnumerable<string> SplitDeclarators(string value)
    {
        int start = 0;
        int nesting = 0;
        char quote = '\0';
        for (int index = 0; index < value.Length; index++)
        {
            char current = value[index];
            if (quote != '\0')
            {
                if (current == quote && (index == 0 || value[index - 1] != '\\')) quote = '\0';
            }
            else if (current is '"' or '\'') quote = current;
            else if (current is '(' or '[') nesting++;
            else if (current is ')' or ']') nesting--;
            else if (current == ',' && nesting == 0)
            {
                yield return value[start..index];
                start = index + 1;
            }
        }
        yield return value[start..];
    }

    private static string Normalize(string value) => Regex.Replace(value.Trim(), @"\s+", " ");

    private static string NormalizeStatement(string value)
    {
        string result = Normalize(value);
        result = Regex.Replace(result, @"^(?:}\s*)+", "", RegexOptions.CultureInvariant);
        result = Regex.Replace(result, @"\s*{$", "", RegexOptions.CultureInvariant);
        return result.Trim();
    }

    private static int LeadingClosingBraces(string value)
    {
        int count = 0;
        foreach (char character in value)
        {
            if (char.IsWhiteSpace(character)) continue;
            if (character != '}') break;
            count++;
        }
        return count;
    }

    private static int Count(string value, char character)
    {
        int count = 0;
        bool quoted = false;
        for (int index = 0; index < value.Length; index++)
        {
            if (value[index] == '"' && (index == 0 || value[index - 1] != '\\'))
            {
                quoted = !quoted;
            }
            else if (!quoted && value[index] == character)
            {
                count++;
            }
        }
        return count;
    }

    private static string StripComments(string line, ref bool block)
    {
        StringBuilder result = new();
        bool quoted = false;
        for (int index = 0; index < line.Length; index++)
        {
            char current = line[index];
            char next = index + 1 < line.Length ? line[index + 1] : '\0';
            if (block)
            {
                if (current == '*' && next == '/')
                {
                    block = false;
                    index++;
                }
                continue;
            }
            if (!quoted && current == '/' && next == '*')
            {
                block = true;
                index++;
                continue;
            }
            if (!quoted && current == '/' && next == '/')
            {
                break;
            }
            if (current == '"' && (index == 0 || line[index - 1] != '\\'))
            {
                quoted = !quoted;
            }
            result.Append(current);
        }
        return result.ToString();
    }

    private static uint Hash(string value)
    {
        uint result = 2166136261;
        foreach (byte current in Encoding.UTF8.GetBytes(value))
        {
            result ^= current;
            result *= 16777619;
        }
        return result;
    }

    public static string Checksum(string value) => Hash(value).ToString("X8", CultureInfo.InvariantCulture);
}
