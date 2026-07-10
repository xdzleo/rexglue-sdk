#!/usr/bin/env python3
"""codegen_ub_lint.py -- decidable UB / inconsistency lint for the codegen builders.

The recompiler emits C as string templates from src/codegen/builders/*.cpp. A wrong
template silently miscompiles EVERY port that hits that opcode -- run-heal never sees
it (it is not a FATAL, just wrong bits). This lint catches the one class of such bug
that is *decidable from the template alone*, so it can gate a commit:

  ERROR  shift-past-width  --  `intN_t(EXPR) >> K` (or `<< K`) with a LITERAL K >= N,
         or `{}.uNN >> K` with K >= NN. Shifting an integer by >= its width is
         C/C++ undefined behavior. This is exactly the NORMPACKED64 bug
         (`int32_t(x << 44) >> 44`, fixed 78af0a8): the >>44 on a 32-bit value was UB
         AND the int32_t cast dropped the field, decoding x=y=z to 0.0. Green today
         (that was the only instance); kept green forward.

  INFO   flag-inconsistency  --  the same helper (e.g. rex::float_to_xenos_half) called
         with DIFFERENT trailing boolean args across sibling arms of the SAME builder
         file. NOT asserted a bug -- FLOAT16_2 (RTZ) vs FLOAT16_4 (RTE) is a KNOWN,
         reviewed deviation (see silent-miscompile audit 2026-07-10: pervasive, working,
         1-ULP, deliberately left). Reported so a NEW divergence gets a human look.

Scans only inside double-quoted string literals (the emitted templates), never real
host-side C++, so `w >> 26` in analysis code is not matched.

Usage:  python tools/codegen_ub_lint.py [--builders DIR]
Exit 0 = no ERROR-class findings; exit 1 = at least one (gate-fails a commit).
"""
import argparse
import os
import re
import sys

# ---- width of an integer expression that appears immediately left of a shift ----
_SUFFIX_W = {"u8": 8, "s8": 8, "u16": 16, "s16": 16, "u32": 32, "s32": 32,
             "u64": 64, "s64": 64}
_CAST_RE = re.compile(r"(?:^|[^A-Za-z0-9_])(u?int(8|16|32|64)_t)\s*\($")
_SUFFIX_RE = re.compile(r"\.(u8|s8|u16|s16|u32|s32|u64|s64)(?:\[[^\]]*\])?\s*$")
# a shift by a decimal literal (optionally u/ul suffixed): the only decidable case
_SHIFT_RE = re.compile(r"(<<|>>)\s*(\d+)[uUlL]*")


def _match_paren_left(s, close_idx):
    """Given index of a ')', return index of its matching '('. -1 if unbalanced."""
    depth = 0
    for i in range(close_idx, -1, -1):
        c = s[i]
        if c == ")":
            depth += 1
        elif c == "(":
            depth -= 1
            if depth == 0:
                return i
    return -1


def _left_operand_width(s, op_start):
    """Width (bits) of the operand ending just before position op_start, or None."""
    left = s[:op_start].rstrip()
    if not left:
        return None
    if left.endswith(")"):
        open_i = _match_paren_left(left, len(left) - 1)
        if open_i < 0:
            return None
        m = _CAST_RE.search(left[:open_i + 1])  # cast token right before '('
        if m:
            return int(m.group(2))
        return None  # a parenthesized non-cast expr -> width unknown, stay silent
    m = _SUFFIX_RE.search(left)               # e.g. {}.u64[0]  or  x.s32
    if m:
        return _SUFFIX_W[m.group(1)]
    return None


def _string_spans(line):
    """Yield (start, end, inner) for each double-quoted literal on the line."""
    out, i, n = [], 0, len(line)
    while i < n:
        if line[i] == '"':
            j = i + 1
            while j < n and not (line[j] == '"' and line[j - 1] != "\\"):
                j += 1
            out.append((i + 1, j, line[i + 1:j]))
            i = j + 1
        else:
            i += 1
    return out


def lint_file(path):
    errors, calls = [], {}  # calls: helper -> set of (argsig, line)
    with open(path, encoding="utf-8", errors="replace") as f:
        for lno, line in enumerate(f, 1):
            for _, _, inner in _string_spans(line):
                for m in _SHIFT_RE.finditer(inner):
                    k = int(m.group(2))
                    w = _left_operand_width(inner, m.start())
                    if w is not None and k >= w:
                        errors.append((lno, w, m.group(1), k, inner.strip()[:96]))
            # flag-inconsistency: helper(...) trailing-arg signature
            for hm in re.finditer(r"(rex::float_to_xenos_half)\(([^;]*?)\)", line):
                args = hm.group(2)
                # trailing boolean-literal args only (true/false), order-preserving
                bools = tuple(re.findall(r"\b(true|false)\b", args))
                calls.setdefault(hm.group(1), set()).add((bools, lno))
    return errors, calls


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--builders", default=None,
                    help="builders dir (default: <repo>/src/codegen/builders)")
    args = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    bdir = args.builders or os.path.join(here, "..", "src", "codegen", "builders")
    bdir = os.path.normpath(bdir)

    if not os.path.isdir(bdir):
        print("codegen_ub_lint: builders dir not found: %s" % bdir, file=sys.stderr)
        return 2

    total_err = 0
    all_calls = {}
    for root, _, files in os.walk(bdir):
        for fn in sorted(files):
            if not fn.endswith((".cpp", ".h")):
                continue
            path = os.path.join(root, fn)
            errs, calls = lint_file(path)
            for (lno, w, opk, k, snip) in errs:
                total_err += 1
                rel = os.path.relpath(path, os.path.join(here, ".."))
                print("ERROR shift-past-width  %s:%d  int%d %s %d (>= width %d = UB)"
                      % (rel.replace("\\", "/"), lno, w, opk, k, w))
                print("      | %s" % snip)
            for helper, sigs in calls.items():
                d = all_calls.setdefault(helper, set())
                d |= sigs

    info = 0
    for helper, sigs in sorted(all_calls.items()):
        variants = {b for (b, _) in sigs}
        if len(variants) > 1:
            info += 1
            print("INFO  flag-inconsistency  %s called %d distinct trailing-flag ways:"
                  % (helper, len(variants)))
            for (b, lno) in sorted(sigs, key=lambda x: x[1]):
                print("      L%-6d args=%s" % (lno, ("(default)" if not b else b)))

    print("\ncodegen_ub_lint: %d ERROR (shift-past-width), %d INFO (flag-inconsistency)"
          % (total_err, info))
    return 1 if total_err else 0


if __name__ == "__main__":
    sys.exit(main())
