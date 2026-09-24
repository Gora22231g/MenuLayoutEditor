#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/VideoOptionsLayer.hpp>
#include "MenuModel.hpp"
#include "FirstRunGuide.hpp"
#include "LayoutStorage.hpp"
#include "FileDialog.hpp"
using namespace geode::prelude;
using namespace layout;
namespace {
MenuLayer *findMenu(CCNode *n) {
    if (auto m = typeinfo_cast<MenuLayer *>(n))
        return m;
    if (auto children = n->getChildren())
        for (auto child : CCArrayExt<CCNode *>(children))
            if (auto m = findMenu(child))
                return m;
    return nullptr;
}
class LayoutEditor : public CCLayer {
    Ref<MenuLayoutState> m_layoutState;
    matjson::Value m_sessionLayout;
    bool m_snapEnabled = true, m_initialSnap = true, m_dragging = false;
    enum class EditTool { Move, VerticalCut, HorizontalCut, Crop };
    int m_selectedIndex = -1;
    EditTool m_tool = EditTool::Move;
    CCPoint m_dragStart, m_dragOrigin, m_cropStart;
    CCSize m_windowSize;
    bool m_panelBottom = false;
    CCNode *m_panel = nullptr;
    std::vector<Ref<CCNode>> m_popups;
    struct PopupInput {
        Ref<CCLayer> layer;
        bool touch;
        bool keypad;
    };
    std::vector<PopupInput> m_popupInput;
    std::vector<CCLabelBMFont *> m_buttons;
    CCLabelBMFont *m_status = nullptr;
    CCDrawNode *m_outline = nullptr;

