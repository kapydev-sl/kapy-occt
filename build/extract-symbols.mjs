#!/usr/bin/env node
// services/occt/build/extract-symbols.mjs
//
// Scans src/cad/** for every OCCT symbol the worker code references
// (`oc.<Symbol>` / `instance.<Symbol>` / `new oc.<Symbol>`) and diffs
// that set against the `bindings:` list in kapy-occt.yml. The custom
// OCCT build must bind exactly these classes; this script is the
// maintainable check that the yml has not drifted from the code —
// same spirit as the planegcs binding scanner in
// services/planegcs/build/build-bindings.
//
// Usage:
//   node services/occt/build/extract-symbols.mjs          # report drift
//   node services/occt/build/extract-symbols.mjs --list   # print symbols
//
// Exit code 1 if the yml is missing any symbol the code uses (a build
// against a drifted yml would fail on that symbol anyway).

import { readFileSync, readdirSync, statSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join, resolve } from 'node:path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const srcRoot = resolve(__dirname, '../../../src/cad');
const ymlPath = resolve(__dirname, 'kapy-occt.yml');

// Emscripten appends `_N` overload suffixes to bound C++ constructors /
// methods (gp_Pnt_3, TopoDS.Face_1). The custom-build binds the base
// class symbol, so strip the numeric suffix to get the class name.
// OCCT symbols are either Package_Class (has an underscore: gp_Pnt,
// BRep_Tool, TopoDS_Shape — the lowercase `gp_` prefix matters) or a
// PascalCase package namespace (TopExp, ShapeAnalysis, StlAPI). The two
// alternatives below match exactly those and skip lowercase method calls
// (`instance.delete`, `oc.ready`).
const REF =
    /(?:\bnew\s+)?(?:oc|instance)\.([A-Za-z][A-Za-z0-9]*_[A-Za-z0-9_]+|[A-Z][A-Za-z0-9]+)/g;

// Not OCCT classes: `oc.FS` is the Emscripten virtual FS, `oc.OCJS` is
// opencascade.js's own helper namespace, `Transformation` is a method on
// TopLoc_Location (loc.Transformation()), not a bindable class.
const NOT_A_BINDING = new Set(['FS', 'OCJS', 'Transformation']);

function walk(dir) {
    const out = [];
    for (const name of readdirSync(dir)) {
        const p = join(dir, name);
        const st = statSync(p);
        if (st.isDirectory()) out.push(...walk(p));
        else if (name.endsWith('.ts') || name.endsWith('.tsx')) out.push(p);
    }
    return out;
}

function stripOverload(sym) {
    return sym.replace(/_\d+$/, '');
}

const used = new Set();
for (const file of walk(srcRoot)) {
    const text = readFileSync(file, 'utf8');
    let m;
    while ((m = REF.exec(text)) !== null) {
        const sym = stripOverload(m[1]);
        if (!NOT_A_BINDING.has(sym)) used.add(sym);
    }
}

if (process.argv.includes('--list')) {
    for (const s of [...used].sort()) console.log(s);
    process.exit(0);
}

const yml = readFileSync(ymlPath, 'utf8');
const bound = new Set(
    [...yml.matchAll(/^\s*-\s*symbol:\s*([A-Za-z0-9_]+)\s*$/gm)].map((m) => m[1]),
);

const missing = [...used].filter((s) => !bound.has(s)).sort();
// TopoDS.Face_1 etc. resolve through the `TopoDS` namespace symbol, and a
// few sub-shape classes (TopoDS_Face…) are reached via TopoDS methods, so
// presence of the namespace covers them — report anyway for review.
const extra = [...bound].filter(
    (s) => !used.has(s) && !s.startsWith('TopoDS_') && !s.startsWith('HLR'),
);

console.log(`code references ${used.size} OCCT symbols; yml binds ${bound.size}.`);
if (missing.length) {
    console.log(`\nMISSING from kapy-occt.yml (${missing.length}):`);
    for (const s of missing) console.log('  - ' + s);
} else {
    console.log('\nOK: every symbol used in src/cad is present in kapy-occt.yml.');
}
if (extra.length) {
    console.log(`\nBound but not seen in src (review — may be indirect):`);
    for (const s of extra) console.log('  · ' + s);
}
process.exit(missing.length ? 1 : 0);
