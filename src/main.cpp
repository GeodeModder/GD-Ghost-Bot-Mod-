#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <sstream>
#include <ctime>

using namespace geode::prelude;

struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    int type = 0;
    int id = 152;
    bool isUpsideDown = false;
};

struct CustomGhostProfile {
    std::string name;
    int r = 0, g = 255, b = 255; 
    std::vector<GhostFrame> frames;
};

// Global cache to hold data even if level resets
static std::vector<GhostFrame> g_lastAttemptData;

struct LiveGhostTrack {
    CustomGhostProfile profile;
    cocos2d::CCSprite* sprite = nullptr;
    std::string filePath;
};

void refreshGhostsForLayer(PlayLayer* pl);

// ==========================================
// 📦 JSON SERIALIZATION
// ==========================================
template <>
struct matjson::Serialize<GhostFrame> {
    static geode::Result<GhostFrame> fromJson(matjson::Value const& value) {
        GhostFrame frame;
        if (value.contains("x")) frame.position.x = static_cast<float>(value["x"].asDouble().unwrapOrDefault());
        if (value.contains("y")) frame.position.y = static_cast<float>(value["y"].asDouble().unwrapOrDefault());
        if (value.contains("rot")) frame.rotation = static_cast<float>(value["rot"].asDouble().unwrapOrDefault());
        if (value.contains("u")) frame.isUpsideDown = value["u"].asBool().unwrapOrDefault();
        return geode::Ok(frame);
    }
    static matjson::Value toJson(GhostFrame const& value) {
        auto obj = matjson::Value();
        obj["x"] = static_cast<double>(value.position.x);
        obj["y"] = static_cast<double>(value.position.y);
        obj["rot"] = static_cast<double>(value.rotation);
        obj["u"] = value.isUpsideDown;
        return obj;
    }
};

template <>
struct matjson::Serialize<CustomGhostProfile> {
    static geode::Result<CustomGhostProfile> fromJson(matjson::Value const& value) {
        CustomGhostProfile prof;
        if (value.contains("name")) prof.name = value["name"].asString().unwrapOrDefault();
        if (value.contains("r")) prof.r = value["r"].asInt().unwrapOrDefault();
        if (value.contains("g")) prof.g = value["g"].asInt().unwrapOrDefault();
        if (value.contains("b")) prof.b = value["b"].asInt().unwrapOrDefault();
        if (value.contains("frames")) {
            prof.frames = value["frames"].as<std::vector<GhostFrame>>().unwrapOrDefault();
        }
        return geode::Ok(prof);
    }
    static matjson::Value toJson(CustomGhostProfile const& value) {
        auto obj = matjson::Value();
        obj["name"] = value.name;
        obj["r"] = value.r;
        obj["g"] = value.g;
        obj["b"] = value.b;
        obj["frames"] = value.frames;
        return obj;
    }
};

