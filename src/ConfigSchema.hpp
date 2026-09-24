#pragma once
#include <matjson.hpp>
#include "Geometry.hpp"

namespace layout::config {
constexpr size_t maxFileBytes = 1024 * 1024;
inline bool boundedJson(std::string_view text) {
    if (text.size() > maxFileBytes)
        return false;
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char ch : text) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                quoted = false;
            continue;
        }
        if (ch == '"')
            quoted = true;
        else if (ch == '{' || ch == '[') {
            if (++depth > 16)
                return false;
        } else if (ch == '}' || ch == ']') {
            if (--depth < 0)
                return false;
        }
    }
    return !quoted && depth == 0;
}
inline bool finitePosition(matjson::Value const &node) {
    auto x = node["x"].asDouble(), y = node["y"].asDouble();
    return x.isOk() && y.isOk() && std::isfinite(x.unwrap()) && std::isfinite(y.unwrap()) &&
           std::abs(x.unwrap()) <= 1 && std::abs(y.unwrap()) <= 1 && node["hidden"].isBool();
}
inline geode::Result<> validate(matjson::Value const &file) {
    if (file["format"].asString().unwrapOr("") != "menu-layout-editor" ||
        file["version"].asInt().unwrapOr(0) != 1)
        return geode::Err("Unsupported config format or version.");
    if (!file["snap"].isBool() || !file["layout"].isObject() || file["layout"].size() > 512)
        return geode::Err("Invalid layout or Snap setting.");
    size_t count = 0;
    for (auto const &node : file["layout"]) {
        if (node.getKey().value_or("").size() > 512 || !node.isObject() || !finitePosition(node) ||
            !node["parts"].isArray())
            return geode::Err("Invalid menu element.");
        for (auto const &part : node["parts"]) {
            if (++count > 64 || !part.isObject() || !finitePosition(part))
                return geode::Err("Invalid part or too many parts (maximum 64).");
            auto l = part["l"].asDouble(), b = part["b"].asDouble(), w = part["w"].asDouble(),
                 h = part["h"].asDouble();
            if (l.isErr() || b.isErr() || w.isErr() || h.isErr() ||
                !geometry::validRegion(
                    {float(l.unwrap()), float(b.unwrap()), float(w.unwrap()), float(h.unwrap())}))
                return geode::Err("Invalid crop rectangle.");
        }
    }
    return geode::Ok();
}
inline geode::Result<matjson::Value> parse(std::string_view text) {
    if (!boundedJson(text))
        return geode::Err("Config is too large, too deeply nested or malformed.");
    auto parsed = matjson::parse(text);
    if (parsed.isErr())
        return geode::Err("Invalid JSON file.");
    auto checked = validate(parsed.unwrap());
    if (checked.isErr())
        return geode::Err(checked.unwrapErr());
    return geode::Ok(std::move(parsed).unwrap());
}
inline matjson::Value document(matjson::Value layout, bool snap) {
    if (!layout.isObject())
        layout = matjson::Value::object();
    for (auto &node : layout)
        if (node.isObject() && (!node.contains("parts") || node["parts"].isNull()))
            node["parts"] = matjson::Value::array();
    return matjson::makeObject({{"format", "menu-layout-editor"},
                                {"version", 1},
                                {"layout", std::move(layout)},
                                {"snap", snap}});
}
}
