#include "../src/Geometry.hpp"
#include <stdexcept>
#include <iostream>
using namespace geometry;
void check(bool v) {
    if (!v)
        throw std::runtime_error("Geometry regression failed");
}
bool near(float a, float b) {
    return std::abs(a - b) < .0001f;
}
int main() {
    auto parts = split({0, 0, 1, 1}, .7f, true);
    check(bool(parts));
    check(near(parts->first.w, .7f) && near(parts->second.x, .7f) && near(parts->second.w, .3f));
    auto again = split(parts->second, .5f, false);
    check(bool(again));
    check(near(again->first.x, .7f) && near(again->first.h, .5f) && near(again->second.y, .5f));
    auto c = crop(parts->second, .9f, .8f, .1f, .2f);
    check(near(c.x, .73f) && near(c.y, .2f) && near(c.w, .24f) && near(c.h, .6f));
    auto full = crop({0, 0, 1, 1}, -2, -3, 4, 5);
    check(near(full.w, 1) && near(full.h, 1));
    check(!split({0, 0, 1, 1}, 0, true) && !split({0, 0, 1, 1}, NAN, false));
    auto s = snap({0, 0, 20, 20}, {{24, 2, 20, 20}});
    check(s.snappedX && s.snappedY && near(s.x, 4) && near(s.y, 2));
    auto distant = snap({0, 0, 20, 20}, {{1000, 2, 20, 20}});
    check(!distant.snappedX && !distant.snappedY);
    auto disabled = snap({0, 0, 20, 20}, {{24, 2, 20, 20}}, 0);
    check(!disabled.snappedX && !disabled.snappedY);
    auto pullAway = snap({0, 0, 20, 20}, {{30, 30, 20, 20}});
    check(!pullAway.snappedX && !pullAway.snappedY);
    auto focus = spotlight({-3, 90, 20, 20}, 100, 100);
    check(near(focus.x, 0) && near(focus.y, 82) && near(focus.w, 25) && near(focus.h, 18));
    auto outside = spotlight({120, 120, 10, 10}, 100, 100);
    check(near(outside.w, 0) && near(outside.h, 0));
    check(fitsPartLimit(64, 1, true) && !fitsPartLimit(64, 2, true) &&
          !fitsPartLimit(64, 1, false));
    check(!validRegion({0, 0, NAN, 1}) && !validRegion({0, 0, 0, 1}) &&
          !validRegion({.8f, 0, .3f, 1}));
    check(snapshotPixels(400, 100, 2) == 1048576 / 4 && snapshotPixels(1, 1, NAN) == 0 &&
          snapshotPixels(2048, 2048, 3) == 0);
    std::cout << "PASS: split, nested split, reversed crop, clamp, invalid cuts, edge snap, "
                 "distant isolation, threshold\n";
}
