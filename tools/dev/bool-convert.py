#!/usr/bin/env python3
"""Convert SIMH t_bool spellings to C stdbool spellings.

This is a prototype conversion tool. It rewrites only C identifier tokens in
code by default. It can also rewrite comments after a chosen boundary, while
always leaving strings and character literals unchanged. Comments and strings
that mention legacy boolean spellings are reported for manual review.
"""

from __future__ import annotations

import argparse
import dataclasses
import difflib
import os
from pathlib import Path
import re
import sys
import tempfile
import textwrap


REPLACEMENTS = {
    "t_bool": "bool",
    "TRUE": "true",
    "FALSE": "false",
}
ALIGNMENT_SHRINK = {
    old: len(old) - len(new)
    for old, new in REPLACEMENTS.items()
    if len(old) > len(new)
}
STDBOOL_IDENTIFIERS = {"bool", "true", "false"}
LEGACY_IDENTIFIERS = set(REPLACEMENTS)
SOURCE_SUFFIXES = {".c", ".h", ".inc"}
INCLUDE_RE = re.compile(r"^(\s*)#\s*include\s+([<\"])([^>\"]+)([>\"])")
PP_DIRECTIVE_RE = re.compile(
    r"^\s*#\s*([A-Za-z_][A-Za-z0-9_]*)(?:\s+([A-Za-z_][A-Za-z0-9_]*))?"
)
IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
WIN32_CONDITIONAL_RE = re.compile(
    r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b.*\b(?:_WIN32|WIN32|_WIN64)\b"
)
WINDOWS_HEADER_RE = re.compile(r"^(?:windows|winsock|win)[^/\\]*\.h$", re.I)


@dataclasses.dataclass
class Occurrence:
    line: int
    kind: str
    token: str


@dataclasses.dataclass
class FileReport:
    path: Path
    replacements: dict[str, int]
    include_action: str
    changed: bool
    comment_hits: list[Occurrence]
    comment_rewrites: list[Occurrence]
    string_hits: list[Occurrence]
    windows_hits: list[Occurrence]
    manual_include_reason: str | None = None
    comment_start_line: int | None = None


def split_at_start_line(text: str, start_line: int) -> tuple[str, str, int]:
    if start_line < 1:
        raise ValueError(f"start line must be >= 1, got {start_line}")
    if start_line == 1:
        return "", text, 0

    line = 1
    for idx, ch in enumerate(text):
        if line == start_line:
            return text[:idx], text[idx:], start_line - 1
        if ch == "\n":
            line += 1

    if line == start_line:
        return text, "", start_line - 1
    raise ValueError(f"start line must be <= {line}, got {start_line}")


def offset_occurrences(
    occurrences: list[Occurrence],
    line_offset: int,
) -> list[Occurrence]:
    if line_offset == 0:
        return occurrences
    return [
        Occurrence(hit.line + line_offset, hit.kind, hit.token)
        for hit in occurrences
    ]


def first_preprocessor_line(text: str) -> int:
    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.lstrip().startswith("#"):
            return line_number
    return 1


def parse_comment_start(comment_start: str, text: str) -> int:
    if comment_start == "auto":
        return first_preprocessor_line(text)
    try:
        line = int(comment_start)
    except ValueError as exc:
        raise ValueError(
            f"comment start must be a positive integer or 'auto', got {comment_start!r}"
        ) from exc
    if line < 1:
        raise ValueError(f"comment start must be >= 1, got {line}")
    return line


def is_identifier_start(ch: str) -> bool:
    return ch == "_" or ch.isalpha()


def is_identifier_continue(ch: str) -> bool:
    return ch == "_" or ch.isalnum()


def token_hits(text: str, tokens: set[str]) -> list[str]:
    found = []
    for match in IDENT_RE.finditer(text):
        token = match.group(0)
        if token in tokens:
            found.append(token)
    return found


def preprocessor_prefix(text: str, pos: int) -> str:
    line_start = text.rfind("\n", 0, pos) + 1
    return text[line_start:pos]


