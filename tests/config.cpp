#include "../src/ConfigSchema.hpp"
#include <iostream>
#include <stdexcept>
#include <source_location>
using namespace layout;
void require(bool condition, std::source_location where = std::source_location::current()) {
    if (!condition) {
        std::cerr << "Config regression failed at line " << where.line() << std::endl;
        std::exit(1);
    }
}
int main() {
    auto layout = matjson::Value::object();
    layout["/main-menu/play-button"] = matjson::makeObject(
        {{"x", .1}, {"y", -.2}, {"hidden", false}, {"parts", matjson::Value::array()}});
    auto exported = config::document(layout, false);
    require(config::validate(exported).isOk());
    require(!exported.contains("guide-completed"));
    auto decoded = config::parse(exported.dump());
    require(decoded.isOk() && decoded.unwrap() == exported);
    auto state = exported;
    state["guide-completed"] = true;
    auto restart = config::parse(state.dump());
    require(restart.isOk() && restart.unwrap()["guide-completed"].asBool().unwrapOr(false));
    auto legacy = layout;
    legacy["/main-menu/play-button"].erase("parts");
    require(config::validate(config::document(legacy, true)).isOk());
    auto invalid = exported;
    invalid["version"] = 2;
    require(config::validate(invalid).isErr());
    invalid = exported;
    invalid["snap"] = "yes";
    require(config::validate(invalid).isErr());
    invalid = exported;
    invalid["layout"]["/main-menu/play-button"]["x"] = 100;
    require(config::validate(invalid).isErr());
    invalid = exported;
    auto part = matjson::makeObject(
        {{"x", 0}, {"y", 0}, {"hidden", false}, {"l", 0}, {"b", 0}, {"w", .5}, {"h", 1}});
    for (int i = 0; i < 65; ++i)
        invalid["layout"]["/main-menu/play-button"]["parts"].push(part);
    require(config::validate(invalid).isErr());
    require(config::parse(std::string(100, '[') + std::string(100, ']')).isErr());
    require(config::parse(std::string(config::maxFileBytes + 1, ' ')).isErr());
    require(config::parse("{invalid}").isErr());
    require(config::boundedJson(R"({"brackets":"[[[{}]]]","escaped":"\""})"));
    std::cout << "PASS: JSON round-trip, persisted guide flag, export isolation, legacy migration, "
                 "schema and input limits\n";
}