// ==========================================
// 🕹️ PLAYLAYER HOOK
// ==========================================
struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<GhostFrame> m_liveRecording;
        std::vector<LiveGhostTrack> m_activeGhosts;
        int m_levelID = 0;
        int m_frameCounter = 0; 
    };

    bool init(GJGameLevel* level, bool usePractice, bool isPlatformer) {
        if (!PlayLayer::init(level, usePractice, isPlatformer)) return false;

        m_fields->m_levelID = level->m_levelID;
        m_fields->m_liveRecording.clear();
        m_fields->m_frameCounter = 0;
        
        refreshGhostsForLayer(this);
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        m_fields->m_frameCounter++;

        if (m_player1 && !m_player1->m_isDead) {
            GhostFrame currentFrame {
                .position = m_player1->getPosition(),
                .rotation = m_player1->getRotation(),
                .isUpsideDown = m_player1->m_isUpsideDown
            };
            m_fields->m_liveRecording.push_back(currentFrame);
            
            if (m_fields->m_liveRecording.size() > 5) {
                g_lastAttemptData = m_fields->m_liveRecording;
            }
        }

        size_t playbackIndex = static_cast<size_t>(m_fields->m_frameCounter);

        for (auto& ghost : m_fields->m_activeGhosts) {
            if (!ghost.sprite && m_objectLayer) {
                ghost.sprite = cocos2d::CCSprite::createWithSpriteFrameName("square02_001.png");
                if (ghost.sprite) {
                    ghost.sprite->setColor({(unsigned char)ghost.profile.r, (unsigned char)ghost.profile.g, (unsigned char)ghost.profile.b});
                    ghost.sprite->setOpacity(120);
                    ghost.sprite->setScale(0.5f);
                    ghost.sprite->setVisible(false);
                    m_objectLayer->addChild(ghost.sprite, 2000);
                }
            }

            if (ghost.sprite && playbackIndex < ghost.profile.frames.size()) {
                auto const& f = ghost.profile.frames[playbackIndex];
                ghost.sprite->setPosition(f.position);
                ghost.sprite->setRotation(f.rotation);
                ghost.sprite->setScaleY(f.isUpsideDown ? -0.5f : 0.5f);
                ghost.sprite->setVisible(true);
            } else if (ghost.sprite) {
                ghost.sprite->setVisible(false);
            }
        }
    }

    void resetLevel() {
        if (m_fields->m_liveRecording.size() > 5) { 
            g_lastAttemptData = m_fields->m_liveRecording;
        }
        PlayLayer::resetLevel();
        m_fields->m_liveRecording.clear();
        m_fields->m_frameCounter = 0;
        refreshGhostsForLayer(this);
    }
};

// ==========================================
// 🛠️ REFRESHER
// ==========================================
void refreshGhostsForLayer(PlayLayer* pl) {
    if (!pl) return;
    auto myPl = static_cast<MyPlayLayer*>(pl);
    for (auto& ghost : myPl->m_fields->m_activeGhosts) {
        if (ghost.sprite) ghost.sprite->removeFromParentAndCleanup(true);
    }
    myPl->m_fields->m_activeGhosts.clear();

    auto saveDir = geode::Mod::get()->getSaveDir();
    std::filesystem::create_directories(saveDir);
    std::string prefix = "ghost_" + std::to_string(myPl->m_fields->m_levelID) + "_";
    
    for (auto const& entry : std::filesystem::directory_iterator(saveDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.rfind(prefix, 0) == 0 && filename.substr(filename.find_last_of('.')) == ".json") {
            try {
                std::ifstream file(entry.path());
                std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                auto json = matjson::parse(str);
                if (json) {
                    auto profileRes = json.unwrap().as<CustomGhostProfile>();
                    if (profileRes) {
                        LiveGhostTrack track;
                        track.profile = profileRes.unwrap();
                        track.filePath = entry.path().string();
                        myPl->m_fields->m_activeGhosts.push_back(track);
                    }
                }
            } catch(...) {}
        }
    }
}

// ==========================================
// 🎛️ POPUP UI
// ==========================================
class AdvancedGhostPopup : public FLAlertLayer {
protected:
    int m_levelID;
    CCTextInputNode* m_inputField = nullptr;

    bool init(int levelID) {
        if (!FLAlertLayer::init(150)) return false;
        m_levelID = levelID;
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

        auto bg = cocos2d::extension::CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({340.f, 210.f});
        bg->setPosition(winSize / 2);
        m_mainLayer->addChild(bg);

        auto title = cocos2d::CCLabelBMFont::create("Custom Ghost Control Room", "goldFont.fnt");
        title->setPosition({winSize.width / 2, winSize.height / 2 + 82.f});
        title->setScale(0.7f);
        m_mainLayer->addChild(title);

        auto menu = cocos2d::CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);

