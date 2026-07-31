"""Fold direct _D3XP preprocessor conditionals to their enabled branch."""

from __future__ import annotations

import argparse
import pathlib
import re


SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".inl", ".rc"}
CONDITIONAL_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b(.*)$")
ELIF_RE = re.compile(r"^\s*#\s*elif\b")
ELSE_RE = re.compile(r"^\s*#\s*else\b")
ENDIF_RE = re.compile(r"^\s*#\s*endif\b")


def uncommented_line(line: str, in_comment: bool) -> tuple[str, bool]:
    """Return code outside C/C++ comments and the updated block-comment state."""
    result: list[str] = []
    index = 0
    quote = ""

    while index < len(line):
        if in_comment:
            end = line.find("*/", index)
            if end < 0:
                return "".join(result), True
            index = end + 2
            in_comment = False
            continue

        char = line[index]
        next_char = line[index + 1] if index + 1 < len(line) else ""

        if quote:
            result.append(char)
            if char == "\\" and index + 1 < len(line):
                index += 1
                result.append(line[index])
            elif char == quote:
                quote = ""
            index += 1
            continue

        if char in {'"', "'"}:
            quote = char
            result.append(char)
            index += 1
        elif char == "/" and next_char == "/":
            break
        elif char == "/" and next_char == "*":
            in_comment = True
            index += 2
        else:
            result.append(char)
            index += 1

    return "".join(result), in_comment


def d3xp_condition(kind: str, expression: str) -> bool | None:
    expression = expression.strip()
    if kind == "ifdef" and expression == "_D3XP":
        return True
    if kind == "ifndef" and expression == "_D3XP":
        return False
    if kind == "if" and re.fullmatch(r"defined\s*\(\s*_D3XP\s*\)", expression):
        return True
    return None


def simplify(contents: str, path: pathlib.Path) -> tuple[str, int]:
    lines = contents.splitlines(keepends=True)
    output: list[str] = []
    stack: list[dict[str, object]] = []
    keep = True
    in_comment = False
    removed_directives = 0

    for line_number, line in enumerate(lines, 1):
        code, in_comment = uncommented_line(line, in_comment)
        match = CONDITIONAL_RE.match(code)

        if match:
            kind, expression = match.groups()
            value = d3xp_condition(kind, expression)
            if value is None:
                stack.append({"target": False})
                if keep:
                    output.append(line)
            else:
                stack.append(
                    {
                        "target": True,
                        "parent_keep": keep,
                        "condition": value,
                        "saw_else": False,
                        "line": line_number,
                    }
                )
                keep = keep and value
                removed_directives += 1
            continue

        if ELIF_RE.match(code):
            if not stack:
                raise RuntimeError(f"{path}:{line_number}: unmatched #elif")
            if stack[-1]["target"]:
                raise RuntimeError(
                    f"{path}:{line_number}: _D3XP group with #elif is not supported"
                )
            if keep:
                output.append(line)
            continue

        if ELSE_RE.match(code):
            if not stack:
                raise RuntimeError(f"{path}:{line_number}: unmatched #else")
            frame = stack[-1]
            if frame["target"]:
                if frame["saw_else"]:
                    raise RuntimeError(f"{path}:{line_number}: duplicate #else")
                frame["saw_else"] = True
                keep = bool(frame["parent_keep"]) and not bool(frame["condition"])
                removed_directives += 1
            elif keep:
                output.append(line)
            continue

        if ENDIF_RE.match(code):
            if not stack:
                raise RuntimeError(f"{path}:{line_number}: unmatched #endif")
            frame = stack.pop()
            if frame["target"]:
                keep = bool(frame["parent_keep"])
                removed_directives += 1
            elif keep:
                output.append(line)
            continue

        if keep:
            output.append(line)

    if stack:
        frame = stack[-1]
        raise RuntimeError(f"{path}:{frame.get('line', '?')}: unclosed conditional")

    return "".join(output), removed_directives


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=pathlib.Path)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    changed_files = 0
    removed_directives = 0
    for path in sorted(args.root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        contents = path.read_bytes().decode("latin1")
        simplified, count = simplify(contents, path)
        if simplified == contents:
            continue
        changed_files += 1
        removed_directives += count
        print(f"{path}: remove {count} directives")
        if args.write:
            path.write_bytes(simplified.encode("latin1"))

    action = "Updated" if args.write else "Would update"
    print(f"{action} {changed_files} files; remove {removed_directives} directives")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
