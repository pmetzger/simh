#!/usr/bin/env python3
"""Convert legacy SIMH integer spellings to project standard spellings.

The tool rewrites only C tokens in code by default. It can also rewrite
comments after a chosen boundary, while always leaving strings and
character literals unchanged. Comments and strings that mention target
spellings are reported for manual review.
"""

from __future__ import annotations

import argparse
import dataclasses
import difflib
import fnmatch
import os
from pathlib import Path
import re
import sys
import tempfile
import textwrap


STDINT_REPLACEMENTS = {
    "int8": "int8_t",
    "uint8": "uint8_t",
    "int16": "int16_t",
    "uint16": "uint16_t",
    "int32": "int32_t",
    "uint32": "uint32_t",
    "t_int64": "int64_t",
    "t_uint64": "uint64_t",
}
PHRASE_REPLACEMENTS = {
    ("unsigned", "int"): "uint_t",
    ("unsigned", "char"): "uchar_t",
}
STDINT_IDENTIFIERS = set(STDINT_REPLACEMENTS.values())
SIM_TYPE_IDENTIFIERS = {"uint_t", "uchar_t"}
LEGACY_IDENTIFIERS = set(STDINT_REPLACEMENTS)
LEGACY_PHRASES = {"unsigned int", "unsigned char"}
SOURCE_SUFFIXES = {".c", ".h", ".inc", ".y"}
DEFAULT_EXCLUDES = {"src/core/sim_defs.h", "src/include/sim_types.h"}
INCLUDE_RE = re.compile(r"^(\s*)#\s*include\s+([<\"])([^>\"]+)([>\"])")
PP_DIRECTIVE_RE = re.compile(
    r"^\s*#\s*([A-Za-z_][A-Za-z0-9_]*)(?:\s+([A-Za-z_][A-Za-z0-9_]*))?"
)
IDENT_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


@dataclasses.dataclass
class Occurrence:
    line: int
    kind: str
    token: str


@dataclasses.dataclass
class FileReport:
    path: Path
    replacements: dict[str, int]
    include_actions: dict[str, str]
    changed: bool
    comment_hits: list[Occurrence]
    comment_rewrites: list[Occurrence]
    string_hits: list[Occurrence]
    manual_include_reasons: list[str]
    comment_start_line: int | None = None
    line_ranges: list["LineRange"] | None = None


@dataclasses.dataclass(frozen=True)
class LineRange:
    start: int
    end: int


def parse_line_range(value: str) -> LineRange:
    match = re.fullmatch(r"(\d+)(?:[:-](\d+))?", value)
    if not match:
        raise argparse.ArgumentTypeError(
            f"line range must be START:END, got {value!r}"
        )
    start = int(match.group(1))
    end = int(match.group(2) or match.group(1))
    if start < 1:
        raise argparse.ArgumentTypeError(
            f"line range start must be >= 1, got {start}"
        )
    if end < start:
        raise argparse.ArgumentTypeError(
            f"line range end must be >= start, got {value!r}"
        )
    return LineRange(start, end)


def line_count(text: str) -> int:
    if not text:
        return 0
    count = text.count("\n")
    if not text.endswith("\n"):
        count += 1
    return count


def normalize_line_ranges(ranges: list[LineRange], text: str) -> list[LineRange]:
    if not ranges:
        return []
    max_line = line_count(text)
    normalized: list[LineRange] = []
    for line_range in sorted(ranges, key=lambda item: (item.start, item.end)):
        if line_range.end > max_line:
            raise ValueError(
                f"line range {line_range.start}:{line_range.end} exceeds "
                f"file length {max_line}"
            )
        if normalized and line_range.start <= normalized[-1].end:
            raise ValueError(
                "line ranges must not overlap or touch; got "
                f"{normalized[-1].start}:{normalized[-1].end} and "
                f"{line_range.start}:{line_range.end}"
            )
        normalized.append(line_range)
    return normalized


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
            "comment start must be a positive integer or 'auto', "
            f"got {comment_start!r}"
        ) from exc
    if line < 1:
        raise ValueError(f"comment start must be >= 1, got {line}")
    return line


