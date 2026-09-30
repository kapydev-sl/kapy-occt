// services/occt/build/embind/bindings/factsHistory.cpp
//
// An operand's history against a result, as the facts' INTS blocks: for every
// operand element in its map order `[self, modCount, mod..., genCount,
// gen...]`, per kind (`propagate_maker` / `propagate_history`, namer.cpp).
// Several makers behind one compound are merged the way historyFacts.ts'
// `mergeFlats` does: `self` is the result's own lookup (the same in all), and
// the first maker with a non-empty Modified (or Generated) list answers it.
//
// Who includes it: the embind link (see ../CMakeLists.txt).
// What does NOT belong here: what the history means (the core's namer).

#include <TopAbs_ShapeEnum.hxx>

#include "factsInternal.hxx"

using emscripten::val;

namespace kapy_facts {

namespace {

// The merge of the same operand's flat histories from several makers.
std::vector<int> mergeFlats(const std::vector<std::vector<int>>& flats) {
    std::vector<int> out;
    std::vector<size_t> at(flats.size(), 0);
    while (!flats.empty() && at[0] < flats[0].size()) {
        int self = -1;
        std::vector<int> mods, gens;
        bool haveMods = false, haveGens = false;
        for (size_t j = 0; j < flats.size(); ++j) {
            const std::vector<int>& f = flats[j];
            size_t p = at[j];
            const int s = f[p++];
            if (j == 0) self = s;
            const int m = f[p++];
            const std::vector<int> mod(f.begin() + p, f.begin() + p + m);
            p += m;
            const int g = f[p++];
            const std::vector<int> gen(f.begin() + p, f.begin() + p + g);
            p += g;
            at[j] = p;
            if (!haveMods && m > 0) {
                mods = mod;
                haveMods = true;
            }
            if (!haveGens && g > 0) {
                gens = gen;
                haveGens = true;
            }
        }
        out.push_back(self);
        out.push_back(static_cast<int>(mods.size()));
        out.insert(out.end(), mods.begin(), mods.end());
        out.push_back(static_cast<int>(gens.size()));
        out.insert(out.end(), gens.begin(), gens.end());
    }
    return out;
}

std::vector<int> kindHistory(int sourceKind, const val& source, const ShapeIndexedMap& opMap,
                             const ShapeIndexedMap& resultMap, TopAbs_ShapeEnum kind) {
    const int k = static_cast<int>(kind);
    if (sourceKind == kSourceHistory) {
        return kapy_namer::propagate_history(source.as<BRepTools_History&>(), opMap, resultMap, k);
    }
    if (sourceKind == kSourceMaker) {
        return kapy_namer::propagate_maker(source.as<BRepBuilderAPI_MakeShape&>(), opMap,
                                           resultMap, k);
    }
    std::vector<std::vector<int>> flats;
    const unsigned n = source["length"].as<unsigned>();
    for (unsigned i = 0; i < n; ++i) {
        flats.push_back(kapy_namer::propagate_maker(
            source[i].as<BRepBuilderAPI_MakeShape&>(), opMap, resultMap, k));
    }
    return mergeFlats(flats);
}

}  // namespace

std::vector<Block> historyBlocks(int sourceKind, const val& source, const Maps& operand,
                                 const Maps& result) {
    return {
        intsBlock(kindHistory(sourceKind, source, *operand.face, *result.face, TopAbs_FACE)),
        intsBlock(kindHistory(sourceKind, source, *operand.edge, *result.edge, TopAbs_EDGE)),
        intsBlock(kindHistory(sourceKind, source, *operand.vertex, *result.vertex, TopAbs_VERTEX)),
    };
}

}  // namespace kapy_facts
