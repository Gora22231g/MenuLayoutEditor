#include "FileDialog.hpp"
#include <Geode/utils/async.hpp>
namespace layout {
void pickConfigFile(bool exporting, std::filesystem::path defaultPath,
                    geode::Function<void(geode::utils::file::PickResult)> callback) {
    namespace file = geode::utils::file;
    file::FilePickOptions options;
    options.defaultPath = std::move(defaultPath);
    options.filters = {{"Menu Layout Config", {"*.json"}}};
    geode::async::spawn(
        file::pick(exporting ? file::PickMode::SaveFile : file::PickMode::OpenFile, options),
        std::move(callback));
}
}
