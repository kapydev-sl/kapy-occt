#!/usr/bin/env python3
#
# The method half of the overload-numbering audit (constructors live in
# compare-overloads.py). It is the nastier half: for methods the generator
# computes `_N` over EVERY same-named child of the class, whatever its
# access (src/wasmGenerator/Common.py:68) — so a new `private:` overload in
# OCCT 8.0 shifts the public numbering our call sites depend on, invisibly.
#
# Driven by the class list, never by the method name: `Append_1` is declared
# by 129 classes in the 7.7 typings, so resolving an owner from a call site
# is hopeless. Instead, for each class we bind, take the numbered methods the
# 7.7 typings give it, keep the ones our code calls, and check whether the
# same position in the OCCT 8.0 header still holds a compatible signature.
#
# Hand-written Embind can bind any name to any overload, so a shift is not a
# blocker — the output is the remap table the bindings must follow.
#
#   python3 services/occt/build/compare-methods.py \
#       --occt-include ~/kapy-occt-build/occt-build/include/opencascade \
#       --libclang <libclang.dylib> --sysroot "$(xcrun --show-sdk-path)" \
#       --resource-dir "$(xcrun clang -print-resource-dir)"
#
# Exits 1 when a used method's number moved.

import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict

import clang.cindex as ci

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
SRC = os.path.join(REPO, "src/cad")
DTS = os.path.join(REPO, "node_modules/opencascade.js/dist/opencascade.full.d.ts")
SYMBOLS = os.path.join(HERE, "extract-symbols.mjs")

CALL = re.compile(r"\.([A-Za-z][A-Za-z0-9_]*)_([0-9]+)\(")

# Type names opencascade.js's typings collapse to a TS primitive.
PRIMITIVE = re.compile(
    r"^(Standard_(Real|Integer|Boolean|ShortReal|Size|CString)"
    r"|double|float|int|unsigned|bool|char|void|size_t)$"
)

# OCCT 8.0 dropped the per-typedef TopTools headers and now spells these
# types as the NCollection templates they always aliased. Same type, new
# spelling: normalise the 7.7 name to the 8.0 template base before comparing.
TOPTOOLS_ALIAS = {
    "TopTools_ListOfShape": "NCollection_List",
    "TopTools_MapOfShape": "NCollection_Map",
    "TopTools_MapOfOrientedShape": "NCollection_Map",
    "TopTools_IndexedMapOfShape": "NCollection_IndexedMap",
    "TopTools_DataMapOfShapeShape": "NCollection_DataMap",
    "TopTools_DataMapOfShapeListOfShape": "NCollection_DataMap",
    "TopTools_IndexedDataMapOfShapeListOfShape": "NCollection_IndexedDataMap",
    "TopTools_SequenceOfShape": "NCollection_Sequence",
}


# The OCCT classes we bind, from the extractor that validates them.
def bound_classes() -> list[str]:
    out = subprocess.check_output(["node", SYMBOLS, "--list"], text=True)
    return [l.strip() for l in out.splitlines() if l.strip()]


# Every `.Name_N(` the TypeScript calls, as {('Name', N), ...}.
def used_methods(root: str) -> set[tuple[str, int]]:
    out: set[tuple[str, int]] = set()
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f.endswith(".ts"):
                text = open(os.path.join(dirpath, f), encoding="utf-8").read()
                out.update((n, int(i)) for n, i in CALL.findall(text))
    return out


# {class: {(method, N): [ts arg types]}} from the 7.7 typings.
def dts_class_methods(path: str) -> dict[str, dict[tuple[str, int], list[str]]]:
    out: dict[str, dict[tuple[str, int], list[str]]] = defaultdict(dict)
    cls = None
    for line in open(path, encoding="utf-8"):
        m = re.match(r"\s*export declare class (\w+)", line)
        if m:
            cls = m.group(1)
            continue
        if re.match(r"\s*\}\s*$", line):
            cls = None
            continue
        if not cls:
            continue
        m = re.match(r"\s*(?:static\s+)?(\w+)_(\d+)\(([^)]*)\):", line)
        if m:
            args = m.group(3).strip()
            types = [a.split(":")[-1].strip() for a in args.split(",")] if args else []
            out[cls][(m.group(1), int(m.group(2)))] = types
    return out


# `const occ::handle<Geom_Curve>&` -> `Geom_Curve`;
# `const NCollection_List<TopoDS_Shape>&` -> `NCollection_List`.
def normalise_cpp(spelling: str) -> str:
    t = spelling.replace("const", " ").replace("&", " ").replace("*", " ").strip()
    m = re.match(r"(?:\w+::)*(\w+)\s*<(.+)>$", t)
    if m:
        base, inner = m.group(1), m.group(2)
        if base == "handle":
            return normalise_cpp(inner.split(",")[0])
        return base
    return t.split("::")[-1].split()[-1] if t else ""