def is_macro_name_position(text: str, pos: int) -> bool:
    prefix = preprocessor_prefix(text, pos)
    return re.match(r"^\s*#\s*define\s*$", prefix) is not None


def is_ifdef_symbol_position(text: str, pos: int) -> bool:
    prefix = preprocessor_prefix(text, pos)
    return re.match(r"^\s*#\s*(?:ifdef|ifndef)\s*$", prefix) is not None


def rewrite_tokens(
    text: str,
    replacements_enabled: bool = True,
    rewrite_comments: bool = False,
    rewrite_comments_from_line: int = 1,
) -> tuple[str, dict[str, int], list[Occurrence], list[Occurrence], list[Occurrence]]:
    out: list[str] = []
    replacements = {token: 0 for token in REPLACEMENTS}
    comment_hits: list[Occurrence] = []
    comment_rewrites: list[Occurrence] = []
    string_hits: list[Occurrence] = []
    i = 0
    line = 1

    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if ch == "/" and nxt == "/":
            start_line = line
            end = text.find("\n", i)
            if end == -1:
                end = len(text)
                comment = text[i:end]
                newline = ""
            else:
                comment = text[i:end]
                newline = "\n"
            hits = token_hits(comment, LEGACY_IDENTIFIERS)
            for token in hits:
                comment_hits.append(Occurrence(start_line, "line-comment", token))
            rewrite_comment = (
                replacements_enabled
                and rewrite_comments
                and start_line >= rewrite_comments_from_line
            )
            if rewrite_comment:
                for token in hits:
                    comment_rewrites.append(
                        Occurrence(start_line, "line-comment", token)
                    )
                out.append(rewrite_plain_identifiers(comment, replacements))
            else:
                out.append(comment)
            out.append(newline)
            i = end + (1 if newline else 0)
            line += 1 if newline else 0
            continue

        if ch == "/" and nxt == "*":
            start_line = line
            end = text.find("*/", i + 2)
            if end == -1:
                end = len(text) - 2
            comment = text[i : end + 2]
            hits = token_hits(comment, LEGACY_IDENTIFIERS)
            for token in hits:
                comment_hits.append(Occurrence(start_line, "block-comment", token))
            rewrite_comment = (
                replacements_enabled
                and rewrite_comments
                and start_line >= rewrite_comments_from_line
            )
            if rewrite_comment:
                for token in hits:
                    comment_rewrites.append(
                        Occurrence(start_line, "block-comment", token)
                    )
                out.append(rewrite_plain_identifiers(comment, replacements))
            else:
                out.append(comment)
            line += comment.count("\n")
            i = end + 2
            continue

        if ch in {"'", '"'}:
            quote = ch
            start = i
            start_line = line
            i += 1
            escaped = False
            while i < len(text):
                cur = text[i]
                if cur == "\n":
                    line += 1
                if escaped:
                    escaped = False
                elif cur == "\\":
                    escaped = True
                elif cur == quote:
                    i += 1
                    break
                i += 1
            literal = text[start:i]
            for token in token_hits(literal, LEGACY_IDENTIFIERS):
                kind = "string" if quote == '"' else "char"
                string_hits.append(Occurrence(start_line, kind, token))
            out.append(literal)
            continue

        if is_identifier_start(ch):
            start = i
            i += 1
            while i < len(text) and is_identifier_continue(text[i]):
                i += 1
            ident = text[start:i]
            replacement = None
            if (
                replacements_enabled
                and not is_macro_name_position(text, start)
                and not is_ifdef_symbol_position(text, start)
            ):
                replacement = REPLACEMENTS.get(ident)
            if replacement is None:
                out.append(ident)
            else:
                out.append(replacement)
                if ident in ALIGNMENT_SHRINK:
                    end = i
                    while end < len(text) and text[end] == " ":
                        end += 1
                    if end - i >= 2:
                        out.append(" " * ALIGNMENT_SHRINK[ident])
                replacements[ident] += 1
            continue

        out.append(ch)
        if ch == "\n":
            line += 1
        i += 1

    return "".join(out), replacements, comment_hits, comment_rewrites, string_hits


