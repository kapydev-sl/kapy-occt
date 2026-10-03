// engine/kernels/occt/build/embind/bindings/exactBrepCore.cpp
//
// A shape written to bytes and read back EXACTLY, for the open-with-cache: the
// shape read is the shape written, to the last bit, so an open with a cache is
// the regen and not a regen a few ulps away.
//
// The body is OCCT's binary shape set (`BinTools_ShapeSet`, format 4), which
// writes every double as its 8 bytes, WITHOUT triangulation (the session's
// meshes are not the shape, and an STL export must not change the bytes), and
// with every shape's `Checked` flag written false: the session toggles it (a
// mesh that replaces a triangulation clears it, `BRepTools::Update` sets it),
// and false only tells OCCT to recompute a face's UV points when it next
// updates it, which is what a fresh read has always meant (BinTools format 1
// forced it).
//
// Two things in the body do not come back exact. The frames of the geometry
// (a direction renormalised, an axis system rebuilt by cross products):
// exactGeometry.cpp carries them. And a location: `BinTools_LocationSet`
// writes a `gp_Trsf` as its 3x4 matrix (the scale folded in) and reads it with
// `gp_Trsf::SetValues`, which takes the scale back as a cube root, divides it
// out, orthogonalises the rotation and forgets the form (a translation comes
// back a compound transform). A moved or patterned body then measures a few
// ulps off. So the bytes carry, AHEAD of the shape set, every elementary
// location's transform field by field — scale, form, matrix, translation — and
// the reader puts exactly those back into the location table before a single
// shape takes one (`ReadGeometry` runs after the table is read and before the
// shapes are), rebuilding the composed ones from them the way the table's own
// reader does.
//
// Layout: "KPYX" · i32 locations · per location u8 kind (1 elementary: f64
// scale, i32 form, 9 × f64 matrix row by row, 3 × f64 translation; 2 composed:
// (i32 index, i32 power) per datum, then i32 0 — as `BinTools_LocationSet`
// writes it) · i32 length and the frames (exactGeometry.cpp) · the shape set ·
// the root shape's reference.
//
// This file is the bytes: `writeExact` / `readExact` work on a string, and the
// C API (capiBrepIo.cpp, straight into the arena) writes and reads them.
//
// Who includes this: the embind link (see ../CMakeLists.txt); its interface is
// exactBrep.hxx.
// What does NOT belong here: STEP / STL exchange (io.cpp), what a cache holds,
// or how the bytes travel.

#include <BinTools.hxx>
#include <BinTools_FormatVersion.hxx>
#include <BinTools_LocationSet.hxx>
#include <BinTools_ShapeSet.hxx>
#include <Message_ProgressRange.hxx>
#include <Standard_Failure.hxx>
#include <TopLoc_Datum3D.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <cstring>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "exactAccess.hxx"
#include "exactBrep.hxx"
#include "exactGeometry.hxx"
#include "exactRegistry.hxx"

namespace {

const char MAGIC[4] = {'K', 'P', 'Y', 'X'};

// A `gp_Trsf` with its fields set as they were, no recomputation: every
// public setter normalises (`SetValues` above all).
gp_Trsf exactTrsf(double s, gp_TrsfForm f, const gp_Mat& m, const gp_XYZ& l) {
    gp_Trsf t;
    t.*member(TrsfScale()) = s;
    t.*member(TrsfForm()) = f;
    t.*member(TrsfMatrix()) = m;
    t.*member(TrsfLoc()) = l;
    return t;
}

// What `BinTools_LocationSet::Write` calls elementary: one datum, power 1.
bool isElementary(const TopLoc_Location& L) {
    return L.NextLocation().IsIdentity() && L.FirstPower() == 1;
}

// One row of the location table: an exact transform, or the (index, power)
// chain of a composed location over earlier rows.
struct Entry {
    bool elementary = false;
    gp_Trsf trsf;
    std::vector<std::pair<int, int>> chain;
};

void writeTrsf(std::ostream& os, const gp_Trsf& t) {
    BinTools::PutReal(os, t.ScaleFactor());
    BinTools::PutInteger(os, static_cast<int>(t.Form()));
    const gp_Mat& m = t.HVectorialPart();
    for (int r = 1; r <= 3; r++)
        for (int c = 1; c <= 3; c++) BinTools::PutReal(os, m.Value(r, c));
    for (int i = 1; i <= 3; i++) BinTools::PutReal(os, t.TranslationPart().Coord(i));
}

gp_Trsf readTrsf(std::istream& is) {
    double s = 0., v = 0.;
    int form = 0;
    BinTools::GetReal(is, s);
    BinTools::GetInteger(is, form);
    gp_Mat m;
    for (int r = 1; r <= 3; r++)
        for (int c = 1; c <= 3; c++) {
            BinTools::GetReal(is, v);
            m.SetValue(r, c, v);
        }
    gp_XYZ l;
    for (int i = 1; i <= 3; i++) {
        BinTools::GetReal(is, v);
        l.SetCoord(i, v);
    }
    return exactTrsf(s, static_cast<gp_TrsfForm>(form), m, l);
}

// The shape set whose location table is replaced, before any shape is read,
// by the same table built from the exact transforms.
class ExactShapeSet : public BinTools_ShapeSet {
public:
    ExactShapeSet(std::vector<Entry> entries, std::string frames)
        : myEntries(std::move(entries)),
          myFrames(std::move(frames)) {}

