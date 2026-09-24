#pragma once
#include <Geode/utils/file.hpp>
#include <Geode/utils/function.hpp>
namespace layout {
void pickConfigFile(bool exporting, std::filesystem::path defaultPath,
                    geode::Function<void(geode::utils::file::PickResult)> callback);
}
