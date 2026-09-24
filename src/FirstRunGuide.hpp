#pragma once
#include "MenuModel.hpp"
#include "GuideShaders.hpp"
#include "LayoutStorage.hpp"

namespace layout {
constexpr auto guideID = "gorik.menu_layout_editor/first-run-guide";
constexpr auto guideButtonID = "gorik.menu_layout_editor/open-editor";
class FirstRunGuide : public CCLayer {
    int m_step = 0, m_attempts = 0;
    bool m_waiting = true, m_pressed = false;
    Ref<CCMenuItem> m_target;
    Ref<CCGLProgram> m_shader;
    CCSprite *m_backdrop = nullptr;
    CCDrawNode *m_frame = nullptr;
    CCNode *m_card = nullptr;
    CCLabelBMFont *m_title = nullptr;
    CCLabelBMFont *m_body = nullptr;
    CCLabelBMFont *m_skip = nullptr;
    CCSize m_windowSize;
    CCRect m_hole;
    bool graphicsCaption(CCNode *n) {
        if (auto label = typeinfo_cast<CCLabelBMFont *>(n)) {
            std::string text = label->getString();
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (text == "graphics" || text == "video" || text == "graphics settings")
                return true;
        }
        if (auto children = n->getChildren())
            for (auto child : CCArrayExt<CCNode *>(children))
                if (graphicsCaption(child))
                    return true;
        return false;
    }
    CCMenuItem *locate(CCNode *node) {
        if (!node || node == this || !node->isVisible())
            return nullptr;
        if (auto item = typeinfo_cast<CCMenuItem *>(node)) {
            std::string id = item->getID();
            bool match =
                m_step == 0 ? id == "settings-button"
                : m_step == 1
                    ? (id == "graphics-button" || id == "video-button" ||
                       (typeinfo_cast<OptionsLayer *>(item->m_pListener) && graphicsCaption(item)))
                    : id == guideButtonID;
            if (match && item->isEnabled())
                return item;
        }
        if (auto children = node->getChildren())
            for (auto child : CCArrayExt<CCNode *>(children))
                if (auto item = locate(child))
                    return item;
        return nullptr;
    }
    bool makeShader() {
        auto shader = new CCGLProgram;
        shader->autorelease();
        if (!shader->initWithVertexShaderByteArray(shaders::vertex, shaders::fragment))
            return false;
        shader->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
        shader->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
        shader->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
        if (!shader->link())
            return false;
        shader->updateUniforms();
        m_shader = shader;
        return true;
    }
    void capture() {
        if (m_backdrop) {
            m_backdrop->removeFromParentAndCleanup(true);
            m_backdrop = nullptr;
        }
        auto rt = CCRenderTexture::create(int(std::ceil(m_windowSize.width)),
                                          int(std::ceil(m_windowSize.height)));
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!rt || !scene)
            return;
        bool shown = isVisible();
        setVisible(false);
        rt->beginWithClear(0, 0, 0, 1);
        scene->visit();
        rt->end();
        setVisible(shown);
        auto rendered = rt->getSprite();
        if (!rendered || !rendered->getTexture())
            return;
        auto sprite =
            CCSprite::createWithTexture(rendered->getTexture(), rendered->getTextureRect());
        if (!sprite)
            return;
        sprite->setAnchorPoint(CCPointZero);
        sprite->setPosition(CCPointZero);
        sprite->setFlipY(true);
        if (m_shader) {
            sprite->setShaderProgram(m_shader);
            m_shader->use();
            auto texture = rt->getSprite()->getTexture();
            auto scale = CCDirector::sharedDirector()->getContentScaleFactor();
            m_shader->setUniformLocationWith2f(m_shader->getUniformLocationForName("u_pixel"),
                                               3.5f * scale / texture->getPixelsWide(),
                                               3.5f * scale / texture->getPixelsHigh());
        } else
            sprite->setVisible(false);
        sprite->setID(std::string(guideID) + "/backdrop");
        addChild(sprite, -1);
        m_backdrop = sprite;
    }
    void fill(CCRect r, ccColor4F color) {
        if (r.size.width <= 0 || r.size.height <= 0)
            return;
        CCPoint p[] = {{r.getMinX(), r.getMinY()},
                       {r.getMaxX(), r.getMinY()},
                       {r.getMaxX(), r.getMaxY()},
                       {r.getMinX(), r.getMaxY()}};
        m_frame->drawPolygon(p, 4, color, 0, {0, 0, 0, 0});
    }
    void focus() {
        m_frame->clear();
        if (m_target) {
            auto r = bounds(m_target);
            auto clipped =
                geometry::spotlight({r.origin.x, r.origin.y, r.size.width, r.size.height},
                                    m_windowSize.width, m_windowSize.height);
            m_hole = {clipped.x, clipped.y, clipped.w, clipped.h};
        } else
            m_hole = {-10, -10, 1, 1};
        if (m_shader) {
            m_shader->use();
            m_shader->setUniformLocationWith4f(m_shader->getUniformLocationForName("u_hole"),
                                               m_hole.getMinX(), m_hole.getMinY(), m_hole.getMaxX(),
                                               m_hole.getMaxY());
        }
        if (!m_shader || !m_backdrop) {
            auto r = m_target ? m_hole : CCRect(0, 0, 0, 0);
            fill({0, 0, m_windowSize.width, r.getMinY()}, {.02f, .03f, .05f, .8f});
            fill({0, r.getMaxY(), m_windowSize.width, m_windowSize.height - r.getMaxY()},
                 {.02f, .03f, .05f, .8f});
            fill({0, r.getMinY(), r.getMinX(), r.size.height}, {.02f, .03f, .05f, .8f});
            fill({r.getMaxX(), r.getMinY(), m_windowSize.width - r.getMaxX(), r.size.height},
                 {.02f, .03f, .05f, .8f});
        }
        float cardY = m_windowSize.height / 2;
        if (m_target)
            cardY = m_hole.getMidY() < m_windowSize.height / 2 ? m_windowSize.height - 69 : 69;
        m_card->setPosition({m_windowSize.width / 2, cardY});
        if (m_target) {
            auto r = m_hole;
            CCPoint p[] = {{r.getMinX(), r.getMinY()},
                           {r.getMaxX(), r.getMinY()},
                           {r.getMaxX(), r.getMaxY()},
                           {r.getMinX(), r.getMaxY()}};
            m_frame->drawPolygon(p, 4, {0, 0, 0, 0}, 2, {.3f, 1, .65f, 1});
            bool above = cardY > r.getMidY();
            CCPoint end = {r.getMidX(), above ? r.getMaxY() + 5 : r.getMinY() - 5};
            CCPoint start = {m_windowSize.width / 2, cardY + (above ? -40.f : 40.f)};
            m_frame->drawSegment(start, end, 1.4f, {.3f, 1, .65f, 1});
            auto d = start - end;
            float length = std::sqrt(d.x * d.x + d.y * d.y);
            if (length > 1) {
                d = d / length;
                CCPoint side = {-d.y, d.x};
                CCPoint arrow[] = {end, end + d * 9 + side * 4, end + d * 9 - side * 4};
                m_frame->drawPolygon(arrow, 3, {.3f, 1, .65f, 1}, 0, {0, 0, 0, 0});
            }
        }
    }
    void ready(float) {
        m_target = locate(CCDirector::sharedDirector()->getRunningScene());
        if (!m_target && m_attempts++ < 10) {
            scheduleOnce(schedule_selector(FirstRunGuide::ready), .25f);
            return;
        }
        m_waiting = false;
        capture();
        setVisible(true);
        const char *titles[] = {"1 / 3   Open Settings", "2 / 3   Open Graphics",
                                "3 / 3   Open Menu Editor"};
        const char *body[] = {
            "Click the highlighted gear button.\nLet's find your menu editor.",
            "Click the highlighted Graphics button.\nYour editor is inside this page.",
            "Click Menu Editor to start customizing.\nDrag, snap, cut and restore. Save keeps your "
            "layout."};
        m_title->setString(m_target ? titles[m_step] : "Guide paused");
        m_body->setString(m_target ? body[m_step]
                                   : "The expected button is not visible.\nUse Settings > Graphics "
                                     "> Menu Editor.\nSkip to return to the game.");
        m_body->limitLabelWidth(std::min(360.f, m_windowSize.width - 52), .55f, .25f);
        focus();
        scheduleUpdate();
    }
    void dismiss(bool complete) {
        if (complete) {
            auto mod = Mod::get();
            auto result = storage::save(mod->getSavedValue<matjson::Value>("layout-v1"),
                                        mod->getSavedValue<bool>("snap-enabled", true), true);
            if (result.isErr())
                log::warn("Could not save guide completion: {}", result.unwrapErr());
        }
        removeFromParentAndCleanup(true);
    }

