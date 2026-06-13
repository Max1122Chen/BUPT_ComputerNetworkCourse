#!/usr/bin/env python3
"""Copy dns_relay include/*.h and src/*.c to code_for_defense/ with comments removed."""

from __future__ import annotations

import re
import sys
from pathlib import Path


def strip_c_comments(text: str) -> str:
    out: list[str] = []
    i = 0
    n = len(text)
    state = "code"

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if state == "code":
            if ch == '"':
                out.append(ch)
                state = "string"
                i += 1
            elif ch == "'":
                out.append(ch)
                state = "char"
                i += 1
            elif ch == "/" and nxt == "/":
                state = "line_comment"
                i += 2
            elif ch == "/" and nxt == "*":
                state = "block_comment"
                i += 2
            else:
                out.append(ch)
                i += 1
        elif state == "string":
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
            elif ch == '"':
                state = "code"
                i += 1
            else:
                i += 1
        elif state == "char":
            out.append(ch)
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
            elif ch == "'":
                state = "code"
                i += 1
            else:
                i += 1
        elif state == "line_comment":
            if ch == "\n":
                out.append("\n")
                state = "code"
            i += 1
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
            else:
                if ch == "\n":
                    out.append("\n")
                i += 1
        else:
            raise RuntimeError(f"unexpected state: {state}")

    cleaned = "".join(out)
    cleaned = re.sub(r"[ \t]+\n", "\n", cleaned)
    cleaned = re.sub(r"\n{3,}", "\n\n", cleaned)
    return cleaned.rstrip() + "\n"


def copy_tree(src_dir: Path, dst_dir: Path, suffix: str) -> int:
    count = 0
    for src in sorted(src_dir.glob(f"*{suffix}")):
        if src.name.startswith("."):
            continue
        dst = dst_dir / src.name
        dst.parent.mkdir(parents=True, exist_ok=True)
        original = src.read_text(encoding="utf-8")
        dst.write_text(strip_c_comments(original), encoding="utf-8", newline="\n")
        count += 1
    return count


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    out_root = root / "code_for_defense"
    include_src = root / "include"
    src_src = root / "src"

    if not include_src.is_dir() or not src_src.is_dir():
        print("error: expected include/ and src/ under dns_relay/", file=sys.stderr)
        return 1

    out_include = out_root / "include"
    out_src = out_root / "src"

    n_h = copy_tree(include_src, out_include, ".h")
    n_c = copy_tree(src_src, out_src, ".c")

    readme = out_root / "README.md"
    readme.write_text(
        "# code_for_defense\n\n"
        "Comment-free copies of `include/*.h` and `src/*.c` for acceptance/defense walkthrough.\n\n"
        "Regenerate from `dns_relay/`:\n\n"
        "```powershell\n"
        "python scripts/strip_comments_copy.py\n"
        "```\n\n"
        "Source files with comments remain in `include/` and `src/`.\n",
        encoding="utf-8",
        newline="\n",
    )

    print(f"Wrote {n_h} headers and {n_c} sources to {out_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
