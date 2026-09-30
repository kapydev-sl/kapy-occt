#!/usr/bin/env python3
#
# Quantify the ONE correctness risk of Option C (see embind/README.md):
# opencascade.js names overloads `_N`, where N is the 1-based position of a
# declaration among its same-named siblings, in header declaration order
# (src/wasmGenerator/Common.py). Our ~333 call sites hard-code the numbers
# OCCT 7.7 produced. If OCCT 8.0 inserted, removed or reordered a
# constructor anywhere in a class, every later `_N` in that class shifts —
# silently, since a shifted number still compiles and still constructs.
#
# So: for every constructor overload the code actually uses, compare the
# 7.7 signature (from opencascade.js's shipped .d.ts, which IS the numbering
# our code targets) against the signature at that same position in the
# OCCT 8.0 header. Agreement means the number is safe to reuse under C1.
#
# Usage (needs the OCCT 8.0 headers a configure run collected):
#   python3 services/occt/build/compare-overloads.py \
#       --occt-include ~/kapy-occt-build/occt-build/include/opencascade
#
# Reads: embind/BINDINGS_SPEC.md (which overloads are used) and
# node_modules/opencascade.js/dist/opencascade.full.d.ts (the 7.7 numbering).
# Writes a verdict per used overload to stdout; exits 1 if any MISMATCH.

import argparse
import os
import re
import sys

import clang.cindex as ci

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
SPEC = os.path.join(REPO, "services/occt/build/embind/BINDINGS_SPEC.md")
DTS = os.path.join(REPO, "node_modules/opencascade.js/dist/opencascade.full.d.ts")

# C++ spellings that opencascade.js collapses to a TS primitive. Comparing
# only the class-typed arguments keeps the match robust against the many
# ways a numeric typedef can be spelled on either side.
PRIMITIVE = re.compile(
    r"^(Standard_(Real|Integer|Boolean|ShortReal|Size|CString)|double|float|int|unsigned|bool|char)\b"
)


# The used constructor overloads per class, as `{'gp_Pnt': [3, 5], ...}`.
def used_constructors(path: str) -> dict[str, list[int]]:
    out: dict[str, list[int]] = {}
    cls = None
    for line in open(path, encoding="utf-8"):
        m = re.match(r"^## (\w+)", line)
        if m:
            cls = m.group(1)
            continue
        m = re.match(r"^- constructors: (.+)", line)
        if m and cls:
            # "(unnumbered)" means the class has a single constructor, so
            # there is no number that can shift. Nothing to check.
            nums = [n.strip().lstrip("_") for n in m.group(1).split(",")]
            picked = sorted(int(n) for n in nums if n.isdigit())
            if picked:
                out[cls] = picked
    return out


# The 7.7 constructor signature for `Class_N`, as a list of argument type
# names, from the shipped typings. Absent → None (the class had a single,
# unnumbered constructor, or the overload is not in the .d.ts).
def dts_signature(dts: str, cls: str, n: int) -> list[str] | None:
    m = re.search(
        r"export declare class %s_%d extends %s \{\s*constructor\(([^)]*)\);" % (cls, n, cls),
        dts,
    )
    if not m:
        return None
    args = m.group(1).strip()
    if not args:
        return []
    return [a.split(":")[-1].strip() for a in args.split(",")]


# Every public constructor of `cls` in `header`, in declaration order, each
# as its list of argument type spellings. This mirrors exactly what the
# generator enumerates when it assigns `_N`.
def header_constructors(
    header: str, cls: str, include: str, sysroot: str, resource_dir: str
) -> list[list[str]] | None:
    index = ci.Index.create()
    args = ["-x", "c++", "-std=c++17", f"-I{include}"]
    # Without the C++ standard library AND clang's own builtin headers on the
    # include path, unresolved types degrade to `int` and the comparison is
    # garbage — `gp_Dir(const gp_XYZ&)` reads as `gp_Dir(const int&)`.
    if sysroot:
        args += ["-isysroot", sysroot]
    if resource_dir:
        args += ["-resource-dir", resource_dir]
    tu = index.parse(
        header,
        args=args,
        options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
        | ci.TranslationUnit.PARSE_INCOMPLETE,
    )
    fatal = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
    if fatal:
        raise SystemExit(
            f"clang could not parse {os.path.basename(header)} cleanly "
            f"({len(fatal)} errors, first: {fatal[0].spelling}). "
            "Types would silently degrade to `int` — refusing to compare."
        )
    for node in tu.cursor.walk_preorder():
        if (
            node.kind in (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL)
            and node.spelling == cls
            and node.is_definition()
        ):
            ctors = [
                c
                for c in node.get_children()
                if c.kind == ci.CursorKind.CONSTRUCTOR
                and c.access_specifier == ci.AccessSpecifier.PUBLIC
            ]
            return [[a.type.spelling for a in c.get_arguments()] for c in ctors]
    return None