def is_identifier_start(ch: str) -> bool:
    return ch == "_" or ch.isalpha()


def is_identifier_continue(ch: str) -> bool:
    return ch == "_" or ch.isalnum()


def skip_numeric_literal(text: str, pos: int) -> int | None:
    """Return the first position after a C numeric literal, if one starts here."""
    if pos >= len(text) or not text[pos].isdigit():
        return None

    i = pos
    if text[i] == "0" and i + 1 < len(text) and text[i + 1] in {"x", "X"}:
        i += 2
        if i >= len(text) or not (text[i].isdigit() or text[i] in "abcdefABCDEF"):
            return None
        while i < len(text) and (
            text[i].isdigit() or text[i] in "abcdefABCDEF" or text[i] == "'"
        ):
            i += 1
    elif text[i] == "0" and i + 1 < len(text) and text[i + 1] in {"b", "B"}:
        i += 2
        if i >= len(text) or text[i] not in "01":
            return None
        while i < len(text) and (text[i] in "01" or text[i] == "'"):
            i += 1
    else:
        saw_dot = False
        while i < len(text):
            ch = text[i]
            if ch.isdigit() or ch == "'":
                i += 1
                continue
            if ch == "." and not saw_dot:
                saw_dot = True
                i += 1
                continue
            break
        if i < len(text) and text[i] in {"e", "E"}:
            exp = i + 1
            if exp < len(text) and text[exp] in {"+", "-"}:
                exp += 1
            if exp < len(text) and text[exp].isdigit():
                i = exp + 1
                while i < len(text) and (text[i].isdigit() or text[i] == "'"):
                    i += 1

    while i < len(text) and text[i] in "uUlLfF":
        i += 1
    return i


def token_hits(text: str) -> list[str]:
    found = []
    for match in IDENT_RE.finditer(text):
        token = match.group(0)
        if token in LEGACY_IDENTIFIERS:
            found.append(token)
    for phrase in LEGACY_PHRASES:
        if re.search(r"\b" + phrase.replace(" ", r"\s+") + r"\b", text):
            found.append(phrase)
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


def is_typedef_alias_position(text: str, start: int, end: int) -> bool:
    stmt_start = max(
        text.rfind(";", 0, start),
        text.rfind("{", 0, start),
        text.rfind("}", 0, start),
    ) + 1
    before = text[stmt_start:start]
    if re.search(r"\btypedef\b", before) is None:
        return False
    return re.match(r"\s*(?:\[[^;\]]*\]\s*)?;", text[end:]) is not None


def next_identifier(text: str, pos: int) -> tuple[str, int, int] | None:
    i = pos
    while i < len(text) and text[i].isspace():
        i += 1
    if i >= len(text) or not is_identifier_start(text[i]):
        return None
    start = i
    i += 1
    while i < len(text) and is_identifier_continue(text[i]):
        i += 1
    return text[start:i], start, i


def replacement_counter() -> dict[str, int]:
    counts = {token: 0 for token in STDINT_REPLACEMENTS}
    counts.update({"unsigned int": 0, "unsigned char": 0})
    return counts


def rewrite_plain_text(text: str, replacements: dict[str, int]) -> str:
    text = re.sub(
        r"\bunsigned\s+int\b",
        lambda m: count_phrase(m, replacements, "unsigned int", "uint_t"),
        text,
    )
    text = re.sub(
        r"\bunsigned\s+char\b",
        lambda m: count_phrase(m, replacements, "unsigned char", "uchar_t"),
        text,
    )

    def replace_ident(match: re.Match[str]) -> str:
        ident = match.group(0)
        replacement = STDINT_REPLACEMENTS.get(ident)
        if replacement is None:
            return ident
        replacements[ident] += 1
        return replacement

    return IDENT_RE.sub(replace_ident, text)


def count_phrase(
    _match: re.Match[str],
    replacements: dict[str, int],
    old: str,
    new: str,
) -> str:
    replacements[old] += 1
    return new


