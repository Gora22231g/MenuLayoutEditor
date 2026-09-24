#pragma once
#include <Geode/Geode.hpp>
#include <cctype>
#include <map>
#include "Geometry.hpp"
using namespace geode::prelude;
namespace layout {
constexpr auto dataID = "gorik.menu_layout_editor/data";
constexpr auto editorID = "gorik.menu_layout_editor/editor";
constexpr auto entryID = "gorik.menu_layout_editor/entry";
inline void nameOwnedNodes(CCNode *node, std::string const &path) {
    if (!node)
        return;
    if (node->getID().empty())
        node->setID(path);
    if (typeinfo_cast<CCLabelBMFont *>(node) || typeinfo_cast<CCLabelTTF *>(node))
        return;
    if (auto children = node->getChildren())
        for (unsigned i = 0; i < children->count(); ++i)
            nameOwnedNodes(static_cast<CCNode *>(children->objectAtIndex(i)),
                           path + "/" + std::to_string(i));
}
struct MenuElement {
    Ref<CCNode> node;
    std::string key;
    CCPoint base;
    bool visible = true, locked = false;
    int root = -1;
    geometry::Rect region;
};
inline CCRect bounds(CCNode *n) {
    if (!n)
        return {};
    auto s = n->getContentSize();
    CCPoint p[] = {n->convertToWorldSpace(CCPointZero), n->convertToWorldSpace({s.width, 0}),
                   n->convertToWorldSpace({0, s.height}),
                   n->convertToWorldSpace({s.width, s.height})};
    float l = p[0].x, r = l, b = p[0].y, t = b;
    for (auto v : p) {
        l = std::min(l, v.x);
        r = std::max(r, v.x);
        b = std::min(b, v.y);
        t = std::max(t, v.y);
    }
    return {l, b, r - l, t - b};
}
inline bool visible(CCNode *n) {
    if (!n || !n->getParent())
        return false;
    for (auto a = n; a; a = a->getParent())
        if (!a->isVisible())
            return false;
    return true;
}
class PartButtonAction : public CCNode {
  public:
    Ref<CCMenuItem> original;
    void run(CCObject *) {
        Ref<PartButtonAction> keep = this;
        if (original && original->getParent() && original->isRunning() && original->isEnabled())
            original->activate();
    }
};
class MenuLayoutState : public CCNode {
    size_t m_originalCount = 0;
    unsigned m_nextPartID = 0;
    std::map<int, Ref<CCTexture2D>> m_snapshots;
    std::map<int, std::uint64_t> m_snapshotSizes;
    std::map<int, CCSize> m_snapshotContentSizes;
    std::map<int, Ref<CCMenu>> m_pieceMenus;
    CCMenu *pieceMenu(int root, CCNode *sourceParent) {
        auto found = m_pieceMenus.find(root);
        if (found != m_pieceMenus.end())
            return found->second;
        auto menuLayer = getParent();
        if (!menuLayer)
            return nullptr;
        auto host = CCMenu::create();
        if (!host)
            return nullptr;
        host->setAnchorPoint(CCPointZero);
        host->setPosition(CCPointZero);
        host->setID("gorik.menu_layout_editor/parts-" + std::to_string(root));
        CCNode *branch = sourceParent;
        while (branch->getParent() && branch->getParent() != menuLayer)
            branch = branch->getParent();
        menuLayer->addChild(host, branch->getZOrder());
        host->setAdditionalTransform(CCAffineTransformConcat(sourceParent->nodeToWorldTransform(),
                                                             menuLayer->worldToNodeTransform()));
        m_pieceMenus[root] = host;
        return host;
    }
    bool isRobTop(CCNode *n) {
        std::string id = n->getID();
        std::transform(id.begin(), id.end(), id.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (id.find("robtop") != std::string::npos)
            return true;
        if (auto label = typeinfo_cast<CCLabelBMFont *>(n)) {
            std::string text = label->getString();
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (text.find("robtop") != std::string::npos)
                return true;
        }
        if (auto children = n->getChildren())
            for (auto child : CCArrayExt<CCNode *>(children))
                if (isRobTop(child))
                    return true;
        return false;
    }
    void scan(CCNode *parent, std::string path) {
        if (auto children = parent->getChildren())
            for (unsigned i = 0; i < children->count(); ++i) {
                auto n = static_cast<CCNode *>(children->objectAtIndex(i));
                std::string id = n->getID();
                if (id.starts_with("gorik.menu_layout_editor/"))
                    continue;
                auto key = path + "/" + (id.empty() ? "#" + std::to_string(i) : id);
                auto s = n->getContentSize();
                bool leaf = typeinfo_cast<CCMenuItem *>(n) || typeinfo_cast<CCLabelBMFont *>(n) ||
                            typeinfo_cast<CCLabelTTF *>(n) ||
                            (typeinfo_cast<CCSprite *>(n) && s.width < 650 && s.height < 180);
                if (leaf) {
                    items.push_back({n, key, n->getPosition(), n->isVisible(), isRobTop(n)});
                    continue;
                }
                if (typeinfo_cast<CCMenu *>(n) || !typeinfo_cast<CCLayer *>(n))
                    scan(n, key);
            }
    }
    CCPoint offset(MenuElement const &e) {
        if (!e.node->getParent())
            return CCPointZero;
        auto p = e.node->getParent();
        return p->convertToWorldSpace(e.node->getPosition()) - p->convertToWorldSpace(e.base);
    }
    void place(MenuElement &e, matjson::Value const &value) {
        auto p = e.node->getParent();
        if (!p)
            return;
        float x = value["x"].asDouble().unwrapOr(0), y = value["y"].asDouble().unwrapOr(0);
        if (!std::isfinite(x) || !std::isfinite(y))
            x = y = 0;
        auto w = CCDirector::sharedDirector()->getWinSize();
        e.node->setPosition(p->convertToNodeSpace(
            p->convertToWorldSpace(e.base) +
            ccp(std::clamp(x, -1.f, 1.f) * w.width, std::clamp(y, -1.f, 1.f) * w.height)));
        e.node->setVisible(!value["hidden"].asBool().unwrapOr(!e.visible));
        if (e.locked) {
            e.node->setPosition(e.base);
            e.node->setVisible(true);
        }
    }
    CCNode *createPartNode(int root, geometry::Rect region) {
        auto &src = items[root];
        auto n = src.node.data();
        auto size = n->getContentSize();
        if (size.width < 1 || size.height < 1 || size.width > 2048 || size.height > 2048 ||
            !n->getParent())
            return nullptr;
        if (!geometry::validRegion(region) || region.w * size.width < 2 ||
            region.h * size.height < 2)
            return nullptr;
        auto cached = m_snapshots.find(root);
        CCTexture2D *texture = cached == m_snapshots.end() ? nullptr : cached->second.data();
        if (texture)
            size = m_snapshotContentSizes[root];
        if (!texture) {
            auto pixels = geometry::snapshotPixels(
                size.width, size.height, CCDirector::sharedDirector()->getContentScaleFactor());
            std::uint64_t total = pixels;
            for (auto const &[key, count] : m_snapshotSizes)
                total += count;
            if (!pixels || total > 16 * 1024 * 1024) {
                error = "Snapshot memory limit reached. Restore another element first.";
                return nullptr;
            }
            auto rt =
                CCRenderTexture::create(int(std::ceil(size.width)), int(std::ceil(size.height)));
            if (!rt)
                return nullptr;
            auto pos = n->getPosition(), anchor = n->getAnchorPoint();
            float sx = n->getScaleX(), sy = n->getScaleY(), rx = n->getRotationX(),
                  ry = n->getRotationY();
            float kx = n->getSkewX(), ky = n->getSkewY();
            bool show = n->isVisible();
            n->setAnchorPoint(CCPointZero);
            n->setPosition(CCPointZero);
            n->setScaleX(1);
            n->setScaleY(1);
            n->setRotationX(0);
            n->setRotationY(0);
            n->setSkewX(0);
            n->setSkewY(0);
            n->setVisible(true);
            rt->beginWithClear(0, 0, 0, 0);
            n->visit();
            rt->end();
            n->setAnchorPoint(anchor);
            n->setPosition(pos);
            n->setScaleX(sx);
            n->setScaleY(sy);
            n->setRotationX(rx);
            n->setRotationY(ry);
            n->setSkewX(kx);
            n->setSkewY(ky);
            n->setVisible(show);
            if (!rt->getSprite() || !rt->getSprite()->getTexture())
                return nullptr;
            texture = rt->getSprite()->getTexture();
            m_snapshots[root] = texture;
            m_snapshotSizes[root] = pixels;
            m_snapshotContentSizes[root] = size;
        }
        auto rect = CCRect(region.x * size.width,
                           std::ceil(size.height) - (region.y + region.h) * size.height,
                           region.w * size.width, region.h * size.height);
        auto image = CCSprite::createWithTexture(texture, rect);
        if (!image)
            return nullptr;
        image->setFlipY(true);
        image->setBlendFunc({GL_ONE, GL_ONE_MINUS_SRC_ALPHA});
        CCNode *part = image;
        if (auto button = typeinfo_cast<CCMenuItem *>(n)) {
            auto forward = new PartButtonAction;
            forward->autorelease();
            forward->original = button;
            part =
                CCMenuItemSpriteExtra::create(image, forward, menu_selector(PartButtonAction::run));
            if (!part)
                return nullptr;
            part->addChild(forward);
        }
        part->setAnchorPoint({.5f, .5f});
        part->setScaleX(n->getScaleX());
        part->setScaleY(n->getScaleY());
        part->setRotationX(n->getRotationX());
        part->setRotationY(n->getRotationY());
        part->setSkewX(n->getSkewX());
        part->setSkewY(n->getSkewY());
        nameOwnedNodes(part, "gorik.menu_layout_editor/part-" + std::to_string(m_nextPartID++));
        return part;
    }
    bool addPart(int root, geometry::Rect region, CCPoint delta) {
        auto part = createPartNode(root, region);
        if (!part)
            return false;
        auto &src = items[root];
        auto n = src.node.data();
        auto size = m_snapshotContentSizes[root];
        auto p = n->getParent();
        CCPoint center = {(region.x + region.w * .5f) * size.width,
                          (region.y + region.h * .5f) * size.height};
        if (!p)
            return false;
        auto base =
            src.base + p->convertToNodeSpace(n->convertToWorldSpace(center)) - n->getPosition();
        auto host = pieceMenu(root, p);
        if (!host)
            return false;
        host->addChild(part, n->getZOrder());
        part->setPosition(host->convertToNodeSpace(p->convertToWorldSpace(base) + delta));
        items.push_back({part, src.key + "/part", base, true, false, root, region});
        return true;
    }

  public:
    std::vector<MenuElement> items;
    std::string error;
    void collect(CCNode *menu) {
        scan(menu, "");
        m_originalCount = items.size();
        scheduleUpdate();
    }
    void update(float) override {
        for (auto const &[root, host] : m_pieceMenus) {
            auto sourceParent = items[root].node->getParent();
            if (!sourceParent || !host->getParent()) {
                host->setVisible(false);
                continue;
            }
            host->setVisible(visible(sourceParent));
            host->setAdditionalTransform(CCAffineTransformConcat(
                sourceParent->nodeToWorldTransform(), host->getParent()->worldToNodeTransform()));
        }
    }
    bool hasParts(int root) const {
        for (size_t i = m_originalCount; i < items.size(); ++i)
            if (items[i].root == root)
                return true;
        return false;
    }
    bool canSelect(int index) const {
        return index >= 0 && index < int(items.size()) && items[index].node->getParent() &&
               !(items[index].root < 0 && hasParts(index));
    }
    void restore(int index) {
        if (index < 0 || index >= int(items.size()))
            return;
        int root = items[index].root < 0 ? index : items[index].root;
        for (size_t i = items.size(); i-- > m_originalCount;)
            if (items[i].root == root) {
                items[i].node->removeFromParentAndCleanup(true);
                items.erase(items.begin() + i);
            }
        auto &e = items[root];
        e.node->setPosition(e.base);
        e.node->setVisible(e.locked || e.visible);
        m_snapshots.erase(root);
        m_snapshotSizes.erase(root);
        m_snapshotContentSizes.erase(root);
        if (auto host = m_pieceMenus.find(root); host != m_pieceMenus.end()) {
            host->second->removeFromParentAndCleanup(true);
            m_pieceMenus.erase(host);
        }
    }
    void reset() {
        for (int i = int(m_originalCount) - 1; i >= 0; --i)
            restore(i);
    }
    matjson::Value serialize() {
        auto out = matjson::Value::object();
        auto w = CCDirector::sharedDirector()->getWinSize();
        if (w.width <= 0 || w.height <= 0)
            return out;
        for (size_t i = 0; i < m_originalCount; ++i) {
            auto &e = items[i];
            auto d = offset(e);
            if (!e.node->getParent())
                continue;
            auto v = matjson::makeObject(
                {{"x", d.x / w.width}, {"y", d.y / w.height}, {"hidden", !e.node->isVisible()}});
            auto parts = matjson::Value::array();
            for (size_t j = m_originalCount; j < items.size(); ++j)
                if (items[j].root == int(i)) {
                    auto &part = items[j];
                    auto r = part.region;
                    auto shift = offset(part);
                    if (!part.node->getParent())
                        continue;
                    parts.push(matjson::makeObject({{"l", r.x},
                                                    {"b", r.y},
                                                    {"w", r.w},
                                                    {"h", r.h},
                                                    {"x", shift.x / w.width},
                                                    {"y", shift.y / w.height},
                                                    {"hidden", !part.node->isVisible()}}));
                }
            v["parts"] = parts;
            out[e.key] = v;
        }
        return out;
    }
    void load(matjson::Value const &state) {
        error.clear();
        reset();
        for (size_t i = 0; i < m_originalCount; ++i) {
            auto v = state[items[i].key];
            place(items[i], v);
            if (items[i].locked)
                continue;
            auto parts = v["parts"].asArray();
            if (parts.isErr())
                continue;
            auto start = items.size();
            bool ok = true;
            for (auto const &p : parts.unwrap()) {
                geometry::Rect r{
                    float(p["l"].asDouble().unwrapOr(-1)), float(p["b"].asDouble().unwrapOr(-1)),
                    float(p["w"].asDouble().unwrapOr(0)), float(p["h"].asDouble().unwrapOr(0))};
                if (!geometry::validRegion(r) || items.size() - m_originalCount >= 64) {
                    ok = false;
                    break;
                }
                if (!addPart(int(i), r, CCPointZero)) {
                    ok = false;
                    break;
                }
                place(items.back(), p);
            }
            if (!ok) {
                restore(int(i));
                error = "Some cuts could not be restored; original element was restored.";
            } else if (items.size() > start)
                items[i].node->setVisible(false);
        }
    }
    bool cut(int index, std::vector<geometry::Rect> const &regions) {
        error.clear();
        if (!canSelect(index) || items[index].locked)
            return false;
        if (!geometry::fitsPartLimit(items.size() - m_originalCount, regions.size(),
                                     index >= int(m_originalCount))) {
            error = "Maximum 64 parts. Restore an element first.";
            return false;
        }
        int root = items[index].root < 0 ? index : items[index].root;
        auto delta = offset(items[index]);
        auto start = items.size();
        for (auto r : regions)
            if (!addPart(root, r, delta)) {
                while (items.size() > start) {
                    items.back().node->removeFromParentAndCleanup(true);
                    items.pop_back();
                }
                if (start == m_originalCount || !hasParts(root)) {
                    m_snapshots.erase(root);
                    m_snapshotSizes.erase(root);
                    m_snapshotContentSizes.erase(root);
                    if (auto host = m_pieceMenus.find(root); host != m_pieceMenus.end()) {
                        host->second->removeFromParentAndCleanup(true);
                        m_pieceMenus.erase(host);
                    }
                }
                if (error.empty())
                    error = "This element could not be cut (minimum part size: 2 x 2).";
                return false;
            }
        if (index >= int(m_originalCount)) {
            items[index].node->removeFromParentAndCleanup(true);
            items.erase(items.begin() + index);
        }
        items[root].node->setVisible(false);
        return true;
    }
};
}