  public:
    static FirstRunGuide *create() {
        auto p = new FirstRunGuide;
        if (p->init()) {
            p->autorelease();
            return p;
        }
        delete p;
        return nullptr;
    }
    bool init() override {
        if (!CCLayer::init())
            return false;
        setID(guideID);
        m_windowSize = CCDirector::sharedDirector()->getWinSize();
        makeShader();
        m_frame = CCDrawNode::create();
        addChild(m_frame);
        m_card = CCNode::create();
        addChild(m_card, 1);
        float width = std::min(390.f, m_windowSize.width - 24);
        auto panel = CCLayerColor::create({16, 24, 39, 245}, width, 82);
        panel->setPosition({-width / 2, -41});
        m_card->addChild(panel);
        m_title = CCLabelBMFont::create("", "bigFont.fnt");
        m_title->setScale(.45f);
        m_title->setPosition({0, 21});
        m_card->addChild(m_title);
        m_body = CCLabelBMFont::create("", "chatFont.fnt");
        m_body->setPosition({0, -10});
        m_body->setAlignment(kCCTextAlignmentCenter);
        m_card->addChild(m_body);
        m_skip = CCLabelBMFont::create("Skip guide  /  Esc", "chatFont.fnt");
        m_skip->setScale(.6f);
        m_skip->setPosition({m_windowSize.width - 70, 14});
        addChild(m_skip, 2);
        nameOwnedNodes(this, guideID);
        setTouchEnabled(true);
        setKeypadEnabled(true);
        setVisible(false);
        return true;
    }
    void onEnter() override {
        CCLayer::onEnter();
        scheduleOnce(schedule_selector(FirstRunGuide::ready), .1f);
    }
    void registerWithTouchDispatcher() override {
        CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -20000, true);
    }
    void update(float) override {
        if (m_target && (!m_target->getParent() || !visible(m_target))) {
            dismiss(false);
            return;
        }
        focus();
    }
    bool ccTouchBegan(CCTouch *touch, CCEvent *) override {
        auto p = touch->getLocation();
        m_pressed = false;
        if (p.y < 30 && p.x > m_windowSize.width - 145) {
            dismiss(true);
            return true;
        }
        if (!m_waiting && m_target && m_hole.containsPoint(p))
            m_pressed = true;
        return true;
    }
    void ccTouchEnded(CCTouch *touch, CCEvent *) override {
        if (!m_pressed || m_waiting || !m_target || !visible(m_target) || !m_target->isEnabled() ||
            !m_hole.containsPoint(touch->getLocation()))
            return;
        m_pressed = false;
        Ref<FirstRunGuide> keep = this;
        Ref<CCMenuItem> button = m_target;
        if (m_step == 2) {
            dismiss(true);
            button->activate();
            return;
        }
        unscheduleUpdate();
        m_target = nullptr;
        m_waiting = true;
        m_attempts = 0;
        setVisible(false);
        ++m_step;
        button->activate();
        scheduleOnce(schedule_selector(FirstRunGuide::ready), .65f);
    }
    void ccTouchCancelled(CCTouch *, CCEvent *) override {
        m_pressed = false;
    }
    void keyBackClicked() override {
        dismiss(true);
    }
};
}