    void ReadGeometry(Standard_IStream& IS, const Message_ProgressRange& theRange) override {
        BinTools_LocationSet& locs = ChangeLocations();
        const int n = locs.NbLocations();
        if (n != static_cast<int>(myEntries.size())) {
            throw Standard_Failure("exact brep: the location table does not match its transforms");
        }
        std::vector<TopLoc_Location> exact(n + 1);
        for (int i = 1; i <= n; i++) {
            const TopLoc_Location& read = locs.Location(i);
            if (myEntries[i - 1].elementary != isElementary(read)) {
                throw Standard_Failure("exact brep: a location changed kind");
            }
            const Entry& e = myEntries[i - 1];
            if (e.elementary) {
                exact[i] = TopLoc_Location(new TopLoc_Datum3D(e.trsf));
                continue;
            }
            // Composed: rebuilt as `BinTools_LocationSet::Read` rebuilds it,
            // on the exact rows.
            TopLoc_Location rebuilt;
            for (const auto& [j, p] : e.chain) {
                if (j <= 0 || j >= i) throw Standard_Failure("exact brep: a datum out of order");
                rebuilt = exact[j].Powered(p) * rebuilt;
            }
            exact[i] = rebuilt;
        }
        locs.Clear();
        for (int i = 1; i <= n; i++) {
            if (locs.Add(exact[i]) != i) throw Standard_Failure("exact brep: a location moved");
        }
        BinTools_ShapeSet::ReadGeometry(IS, theRange);
        std::istringstream frames(myFrames, std::ios::in | std::ios::binary);
        kapy_exact::restoreFrames(frames, *this);
    }

private:
    std::vector<Entry> myEntries;
    std::string myFrames;
};

} // namespace

namespace kapy_exact {

std::string writeExact(const TopoDS_Shape& shape, std::string& bytes_out) {
    try {
        BinTools_ShapeSet set;
        set.SetWithTriangles(false);
        set.SetWithNormals(false);
        set.SetFormatNb(BinTools_FormatVersion_VERSION_4);
        set.Add(shape);
        // `Checked` written false, and put back as it was.
        std::vector<bool> checked(static_cast<size_t>(set.NbShapes()));
        for (int i = 1; i <= set.NbShapes(); i++) {
            TopoDS_Shape s = set.Shape(i);
            checked[i - 1] = s.Checked();
            s.Checked(false);
        }
        std::ostringstream body(std::ios::out | std::ios::binary);
        set.Write(body, Message_ProgressRange());
        set.Write(shape, body);
        for (int i = 1; i <= set.NbShapes(); i++) {
            TopoDS_Shape s = set.Shape(i);
            s.Checked(checked[i - 1]);
        }
        std::ostringstream frames(std::ios::out | std::ios::binary);
        writeFrames(frames, set);

        std::ostringstream os(std::ios::out | std::ios::binary);
        os.write(MAGIC, 4);
        const BinTools_LocationSet& locs = set.Locations();
        const int n = locs.NbLocations();
        BinTools::PutInteger(os, n);
        for (int i = 1; i <= n; i++) {
            const TopLoc_Location& L = locs.Location(i);
            const bool elementary = isElementary(L);
            os.put(static_cast<char>(elementary ? 1 : 2));
            if (elementary) {
                writeTrsf(os, L.Transformation());
                continue;
            }
            // The chain as `BinTools_LocationSet::Write` walks it.
            for (TopLoc_Location rest = L; !rest.IsIdentity(); rest = rest.NextLocation()) {
                BinTools::PutInteger(os, locs.Index(TopLoc_Location(rest.FirstDatum())));
                BinTools::PutInteger(os, rest.FirstPower());
            }
            BinTools::PutInteger(os, 0);
        }
        const std::string frameBytes = frames.str();
        BinTools::PutInteger(os, static_cast<int>(frameBytes.size()));
        os.write(frameBytes.data(), static_cast<std::streamsize>(frameBytes.size()));
        const std::string bytes = body.str();
        os.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        os.flush();
        if (!(body.good() && frames.good() && os.good())) return "failed";
        bytes_out = os.str();
        return "";
    } catch (const Unregistered& u) {
        return "unregisteredGeometry:" + u.className;
    } catch (const Standard_Failure&) {
        return "failed";
    }
}

bool readExact(const std::string& bytes, TopoDS_Shape& shape) {
    try {
        std::istringstream is(bytes, std::ios::in | std::ios::binary);
        char magic[4] = {0, 0, 0, 0};
        is.read(magic, 4);
        if (!is.good() || std::memcmp(magic, MAGIC, 4) != 0) return false;
        int n = 0;
        BinTools::GetInteger(is, n);
        if (!is.good() || n < 0) return false;
        std::vector<Entry> entries(static_cast<size_t>(n));
        for (Entry& e : entries) {
            const int kind = is.get();
            if (kind != 1 && kind != 2) return false;
            e.elementary = kind == 1;
            if (e.elementary) {
                e.trsf = readTrsf(is);
                continue;
            }
            int j = 0, p = 0;
            for (BinTools::GetInteger(is, j); is.good() && j != 0; BinTools::GetInteger(is, j)) {
                BinTools::GetInteger(is, p);
                e.chain.emplace_back(j, p);
            }
        }
        int frameBytes = 0;
        BinTools::GetInteger(is, frameBytes);
        if (!is.good() || frameBytes < 0) return false;
        std::string frames(static_cast<size_t>(frameBytes), '\0');
        is.read(frames.data(), frameBytes);
        if (!is.good()) return false;
        ExactShapeSet set(std::move(entries), std::move(frames));
        set.SetWithTriangles(true);
        set.Read(is, Message_ProgressRange());
        if (!is.good() || set.NbShapes() == 0) return false;
        set.ReadSubs(shape, is, set.NbShapes());
        return !is.fail() && !shape.IsNull();
    } catch (const Standard_Failure&) {
        return false;
    }
}

} // namespace kapy_exact