    bool m_fileBusy = false;
    static constexpr float toolbar = 91;
    void configFile(bool exporting) {
        if (m_fileBusy)
            return;
        auto directory = Mod::get()->getSaveDir() / "exports";
        if (auto made = file::createDirectoryAll(directory); made.isErr()) {
            message("Cannot open the config directory.");
            return;
        }
        m_fileBusy = true;
        m_dragging = false;
        m_tool = EditTool::Move;
        WeakRef<LayoutEditor> editor = this;
        pickConfigFile(exporting, directory / "menu-layout-config.json",
                       [editor, exporting](file::PickResult result) {
                           if (auto layer = editor.lock(); layer && layer->isRunning())
                               layer->finishFilePick(exporting, std::move(result));
                       });
    }
    void finishFilePick(bool exporting, file::PickResult result) {
        m_fileBusy = false;
        if (!isRunning())
            return;
        if (result.isErr()) {
            message("Could not open the file dialog.");
            return;
        }
        auto selected = std::move(result).unwrap();
        if (!selected) {
            refresh();
            return;
        }
        if (exporting) {
            auto saved = storage::writeConfig(
                *selected, config::document(m_layoutState->serialize(), m_snapEnabled));
            if (saved.isErr()) {
                message("Export failed: " + saved.unwrapErr());
                return;
            }
            message("Config exported. Save applies this layout to your game.");
            return;
        }
        auto imported = storage::readConfig(*selected);
        if (imported.isErr()) {
            message("Import failed: " + imported.unwrapErr());
            return;
        }
        auto const &document = imported.unwrap();
        int matched = 0;
        for (auto const &element : m_layoutState->items)
            if (element.root < 0 && document["layout"].contains(element.key))
                ++matched;
        if (document["layout"].size() > 0 && matched == 0) {
            message("No matching menu elements. Config was not applied.");
            return;
        }
        auto previous = m_layoutState->serialize();
        m_layoutState->load(document["layout"]);
        if (!m_layoutState->error.empty()) {
            auto reason = m_layoutState->error;
            m_layoutState->load(previous);
            message("Import rolled back: " + reason);
            return;
        }
        m_snapEnabled = document["snap"].asBool().unwrapOr(true);
        m_selectedIndex = -1;
        refresh();
        message("Config preview loaded (" + std::to_string(matched) +
                " matched). Save keeps it; Cancel restores your menu.");
    }
    void suspendInput(CCNode *node) {
        if (auto layer = typeinfo_cast<CCLayer *>(node)) {
            m_popupInput.push_back({layer, layer->isTouchEnabled(), layer->isKeypadEnabled()});
            layer->setTouchEnabled(false);
            layer->setKeypadEnabled(false);
        }
        if (auto children = node->getChildren())
            for (auto child : CCArrayExt<CCNode *>(children))
                suspendInput(child);
    }
    void hidePopups(CCNode *n) {
        if (n == this)
            return;
        if ((typeinfo_cast<OptionsLayer *>(n) || typeinfo_cast<FLAlertLayer *>(n)) &&
            n->isVisible()) {
            m_popups.emplace_back(n);
            suspendInput(n);
            n->setVisible(false);
            return;
        }
        if (auto children = n->getChildren())
            for (auto child : CCArrayExt<CCNode *>(children))
                hidePopups(child);
    }
    void message(std::string const &s) {
        m_status->setString(s.c_str());
        m_status->limitLabelWidth(m_windowSize.width - 12, .42f, .18f);
    }
    void box(CCRect r, ccColor4F color) {
        CCPoint p[] = {{r.getMinX(), r.getMinY()},
                       {r.getMaxX(), r.getMinY()},
                       {r.getMaxX(), r.getMaxY()},
                       {r.getMinX(), r.getMaxY()}};
        m_outline->drawPolygon(p, 4, {0, 0, 0, 0}, 1, color);
    }
    void refresh() {
        m_outline->clear();
        m_buttons[8]->setString(m_snapEnabled ? "Snap: ON" : "Snap: OFF");
        for (int i = 9; i <= 11; ++i)
            m_buttons[i]->setColor(static_cast<EditTool>(i - 8) == m_tool
                                       ? ccColor3B{80, 255, 150}
                                       : ccColor3B{255, 255, 255});
        if (!m_layoutState->canSelect(m_selectedIndex)) {
            m_selectedIndex = -1;
            m_tool = EditTool::Move;
            message(
                "Select and drag. Cut V / Cut H: click a cut line. Crop: drag the area to keep.");
            return;
        }
        auto &e = m_layoutState->items[m_selectedIndex];
        std::string name = e.key;
        if (e.root >= 0)
            name += " #" + std::to_string(m_selectedIndex);
        auto prefix = m_tool == EditTool::VerticalCut     ? "Click vertical cut: "
                      : m_tool == EditTool::HorizontalCut ? "Click horizontal cut: "
                      : m_tool == EditTool::Crop          ? "Drag crop rectangle: "
                                                          : "";
        message(prefix + name +
                (e.locked               ? " [RobTop LOCKED]"
                 : !e.node->isVisible() ? " [HIDDEN]"
                                        : ""));
        box(bounds(e.node), e.locked ? ccColor4F{1, .7f, .2f, 1} : ccColor4F{.2f, 1, .6f, 1});
    }
    void close(bool save) {
        if (save) {
            auto result =
                storage::save(m_layoutState->serialize(), m_snapEnabled,
                              Mod::get()->getSavedValue<bool>("guide-completed-v1", false));
            if (result.isErr()) {
                message("Save failed: " + result.unwrapErr());
                return;
            }
        } else {
            m_layoutState->load(m_sessionLayout);
            m_snapEnabled = m_initialSnap;
        }
        removeFromParentAndCleanup(true);
    }
    void action(int index) {
        if (m_fileBusy)
            return;
        m_dragging = false;
        if (!m_layoutState->canSelect(m_selectedIndex)) {
            m_selectedIndex = -1;
            m_tool = EditTool::Move;
        }
        if (index == 6) {
            close(true);
            return;
        }
        if (index == 7) {
            close(false);
            return;
        }
        if (index == 8) {
            m_snapEnabled = !m_snapEnabled;
            refresh();
            return;
        }
        if (index >= 9 && index <= 11) {
            if (m_selectedIndex < 0) {
                message("Select an element first.");
                return;
            }
            if (m_layoutState->items[m_selectedIndex].locked) {
                message("RobTop cannot be hidden, moved, cropped or split.");
                return;
            }
            m_tool = m_tool == static_cast<EditTool>(index - 8) ? EditTool::Move
                                                                : static_cast<EditTool>(index - 8);
            refresh();
            return;
        }
        if (index == 12) {
            m_tool = EditTool::Move;
            refresh();
            return;
        }
        if (index == 13) {
            m_panelBottom = !m_panelBottom;
            m_panel->setPositionY(m_panelBottom ? toolbar - m_windowSize.height : 0);
            refresh();
            return;
        }
        if (index == 14) {
            refresh();
            message("Cut: select, choose Cut V/H, click. Crop: drag area to KEEP. Restore rejoins "
                    "all parts.");
            return;
        }
        if (index == 15) {
            configFile(false);
            return;
        }
        if (index == 16) {
            configFile(true);
            return;
        }
        if (index == 17) {
            auto folder = Mod::get()->getSaveDir();
            if (auto made = file::createDirectoryAll(folder); made.isErr()) {
                message("Cannot create save directory.");
                return;
            }
            if (!file::openFolder(folder))
                message("Cannot open save directory.");
            return;
        }
        m_tool = EditTool::Move;
        int count = int(m_layoutState->items.size());
        if (!count)
            return;
        if (index == 0 || index == 1) {
            int step = index == 0 ? -1 : 1;
            if (m_selectedIndex < 0)
                m_selectedIndex = index == 0 ? 0 : -1;
            for (int i = 0; i < count; ++i) {
                m_selectedIndex = (m_selectedIndex + step + count) % count;
                if (m_layoutState->canSelect(m_selectedIndex))
                    break;
            }
        }
        if (index == 2 && m_selectedIndex >= 0) {
            auto &e = m_layoutState->items[m_selectedIndex];
            if (e.locked) {
                message("RobTop cannot be hidden.");
                return;
            }
            e.node->setVisible(!e.node->isVisible());
        }
        if (index == 3 && m_selectedIndex >= 0) {
            int root = m_layoutState->items[m_selectedIndex].root;
            m_layoutState->restore(m_selectedIndex);
            if (root >= 0)
                m_selectedIndex = root;
        }
        if (index == 4)
            for (int i = 0; i < count; ++i)
                if (m_layoutState->canSelect(i))
                    m_layoutState->items[i].node->setVisible(m_layoutState->items[i].locked ||
                                                             m_layoutState->items[i].visible);
        if (index == 5) {
            m_layoutState->reset();
            m_selectedIndex = -1;
        }
        refresh();
    }
    CCPoint normalized(CCNode *n, CCPoint world) {
        auto p = n->convertToNodeSpace(world);
        auto s = n->getContentSize();
        return {p.x / std::max(1.f, s.width), p.y / std::max(1.f, s.height)};
    }
    void finishCut(std::vector<geometry::Rect> regions) {
        if (m_layoutState->cut(m_selectedIndex, regions)) {
            m_selectedIndex = int(m_layoutState->items.size()) - int(regions.size());
            m_tool = EditTool::Move;
            refresh();
        } else {
            refresh();
            message(m_layoutState->error.empty() ? "Could not cut this element."
                                                 : m_layoutState->error);
        }
    }

