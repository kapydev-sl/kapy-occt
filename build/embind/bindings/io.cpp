// services/occt/build/embind/bindings/io.cpp
//
// STEP and STL exchange. These pull in TKDESTEP / TKDESTL, the heaviest
// toolkits in the link — worth remembering when reading the wasm size.
//
// The reader/writer take C strings; embind marshals std::string, so the
// wrappers convert. Files are read from and written to emscripten's virtual
// FS, which is why the module exports `FS` (see ../CMakeLists.txt).
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <IFSelect_ReturnStatus.hxx>
#include <Message_ProgressRange.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI.hxx>
#include <TopoDS_Shape.hxx>

using namespace emscripten;

static IFSelect_ReturnStatus Reader_ReadFile(STEPControl_Reader& r, std::string path) {
    return r.ReadFile(path.c_str());
}
static int Reader_TransferRoots(STEPControl_Reader& r, const Message_ProgressRange& range) {
    return r.TransferRoots(range);
}
static int Reader_NbRootsForTransfer(STEPControl_Reader& r) { return r.NbRootsForTransfer(); }
static int Reader_NbShapes(STEPControl_Reader& r) { return r.NbShapes(); }
static TopoDS_Shape Reader_OneShape(STEPControl_Reader& r) { return r.OneShape(); }

static IFSelect_ReturnStatus Writer_Transfer(STEPControl_Writer& w, const TopoDS_Shape& s,
                                             STEPControl_StepModelType mode, bool compgraph,
                                             const Message_ProgressRange& range) {
    return w.Transfer(s, mode, compgraph, range);
}
static IFSelect_ReturnStatus Writer_Write(STEPControl_Writer& w, std::string path) {
    return w.Write(path.c_str());
}

// `oc.BRepTools.Write_3(shape, file, range)` / `Read_2(shape, file, builder, range)`.
// The .brep format is OCCT's own, lossless round-trip — the only way to hand
// the exact same topology to two different kernels.
struct BRepTools_Package {};
static bool BRepTools_Write_3(const TopoDS_Shape& s, std::string file,
                              const Message_ProgressRange& r) {
    return BRepTools::Write(s, file.c_str(), r);
}
static bool BRepTools_Read_2(TopoDS_Shape& s, std::string file, BRep_Builder& b,
                             const Message_ProgressRange& r) {
    return BRepTools::Read(s, file.c_str(), b, r);
}

// `oc.StlAPI.Write(shape, file, ascii)`.
struct StlAPI_Package {};
static bool Stl_Write(const TopoDS_Shape& s, std::string file, bool ascii) {
    return StlAPI::Write(s, file.c_str(), ascii);
}

EMSCRIPTEN_BINDINGS(kapy_occt_io) {
    class_<STEPControl_Reader>("STEPControl_Reader")
        .constructor<>()
        .function("ReadFile", &Reader_ReadFile)
        .function("NbRootsForTransfer", &Reader_NbRootsForTransfer)
        .function("TransferRoots", &Reader_TransferRoots)
        .function("NbShapes", &Reader_NbShapes)
        .function("OneShape", &Reader_OneShape);
    function("STEPControl_Reader_1", +[]() { return STEPControl_Reader(); });

    class_<STEPControl_Writer>("STEPControl_Writer")
        .constructor<>()
        .function("Transfer", &Writer_Transfer)
        .function("Write", &Writer_Write);
    function("STEPControl_Writer_1", +[]() { return STEPControl_Writer(); });

    class_<StlAPI_Package>("StlAPI").class_function("Write", &Stl_Write);

    class_<BRepTools_Package>("BRepTools")
        .class_function("Write_3", &BRepTools_Write_3)
        .class_function("Read_2", &BRepTools_Read_2);
}
