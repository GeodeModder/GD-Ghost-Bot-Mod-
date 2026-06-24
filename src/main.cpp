#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace geode::prelude;

// ==========================================
// 🏗️ GHOST STORAGE & RENDERER
// ==========================================
static cocos2d::CCSprite* g_ghostSprite = nullptr;

// ==========================================
// 🕹️ RECORDING ENGINE
// ==========================================
struct $modify(GhostPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool usePractice, bool isPlatformer) {
        if (!PlayLayer::init(level, usePractice, isPlatformer)) return false;
        
        if (!g_ghostSprite) {
            g_ghostSprite = cocos2d::CCSprite::createWithSpriteFrameName("square02_001.png");
            g_ghostSprite->setColor({0, 255, 255}); // CYN
            g_ghostSprite->setOpacity(150);
            this->m_objectLayer->addChild(g_ghostSprite, 999);
        }
        g_ghostSprite->setVisible(true);
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (m_player1 && !m_player1->m_isDead) {
            g_ghostSprite->setPosition(m_player1->getPosition());
        }
    }
};

// ==========================================
// 🎛️ UI: GHOST MANAGER
// ==========================================
class GhostPopup : public FLAlertLayer, public FLAlertLayerProtocol {
public:
    // FIXED: Corrected init to pass all 9 arguments
    bool init() override {
        // (Delegate, Title, Desc, Btn1, Btn2, Width, Scroll, Height, TextScale)
        if (!FLAlertLayer::init(this, "Ghost Manager", "Manage Ghosts", "OK", nullptr, 350.f, false, 250.f, 1.f)) return false;
        
        auto menu = CCMenu::create();
        m_mainLayer->addChild(menu);

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        
        // Delete Button (Left)
        auto delBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Del", "goldFont.fnt", "GJ_button_02.png"), this, menu_selector(GhostPopup::onDelete));
        delBtn->setPosition({-60, 0});
        menu->addChild(delBtn);

        // Color Button (Right)
        auto colBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Col", "goldFont.fnt", "GJ_button_01.png"), this, menu_selector(GhostPopup::onColorChange));
        colBtn->setPosition({60, 0});
        menu->addChild(colBtn);

        return true;
    }
    
    void onDelete(CCObject*) { Notification::create("Delete Action", NotificationIcon::Info)->show(); }
    void onColorChange(CCObject*) { Notification::create("Color Action", NotificationIcon::Info)->show(); }
    
    void FLAlert_Clicked(FLAlertLayer* btn, bool btn2) override {}

    static void open() {
        auto p = new GhostPopup();
        if (p && p->init()) p->show();
    }
};

// ==========================================
// ⏸️ PAUSE MENU HOOK
// ==========================================
struct $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = this->getChildByID("right-button-menu");
        if (menu) {
            auto btn = CCMenuItemSpriteExtra::create(
                cocos2d::CCSprite::createWithSpriteFrameName("GJ_downloadsIcon_001.png"), 
                this, menu_selector(MyPauseLayer::onOpenManager));
            menu->addChild(btn);
            menu->updateLayout();
        }
    }
    void onOpenManager(CCObject*) { GhostPopup::open(); }
};