  public:
    static LayoutEditor *create(MenuLayoutState *data) {
        auto p = new LayoutEditor;
        if (p->init(data)) {
            p->autorelease();
            return p;
        }
        delete p;
        return nullptr;
    }
    bool init(MenuLayoutState *data) {
        if (!data || !CCLayer::init())
            return false;
        m_layoutState = data;
        m_sessionLayout = data->serialize();
        m_snapEnabled = m_initialSnap = Mod::get()->getSavedValue<bool>("snap-enabled", true);
        m_windowSize = CCDirector::sharedDirector()->getWinSize();
        setID(editorID);
        m_panel = CCNode::create();
        addChild(m_panel, 2);
        auto bar = CCLayerColor::create({15, 20, 32, 245}, m_windowSize.width, toolbar);
        bar->setPositionY(m_windowSize.height - toolbar);
        m_panel->addChild(bar, 2);
        const char *names[] = {"Prev",       "Next",       "Hide/Show",  "Restore",  "Show all",
                               "Reset all",  "Save",       "Cancel",     "Snap: ON", "Cut V",
                               "Cut H",      "Crop",       "Move",       "Panel",    "Help",
                               "Import CFG", "Export CFG", "Save folder"};
        for (int i = 0; i < 18; ++i) {
            int row = i < 8    ? 0
                      : i < 15 ? 1
                               : 2,
                col = i < 8    ? i
                      : i < 15 ? i - 8
                               : i - 15,
                columns = i < 8    ? 8
                          : i < 15 ? 7
                                   : 3;
            auto label = CCLabelBMFont::create(names[i], "bigFont.fnt");
            label->limitLabelWidth(m_windowSize.width / columns - 6, .37f, .15f);
            label->setPosition({(col + .5f) * m_windowSize.width / columns,
                                m_windowSize.height - 12 - row * 23.f});
            m_panel->addChild(label, 3);
            m_buttons.push_back(label);
        }
        m_status = CCLabelBMFont::create("", "chatFont.fnt");
        m_status->setPosition({m_windowSize.width / 2, m_windowSize.height - 80});
        m_panel->addChild(m_status, 3);
        m_outline = CCDrawNode::create();
        addChild(m_outline, 1);
        nameOwnedNodes(this, editorID);
        setTouchEnabled(true);
        setKeypadEnabled(true);
        refresh();
        return true;
    }
    void onEnter() override {
        CCLayer::onEnter();
        hidePopups(CCDirector::sharedDirector()->getRunningScene());
        auto result = storage::save(m_sessionLayout, m_initialSnap, true);
        if (result.isErr()) {
            Mod::get()->setSavedValue("guide-completed-v1", true);
            (void)Mod::get()->saveData();
            message("Could not save guide completion: " + result.unwrapErr());
        }
    }
    void onExit() override {
        CCLayer::onExit();
        for (auto &input : m_popupInput) {
            input.layer->setTouchEnabled(input.touch);
            input.layer->setKeypadEnabled(input.keypad);
        }
        m_popupInput.clear();
        for (auto &popup : m_popups)
            popup->setVisible(true);
        m_popups.clear();
    }
    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -10000, true);
    }
    bool ccTouchBegan(CCTouch *touch, CCEvent *) override {
        if (m_fileBusy)
            return true;
        auto p = touch->getLocation();
        m_dragging = false;
        if (!m_layoutState->canSelect(m_selectedIndex)) {
            m_selectedIndex = -1;
            m_tool = EditTool::Move;
        }
        float panelY = p.y - m_panel->getPositionY();
        if (panelY >= m_windowSize.height - toolbar && panelY <= m_windowSize.height) {
            if (panelY >= m_windowSize.height - 24)
                action(std::clamp(int(p.x * 8 / m_windowSize.width), 0, 7));
            else if (panelY >= m_windowSize.height - 47)
                action(8 + std::clamp(int(p.x * 7 / m_windowSize.width), 0, 6));
            else if (panelY >= m_windowSize.height - 70)
                action(15 + std::clamp(int(p.x * 3 / m_windowSize.width), 0, 2));
            return true;
        }
        if (m_tool != EditTool::Move && m_selectedIndex >= 0) {
            auto &e = m_layoutState->items[m_selectedIndex];
            auto local = normalized(e.node, p);
            if (local.x < 0 || local.x > 1 || local.y < 0 || local.y > 1) {
                message("Click inside the selected element. Move exits cut mode.");
                return true;
            }
            if (m_tool == EditTool::Crop) {
                m_cropStart = local;
                m_dragging = true;
                return true;
            }
            auto split =
                geometry::split(e.region, m_tool == EditTool::VerticalCut ? local.x : local.y,
                                m_tool == EditTool::VerticalCut);
            if (split)
                finishCut({split->first, split->second});
            else
                message("Cut farther from the edge (at least 2%).");
            return true;
        }
        float area = 1e30f;
        for (size_t i = 0; i < m_layoutState->items.size(); ++i) {
            auto n = m_layoutState->items[i].node;
            if (!m_layoutState->canSelect(int(i)) || !visible(n))
                continue;
            auto r = bounds(n);
            if (r.containsPoint(p) && r.size.width * r.size.height < area) {
                m_selectedIndex = int(i);
                area = r.size.width * r.size.height;
                m_dragging = !m_layoutState->items[i].locked;
            }
        }
        if (m_dragging) {
            m_dragStart = p;
            m_dragOrigin = m_layoutState->items[m_selectedIndex].node->getPosition();
        }
        refresh();
        return true;
    }
    void ccTouchMoved(CCTouch *touch, CCEvent *) override {
        if (!m_dragging || !m_layoutState->canSelect(m_selectedIndex))
            return;
        auto n = m_layoutState->items[m_selectedIndex].node;
        auto parent = n->getParent();
        if (!parent)
            return;
        if (m_tool == EditTool::Crop) {
            auto end = normalized(n, touch->getLocation());
            auto crop = geometry::crop({0, 0, 1, 1}, m_cropStart.x, m_cropStart.y, end.x, end.y);
            auto s = n->getContentSize();
            CCPoint a = n->convertToWorldSpace({crop.x * s.width, crop.y * s.height});
            CCPoint b =
                n->convertToWorldSpace({(crop.x + crop.w) * s.width, (crop.y + crop.h) * s.height});
            refresh();
            box({std::min(a.x, b.x), std::min(a.y, b.y), std::abs(a.x - b.x), std::abs(a.y - b.y)},
                {1, .65f, .15f, 1});
            return;
        }
        auto p = touch->getLocation();
        p.x = std::clamp(p.x, 0.f, m_windowSize.width);
        p.y = std::clamp(p.y, 0.f, m_windowSize.height);
        n->setPosition(m_dragOrigin + parent->convertToNodeSpace(p) -
                       parent->convertToNodeSpace(m_dragStart));
        geometry::Snap snapped;
        if (m_snapEnabled) {
            std::vector<geometry::Rect> others;
            for (int i = 0; i < int(m_layoutState->items.size()); ++i)
                if (i != m_selectedIndex && m_layoutState->canSelect(i) &&
                    visible(m_layoutState->items[i].node)) {
                    auto r = bounds(m_layoutState->items[i].node);
                    others.push_back({r.origin.x, r.origin.y, r.size.width, r.size.height});
                }
            auto r = bounds(n);
            snapped = geometry::snap({r.origin.x, r.origin.y, r.size.width, r.size.height}, others);
            auto world = parent->convertToWorldSpace(n->getPosition());
            n->setPosition(parent->convertToNodeSpace(world + ccp(snapped.x, snapped.y)));
        }
        refresh();
        if (snapped.snappedX || snapped.snappedY)
            box(bounds(n), {.3f, .8f, 1, 1});
    }
    void ccTouchEnded(CCTouch *touch, CCEvent *) override {
        if (m_dragging && m_tool == EditTool::Crop && m_layoutState->canSelect(m_selectedIndex)) {
            auto &e = m_layoutState->items[m_selectedIndex];
            auto end = normalized(e.node, touch->getLocation());
            auto region = geometry::crop(e.region, m_cropStart.x, m_cropStart.y, end.x, end.y);
            auto size =
                m_layoutState->items[e.root < 0 ? m_selectedIndex : e.root].node->getContentSize();
            if (region.w * size.width >= 2 && region.h * size.height >= 2)
                finishCut({region});
            else {
                refresh();
                message("Crop too small. Drag a rectangle at least 2 x 2 pixels.");
            }
        }
        m_dragging = false;
    }
    void ccTouchCancelled(CCTouch *, CCEvent *) override {
        m_dragging = false;
        refresh();
    }
    void keyBackClicked() override {
        close(false);
    }
};
class Launcher : public CCNode {
  public:
    void open(CCObject *) {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene || scene->getChildByID(editorID))
            return;
        auto menu = findMenu(scene);
        auto data = menu ? typeinfo_cast<MenuLayoutState *>(menu->getChildByID(dataID)) : nullptr;
        if (!data) {
            FLAlertLayer::create("Menu Editor", "Open Graphics from the main menu to edit it.",
                                 "OK")
                ->show();
            return;
        }
        if (auto editor = LayoutEditor::create(data)) {
            if (auto guide = scene->getChildByID(guideID)) {
                if (auto layer = typeinfo_cast<CCLayer *>(guide)) {
                    layer->setTouchEnabled(false);
                    layer->setKeypadEnabled(false);
                }
                guide->removeFromParentAndCleanup(true);
            }
            scene->addChild(editor, 10000);
        }
    }
};
void addLauncher(CCNode *parent, CCPoint position) {
    if (!parent || parent->getChildByID(entryID))
        return;
    auto target = new Launcher;
    target->autorelease();
    target->setID(std::string(entryID) + "/callback");
    parent->addChild(target);
    auto menu = CCMenu::create();
    menu->setPosition(position);
    menu->setID(entryID);
    auto label = ButtonSprite::create("Menu Editor");
    if (!label)
        return;
    label->setScale(.45f);
    auto button = CCMenuItemSpriteExtra::create(label, target, menu_selector(Launcher::open));
    if (!button)
        return;
    button->setID(guideButtonID);
    menu->addChild(button);
    nameOwnedNodes(menu, entryID);
    parent->addChild(menu, 999);
}
}
class $modify(LayoutMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init())
            return false;
        scheduleOnce(schedule_selector(LayoutMenu::prepare), 0.f);
        return true;
    }
    void prepare(float) {
        storage::initialize();
        if (getChildByID(dataID))
            return;
        auto data = new MenuLayoutState;
        if (!data->init()) {
            delete data;
            return;
        }
        data->autorelease();
        data->setID(dataID);
        data->collect(this);
        addChild(data);
        data->load(Mod::get()->getSavedValue<matjson::Value>("layout-v1"));
        scheduleOnce(schedule_selector(LayoutMenu::guide), .8f);
    }
    void guide(float) {
        if (Mod::get()->getSavedValue<bool>("guide-completed-v1", false))
            return;
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene || findMenu(scene) != this || scene->getChildByID(guideID) ||
            scene->getChildByID(editorID))
            return;
        if (auto tutorial = FirstRunGuide::create())
            scene->addChild(tutorial, 20000);
    }
};
class $modify(LayoutVideo, VideoOptionsLayer) {
    bool init() {
        if (!VideoOptionsLayer::init())
            return false;
        auto win = CCDirector::sharedDirector()->getWinSize();
        addLauncher(m_mainLayer, {win.width / 2, win.height - 24});
        return true;
    }
};