def append_replacement_with_alignment(
    out: list[str],
    text: str,
    pos: int,
    old_len: int,
    replacement: str,
) -> int:
    """Preserve simple declaration columns after a rewritten type token."""
    out.append(replacement)
    space_end = pos
    while space_end < len(text) and text[space_end] == " ":
        space_end += 1
    spaces = space_end - pos
    if spaces < 2:
        return pos

    delta = old_len - len(replacement)
    if delta > 0:
        out.append(" " * delta)
        return pos
    if delta < 0:
        return pos + min(-delta, spaces - 1)
    return pos


def rewrite_tokens(
    text: str,
    replacements_enabled: bool = True,
    rewrite_comments: bool = False,
    rewrite_comments_from_line: int = 1,
) -> tuple[str, dict[str, int], list[Occurrence], list[Occurrence], list[Occurrence]]:
    out: list[str] = []
    replacements = replacement_counter()
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
            hits = token_hits(comment)
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
                out.append(rewrite_plain_text(comment, replacements))
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
            hits = token_hits(comment)
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
                out.append(rewrite_plain_text(comment, replacements))
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
            for token in token_hits(literal):
                kind = "string" if quote == '"' else "char"
                string_hits.append(Occurrence(start_line, kind, token))
            out.append(literal)
            continue

        numeric_end = skip_numeric_literal(text, i)
        if numeric_end is not None:
            out.append(text[i:numeric_end])
            i = numeric_end
            continue

        if is_identifier_start(ch):
            start = i
            i += 1
            while i < len(text) and is_identifier_continue(text[i]):
                i += 1
            ident = text[start:i]

            phrase = None
            next_ident = None
            if ident == "unsigned":
                next_ident = next_identifier(text, i)
                if next_ident is not None:
                    phrase = PHRASE_REPLACEMENTS.get((ident, next_ident[0]))

            if (
                replacements_enabled
                and phrase is not None
                and not is_macro_name_position(text, start)
                and not is_ifdef_symbol_position(text, start)
            ):
                old = f"{ident} {next_ident[0]}"
                i = append_replacement_with_alignment(
                    out,
                    text,
                    next_ident[2],
                    next_ident[2] - start,
                    phrase,
                )
                replacements[old] += 1
                continue

            replacement = None
            if (
                replacements_enabled
                and not is_macro_name_position(text, start)
                and not is_ifdef_symbol_position(text, start)
                and not is_typedef_alias_position(text, start, i)
            ):
                replacement = STDINT_REPLACEMENTS.get(ident)
            if replacement is None:
                out.append(ident)
            else:
                i = append_replacement_with_alignment(
                    out,
                    text,
                    i,
                    len(ident),
                    replacement,
                )
                replacements[ident] += 1
            continue

        out.append(ch)
        if ch == "\n":
            line += 1
        i += 1

    return "".join(out), replacements, comment_hits, comment_rewrites, string_hits


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


def trailing_backslash_column(line: str) -> tuple[int, str] | None:
    body = line.rstrip("\r\n")
    idx = body.rfind("\\")
    if idx <= 0 or idx != len(body) - 1:
        return None
    if not body[idx - 1].isspace():
        return None
    return idx, "\\"


def adjust_marker_column(line: str, idx: int, target_idx: int) -> str:
    if idx == target_idx:
        return line
    if idx < target_idx:
        return line[:idx] + (" " * (target_idx - idx)) + line[idx:]

    remove = idx - target_idx
    space_start = idx
    while space_start > 0 and line[space_start - 1] == " ":
        space_start -= 1
    removable = idx - space_start
    if removable <= 1:
        return line
    remove = min(remove, removable - 1)
    return line[: idx - remove] + line[idx:]


def preserve_trailing_marker_columns(original: str, rewritten: str) -> str:
    original_lines = original.splitlines(keepends=True)
    rewritten_lines = rewritten.splitlines(keepends=True)
    if len(original_lines) != len(rewritten_lines):
        return rewritten

    adjusted: list[str] = []
    for old, new in zip(original_lines, rewritten_lines):
        old_column = trailing_comment_column(old) or trailing_backslash_column(old)
        new_column = trailing_comment_column(new) or trailing_backslash_column(new)
        if old_column is None or new_column is None:
            adjusted.append(new)
            continue

        old_idx, old_marker = old_column
        new_idx, new_marker = new_column
        if old_marker != new_marker:
            adjusted.append(new)
            continue

        adjusted.append(adjust_marker_column(new, new_idx, old_idx))

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
        numeric_end = skip_numeric_literal(text, i)
        if numeric_end is not None:
            i = numeric_end
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


