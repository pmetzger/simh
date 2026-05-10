#!/usr/bin/env python3
"""Insert an include or other line at an explicit file line.

The tool is intentionally small: it does not infer include blocks or sort
headers. LINE is 1-based and inserts before that line. Use --write to edit;
the default is a dry run.
"""

from __future__ import annotations

import argparse
import difflib
from pathlib import Path
import sys
import tempfile


def detect_newline(text: str) -> str:
    if "\r\n" in text:
        return "\r\n"
    return "\n"


def split_keepends(text: str) -> list[str]:
    lines = text.splitlines(keepends=True)
    if not lines and text == "":
        return []
    return lines


def ensure_line_ending(line: str, newline: str) -> str:
    stripped = line.rstrip("\r\n")
    return stripped + newline


def insert_line(
    text: str,
    line_no: int,
    inserted: str,
    blank_before: bool = False,
    blank_after: bool = False,
) -> str:
    lines = split_keepends(text)
    if line_no < 1 or line_no > len(lines) + 1:
        raise ValueError(f"line must be between 1 and {len(lines) + 1}, got {line_no}")

    newline = detect_newline(text)
    insertion: list[str] = []
    if blank_before:
        insertion.append(newline)
    insertion.append(ensure_line_ending(inserted, newline))
    if blank_after:
        insertion.append(newline)

    idx = line_no - 1
    lines[idx:idx] = insertion
    return "".join(lines)


def exact_line_exists(text: str, line: str) -> bool:
    wanted = line.rstrip("\r\n")
    return any(existing.rstrip("\r\n") == wanted for existing in text.splitlines())


def read_text_preserve_newlines(path: Path) -> str:
    with path.open("r", encoding="utf-8", newline="") as f:
        return f.read()


def write_text_preserve_newlines(path: Path, text: str) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        f.write(text)


def run(args: argparse.Namespace) -> int:
    path = Path(args.file)
    text = read_text_preserve_newlines(path)

    if args.if_missing and exact_line_exists(text, args.line_text):
        print(f"{path}: unchanged; line already present")
        return 0

    try:
        final = insert_line(
            text,
            args.line,
            args.line_text,
            blank_before=args.blank_before,
            blank_after=args.blank_after,
        )
    except ValueError as exc:
        print(f"{path}: {exc}", file=sys.stderr)
        return 2

    if args.diff or not args.write:
        print("".join(difflib.unified_diff(
            text.splitlines(keepends=True),
            final.splitlines(keepends=True),
            fromfile=str(path),
            tofile=str(path),
        )))

    if args.write:
        write_text_preserve_newlines(path, final)
        print(f"{path}: written; inserted before line {args.line}")
    else:
        print(f"{path}: would insert before line {args.line}")
    return 0


def assert_text(path: Path, expected: str) -> None:
    actual = path.read_text(encoding="utf-8")
    if actual != expected:
        raise AssertionError(
            f"{path} contents differ\nexpected:\n{expected!r}\nactual:\n{actual!r}"
        )


def self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="stdint-insert-include-") as td:
        root = Path(td)

        sample = root / "sample.c"
        sample.write_text("#include <stdio.h>\nint main(void);\n", encoding="utf-8")
        args = argparse.Namespace(
            file=str(sample),
            line=1,
            line_text="#include <stdint.h>",
            blank_before=False,
            blank_after=False,
            if_missing=False,
            write=False,
            diff=False,
        )
        if run(args) != 0:
            return 1
        assert_text(sample, "#include <stdio.h>\nint main(void);\n")

        args.write = True
        if run(args) != 0:
            return 1
        assert_text(sample, "#include <stdint.h>\n#include <stdio.h>\nint main(void);\n")

        args.if_missing = True
        if run(args) != 0:
            return 1
        assert_text(sample, "#include <stdint.h>\n#include <stdio.h>\nint main(void);\n")

        quoted = root / "quoted.c"
        quoted.write_text("#include <stdint.h>\nint x;\n", encoding="utf-8")
        args = argparse.Namespace(
            file=str(quoted),
            line=2,
            line_text="#include \"sim_types.h\"",
            blank_before=False,
            blank_after=False,
            if_missing=False,
            write=True,
            diff=False,
        )
        if run(args) != 0:
            return 1
        assert_text(
            quoted,
            "#include <stdint.h>\n#include \"sim_types.h\"\nint x;\n",
        )

        blanks = root / "blanks.c"
        blanks.write_text("a\nb\n", encoding="utf-8")
        args = argparse.Namespace(
            file=str(blanks),
            line=2,
            line_text="#include <stdint.h>",
            blank_before=True,
            blank_after=True,
            if_missing=False,
            write=True,
            diff=False,
        )
        if run(args) != 0:
            return 1
        assert_text(blanks, "a\n\n#include <stdint.h>\n\nb\n")

        crlf = root / "crlf.c"
        crlf.write_bytes(b"a\r\nb\r\n")
        args = argparse.Namespace(
            file=str(crlf),
            line=2,
            line_text="#include <stdint.h>",
            blank_before=False,
            blank_after=False,
            if_missing=False,
            write=True,
            diff=False,
        )
        if run(args) != 0:
            return 1
        if crlf.read_bytes() != b"a\r\n#include <stdint.h>\r\nb\r\n":
            raise AssertionError("CRLF newline style was not preserved")

    print("self-test: PASS")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", nargs="?", help="file to edit")
    parser.add_argument(
        "line",
        nargs="?",
        type=int,
        help="1-based line number to insert before",
    )
    parser.add_argument("line_text", nargs="?", help="line text to insert")
    parser.add_argument(
        "--blank-before",
        action="store_true",
        help="insert a blank line before the inserted line",
    )
    parser.add_argument(
        "--blank-after",
        action="store_true",
        help="insert a blank line after the inserted line",
    )
    parser.add_argument(
        "--if-missing",
        action="store_true",
        help="do nothing if the exact line already exists",
    )
    parser.add_argument("--write", action="store_true", help="edit the file")
    parser.add_argument("--diff", action="store_true", help="print a unified diff")
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run temporary fixture tests",
    )
    args = parser.parse_args(argv)

    if args.self_test:
        return args
    missing = [
        name for name in ("file", "line", "line_text")
        if getattr(args, name) is None
    ]
    if missing:
        parser.error("file, line, and line_text are required unless --self-test is used")
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_test:
        return self_test()
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
