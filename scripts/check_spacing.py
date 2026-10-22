"""Complement clang-format 18's definition spacing with C block/include spacing.

Run after clang-format. Only blank lines change; literals, comments and multiline
macros are opaque. The CI uses check mode, while --fix updates repository C sources.
"""

import argparse
import re
import subprocess
from pathlib import Path


TOKENS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def spaced(source):
    # Keep line numbers while hiding braces and directive markers inside literals.
    protected_gaps = set()

    def hide(match):
        first_line = source.count("\n", 0, match.start())
        protected_gaps.update(range(first_line, first_line + match.group().count("\n")))
        return re.sub(r"[^\n]", " ", match.group())

    masked = TOKENS.sub(hide, source)
    lines = source.splitlines(keepends=True)
    code = masked.splitlines()
    ignored = set()
    continuation = False
    for index, line in enumerate(code):
        if continuation or line.lstrip().startswith("#"):
            ignored.add(index)
            continuation = lines[index].rstrip("\r\n").endswith("\\")

    braces = []
    do_closers = set()
    previous = ""
    for index, line in enumerate(code):
        if index in ignored:
            continue
        for character in line:
            if character == "{":
                braces.append(previous.strip() == "do" and line.strip() == "{")
            elif character == "}" and braces:
                if braces.pop() and line.strip() == "}":
                    do_closers.add(index)
        if line.strip():
            previous = line

    result = []
    for index, line in enumerate(lines):
        result.append(line)
        if index + 1 == len(lines) or not lines[index + 1].strip() or index in protected_gaps:
            continue
        current, following = code[index].strip(), code[index + 1].strip()
        # A comment between `}` and else/while remains attached to that statement.
        next_code = index + 1
        while not following and next_code + 1 < len(code):
            next_code += 1
            following = code[next_code].strip()
        block_end = current == "}" and index not in ignored
        if block_end:
            # else and do/while belong to the same statement; enclosing braces
            # and preprocessor branches do not start a new logical paragraph.
            if re.match(r"(?:else\b|[};,)\]#])", following):
                continue
            if index in do_closers and re.match(r"while\b", following):
                continue
        include_end = (
            current.startswith("#include")
            and not line.rstrip("\r\n").endswith("\\")
            and not re.match(r"#\s*(?:include|if|ifdef|ifndef|else|elif|endif)\b", following)
        )
        if block_end or include_end:
            result.append("\r\n" if line.endswith("\r\n") else "\n")
    return "".join(result)


def self_test():
    cases = [
        ("if (ok)\n{\n    work();\n}\nnext();\n", "}\n\nnext();"),
        ("if (ok)\n{\n}\nelse\n{\n}\n", "}\nelse"),
        ("do\n{\n}\nwhile (ok);\n", "}\nwhile"),
        ("while (a)\n{\n}\nwhile (b)\n{\n}\n", "}\n\nwhile (b)"),
        ("#include <stdio.h>\n#define LIMIT 2\n", ">\n\n#define"),
        ('const char *s = "}\\ntext";\nnext();\n', '";\nnext();'),
        ("/*\n}\ntext\n*/\n", "}\ntext"),
        ("if (ok)\n{\n} /* reason\n * continued\n */\nnext();\n", "reason\n * continued"),
        ("if (ok)\n{\n}\n/* reason */\nelse\n{\n}\n", "}\n/* reason */\nelse"),
        ('#include \\\n"header.h"\n', '#include \\\n"header.h"'),
        ("#define BLOCK \\\n{ \\\n} \\\nnext()\n", "} \\\nnext()"),
    ]
    for source, expected in cases:
        actual = spaced(source)
        if expected not in actual or spaced(actual) != actual:
            raise RuntimeError(f"Spacing regression: {source!r} -> {actual!r}")
    print("Spacing self-tests passed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    root = Path(__file__).resolve().parents[1]
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", "*.c", "*.h"],
        cwd=root,
    ).decode("utf-8").split("\0")
    failures = []
    for relative in filter(None, tracked):
        path = root / relative
        if not path.is_file():  # Staged removals are not source files.
            continue
        original = path.read_bytes().decode("utf-8")
        formatted = spaced(original)
        if formatted != original:
            if args.fix:
                path.write_bytes(formatted.encode("utf-8"))
            else:
                failures.append(relative)
    for relative in failures:
        print(f"{relative}: missing blank line; run python scripts/check_spacing.py --fix")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
