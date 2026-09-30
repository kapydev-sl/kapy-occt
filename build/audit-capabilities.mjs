// services/occt/build/audit-capabilities.mjs
//
// The app probes the OCCT build at runtime — `typeof map.FindIndex ===
// 'function'`, `'SetMirror_1' in trsfProto`, `typeof oc.getExceptionMessage` —
// and quietly degrades when a member is missing. A stripped build therefore
// passes every test while running six times slower (the TopoNamer's O(n^2)
// fallback) or reporting `OCCT error 0x1234` instead of a message.
//
// This asserts that a custom build trips NONE of those guards. Every entry is
// a member the app checks for before using; grep the sources for the guard if
// you wonder why one is here.
//
//   node services/occt/build/audit-capabilities.mjs services/occt/dist/kapy-occt.js
//
// Exits 1 when any probe would fall back.

import { pathToFileURL } from 'node:url';
import { resolve } from 'node:path';

const modulePath = process.argv[2];
if (!modulePath) {
    console.error('usage: audit-capabilities.mjs <kapy-occt.js>');
    process.exit(2);
}

const oc = await (await import(pathToFileURL(resolve(modulePath)).href)).default();

// [where the guard lives, how to reach the member, what it degrades to]
const PROBES = [
    ['occtApi.ts:110', () => typeof oc.getExceptionMessage === 'function', 'errors show as OCCT error 0x…'],
    ['namer.helpers.ts:105', () => typeof new oc.TopTools_IndexedMapOfShape_1().FindIndex === 'function', 'TopoNamer becomes O(n^2)'],
    ['extrudeProfile.helpers.ts:148', () => typeof new oc.TopTools_IndexedMapOfShape_1().FindIndex === 'function', 'profile lookup becomes O(n^2)'],
    ['capabilities.ts:59', () => typeof new oc.gp_Trsf_1().SetMirror_3 === 'function', 'mirroring is disabled'],
    ['runBoolean.ts:88', () => typeof new oc.BRepAlgoAPI_Cut_1().SetFuzzyValue === 'function', 'sliver faces survive booleans'],
    ['runBoolean.ts:91', () => typeof new oc.BRepAlgoAPI_Cut_1().SetRunParallel === 'function', 'booleans stay single-threaded'],
    ['runBoolean.ts:96', () => typeof new oc.BRepAlgoAPI_Cut_1().SetUseOBB === 'function', 'booleans lose the OBB pre-filter'],
    ['runBoolean.ts:102', () => typeof new oc.BRepAlgoAPI_Cut_1().SetCheckInverted === 'function', 'inverted-solid check stays on'],
    ['runBoolean.ts:107', () => typeof new oc.BRepAlgoAPI_Cut_1().SetGlue === 'function', 'glue mode is never applied'],
    ['runFuseMany.ts:64', () => typeof new oc.BRepAlgoAPI_Fuse_1().SetRunParallel === 'function', 'fuse stays single-threaded'],
    ['runFuseMany.ts:71', () => typeof new oc.BRepAlgoAPI_Fuse_1().IsDone === 'function', 'fuse result is never checked'],
    ['unifyFuse.ts:38', () => typeof oc.ShapeUpgrade_UnifySameDomain.prototype.SetLinearTolerance === 'function', 'unify uses default tolerance'],
    ['unifyFuse.ts:39', () => typeof oc.ShapeUpgrade_UnifySameDomain.prototype.SetAngularTolerance === 'function', 'unify uses default tolerance'],
    ['threadSweepShape.ts:118', () => typeof oc.BRepOffsetAPI_MakePipeShell.prototype.SetForceApproxC1 === 'function', 'thread sweep loses C1 approximation'],
];

let failed = 0;
for (const [where, probe, degradation] of PROBES) {
    let ok = false;
    try {
        ok = probe() === true;
    } catch {
        ok = false;
    }
    if (!ok) {
        failed++;
        console.log(`MISSING  ${where.padEnd(30)} -> ${degradation}`);
    }
}

console.log(`\n${PROBES.length - failed}/${PROBES.length} capability probes satisfied`);
process.exit(failed ? 1 : 0);