def rewrite_plain_identifiers(text: str, replacements: dict[str, int]) -> str:
    out: list[str] = []
    i = 0
    while i < len(text):
        ch = text[i]
        if is_identifier_start(ch):
            start = i
            i += 1
            while i < len(text) and is_identifier_continue(text[i]):
                i += 1
            ident = text[start:i]
            replacement = REPLACEMENTS.get(ident)
            if replacement is None:
                out.append(ident)
            else:
                out.append(replacement)
                replacements[ident] += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def trailing_comment_column(line: str) -> tuple[int, str] | None:
    candidates = []
    for marker in ("/*", "//"):
        idx = line.find(marker)
        if idx > 0:
            candidates.append((idx, marker))
    if not candidates:
        return None
    idx, marker = min(candidates)
    if line[:idx].strip() == "":
        return None
    if not line[idx - 1].isspace():
        return None
    return idx, marker


def preserve_trailing_comment_columns(original: str, rewritten: str) -> str:
    original_lines = original.splitlines(keepends=True)
    rewritten_lines = rewritten.splitlines(keepends=True)
    if len(original_lines) != len(rewritten_lines):
        return rewritten

    adjusted: list[str] = []
    for old, new in zip(original_lines, rewritten_lines):
        old_column = trailing_comment_column(old)
        new_column = trailing_comment_column(new)
        if old_column is None or new_column is None:
            adjusted.append(new)
            continue

        old_idx, old_marker = old_column
        new_idx, new_marker = new_column
        if old_marker != new_marker or new_idx >= old_idx:
            adjusted.append(new)
            continue

        adjusted.append(new[:new_idx] + (" " * (old_idx - new_idx)) + new[new_idx:])

    return "".join(adjusted)


def code_identifiers(text: str) -> set[str]:
    identifiers: set[str] = set()
    i = 0
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if ch == "/" and nxt == "/":
            end = text.find("\n", i)
            i = len(text) if end == -1 else end + 1
            continue
        if ch == "/" and nxt == "*":
            end = text.find("*/", i + 2)
            i = len(text) if end == -1 else end + 2
            continue
        if ch in {"'", '"'}:
            quote = ch
            i += 1
            escaped = False
            while i < len(text):
                cur = text[i]
                if escaped:
                    escaped = False
                elif cur == "\\":
                    escaped = True
                elif cur == quote:
                    i += 1
                    break
                i += 1
            continue
        if is_identifier_start(ch):
            start = i
            i += 1
            while i < len(text) and is_identifier_continue(text[i]):
                i += 1
            identifiers.add(text[start:i])
            continue
        i += 1
    return identifiers


def has_stdbool_include(text: str) -> bool:
    return any(
        INCLUDE_RE.match(line) and INCLUDE_RE.match(line).group(3) == "stdbool.h"
        for line in text.splitlines()
    )


def has_sim_defs_include(text: str) -> bool:
    for line in text.splitlines():
        match = INCLUDE_RE.match(line)
        if match and Path(match.group(3)).name == "sim_defs.h":
            return True
    return False


def windows_sensitivity_hits(text: str) -> list[Occurrence]:
    hits = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        include = INCLUDE_RE.match(line)
        if include and WINDOWS_HEADER_RE.match(Path(include.group(3)).name):
            hits.append(
                Occurrence(line_number, "windows-include", include.group(3))
            )
        elif WIN32_CONDITIONAL_RE.match(line):
            hits.append(Occurrence(line_number, "windows-conditional", "_WIN32"))
    return hits


def preprocessor_depths(lines: list[str]) -> list[int]:
    depths = []
    depth = 0
    for line in lines:
        stripped = line.lstrip()
        if re.match(r"#\s*endif\b", stripped):
            depth = max(0, depth - 1)
        depths.append(depth)
        if re.match(r"#\s*(if|ifdef|ifndef)\b", stripped):
            depth += 1
    return depths


