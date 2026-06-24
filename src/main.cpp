#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <filesystem>
#include <fstream>

using namespace geode::prelude;

// Storage
static std::vector<std::vector<cocos2d::CCPoint>> g_segments;
static std::vector<cocos2d::CCPoint> g_currentSegment;
static cocos2d::CCSprite* g_ghostSprite = nullptr;

// ==========================================
// 🕹️ PLAYLAYER RECORDING & RENDERING
// ==========================================
struct $modify(GhostPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool usePractice, bool isPlatformer) {
        if (!PlayLayer::init(level, usePractice, isPlatformer)) return false;
        
        // Ensure ghost sprite exists
        if (!g_ghostSprite) {
            g_ghostSprite = cocos2d::CCSprite::createWithSpriteFrameName("square02_001.png");
            g_ghostSprite->setColor({0, 255, 255}); // CYN
            g_ghostSprite->setOpacity(150);
            this->m_objectLayer->addChild(g_ghostSprite, 999);
        }
        g_ghostSprite->setVisible(true);

        g_segments.clear();
        g_currentSegment.clear();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        
        // Recording
        if (m_player1 && !m_player1->m_isDead) {
            g_currentSegment.push_back({m_player1->getPositionX(), m_player1->getPositionY()});
            
            // Rendering - Simple playback
            if (!g_currentSegment.empty()) {
                g_ghostSprite->setPosition(g_currentSegment.back());
            }
        }
    }

    void checkpointActivated(CheckpointGameObject* obj) {
        PlayLayer::checkpointActivated(obj);
        g_segments.push_back(g_currentSegment);
        g_currentSegment.clear();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g_currentSegment.clear();
    }
};

// ==========================================
// 🎛️ UI GHOST MANAGER
// ==========================================
class GhostPopup : public FLAlertLayer {
public:
    static GhostPopup* create() {
        auto ret = new GhostPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        // Safe standard init
        if (!FLAlertLayer::init(nullptr, "Ghost Manager", "OK", nullptr, 350.f)) return false;
        
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        
        // Delete Left
        auto delBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Del", "goldFont.fnt", "GJ_button_02.png"), this, menu_selector(GhostPopup::onDelete));
        delBtn->setPosition({winSize.width/2 - 80, winSize.height/2});
        menu->addChild(delBtn);

        // Color Right
        auto colBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Col", "goldFont.fnt", "GJ_button_01.png"), this, menu_selector(GhostPopup::onColorChange));
        colBtn->setPosition({winSize.width/2 + 80, winSize.height/2});
        menu->addChild(colBtn);

        return true;
    }

    void onDelete(CCObject*) { Notification::create("Delete Action", NotificationIcon::Info)->show(); }
    void onColorChange(CCObject*) { Notification::create("Color Action", NotificationIcon::Info)->show(); }
};

// ==========================================
// ⏸️ PAUSE MENU
// ==========================================
struct $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = this->getChildByID("right-button-menu");
        if (menu) {
            auto btn = CCMenuItemSpriteExtra::create(
                cocos2d::CCSprite::createWithSpriteFrameName("GJ_downloadsIcon_001.png"), 
                this, menu_selector(MyPauseLayer::onOpen));
            menu->addChild(btn);
            menu->updateLayout();
        }
    }
    void onOpen(CCObject*) {
        auto popup = GhostPopup::create();
        popup->show();
    }
};