        auto closeBtn = CCMenuItemSpriteExtra::create(cocos2d::CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"), this, menu_selector(AdvancedGhostPopup::onCloseBtn));
        closeBtn->setPosition({winSize.width / 2 - 155.f, winSize.height / 2 + 90.f});
        menu->addChild(closeBtn);

        m_inputField = CCTextInputNode::create(180.f, 30.f, "Name your ghost...", "bigFont.fnt");
        m_inputField->setPosition({winSize.width / 2, winSize.height / 2 + 32.f});
        m_mainLayer->addChild(m_inputField);

        std::string statStr = g_lastAttemptData.empty() ? "No run data cached." : "Cached Run: " + std::to_string(g_lastAttemptData.size()) + " frames";
        auto status = cocos2d::CCLabelBMFont::create(statStr.c_str(), "goldFont.fnt");
        status->setScale(0.35f);
        status->setPosition({winSize.width / 2, winSize.height / 2 + 2.f});
        m_mainLayer->addChild(status);

        float startX = winSize.width / 2 - 100.f;
        float yPos = winSize.height / 2 - 38.f;
        float spacing = 50.f;

        auto makeBtn = [&](const char* text, const char* asset, SEL_MenuHandler selector, int index) {
            auto btn = CCMenuItemSpriteExtra::create(ButtonSprite::create(text, "goldFont.fnt", asset), this, selector);
            btn->setPosition({startX + (index * spacing), yPos});
            btn->setScale(0.7f);
            menu->addChild(btn);
        };

        makeBtn("CYN", "GJ_button_01.png", menu_selector(AdvancedGhostPopup::onCyan), 0);
        makeBtn("GLD", "GJ_button_03.png", menu_selector(AdvancedGhostPopup::onGold), 1);
        makeBtn("RED", "GJ_button_06.png", menu_selector(AdvancedGhostPopup::onRed), 2);
        makeBtn("GRN", "GJ_button_02.png", menu_selector(AdvancedGhostPopup::onGreen), 3);
        makeBtn("PRP", "GJ_button_04.png", menu_selector(AdvancedGhostPopup::onPurple), 4);

        return true;
    }

    void saveWithColor(int r, int g, int b) {
        if (g_lastAttemptData.empty()) {
            FLAlertLayer::create("Error", "No run data!", "OK")->show();
            return;
        }
        std::string name = m_inputField->getString();
        CustomGhostProfile prof{ name.empty() ? "Ghost" : name, r, g, b, g_lastAttemptData };
        std::string filename = "ghost_" + std::to_string(m_levelID) + "_" + std::to_string(time(0)) + ".json";
        std::ofstream file(geode::Mod::get()->getSaveDir() / filename);
        matjson::Value json = prof;
        file << json.dump();
        file.close();
        if (auto pl = PlayLayer::get()) refreshGhostsForLayer(pl);
        onCloseBtn(nullptr);
    }

    void onCloseBtn(cocos2d::CCObject*) { removeFromParentAndCleanup(true); }
    void onCyan(cocos2d::CCObject*) { saveWithColor(0, 255, 255); }
    void onGold(cocos2d::CCObject*) { saveWithColor(255, 215, 0); }
    void onRed(cocos2d::CCObject*) { saveWithColor(255, 60, 60); }
    void onGreen(cocos2d::CCObject*) { saveWithColor(60, 255, 60); }
    void onPurple(cocos2d::CCObject*) { saveWithColor(190, 60, 255); }

public:
    static AdvancedGhostPopup* create(int id) {
        auto ret = new AdvancedGhostPopup();
        if (ret && ret->init(id)) { ret->autorelease(); return ret; }
        CC_SAFE_DELETE(ret); return nullptr;
    }
};

struct $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = this->getChildByID("right-button-menu");
        if (!menu) menu = this->getChildByID("center-button-menu");
        if (menu) {
            auto btn = CCMenuItemSpriteExtra::create(cocos2d::CCSprite::createWithSpriteFrameName("GJ_downloadsIcon_001.png"), this, menu_selector(MyPauseLayer::onOpen));
            menu->addChild(btn);
            menu->updateLayout();
        }
    }
    void onOpen(cocos2d::CCObject*) {
        if (auto pl = PlayLayer::get()) {
            auto myPl = static_cast<MyPlayLayer*>(pl);
            if (!myPl->m_fields->m_liveRecording.empty()) g_lastAttemptData = myPl->m_fields->m_liveRecording;
            AdvancedGhostPopup::create(pl->m_level->m_levelID)->show();
        }
    }
};
