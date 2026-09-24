#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>
#include <cstdint>

namespace geometry {
struct Rect {
    float x = 0, y = 0, w = 1, h = 1;
};
inline bool validRegion(Rect r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h) &&
           r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0 && r.x + r.w <= 1.000001f &&
           r.y + r.h <= 1.000001f;
}
inline bool fitsPartLimit(size_t current, size_t added, bool replacesPart) {
    return current <= 64 && added > 0 && added <= 64 &&
           current + added - (replacesPart ? 1 : 0) <= 64;
}
inline std::uint64_t snapshotPixels(float width, float height, float scale) {
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(scale) || width < 1 ||
        height < 1 || scale <= 0 || width * scale > 4096 || height * scale > 4096)
        return 0;
    std::uint64_t w = 1, h = 1;
    while (w < std::ceil(width) * scale)
        w *= 2;
    while (h < std::ceil(height) * scale)
        h *= 2;
    return w * h;
}
inline Rect spotlight(Rect r, float width, float height, float padding = 8) {
    float l = std::clamp(r.x - padding, 0.f, width), b = std::clamp(r.y - padding, 0.f, height);
    float right = std::clamp(r.x + r.w + padding, 0.f, width),
          top = std::clamp(r.y + r.h + padding, 0.f, height);
    return {l, b, std::max(0.f, right - l), std::max(0.f, top - b)};
}
inline std::optional<std::pair<Rect, Rect>> split(Rect r, float fraction, bool vertical) {
    if (!std::isfinite(fraction) || fraction < .02f || fraction > .98f)
        return {};
    auto a = r, b = r;
    if (vertical) {
        a.w = r.w * fraction;
        b.x += a.w;
        b.w -= a.w;
    } else {
        a.h = r.h * fraction;
        b.y += a.h;
        b.h -= a.h;
    }
    return std::pair{a, b};
}
inline Rect crop(Rect r, float x0, float y0, float x1, float y1) {
    auto l = std::clamp(std::min(x0, x1), 0.f, 1.f), b = std::clamp(std::min(y0, y1), 0.f, 1.f);
    auto right = std::clamp(std::max(x0, x1), 0.f, 1.f),
         top = std::clamp(std::max(y0, y1), 0.f, 1.f);
    return {r.x + l * r.w, r.y + b * r.h, (right - l) * r.w, (top - b) * r.h};
}
struct Snap {
    float x = 0, y = 0;
    bool snappedX = false, snappedY = false;
};
inline Snap snap(Rect r, std::vector<Rect> const &others, float threshold = 6.f) {
    Snap out;
    float bestX = threshold, bestY = threshold;
    for (auto t : others) {
        float gapX = std::max({t.x - (r.x + r.w), r.x - (t.x + t.w), 0.f});
        float gapY = std::max({t.y - (r.y + r.h), r.y - (t.y + t.h), 0.f});
        if (gapX > threshold * 3 || gapY > threshold * 3)
            continue;
        float xs[] = {t.x - r.x, t.x + t.w - r.x - r.w, t.x + t.w * .5f - r.x - r.w * .5f,
                      t.x + t.w - r.x, t.x - r.x - r.w};
        float ys[] = {t.y - r.y, t.y + t.h - r.y - r.h, t.y + t.h * .5f - r.y - r.h * .5f,
                      t.y + t.h - r.y, t.y - r.y - r.h};
        for (float d : xs)
            if (std::abs(d) < bestX) {
                bestX = std::abs(d);
                out.x = d;
                out.snappedX = true;
            }
        for (float d : ys)
            if (std::abs(d) < bestY) {
                bestY = std::abs(d);
                out.y = d;
                out.snappedY = true;
            }
    }
    return out;
}
}
