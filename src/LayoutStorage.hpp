#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include "ConfigSchema.hpp"

namespace layout::storage {
inline std::filesystem::path statePath() {
    return geode::Mod::get()->getSaveDir() / "menu-layout.json";
}
inline geode::Result<matjson::Value> readConfig(std::filesystem::path const &path) {
    std::error_code error;
    auto size = std::filesystem::file_size(path, error);
    if (error || size > config::maxFileBytes)
        return geode::Err("Cannot read config, or it exceeds 1 MiB.");
    auto text = geode::utils::file::readString(path);
    if (text.isErr())
        return geode::Err("Could not read the selected config.");
    return config::parse(text.unwrap());
}
inline geode::Result<> writeConfig(std::filesystem::path const &path,
                                   matjson::Value const &document) {
    auto checked = config::validate(document);
    if (checked.isErr())
        return checked;
    auto text = document.dump(2);
    if (text.size() > config::maxFileBytes)
        return geode::Err("Config exceeds 1 MiB.");
    return geode::utils::file::writeStringSafe(path, text);
}
inline geode::Result<> save(matjson::Value layout, bool snap, bool guideCompleted) {
    auto file = config::document(std::move(layout), snap);
    file["guide-completed"] = guideCompleted;
    auto directory = geode::utils::file::createDirectoryAll(geode::Mod::get()->getSaveDir());
    if (directory.isErr())
        return geode::Err("Cannot create the mod save directory.");
    auto result = writeConfig(statePath(), file);
    if (result.isErr())
        return result;
    auto mod = geode::Mod::get();
    mod->setSavedValue("layout-v1", file["layout"]);
    mod->setSavedValue("snap-enabled", snap);
    mod->setSavedValue("guide-completed-v1", guideCompleted);
    auto mirror = mod->saveData();
    if (mirror.isErr())
        geode::log::warn("Could not update Geode save mirror");
    return geode::Ok();
}
inline void initialize() {
    static bool loaded = false;
    if (loaded)
        return;
    loaded = true;
    std::error_code error;
    if (!std::filesystem::exists(statePath(), error))
        return;
    auto state = readConfig(statePath());
    if (state.isErr()) {
        geode::log::warn("Could not read menu-layout.json; using Geode saved values");
        return;
    }
    auto mod = geode::Mod::get();
    auto const &file = state.unwrap();
    mod->setSavedValue("layout-v1", file["layout"]);
    mod->setSavedValue("snap-enabled", file["snap"].asBool().unwrapOr(true));
    mod->setSavedValue("guide-completed-v1", file["guide-completed"].asBool().unwrapOr(false));
}
}
