#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace geode::prelude;

struct GhostFrame { float x, y, rot; };
static std::vector<std::vector<GhostFrame>> g_segments;
static std::vector<GhostFrame> g_currentSegment;
static ccColor3B g_ghostColor = {0, 255, 255}; 

// ==========================================
// 💾 FILE ENGINE (Declared first so the compiler can find them)
// ==========================================
void saveMacroToFile(int levelID) {
    auto path = Mod::get()->getSaveDir() / (std::to_string(levelID) + ".json");
    std::ofstream file(path);
    file << "{\"frames\":[";
    bool first = true;
    for (auto& seg : g_segments) {
        for (auto& f : seg) {
            if (!first) file << ",";
            file << "{\"x\":" << f.x << ",\"y\":" << f.y << ",\"rot\":" << f.rot << "}";
            first = false;
        }
    }
    file << "]}";
    file.close();
    Notification::create("Macro Saved as " + std::to_string(levelID) + ".json", NotificationIcon::Success)->show();
}

// ==========================================
// 🕹️ RECORDING ENGINE
// ==========================================
struct $modify(GhostPlayLayer, PlayLayer) {
    void update(float dt) {
        PlayLayer::update(dt);
        if (m_player1 && !m_player1->m_isDead) {
            g_currentSegment.push_back({m_player1->getPositionX(), m_player1->getPositionY(), m_player1->getRotation()});
        }
        
        // Auto-save at 99.99%
        if (this->getCurrentPercent() >= 99.99f) {
            saveMacroToFile(m_level->m_levelID);
        }
    }

    void checkpointActivated(CheckpointGameObject* obj) {
        PlayLayer::checkpointActivated(obj);
        g_segments.push_back(g_currentSegment);
        g_currentSegment.clear();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g_currentSegment.clear(); // Reset frame to checkpoint
    }
};

// ==========================================
// 🎛️ GHOST MANAGER UI
// ==========================================
class GhostPopup : public FLAlertLayer, public FLAlertLayerProtocol {
public:
    bool init() override { // Marked as override
        if (!FLAlertLayer::create(this, "Ghost Manager", "Select Action", "OK", nullptr)) return false;
        auto menu = CCMenu::create();
        m_mainLayer->addChild(menu);

        auto saveDir = Mod::get()->getSaveDir();
        int yOffset = 50;
        for (auto const& entry : std::filesystem::directory_iterator(saveDir)) {
            // Left: Delete, Right: Color
            auto delBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Del", "goldFont.fnt", "GJ_button_02.png"), this, menu_selector(GhostPopup::onDelete));
            auto colBtn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("Col", "goldFont.fnt", "GJ_button_01.png"), this, menu_selector(GhostPopup::onColorChange));
            
            delBtn->setPosition({-50, (float)yOffset});
            colBtn->setPosition({50, (float)yOffset});
            menu->addChild(delBtn);
            menu->addChild(colBtn);
            yOffset -= 40;
        }
        return true;
    }
    
    void onDelete(CCObject*) { Notification::create("Delete Logic Triggered", NotificationIcon::Info)->show(); }
    void onColorChange(CCObject*) { Notification::create("Color Swapped", NotificationIcon::Info)->show(); }
    
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
