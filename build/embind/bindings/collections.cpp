// services/occt/build/embind/bindings/collections.cpp
//
// The OCCT collections our code iterates: the shape list returned by
// Modified()/Generated() and the indexed shape map TopExp::MapShapes fills.
//
// OCCT 8.0 deleted the per-typedef TopTools headers; these types are now
// spelled as the NCollection templates they always aliased. We register them
// under the names opencascade.js used (`TopTools_ListOfShape`,
// `TopTools_IndexedMapOfShape`) so the TypeScript is unchanged.
//
// Who includes this: the embind link (see ../CMakeLists.txt).

#include <emscripten/bind.h>

#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Shape.hxx>
#include <TopTools_ShapeMapHasher.hxx>

using namespace emscripten;

using ShapeList = NCollection_List<TopoDS_Shape>;
using ShapeIndexedMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

// `Size`/`Extent`/`IsEmpty` are declared on NCollection_BaseList and
// NCollection_BaseMap, so `&ShapeList::Size` is a pointer-to-member of the
// BASE: embind would register it against a type JS never sees and the method
// would silently go missing. Wrap every inherited member. `First()` is
// overloaded (const + non-const); opencascade.js numbers the const one `_1`
// because it is declared first.
static int ShapeList_Size(ShapeList& l) { return l.Size(); }
static int ShapeList_Extent(ShapeList& l) { return l.Extent(); }
static bool ShapeList_IsEmpty(ShapeList& l) { return l.IsEmpty(); }
static void ShapeList_RemoveFirst(ShapeList& l) { l.RemoveFirst(); }
static TopoDS_Shape ShapeList_First_1(ShapeList& l) { return l.First(); }
static void ShapeList_Append_1(ShapeList& l, const TopoDS_Shape& s) { l.Append(s); }

static int ShapeIndexedMap_Extent(ShapeIndexedMap& m) { return m.Extent(); }
static int ShapeIndexedMap_Size(ShapeIndexedMap& m) { return m.Size(); }
static TopoDS_Shape ShapeIndexedMap_FindKey(ShapeIndexedMap& m, int i) { return m.FindKey(i); }
// The hashed reverse lookup. namer.helpers.ts falls back to a LINEAR scan when
// this is absent (`typeof map.FindIndex === 'function'`), turning the TopoNamer
// into O(n^2) over every subshape — 6x slower on a gear. Bind it.
static int ShapeIndexedMap_FindIndex(ShapeIndexedMap& m, const TopoDS_Shape& s) {
    return m.FindIndex(s);
}
static int ShapeIndexedMap_Add(ShapeIndexedMap& m, const TopoDS_Shape& s) { return m.Add(s); }

EMSCRIPTEN_BINDINGS(kapy_occt_collections) {
    class_<ShapeList>("TopTools_ListOfShape")
        .constructor<>()
        .function("Size", &ShapeList_Size)
        .function("Extent", &ShapeList_Extent)
        .function("IsEmpty", &ShapeList_IsEmpty)
        .function("RemoveFirst", &ShapeList_RemoveFirst)
        .function("First_1", &ShapeList_First_1)
        .function("Append_1", &ShapeList_Append_1);
    function("TopTools_ListOfShape_1", +[]() { return ShapeList(); });

    class_<ShapeIndexedMap>("TopTools_IndexedMapOfShape")
        .constructor<>()
        .function("Extent", &ShapeIndexedMap_Extent)
        .function("Size", &ShapeIndexedMap_Size)
        .function("FindKey", &ShapeIndexedMap_FindKey)
        .function("FindIndex", &ShapeIndexedMap_FindIndex)
        .function("Add", &ShapeIndexedMap_Add);
    function("TopTools_IndexedMapOfShape_1", +[]() { return ShapeIndexedMap(); });
}
