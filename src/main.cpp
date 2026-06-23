#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <vector>

using namespace geode::prelude;

struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    IconType iconType;
    int iconID;
};

static inline std::vector<GhostFrame> g_ghostTape;
static inline size_t g_liveFrameCounter = 0;
static inline SimplePlayer* g_mirrorGhost = nullptr;
static inline IconType g_lastGhostType = IconType::Cube;

// --- Helper Functions ---
IconType getCurrentIconType(PlayerObject* player) {
    if (player->m_isShip) return IconType::Ship;
    if (player->m_isBall) return IconType::Ball;
    if (player->m_isBird) return IconType::Ufo;
    if (player->m_isDart) return IconType::Wave;
    if (player->m_isRobot) return IconType::Robot;
    if (player->m_isSpider) return IconType::Spider;
    if (player->m_isSwing) return IconType::Swing;
    return IconType::Cube;
}

int getIconIdForType(IconType type) {
    auto gm = GameManager::sharedState();
    if (!gm) return 1;
    switch (type) {
        case IconType::Ship:   return gm->getPlayerShip();
        case IconType::Ball:   return gm->getPlayerBall();
        case IconType::Ufo:    return gm->getPlayerBird();
        case IconType::Wave:   return gm->getPlayerDart();
        case IconType::Robot:  return gm->getPlayerRobot();
        case IconType::Spider: return gm->getPlayerSpider();
        case IconType::Swing:  return gm->getPlayerSwing();
        default:               return gm->getPlayerFrame();
    }
}

void spawnGhostBot(PlayLayer* playLayer) {
    if (g_mirrorGhost || !playLayer->m_objectLayer) return;
    auto ghost = SimplePlayer::create(GameManager::sharedState()->getPlayerFrame());
    if (ghost) {
        ghost->setOpacity(130);
        ghost->setColor(cocos2d::ccColor3B{0, 255, 255});
        playLayer->m_objectLayer->addChild(ghost, 999);
        g_mirrorGhost = ghost;
        g_lastGhostType = IconType::Cube;
    }
}

// --- Persistence ---
void saveGhostData() {
    matjson::Value data = matjson::Array();
    for (const auto& frame : g_ghostTape) {
        matjson::Value obj = matjson::Object();
        obj["x"] = frame.position.x;
        obj["y"] = frame.position.y;
        obj["rot"] = frame.rotation;
        obj["type"] = (int)frame.iconType;
        obj["id"] = frame.iconID;
        data.push_back(obj);
    }
    Mod::get()->saveData("ghost_data.json", data);
}

void loadGhostData() {
    auto data = Mod::get()->getSavedData(); // Correct Geode call
    if (data.contains("ghost_data.json")) {
        g_ghostTape.clear();
        for (auto& item : data["ghost_data.json"].asArray()) {
            g_ghostTape.push_back({
                { (float)item["x"].asDouble(), (float)item["y"].asDouble() },
                (float)item["rot"].asDouble(),
                (IconType)item["type"].asInt(),
                item["id"].asInt()
            });
        }
    }
}

class $modify(GhostPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;
        
        g_liveFrameCounter = 0;
        g_mirrorGhost = nullptr;
        loadGhostData(); // Load on start

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot(this);
        }
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        g_liveFrameCounter = 0;
        g_lastGhostType = IconType::Cube;
        if (g_mirrorGhost) {
            g_mirrorGhost->updatePlayerFrame(GameManager::sharedState()->getPlayerFrame(), IconType::Cube);
            if (!g_ghostTape.empty()) {
                g_mirrorGhost->setPosition(g_ghostTape[0].position);
            }
        }
    }

    void onQuit() {
        saveGhostData(); // Save on exit
        PlayLayer::onQuit();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g_mirrorGhost && Mod::get()->getSettingValue<bool>("ghost-enabled")) spawnGhostBot(this);
        if (!g_mirrorGhost) return;

        if (this->m_isPracticeMode && this->m_player1) {
            g_ghostTape.push_back({this->m_player1->getPosition(), this->m_player1->getRotation(), getCurrentIconType(this->m_player1), getIconIdForType(getCurrentIconType(this->m_player1))});
        }
        else if (!this->m_isPracticeMode && !g_ghostTape.empty()) {
            size_t target = g_liveFrameCounter + 80;
            if (target < g_ghostTape.size()) {
                g_mirrorGhost->setVisible(true);
                g_mirrorGhost->setPosition(g_ghostTape[target].position);
                g_mirrorGhost->setRotation(g_ghostTape[target].rotation);
                if (g_ghostTape[target].iconType != g_lastGhostType) {
                    g_mirrorGhost->updatePlayerFrame(g_ghostTape[target].iconID, g_ghostTape[target].iconType);
                    g_lastGhostType = g_ghostTape[target].iconType;
                }
            } else g_mirrorGhost->setVisible(false);
            g_liveFrameCounter++;
        }
    }
};