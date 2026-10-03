// engine/kernels/occt/build/embind/bindings/capiTransform.cpp
//
// Rigid moves through the C API: the four body transforms of
// `runRigidTransform` (translate, rotate, scale, reflect) and `instanceBody`,
// which places one body at many poses inside a compound. The arithmetic the
// binding does in JavaScript (normalising a direction) is done by the caller;
// what arrives is the numbers that go into the OCCT objects.
//
// Blobs (little-endian; see capiBlob.hxx, and the writer in
// engine/crates/kpy-core/src/kernel/capi_build/):
//   transform      u8 kind (0 translate, 1 rotate, 2 scale, 3 reflect), bornIn,
//                  u32 previous, then
//                    translate  f64[3] delta
//                    rotate     f64[3] origin, f64[3] unit direction, f64 angle
//                    scale      f64[3] center, f64 factor
//                    reflect    f64[3] origin, f64[3] unit normal
//   instance_body  bornIn, u32 seed, u8 includeSeed, u8 skipNamer, u32 count,
//                  per pose u8 kind (0 translate, 1 rotate, 2 reflect) and the
//                  fields of that kind as above
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: the trim's own moves (capiTrim.cpp).

#include <string>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include "capiFinish.hxx"
#include "capiOp.hxx"
#include "capiStore.hxx"
#include "factsInternal.hxx"
#include "kapy_capi.h"

using namespace kapy_capi;

namespace {

// serialSeedOf('transformBodyTranslate'), ('transformBodyRotate'),
// ('transformBodyScale'), ('transformBodyReflect'), ('instanceBody').
constexpr size_t SEED_TRANSLATE = 1102354961u;
constexpr size_t SEED_ROTATE = 2146321400u;
constexpr size_t SEED_SCALE = 1828108317u;
constexpr size_t SEED_REFLECT = 1818422008u;
constexpr size_t SEED_INSTANCE = 1165732795u;

// Red control (perturbation 5): every translation goes a metre further along x.
constexpr double PERTURB_SHIFT = 1000.0;

enum Kind : uint8_t { kTranslate = 0, kRotate = 1, kScale = 2, kReflect = 3 };
// The kind numbers an instance pose uses (it has no scale).
enum PoseKind : uint8_t { kPoseTranslate = 0, kPoseRotate = 1, kPoseReflect = 2 };

void read3(Blob& in, double* v) {
    for (int i = 0; i < 3; ++i) v[i] = in.f64();
}

gp_Trsf translation(Blob& in) {
    double d[3];
    read3(in, d);
    if (perturbation() == 5) d[0] += PERTURB_SHIFT;
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(d[0], d[1], d[2]));
    return trsf;
}

gp_Trsf rotation(Blob& in) {
    double o[3], d[3];
    read3(in, o);
    read3(in, d);
    const double angle = in.f64();
    gp_Trsf trsf;
    trsf.SetRotation(gp_Ax1(gp_Pnt(o[0], o[1], o[2]), gp_Dir(d[0], d[1], d[2])), angle);
    return trsf;
}

gp_Trsf scaling(Blob& in) {
    double c[3];
    read3(in, c);
    const double factor = in.f64();
    gp_Trsf trsf;
    trsf.SetScale(gp_Pnt(c[0], c[1], c[2]), factor);
    return trsf;
}

// The mirror plane: the axis system whose Z is the normal.
gp_Trsf reflection(Blob& in) {
    double o[3], n[3];
    read3(in, o);
    read3(in, n);
    gp_Trsf trsf;
    trsf.SetMirror(gp_Ax2(gp_Pnt(o[0], o[1], o[2]), gp_Dir(n[0], n[1], n[2])));
    return trsf;
}

size_t seedOf(uint8_t kind) {
    switch (kind) {
        case kTranslate: return SEED_TRANSLATE;
        case kRotate: return SEED_ROTATE;
        case kScale: return SEED_SCALE;
        default: return SEED_REFLECT;
    }
}

const char* nameOf(uint8_t kind) {
    switch (kind) {
        case kTranslate: return "transformBodyTranslate";
        case kRotate: return "transformBodyRotate";
        case kScale: return "transformBodyScale";
        default: return "transformBodyReflect";
    }
}

}  // namespace

KAPY_API int32_t kapy_transform(uint32_t ptr, uint32_t length) noexcept {
    const uint8_t first = firstByte(ptr, length);
    return runOp(nameOf(first), seedOf(first), ptr, length, [](Blob& in) {
        const uint8_t kind = in.u8();
        if (kind > kReflect) throw BlobError();
        const std::string bornIn = in.str();
        Entry& previous = need(in.u32());
        gp_Trsf trsf;
        switch (kind) {
            case kTranslate: trsf = translation(in); break;
            case kRotate: trsf = rotation(in); break;
            case kScale: trsf = scaling(in); break;
            default: trsf = reflection(in); break;
        }
        // Scale and reflect copy the geometry; translate and rotate relocate it.
        const bool copy = kind == kScale || kind == kReflect;
        BRepBuilderAPI_Transform mover(previous.shape, trsf, copy);
        const TopoDS_Shape result = mover.Shape();
        return finishHistory(result, mover, previous, nullptr, bornIn, false);
    });
}

KAPY_API int32_t kapy_instance_body(uint32_t ptr, uint32_t length) noexcept {
    return runOp("instanceBody", SEED_INSTANCE, ptr, length, [](Blob& in) {
        const std::string bornIn = in.str();
        const TopoDS_Shape seed = need(in.u32()).shape;
        const bool includeSeed = in.u8() != 0;
        const bool skipNamer = in.u8() != 0;
        const uint32_t count = in.u32();

        // One independent copy the compound owns; every pose locates it.
        const gp_Trsf identity;
        BRepBuilderAPI_Transform baseMaker(seed, identity, true);
        const TopoDS_Shape baseCopy = baseMaker.Shape();

        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);
        if (includeSeed) builder.Add(compound, baseCopy);
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t kind = in.u8();
            gp_Trsf trsf;
            switch (kind) {
                case kPoseTranslate: trsf = translation(in); break;
                case kPoseRotate: trsf = rotation(in); break;
                case kPoseReflect: trsf = reflection(in); break;
                default: throw BlobError();
            }
            BRepBuilderAPI_Transform located(baseCopy, trsf, false);
            builder.Add(compound, located.Shape());
        }

        if (skipNamer) {
            const uint32_t id = allocTable();
            kapy_facts::buildEmpty(id);
            const uint32_t handle = putNative(compound, id);
            kapy_facts::bind(id, "h_" + std::to_string(handle));
            return handle;
        }
        Finish how;
        how.bornIn = bornIn;
        how.rolesJson = "{\"kind\":\"none\"}";
        how.unify = false;
        return finishSolid(compound, how);
    });
}