def has_conventional_include_guard(lines: list[str]) -> bool:
    directives = []
    for idx, line in enumerate(lines):
        match = PP_DIRECTIVE_RE.match(line)
        if match:
            directives.append((idx, match.group(1), match.group(2)))

    if len(directives) < 3:
        return False

    first_idx, first_directive, guard = directives[0]
    second_idx, second_directive, second_name = directives[1]
    last_idx, last_directive, _ = directives[-1]

    if first_directive != "ifndef" or guard is None:
        return False
    if second_directive != "define" or second_name != guard:
        return False
    if last_directive != "endif":
        return False
    return first_idx < second_idx < last_idx


def add_stdbool_include(text: str) -> tuple[str, str, str | None]:
    if has_stdbool_include(text):
        return text, "already-present", None

    lines = text.splitlines(keepends=True)
    depths = preprocessor_depths(lines)
    include_rows = []
    for idx, line in enumerate(lines):
        match = INCLUDE_RE.match(line)
        if match:
            include_rows.append((idx, depths[idx], match.group(2), match.group(3)))

    include_block = [row for row in include_rows if row[1] == 0]
    if not include_block and has_conventional_include_guard(lines):
        include_block = [row for row in include_rows if row[1] == 1]
    if not include_block:
        return text, "manual", "no top-level include block found"

    system_rows = [row for row in include_block if row[2] == "<"]
    newline = "\n"
    include_line = f"#include <stdbool.h>{newline}"

    if system_rows:
        names = [row[3] for row in system_rows]
        if names != sorted(names):
            return text, "manual", "top-level system includes are not alphabetized"
        insert_at = system_rows[-1][0] + 1
        for row in system_rows:
            if "stdbool.h" < row[3]:
                insert_at = row[0]
                break
        lines.insert(insert_at, include_line)
        return "".join(lines), "added", None

    first_include = include_block[0][0]
    lines.insert(first_include, include_line)
    return "".join(lines), "added", None


def is_excluded(path: Path) -> bool:
    normalized = path.as_posix()
    return normalized.endswith("src/core/sim_defs.h")


def convert_text(
    path: Path,
    text: str,
    force_without_t_bool: bool = False,
    rewrite_comments: bool = False,
    start_line: int = 1,
    comment_start: str = "1",
) -> tuple[str, FileReport]:
    prefix, work_text, line_offset = split_at_start_line(text, start_line)
    comment_start_line = parse_comment_start(comment_start, text)
    work_comment_start_line = max(1, comment_start_line - line_offset)
    windows_hits = windows_sensitivity_hits(text)

    should_convert = (
        force_without_t_bool
        or "t_bool" in code_identifiers(work_text)
        or has_sim_defs_include(text)
    )
    if not should_convert:
        _, replacements, comment_hits, comment_rewrites, string_hits = rewrite_tokens(
            work_text,
            replacements_enabled=False,
            rewrite_comments_from_line=work_comment_start_line,
        )
        return text, FileReport(
            path=path,
            replacements=replacements,
            include_action="skipped-no-t_bool-or-sim_defs",
            changed=False,
            comment_hits=offset_occurrences(comment_hits, line_offset),
            comment_rewrites=offset_occurrences(comment_rewrites, line_offset),
            string_hits=offset_occurrences(string_hits, line_offset),
            windows_hits=windows_hits,
            comment_start_line=comment_start_line if rewrite_comments else None,
        )

    (
        rewritten_work,
        replacements,
        comment_hits,
        comment_rewrites,
        string_hits,
    ) = rewrite_tokens(
        work_text,
        rewrite_comments=rewrite_comments,
        rewrite_comments_from_line=work_comment_start_line,
    )
    rewritten_work = preserve_trailing_comment_columns(work_text, rewritten_work)
    rewritten = prefix + rewritten_work
    include_action = "not-needed"
    manual_reason = None
    final = rewritten

    if STDBOOL_IDENTIFIERS & code_identifiers(rewritten):
        final, include_action, manual_reason = add_stdbool_include(rewritten)

    return final, FileReport(
        path=path,
        replacements=replacements,
        include_action=include_action,
        changed=(final != text),
        comment_hits=offset_occurrences(comment_hits, line_offset),
        comment_rewrites=offset_occurrences(comment_rewrites, line_offset),
        string_hits=offset_occurrences(string_hits, line_offset),
        windows_hits=windows_hits,
        manual_include_reason=manual_reason,
        comment_start_line=comment_start_line if rewrite_comments else None,
    )


