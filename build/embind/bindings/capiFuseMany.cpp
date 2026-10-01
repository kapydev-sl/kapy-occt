// services/occt/build/embind/bindings/capiFuseMany.cpp
//
// Fuse every top-level solid of one shape into a single solid through the C
// API: `runFuseMany` of the binding. One BRepAlgoAPI_Fuse with the first
// member as the argument and the rest as tools; when the caller allows glue,
// full glue is tried first and kept only if it yields one valid solid that
// weighs what its members weigh together, otherwise shift glue answers. A
// rejected full glue is remembered per input (the members' weights and
// places), so the same grid goes straight to shift next time.
//
// Blob (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   fuse_many   bornIn, u32 handle, u8 glue
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the two-operand boolean (capiBoolean.cpp).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include <BOPAlgo_GlueEnum.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Message_ProgressRange.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS_Iterator.hxx>
#include <gp_Trsf.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('fuseMany').
constexpr size_t SEED_FUSE_MANY = 378582987u;
// The fuzzy band every boolean runs with.
constexpr double FUSE_FUZZY_VALUE = 1e-3;

enum Glue { kNone = 0, kShift = 1, kFull = 2 };

// What one member weighs and where its centre of mass is.
struct Member {
    double volume;
    double centre[3];
};

// Inputs whose full-glue attempt was rejected once, keyed on the members and
// not on the feature, so the branch taken depends on the shape in front of it.
std::set<std::string>& rejected() {
    static std::set<std::string> keys;
    return keys;
}

// Nine significant digits, as the binding's `toPrecision(9)`; negative zero
// reads as zero.
std::string digits(double v) {
    char text[40];
    std::snprintf(text, sizeof(text), "%.8e", v + 0.0);
    return text;
}

std::string glueKey(const std::string& bornIn, const std::vector<Member>& members) {
    std::string key = bornIn + "|";
    for (size_t i = 0; i < members.size(); ++i) {
        if (i) key += ",";
        key += digits(members[i].volume) + "@" + digits(members[i].centre[0]) + "/" +
               digits(members[i].centre[1]) + "/" + digits(members[i].centre[2]);
    }
    return key;
}

Member propsOf(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props, true, false, false);
    const gp_Pnt c = props.CentreOfMass();
    return {props.Mass(), {c.X(), c.Y(), c.Z()}};
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props, true, false, false);
    return props.Mass();
}

// One multi-argument fuse at a glue level; null when it does not converge or
// answers a null shape.
TopoDS_Shape fuseWithGlue(const std::vector<TopoDS_Shape>& members, Glue level) {
    TopTools_ListOfShape args;
    args.Append(members[0]);
    TopTools_ListOfShape tools;
    for (size_t i = 1; i < members.size(); ++i) tools.Append(members[i]);
    BRepAlgoAPI_Fuse fuse;
    fuse.SetArguments(args);
    fuse.SetTools(tools);
    fuse.SetNonDestructive(true);
    fuse.SetFuzzyValue(FUSE_FUZZY_VALUE);
    fuse.SetRunParallel(true);
    fuse.SetUseOBB(true);
    fuse.SetCheckInverted(false);
    if (level != kNone) fuse.SetGlue(level == kFull ? BOPAlgo_GlueFull : BOPAlgo_GlueShift);
    fuse.Build(Message_ProgressRange());
    if (!fuse.IsDone()) return TopoDS_Shape();
    const TopoDS_Shape shape = fuse.Shape();
    return shape.IsNull() ? TopoDS_Shape() : shape;
}

bool isOneValidSolid(const TopoDS_Shape& shape) {
    TopTools_IndexedMapOfShape solids;
    TopExp::MapShapes(shape, TopAbs_SOLID, solids);
    return solids.Extent() == 1 && isValid(shape);
}

// The union weighs what its members weigh: full glue promises they only touch.
bool coversMembers(const TopoDS_Shape& shape, const std::vector<Member>& members) {
    double expected = 0;
    for (const Member& m : members) expected += m.volume;
    return std::fabs(volumeOf(shape) - expected) <= 1e-6 * std::max(1.0, expected);
}

bool atLeastLargest(const TopoDS_Shape& shape, const std::vector<Member>& members) {
    double largest = 0;
    for (const Member& m : members) largest = std::max(largest, m.volume);
    return volumeOf(shape) >= largest * (1 - 1e-6);
}

TopoDS_Shape copyOf(const TopoDS_Shape& shape) {
    const gp_Trsf identity;
    BRepBuilderAPI_Transform copy(shape, identity, true);
    return copy.Shape();
}

}  // namespace

KAPY_API int32_t kapy_fuse_many(uint32_t ptr, uint32_t length) noexcept {
    return runOp("fuseMany", SEED_FUSE_MANY, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const uint32_t handle = in.u32();
        const bool glue = in.u8() != 0;
        const TopoDS_Shape shape = need(handle).shape;

        std::vector<TopoDS_Shape> members;
        for (TopoDS_Iterator it(shape, true, true); it.More(); it.Next()) {
            members.push_back(it.Value());
        }
        Finish how;
        how.bornIn = bornIn;
        how.rolesJson = "{\"kind\":\"none\"}";
        if (members.size() <= 1) {
            how.unify = false;
            return finishSolid(copyOf(shape), how);
        }

        std::vector<Member> props;
        for (const TopoDS_Shape& m : members) props.push_back(propsOf(m));
        const std::string key = glueKey(bornIn, props);
        TopoDS_Shape result;
        if (glue && !rejected().count(key)) {
            const TopoDS_Shape full = fuseWithGlue(members, kFull);
            if (!full.IsNull() && isOneValidSolid(full) && coversMembers(full, props)) {
                result = full;
            } else {
                rejected().insert(key);
            }
        }
        if (result.IsNull()) result = fuseWithGlue(members, glue ? kShift : kNone);
        if (result.IsNull()) throw OpError("fuseMany: BRepAlgoAPI_Fuse did not converge");
        if (!atLeastLargest(result, props)) {
            throw OpError("fuseMany: BRepAlgoAPI_Fuse dropped its members");
        }
        how.unify = true;
        return finishSolid(result, how);
    });
}