def normalise_ts(name: str) -> str:
    if name.startswith("Handle_"):
        return name[len("Handle_") :]
    return TOPTOOLS_ALIAS.get(name, name)


def sig_cpp(args: list[str]) -> list[str]:
    out = [normalise_cpp(a) for a in args]
    return [t for t in out if not PRIMITIVE.match(t)]


def sig_ts(args: list[str]) -> list[str]:
    out = [normalise_ts(a) for a in args]
    return [t for t in out if not PRIMITIVE.match(t) and t not in ("number", "boolean", "string")]


# All children of `cls` (class or namespace) grouped by spelling, in
# declaration order, whatever their access — the list the generator indexes.
def header_overloads(
    header: str, cls: str, include: str, sysroot: str, resource_dir: str
) -> dict[str, list[tuple[str, list[str]]]] | None:
    args = ["-x", "c++", "-std=c++17", f"-I{include}"]
    if sysroot:
        args += ["-isysroot", sysroot]
    if resource_dir:
        args += ["-resource-dir", resource_dir]
    tu = ci.Index.create().parse(
        header,
        args=args,
        options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
        | ci.TranslationUnit.PARSE_INCOMPLETE,
    )
    fatal = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
    if fatal:
        raise SystemExit(f"clang cannot parse {os.path.basename(header)}: {fatal[0].spelling}")

    kinds = (ci.CursorKind.CLASS_DECL, ci.CursorKind.STRUCT_DECL, ci.CursorKind.NAMESPACE)
    for node in tu.cursor.walk_preorder():
        if node.kind in kinds and node.spelling == cls:
            # OCCT 8.0 turned some static-method holders (TopoDS) into
            # namespaces of free functions; both shapes are enumerated the
            # same way, in declaration order.
            if node.kind != ci.CursorKind.NAMESPACE and not node.is_definition():
                continue
            found: dict[str, list[tuple[str, list[str]]]] = defaultdict(list)
            for c in node.get_children():
                if c.kind in (ci.CursorKind.CXX_METHOD, ci.CursorKind.FUNCTION_DECL):
                    access = str(c.access_specifier).split(".")[-1]
                    if node.kind == ci.CursorKind.NAMESPACE:
                        access = "PUBLIC"
                    found[c.spelling].append((access, [a.type.spelling for a in c.get_arguments()]))
            if found:
                return found
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--occt-include", required=True)
    ap.add_argument("--libclang", default="")
    ap.add_argument("--sysroot", default="")
    ap.add_argument("--resource-dir", default="")
    a = ap.parse_args()
    if a.libclang:
        ci.Config.set_library_file(a.libclang)
    include = os.path.expanduser(a.occt_include)

    used = used_methods(SRC)
    decls = dts_class_methods(DTS)

    ok = shifted = skipped = 0
    for cls in bound_classes():
        wanted = {k: v for k, v in decls.get(cls, {}).items() if k in used}
        if not wanted:
            continue
        header = os.path.join(include, f"{cls}.hxx")
        if not os.path.exists(header):
            print(f"?? {cls:32} no OCCT 8.0 header — {len(wanted)} methods unchecked")
            skipped += len(wanted)
            continue
        overloads = header_overloads(header, cls, include, a.sysroot, a.resource_dir)
        if overloads is None:
            print(f"?? {cls:32} not declared in its own header")
            skipped += len(wanted)
            continue
        for (name, n), want in sorted(wanted.items()):
            decl = overloads.get(name, [])
            if n > len(decl):
                print(f"XX {cls}.{name}_{n} -> 8.0 declares only {len(decl)} overloads")
                shifted += 1
                continue
            access, got = decl[n - 1]
            if access == "PUBLIC" and sig_cpp(got) == sig_ts(want):
                ok += 1
                continue
            shifted += 1
            moved = [
                i + 1
                for i, (acc, args) in enumerate(decl)
                if acc == "PUBLIC" and sig_cpp(args) == sig_ts(want) and len(args) == len(want)
            ]
            where = f"bind to overload #{moved[0]}" if len(moved) == 1 else f"{len(moved)} candidates"
            print(f"XX {cls}.{name}_{n} -> {where}")
            print(f"     7.7 wants   : ({', '.join(want) or 'void'})")
            print(f"     8.0 at _{n} : {access} ({', '.join(got) or 'void'})")

    total = ok + shifted + skipped
    print(f"\n{ok}/{total} used method overloads keep their number under OCCT 8.0")
    print(f"{shifted} shifted, {skipped} unchecked")
    return 1 if shifted else 0


if __name__ == "__main__":
    sys.exit(main())