# Compare a 7.7 TS signature against an 8.0 C++ one by their class-typed
# arguments only (primitives differ in spelling but never in meaning here).
def classes_of_cpp(args: list[str]) -> list[str]:
    out = []
    for a in args:
        # `const opencascade::handle<Geom_Curve> &` → `Geom_Curve`.
        t = a.replace("const", " ").replace("&", " ").replace("*", " ")
        t = t.replace("<", " ").replace(">", " ").replace("::", " ")
        parts = [p for p in t.split() if p not in ("opencascade", "occ", "handle")]
        name = parts[-1] if parts else ""
        if name and not PRIMITIVE.match(name):
            out.append(name)
    return out


def classes_of_ts(args: list[str]) -> list[str]:
    # opencascade.js spells a handle argument `Handle_Geom_Surface`; OCCT
    # spells the same thing `opencascade::handle<Geom_Surface>`.
    out = []
    for a in args:
        if a in ("number", "boolean", "string") or a.startswith("Standard_"):
            continue
        out.append(a[len("Handle_") :] if a.startswith("Handle_") else a)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--occt-include", required=True)
    ap.add_argument("--libclang", default="")
    ap.add_argument("--sysroot", default="", help="macOS SDK path (xcrun --show-sdk-path)")
    ap.add_argument("--resource-dir", default="", help="clang -print-resource-dir")
    args = ap.parse_args()
    if args.libclang:
        ci.Config.set_library_file(args.libclang)

    include = os.path.expanduser(args.occt_include)
    sysroot = os.path.expanduser(args.sysroot)
    resource_dir = os.path.expanduser(args.resource_dir)
    dts = open(DTS, encoding="utf-8").read()
    used = used_constructors(SPEC)

    ok = shifted = unknown = 0
    for cls, numbers in sorted(used.items()):
        header = os.path.join(include, f"{cls}.hxx")
        if not os.path.exists(header):
            print(f"?? {cls:38} no header in OCCT 8.0 ({os.path.basename(header)})")
            unknown += len(numbers)
            continue
        ctors = header_constructors(header, cls, include, sysroot, resource_dir)
        if ctors is None:
            print(f"?? {cls:38} class not found in its own header")
            unknown += len(numbers)
            continue
        for n in numbers:
            want = dts_signature(dts, cls, n)
            if want is None:
                print(f"?? {cls}_{n:<34} not in the 7.7 typings")
                unknown += 1
                continue
            if n > len(ctors):
                print(f"XX {cls}_{n:<34} 8.0 has only {len(ctors)} public ctors")
                shifted += 1
                continue
            got = classes_of_cpp(ctors[n - 1])
            exp = classes_of_ts(want)
            if got == exp:
                ok += 1
                continue
            shifted += 1
            # Where did the wanted signature go? Hand-written Embind names a
            # binding freely, so the fix is to bind `Class_N` to whatever
            # position now holds the 7.7 signature — never to renumber the app.
            moved = [
                i + 1
                for i, c in enumerate(ctors)
                if classes_of_cpp(c) == exp and len(c) == len(want)
            ]
            if len(moved) == 1:
                sig = ", ".join(ctors[moved[0] - 1]) or "void"
                print(f"XX {cls}_{n} -> bind to 8.0 ctor #{moved[0]} ({sig})")
            else:
                print(f"XX {cls}_{n} -> no unambiguous 8.0 match ({len(moved)} candidates)")
            print(f"     7.7 wants   : ({', '.join(want) or 'void'})")
            print(f"     8.0 at _{n} : ({', '.join(ctors[n - 1]) or 'void'})")

    total = ok + shifted + unknown
    print(f"\n{ok}/{total} constructor overloads keep their number under OCCT 8.0")
    print(f"{shifted} shifted, {unknown} undetermined")
    return 1 if shifted else 0


if __name__ == "__main__":
    sys.exit(main())