def code_has_legacy_phrase(text: str) -> bool:
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
        numeric_end = skip_numeric_literal(text, i)
        if numeric_end is not None:
            i = numeric_end
            continue
        if is_identifier_start(ch):
            start = i
            i += 1
            while i < len(text) and is_identifier_continue(text[i]):
                i += 1
            if text[start:i] == "unsigned":
                next_ident = next_identifier(text, i)
                if (
                    next_ident is not None
                    and ("unsigned", next_ident[0]) in PHRASE_REPLACEMENTS
                ):
                    return True
            continue
        i += 1
    return False


def has_include(text: str, include_name: str) -> bool:
    return any(
        INCLUDE_RE.match(line) and INCLUDE_RE.match(line).group(3) == include_name
        for line in text.splitlines()
    )


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


def conventional_include_guard(
    lines: list[str],
) -> tuple[int, int, int, str] | None:
    directives = []
    for idx, line in enumerate(lines):
        match = PP_DIRECTIVE_RE.match(line)
        if match:
            directives.append((idx, match.group(1), match.group(2)))

    if len(directives) < 3:
        return None

    first_idx, first_directive, guard = directives[0]
    second_idx, second_directive, second_name = directives[1]
    last_idx, last_directive, _ = directives[-1]

    if first_directive != "ifndef" or guard is None:
        return None
    if second_directive != "define" or second_name != guard:
        return None
    if last_directive != "endif":
        return None
    if not first_idx < second_idx < last_idx:
        return None
    return first_idx, second_idx, last_idx, guard


def has_conventional_include_guard(lines: list[str]) -> bool:
    return conventional_include_guard(lines) is not None


def insert_after_include_guard_define(lines: list[str], include_line: str) -> None:
    guard = conventional_include_guard(lines)
    if guard is None:
        raise ValueError("header does not have a conventional include guard")

    insert_at = guard[1] + 1
    if insert_at < len(lines) and lines[insert_at].strip() == "":
        insert_at += 1
    else:
        lines.insert(insert_at, "\n")
        insert_at += 1

    lines.insert(insert_at, include_line)
    after_include = insert_at + 1
    if after_include >= len(lines) or lines[after_include].strip() != "":
        lines.insert(after_include, "\n")


def add_include(
    text: str,
    include_name: str,
    delimiter: str,
) -> tuple[str, str, str | None]:
    if has_include(text, include_name):
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

    wanted_rows = [row for row in include_block if row[2] == delimiter]
    newline = "\n"
    if delimiter == "<":
        include_line = f"#include <{include_name}>{newline}"
    else:
        include_line = f'#include "{include_name}"{newline}'

    if not include_block:
        if has_conventional_include_guard(lines):
            insert_after_include_guard_define(lines, include_line)
            return "".join(separate_system_and_local_includes(lines)), "added", None
        return text, "manual", "no top-level include block found"

    if wanted_rows:
        names = [row[3] for row in wanted_rows]
        if names != sorted(names):
            return text, "manual", f"{delimiter} include block is not alphabetized"
        insert_at = wanted_rows[-1][0] + 1
        for row in wanted_rows:
            if include_name < row[3]:
                insert_at = row[0]
                break
        lines.insert(insert_at, include_line)
        return "".join(separate_system_and_local_includes(lines)), "added", None

    if delimiter == '"':
        system_rows = [row for row in include_block if row[2] == "<"]
        if system_rows:
            insert_at = system_rows[-1][0] + 1
        else:
            insert_at = include_block[0][0]
    else:
        insert_at = include_block[0][0]
    lines.insert(insert_at, include_line)
    return "".join(separate_system_and_local_includes(lines)), "added", None