def iter_source_files(paths: list[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        if path.is_dir():
            for root, dirs, names in os.walk(path):
                dirs[:] = [d for d in dirs if d not in {".git", "build"}]
                for name in names:
                    candidate = Path(root) / name
                    if candidate.suffix in SOURCE_SUFFIXES:
                        files.append(candidate)
        elif path.suffix in SOURCE_SUFFIXES:
            files.append(path)
    return sorted(files)


def print_report(
    report: FileReport,
    show_comments: bool = False,
    write_mode: bool = False,
) -> None:
    counts = ", ".join(
        f"{key}={value}"
        for key, value in report.replacements.items()
        if value
    )
    if not counts:
        counts = "no code replacements"
    if report.changed and write_mode and report.include_action == "manual":
        status = "not-written"
    elif report.changed and write_mode:
        status = "written"
    elif report.changed:
        status = "would-change"
    else:
        status = "unchanged"
    print(f"{report.path}: {status}; {counts}; include={report.include_action}")
    if report.comment_start_line is not None:
        print(f"  comment rewrite start: line {report.comment_start_line}")
    if report.manual_include_reason:
        print(f"  manual include: {report.manual_include_reason}")
    if report.windows_hits:
        print(
            "  windows-sensitive: "
            f"{len(report.windows_hits)} include/conditional hits; "
            "review Windows API TRUE/FALSE uses"
        )
    if report.comment_rewrites:
        print(f"  rewrote comments: {len(report.comment_rewrites)}")
    if report.comment_hits or report.string_hits:
        print(
            "  review legacy spellings in comments/strings: "
            f"comments={len(report.comment_hits)} strings={len(report.string_hits)}"
        )
        if show_comments:
            for hit in report.comment_rewrites:
                print(f"    line {hit.line}: rewrote {hit.kind}: {hit.token}")
            for hit in report.windows_hits:
                print(f"    line {hit.line}: {hit.kind}: {hit.token}")
            for hit in report.comment_hits + report.string_hits:
                print(f"    line {hit.line}: {hit.kind}: {hit.token}")


def process_files(args: argparse.Namespace) -> int:
    paths = [Path(p) for p in args.paths]
    reports: list[FileReport] = []
    for path in iter_source_files(paths):
        if is_excluded(path):
            print(f"{path}: skipped; sim_defs.h is converted later")
            continue
        text = path.read_text(encoding="utf-8")
        final, report = convert_text(
            path,
            text,
            force_without_t_bool=args.force_without_t_bool,
            rewrite_comments=args.rewrite_comments,
            start_line=args.start_line,
            comment_start=args.comment_start,
        )
        reports.append(report)
        if args.diff and final != text:
            print("".join(difflib.unified_diff(
                text.splitlines(keepends=True),
                final.splitlines(keepends=True),
                fromfile=str(path),
                tofile=str(path),
            )))
        if args.write and final != text and report.include_action != "manual":
            path.write_text(final, encoding="utf-8")
        elif args.write and final != text and report.include_action == "manual":
            print("  not written: manual include placement required")
        print_report(report, args.show_comments, args.write)

    changed = sum(1 for report in reports if report.changed)
    manual = sum(1 for report in reports if report.include_action == "manual")
    windows_sensitive = sum(1 for report in reports if report.windows_hits)
    print(
        f"summary: files={len(reports)} changed={changed} "
        f"manual_includes={manual} windows_sensitive={windows_sensitive}"
    )
    return 1 if manual and args.fail_on_manual else 0


def assert_contains(path: Path, needle: str) -> None:
    text = path.read_text(encoding="utf-8")
    if needle not in text:
        raise AssertionError(
            f"{path} does not contain expected text: {needle!r}\n{text}"
        )


def assert_not_contains(path: Path, needle: str) -> None:
    text = path.read_text(encoding="utf-8")
    if needle in text:
        raise AssertionError(f"{path} unexpectedly contains text: {needle!r}\n{text}")


def run_self_test() -> int:
    with tempfile.TemporaryDirectory(prefix="convert-t-bool-") as td:
        root = Path(td)
        ordinary = root / "ordinary.c"
        ordinary.write_text(textwrap.dedent("""\
            /*
             * History: used TRUE and t_bool in old notes.
             */
            #include <stdio.h>

            #define NMI_TRUE 7

            t_bool f(int x) {       /* aligned */
                const char *s = "TRUE and FALSE in a string";
                t_bool local = TRUE; /* bool local */
                return x ? local : FALSE;
            }
            struct aligned {
                t_bool          field;
            };
            """), encoding="utf-8")

        ordered = root / "ordered.c"
        ordered.write_text(textwrap.dedent("""\
            #include <stdarg.h>
            #include <stddef.h>
            #include <stdio.h>

            t_bool ordered(void) { return TRUE; }
            """), encoding="utf-8")

        no_t_bool = root / "no_t_bool.c"
        no_t_bool.write_text(textwrap.dedent("""\
            #include <stdio.h>
            int keep_true(void) { return TRUE; }
            """), encoding="utf-8")

        sim_defs_only = root / "sim_defs_only.c"
        sim_defs_only.write_text(textwrap.dedent("""\
            #include "sim_defs.h"
            int keep_true(void) { return TRUE || FALSE; }
            """), encoding="utf-8")

        macro_names = root / "macro_names.c"
        macro_names.write_text(textwrap.dedent("""\
            #include "sim_defs.h"
            #ifndef FALSE
            #define FALSE 0
            #endif
            #define TRUE 1
            #define LOCAL_DEFAULT TRUE
            int macro_names(void) { return TRUE || FALSE; }
            """), encoding="utf-8")

        already = root / "already.c"
        already.write_text(textwrap.dedent("""\
            #include <stdbool.h>
            #include <stdio.h>

            t_bool already(void) { return FALSE; }
            """), encoding="utf-8")

        header = root / "header.h"
        header.write_text(textwrap.dedent("""\
            #include "sim_defs.h"
            t_bool header_flag(void);
            """), encoding="utf-8")

        guarded_header = root / "guarded_header.h"
        guarded_header.write_text(textwrap.dedent("""\
            #ifndef GUARDED_HEADER_H_
            #define GUARDED_HEADER_H_

            #include <stddef.h>
            #include <stdio.h>

            t_bool guarded_header_flag(void);

            #endif
            """), encoding="utf-8")

        weird = root / "weird.c"
        weird.write_text(textwrap.dedent("""\
            #ifdef HOST
            #include <stdio.h>
            #endif
            t_bool weird(void) { return TRUE; }
            """), encoding="utf-8")

        windows_file = root / "windows.c"
        windows_file.write_text(textwrap.dedent("""\
            #if defined(_WIN32)
            #include <windows.h>
            #endif
            t_bool windows_flag(void) { return TRUE; }
            """), encoding="utf-8")

        sim_defs = root / "src" / "core" / "sim_defs.h"
        sim_defs.parent.mkdir(parents=True)
        sim_defs.write_text("typedef int t_bool;\n#define TRUE 1\n", encoding="utf-8")

        dry_args = argparse.Namespace(
            paths=[str(root)],
            write=False,
            diff=False,
            show_comments=False,
            fail_on_manual=False,
            force_without_t_bool=False,
            rewrite_comments=False,
            start_line=1,
            comment_start="1",
        )
        if process_files(dry_args) != 0:
            return 1
        assert_contains(ordinary, "t_bool f")

        write_args = argparse.Namespace(
            paths=[str(root)],
            write=True,
            diff=True,
            show_comments=True,
            fail_on_manual=False,
            force_without_t_bool=False,
            rewrite_comments=False,
            start_line=1,
            comment_start="1",
        )
        if process_files(write_args) != 0:
            return 1

        assert_contains(ordinary, "#include <stdbool.h>\n#include <stdio.h>")
        assert_contains(ordinary, "bool f(int x) {         /* aligned */")
        assert_contains(ordinary, "bool local = true;")
        assert_contains(ordinary, "bool local = true;   /* bool local */")
        assert_contains(ordinary, "return x ? local : false;")
        assert_contains(ordinary, "bool            field;")
        assert_contains(ordinary, "#define NMI_TRUE 7")
        assert_contains(ordinary, "History: used TRUE and t_bool in old notes.")
        assert_contains(ordinary, '"TRUE and FALSE in a string"')

        assert_contains(
            ordered,
            "#include <stdarg.h>\n#include <stdbool.h>\n#include <stddef.h>",
        )
        assert_contains(ordered, "bool ordered(void) { return true; }")

        assert_contains(no_t_bool, "int keep_true(void) { return TRUE; }")
        assert_not_contains(no_t_bool, "#include <stdbool.h>")

        assert_contains(
            sim_defs_only,
            "#include <stdbool.h>\n#include \"sim_defs.h\"",
        )
        assert_contains(sim_defs_only, "int keep_true(void) { return true || false; }")

        assert_contains(macro_names, "#ifndef FALSE")
        assert_contains(macro_names, "#define FALSE 0")
        assert_contains(macro_names, "#define TRUE 1")
        assert_contains(macro_names, "#define LOCAL_DEFAULT true")
        assert_contains(macro_names, "int macro_names(void) { return true || false; }")

        assert_contains(already, "#include <stdbool.h>\n#include <stdio.h>")
        assert_contains(already, "bool already(void) { return false; }")
        if already.read_text(encoding="utf-8").count("#include <stdbool.h>") != 1:
            raise AssertionError("already.c should contain exactly one stdbool include")

        assert_contains(header, "#include <stdbool.h>\n#include \"sim_defs.h\"")
        assert_contains(header, "bool header_flag(void);")

        assert_contains(
            guarded_header,
            "#include <stdbool.h>\n#include <stddef.h>",
        )
        assert_contains(guarded_header, "bool guarded_header_flag(void);")

        assert_contains(weird, "t_bool weird(void) { return TRUE; }")
        assert_not_contains(weird, "#include <stdbool.h>")

        assert_contains(sim_defs, "typedef int t_bool;")

        final, report = convert_text(
            windows_file,
            windows_file.read_text(encoding="utf-8"),
        )
        if "bool windows_flag(void) { return true; }" not in final:
            raise AssertionError("windows fixture should still be convertible")
        if len(report.windows_hits) != 2:
            raise AssertionError("windows fixture should report include and _WIN32")
        if not any(hit.kind == "windows-include" for hit in report.windows_hits):
            raise AssertionError("windows fixture should report windows include")
        if not any(hit.kind == "windows-conditional" for hit in report.windows_hits):
            raise AssertionError("windows fixture should report _WIN32 conditional")

        force_file = root / "force.c"
        force_file.write_text(textwrap.dedent("""\
            #include <stdio.h>
            int force(void) { return TRUE || FALSE; }
            """), encoding="utf-8")
        final, report = convert_text(force_file, force_file.read_text(encoding="utf-8"))
        if (final != force_file.read_text(encoding="utf-8") or
                report.include_action != "skipped-no-t_bool-or-sim_defs"):
            raise AssertionError("force fixture should be skipped without force mode")
        final, report = convert_text(
            force_file,
            force_file.read_text(encoding="utf-8"),
            force_without_t_bool=True,
        )
        if "return true || false;" not in final:
            raise AssertionError("force mode should rewrite TRUE/FALSE without t_bool")
        if "#include <stdbool.h>" not in final:
            raise AssertionError(
                "force mode should add stdbool when true/false are introduced"
            )

        comment_file = root / "comments.c"
        comment_text = textwrap.dedent("""\
            #include <stdio.h>
            /* TRUE t_bool FALSE */
            t_bool comments(void) { return TRUE; }
            """)
        final, _ = convert_text(comment_file, comment_text, rewrite_comments=False)
        if "/* TRUE t_bool FALSE */" not in final:
            raise AssertionError("comments should be preserved by default")
        final, _ = convert_text(comment_file, comment_text, rewrite_comments=True)
        if "/* true bool false */" not in final:
            raise AssertionError(
                "rewrite_comments mode should rewrite comment identifiers"
            )

        auto_comment_file = root / "auto-comments.c"
        auto_comment_text = textwrap.dedent("""\
            /*
             * TRUE t_bool FALSE history.
             */
            #include <stdio.h>
            /* TRUE after header */
            t_bool auto_comments(void) { return TRUE; } /* FALSE trailing */
            const char *s = "TRUE and FALSE stay strings";
            """)
        final, report = convert_text(
            auto_comment_file,
            auto_comment_text,
            rewrite_comments=True,
            comment_start="auto",
        )
        if report.comment_start_line != 4:
            raise AssertionError(
                "auto comment start should use first preprocessor line"
            )
        if "TRUE t_bool FALSE history" not in final:
            raise AssertionError("auto comment start should preserve header comments")
        if "/* true after header */" not in final:
            raise AssertionError("auto comment start should rewrite later comments")
        if "/* false trailing */" not in final:
            raise AssertionError("auto comment start should rewrite trailing comments")
        if '"TRUE and FALSE stay strings"' not in final:
            raise AssertionError("string literals must never be rewritten")
        if any(hit.line < 4 for hit in report.comment_rewrites):
            raise AssertionError("auto comment start rewrote comments before boundary")
        if not any(hit.line == 5 for hit in report.comment_rewrites):
            raise AssertionError("auto comment start should report rewritten comments")

        start_file = root / "start-line.c"
        start_text = textwrap.dedent("""\
            /* TRUE t_bool FALSE history */
            t_bool before(void) { return TRUE; }
            t_bool after(void) { return FALSE; } /* TRUE after */
            """)
        final, report = convert_text(
            start_file,
            start_text,
            rewrite_comments=True,
            start_line=3,
        )
        if "/* TRUE t_bool FALSE history */" not in final:
            raise AssertionError("start_line should preserve earlier comments")
        if "t_bool before(void) { return TRUE; }" not in final:
            raise AssertionError("start_line should preserve earlier code")
        if "bool after(void) { return false; }   /* true after */" not in final:
            raise AssertionError(
                "start_line should rewrite code/comments at or after start line"
            )
        if not any(hit.line == 3 for hit in report.comment_hits):
            raise AssertionError(
                "start_line should report comment hits with original line numbers"
            )

    print("self-test: PASS")
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "paths",
        nargs="*",
        default=["."],
        help="files or directories to scan",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="write converted files; default is dry-run",
    )
    parser.add_argument("--diff", action="store_true", help="print unified diffs")
    parser.add_argument(
        "--show-comments",
        action="store_true",
        help="show comment/string hit details",
    )
    parser.add_argument(
        "--force-without-t-bool",
        action="store_true",
        help="convert files even if they do not contain a t_bool code token",
    )
    parser.add_argument(
        "--rewrite-comments",
        action="store_true",
        help="rewrite legacy boolean identifiers in comments too",
    )
    parser.add_argument(
        "--start-line",
        type=int,
        default=1,
        help="first 1-based line to scan and rewrite",
    )
    parser.add_argument(
        "--comment-start",
        default="1",
        help=(
            "first 1-based line to rewrite comments from, or 'auto' for the "
            "first preprocessor line"
        ),
    )
    parser.add_argument(
        "--fail-on-manual",
        action="store_true",
        help="exit nonzero if manual include placement is needed",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="run temporary fixture tests",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_test:
        return run_self_test()
    return process_files(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