def separate_system_and_local_includes(lines: list[str]) -> list[str]:
    """Keep system and local include groups visually separate."""
    result: list[str] = []
    depths = preprocessor_depths(lines)
    for idx, line in enumerate(lines):
        if result:
            prev_match = INCLUDE_RE.match(result[-1])
            curr_match = INCLUDE_RE.match(line)
            if (
                prev_match
                and curr_match
                and prev_match.group(2) == "<"
                and curr_match.group(2) == '"'
                and depths[idx - 1] == depths[idx]
            ):
                result.append("\n")
        result.append(line)
    return result


def is_excluded(
    path: Path,
    patterns: list[str],
    use_default_excludes: bool = True,
) -> bool:
    normalized = path.as_posix()
    if use_default_excludes and normalized in DEFAULT_EXCLUDES:
        return True
    return any(fnmatch.fnmatch(normalized, pattern) for pattern in patterns)


def convert_text(
    path: Path,
    text: str,
    force: bool = False,
    rewrite_comments: bool = False,
    start_line: int = 1,
    comment_start: str = "1",
    line_ranges: list[LineRange] | None = None,
) -> tuple[str, FileReport]:
    ranges = normalize_line_ranges(line_ranges or [], text)
    if ranges:
        if start_line != 1:
            raise ValueError("--start-line cannot be combined with --line-range")
        return convert_text_ranges(
            path,
            text,
            ranges,
            force=force,
            rewrite_comments=rewrite_comments,
            comment_start=comment_start,
        )

    prefix, work_text, line_offset = split_at_start_line(text, start_line)
    comment_start_line = parse_comment_start(comment_start, text)
    work_comment_start_line = max(1, comment_start_line - line_offset)
    identifiers = code_identifiers(work_text)

    should_convert = (
        force
        or bool(LEGACY_IDENTIFIERS & identifiers)
        or code_has_legacy_phrase(work_text)
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
            include_actions={"stdint.h": "skipped", "sim_types.h": "skipped"},
            changed=False,
            comment_hits=offset_occurrences(comment_hits, line_offset),
            comment_rewrites=offset_occurrences(comment_rewrites, line_offset),
            string_hits=offset_occurrences(string_hits, line_offset),
            manual_include_reasons=[],
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
    rewritten_work = preserve_trailing_marker_columns(work_text, rewritten_work)
    final = prefix + rewritten_work
    include_actions = {"stdint.h": "not-needed", "sim_types.h": "not-needed"}
    manual_reasons: list[str] = []

    final_identifiers = code_identifiers(final)
    if STDINT_IDENTIFIERS & final_identifiers:
        final, action, reason = add_include(final, "stdint.h", "<")
        include_actions["stdint.h"] = action
        if reason:
            manual_reasons.append(f"stdint.h: {reason}")

    final_identifiers = code_identifiers(final)
    if SIM_TYPE_IDENTIFIERS & final_identifiers:
        final, action, reason = add_include(final, "sim_types.h", '"')
        include_actions["sim_types.h"] = action
        if reason:
            manual_reasons.append(f"sim_types.h: {reason}")

    return final, FileReport(
        path=path,
        replacements=replacements,
        include_actions=include_actions,
        changed=(final != text),
        comment_hits=offset_occurrences(comment_hits, line_offset),
        comment_rewrites=offset_occurrences(comment_rewrites, line_offset),
        string_hits=offset_occurrences(string_hits, line_offset),
        manual_include_reasons=manual_reasons,
        comment_start_line=comment_start_line if rewrite_comments else None,
    )


def convert_text_ranges(
    path: Path,
    text: str,
    ranges: list[LineRange],
    force: bool = False,
    rewrite_comments: bool = False,
    comment_start: str = "1",
) -> tuple[str, FileReport]:
    lines = text.splitlines(keepends=True)
    comment_start_line = parse_comment_start(comment_start, text)
    replacements = replacement_counter()
    comment_hits: list[Occurrence] = []
    comment_rewrites: list[Occurrence] = []
    string_hits: list[Occurrence] = []
    out: list[str] = []
    cursor = 1

    for line_range in ranges:
        start_idx = line_range.start - 1
        end_idx = line_range.end
        out.extend(lines[cursor - 1 : start_idx])
        work_text = "".join(lines[start_idx:end_idx])
        identifiers = code_identifiers(work_text)
        should_convert = (
            force
            or bool(LEGACY_IDENTIFIERS & identifiers)
            or code_has_legacy_phrase(work_text)
        )

        if should_convert:
            (
                rewritten_work,
                range_replacements,
                range_comment_hits,
                range_comment_rewrites,
                range_string_hits,
            ) = rewrite_tokens(
                work_text,
                rewrite_comments=rewrite_comments,
                rewrite_comments_from_line=max(
                    1,
                    comment_start_line - (line_range.start - 1),
                ),
            )
            rewritten_work = preserve_trailing_marker_columns(
                work_text,
                rewritten_work,
            )
        else:
            (
                _,
                range_replacements,
                range_comment_hits,
                range_comment_rewrites,
                range_string_hits,
            ) = rewrite_tokens(
                work_text,
                replacements_enabled=False,
                rewrite_comments_from_line=max(
                    1,
                    comment_start_line - (line_range.start - 1),
                ),
            )
            rewritten_work = work_text

        for key, value in range_replacements.items():
            replacements[key] += value
        line_offset = line_range.start - 1
        comment_hits.extend(offset_occurrences(range_comment_hits, line_offset))
        comment_rewrites.extend(
            offset_occurrences(range_comment_rewrites, line_offset)
        )
        string_hits.extend(offset_occurrences(range_string_hits, line_offset))
        out.append(rewritten_work)
        cursor = line_range.end + 1

    out.extend(lines[cursor - 1 :])
    final = "".join(out)
    include_actions = {"stdint.h": "not-needed", "sim_types.h": "not-needed"}
    manual_reasons: list[str] = []

    final_identifiers = code_identifiers(final)
    if STDINT_IDENTIFIERS & final_identifiers:
        final, action, reason = add_include(final, "stdint.h", "<")
        include_actions["stdint.h"] = action
        if reason:
            manual_reasons.append(f"stdint.h: {reason}")

    final_identifiers = code_identifiers(final)
    if SIM_TYPE_IDENTIFIERS & final_identifiers:
        final, action, reason = add_include(final, "sim_types.h", '"')
        include_actions["sim_types.h"] = action
        if reason:
            manual_reasons.append(f"sim_types.h: {reason}")

    return final, FileReport(
        path=path,
        replacements=replacements,
        include_actions=include_actions,
        changed=(final != text),
        comment_hits=comment_hits,
        comment_rewrites=comment_rewrites,
        string_hits=string_hits,
        manual_include_reasons=manual_reasons,
        comment_start_line=comment_start_line if rewrite_comments else None,
        line_ranges=ranges,
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
    has_manual = bool(report.manual_include_reasons)
    if report.changed and write_mode and has_manual:
        status = "not-written"
    elif report.changed and write_mode:
        status = "written"
    elif report.changed:
        status = "would-change"
    else:
        status = "unchanged"
    includes = ", ".join(
        f"{name}={action}" for name, action in report.include_actions.items()
    )
    print(f"{report.path}: {status}; {counts}; includes=({includes})")
    if report.comment_start_line is not None:
        print(f"  comment rewrite start: line {report.comment_start_line}")
    if report.line_ranges:
        ranges = ", ".join(
            f"{line_range.start}:{line_range.end}"
            for line_range in report.line_ranges
        )
        print(f"  line ranges: {ranges}")
    for reason in report.manual_include_reasons:
        print(f"  manual include: {reason}")
    if report.comment_rewrites:
        print(f"  rewrote comments: {len(report.comment_rewrites)}")
    if report.comment_hits or report.string_hits:
        print(
            "  review target spellings in comments/strings: "
            f"comments={len(report.comment_hits)} strings={len(report.string_hits)}"
        )
        if show_comments:
            for hit in report.comment_rewrites:
                print(f"    line {hit.line}: rewrote {hit.kind}: {hit.token}")
            for hit in report.comment_hits + report.string_hits:
                print(f"    line {hit.line}: {hit.kind}: {hit.token}")


def process_files(args: argparse.Namespace) -> int:
    paths = [Path(p) for p in args.paths]
    reports: list[FileReport] = []
    for path in iter_source_files(paths):
        if is_excluded(path, args.exclude, not args.no_default_excludes):
            print(f"{path}: skipped by --exclude")
            continue
        text = path.read_text(encoding="utf-8")
        final, report = convert_text(
            path,
            text,
            force=args.force,
            rewrite_comments=args.rewrite_comments,
            start_line=args.start_line,
            comment_start=args.comment_start,
            line_ranges=args.line_range,
        )
        reports.append(report)
        if args.diff and final != text:
            print("".join(difflib.unified_diff(
                text.splitlines(keepends=True),
                final.splitlines(keepends=True),
                fromfile=str(path),
                tofile=str(path),
            )))
        if args.write and final != text and not report.manual_include_reasons:
            path.write_text(final, encoding="utf-8")
        elif args.write and final != text and report.manual_include_reasons:
            print("  not written: manual include placement required")
        print_report(report, args.show_comments, args.write)

    changed = sum(1 for report in reports if report.changed)
    manual = sum(1 for report in reports if report.manual_include_reasons)
    print(
        f"summary: files={len(reports)} changed={changed} "
        f"manual_includes={manual}"
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
    with tempfile.TemporaryDirectory(prefix="convert-stdint-") as td:
        root = Path(td)
        ordinary = root / "ordinary.c"
        ordinary.write_text(textwrap.dedent("""\
            /*
             * History: used uint32 and unsigned char in old notes.
             */
            #include <stdio.h>

            #define uint32 LOCAL_UINT32

            uint32 f(unsigned int x) {       /* aligned */
                const char *s = "uint32 and unsigned char in a string";
                unsigned char c = (unsigned char)x; /* unsigned char local */
                return (uint32)c;
            }
            struct aligned {
                uint8           byte_field;
                uint16          word_field;
                unsigned int    count;
            };
            #define BIG_MACRO(v) ((uint32)(v) +      \\
                                  (uint16)(v))
            uint32 shifted_comment;       /* keep comment column */
            typedef uint32 t_lba;
            typedef int32 old_int32;
            """), encoding="utf-8")

        ordered = root / "ordered.c"
        ordered.write_text(textwrap.dedent("""\
            #include <stdarg.h>
            #include <stdio.h>

            t_uint64 ordered(uint16 x) { return (t_uint64)x; }
            """), encoding="utf-8")

        header = root / "header.h"
        header.write_text(textwrap.dedent("""\
            #ifndef HEADER_H_
            #define HEADER_H_

            #include "sim_defs.h"

            unsigned char *bytes(uint8 count);

            #endif
            """), encoding="utf-8")

        dry_args = argparse.Namespace(
            paths=[str(root)],
            write=False,
            diff=False,
            show_comments=False,
            fail_on_manual=False,
            force=False,
            rewrite_comments=False,
            start_line=1,
            line_range=[],
            comment_start="1",
            exclude=[],
            no_default_excludes=False,
        )
        if process_files(dry_args) != 0:
            return 1
        assert_contains(ordinary, "uint32 f")

        write_args = argparse.Namespace(
            paths=[str(root)],
            write=True,
            diff=True,
            show_comments=True,
            fail_on_manual=False,
            force=False,
            rewrite_comments=False,
            start_line=1,
            line_range=[],
            comment_start="1",
            exclude=[],
            no_default_excludes=False,
        )
        if process_files(write_args) != 0:
            return 1

        assert_contains(ordinary, "#include <stdint.h>\n#include <stdio.h>")
        assert_contains(ordinary, '#include "sim_types.h"')
        assert_contains(ordinary, "#define uint32 LOCAL_UINT32")
        assert_contains(ordinary, "uint32_t f(uint_t x) {")
        assert_contains(ordinary, "uchar_t c = (uchar_t)x;")
        assert_contains(ordinary, "return (uint32_t)c;")
        assert_contains(ordinary, "uint8_t         byte_field;")
        assert_contains(ordinary, "uint16_t        word_field;")
        assert_contains(ordinary, "uint_t          count;")
        assert_contains(ordinary, "#define BIG_MACRO(v) ((uint32_t)(v) +    \\")
        assert_contains(
            ordinary,
            "uint32_t shifted_comment;     /* keep comment column */",
        )
        assert_contains(ordinary, "typedef uint32_t t_lba;")
        assert_contains(ordinary, "typedef int32_t old_int32;")
        assert_contains(ordinary, "History: used uint32 and unsigned char")
        assert_contains(ordinary, '"uint32 and unsigned char in a string"')

        assert_contains(
            ordered,
            "#include <stdarg.h>\n#include <stdint.h>\n#include <stdio.h>",
        )
        assert_contains(ordered, "uint64_t ordered(uint16_t x)")

        assert_contains(header, "#include <stdint.h>\n\n")
        assert_contains(header, '#include "sim_defs.h"\n#include "sim_types.h"')
        assert_contains(header, "uchar_t *bytes(uint8_t count);")

        comments = root / "comments.c"
        comment_text = textwrap.dedent("""\
            /*
             * uint32 unsigned char history.
             */
            #include <stdio.h>
            /* uint32 after header */
            uint32 comments(unsigned char c) { return c; } /* int32 trailing */
            const char *s = "uint32 unsigned char stay strings";
            """)
        final, report = convert_text(
            comments,
            comment_text,
            rewrite_comments=True,
            comment_start="auto",
        )
        if report.comment_start_line != 4:
            raise AssertionError("auto comment start should use first # line")
        if "uint32 unsigned char history" not in final:
            raise AssertionError("history comments should be preserved")
        if "/* uint32_t after header */" not in final:
            raise AssertionError("later comments should be rewritten")
        if "/* int32_t trailing */" not in final:
            raise AssertionError("trailing comments should be rewritten")
        if '"uint32 unsigned char stay strings"' not in final:
            raise AssertionError("string literals must not be rewritten")

        no_target = root / "no-target.c"
        no_target.write_text("#include <stdio.h>\nint f(void) { return 1; }\n")
        final, report = convert_text(no_target, no_target.read_text())
        if final != no_target.read_text() or report.include_actions["stdint.h"] != "skipped":
            raise AssertionError("no-target file should be skipped")

        phrase_noise = root / "phrase-noise.c"
        phrase_noise.write_text(textwrap.dedent("""\
            /* unsigned int and unsigned char in a comment */
            const char *s = "unsigned int and unsigned char in a string";
            int f(void) { return 1; }
            """), encoding="utf-8")
        final, report = convert_text(phrase_noise, phrase_noise.read_text())
        if final != phrase_noise.read_text():
            raise AssertionError(
                "comment/string phrases should not trigger conversion"
            )
        if report.include_actions["sim_types.h"] != "skipped":
            raise AssertionError(
                "comment/string phrases should not add sim_types.h"
            )

        ranged = root / "ranged.h"
        ranged.write_text(textwrap.dedent("""\
            typedef uint32 keep_alias;
            uint32 convert_decl(uint16 arg);
            typedef uint32 keep_second_alias;
            """), encoding="utf-8")
        final, report = convert_text(
            ranged,
            ranged.read_text(),
            line_ranges=[LineRange(2, 2)],
        )
        if "typedef uint32 keep_alias;" not in final:
            raise AssertionError("range conversion touched text before range")
        if "uint32_t convert_decl(uint16_t arg);" not in final:
            raise AssertionError("range conversion did not rewrite selected line")
        if "typedef uint32 keep_second_alias;" not in final:
            raise AssertionError("range conversion touched text after range")

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
        "--force",
        action="store_true",
        help="convert files even if they do not contain a target code token",
    )
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="fnmatch pattern for source paths to skip; may be repeated",
    )
    parser.add_argument(
        "--rewrite-comments",
        action="store_true",
        help="rewrite target spellings in comments too",
    )
    parser.add_argument(
        "--start-line",
        type=int,
        default=1,
        help="first 1-based line to scan and rewrite",
    )
    parser.add_argument(
        "--line-range",
        action="append",
        default=[],
        type=parse_line_range,
        metavar="START:END",
        help=(
            "inclusive 1-based line range to scan and rewrite; may be "
            "repeated; cannot be combined with --start-line"
        ),
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
        "--no-default-excludes",
        action="store_true",
        help="allow conversion of files excluded by default, such as sim_defs.h",
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
