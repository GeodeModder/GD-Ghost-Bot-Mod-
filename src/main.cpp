#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <vector>

using namespace geode::prelude;

// --- Advanced Spatial Data Struct ---
struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    IconType iconType;
    int iconID;
};

// --- Global Memory Tape Reels ---
static inline std::vector<GhostFrame> g_ghostTape;
static inline std::vector<size_t> g_checkpointTapeMarks;
static inline size_t g_liveFrameCounter = 0;
static inline SimplePlayer* g_mirrorGhost = nullptr;
static inline GJGameLevel* g_recordedLevel = nullptr;
static inline IconType g_lastGhostType = IconType::Cube;

// Tuned for your POCO X7 Pro's 120Hz display
constexpr size_t OFFSET_FRAMES = 80; 


    Mod::get()->setSavedValue("ghost_tape", data);
}

// --- Persistence Logic ---
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
    Mod::get()->setSavedValue("ghost_tape", data);
}

void loadGhostData() {
    auto data = Mod::get()->getSavedValue<matjson::Value>("ghost_tape");
    if (data.isArray()) {
        g_ghostTape.clear();
        // We use .unwrap() on the array and the individual values to get the raw data
        for (auto& item : data.asArray().unwrap()) {
            g_ghostTape.push_back({
                { (float)item["x"].asDouble().unwrap(), (float)item["y"].asDouble().unwrap() },
                (float)item["rot"].asDouble().unwrap(),
                (IconType)item["type"].asInt().unwrap(),
                item["id"].asInt().unwrap()
            });
        }
    }
}

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

    auto gm = GameManager::sharedState();
    int defaultCubeID = gm ? gm->getPlayerFrame() : 1;

    auto ghost = SimplePlayer::create(defaultCubeID); 
    if (ghost) {
        ghost->setOpacity(130);
        ghost->setColor(cocos2d::ccColor3B{0, 255, 255});
        
        playLayer->m_objectLayer->addChild(ghost, 999);
        g_mirrorGhost = ghost;
        g_lastGhostType = IconType::Cube;
    }
}

// --- Hook Implementations ---
class $modify(GhostPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;
        
        g_liveFrameCounter = 0;
        g_mirrorGhost = nullptr;
        
        if (g_recordedLevel != level) {
            g_ghostTape.clear();
            g_checkpointTapeMarks.clear();
            g_recordedLevel = level;
            loadGhostData(); // Added: Load on level init
        }

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot(this);
        }

        return true;
    }

    // Added: Save when leaving the level
    void onQuit() {
        saveGhostData();
        PlayLayer::onQuit();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        // Reset clock and force ghost to revert to Cube mode on death
        g_liveFrameCounter = 0;
        g_lastGhostType = IconType::Cube; 

        if (g_mirrorGhost) {
            g_mirrorGhost->setVisible(true);
            
            // Force the ghost to re-render as a cube immediately
            auto gm = GameManager::sharedState();
            int cubeID = gm ? gm->getPlayerFrame() : 1;
            g_mirrorGhost->updatePlayerFrame(cubeID, IconType::Cube);
            
            if (!g_ghostTape.empty()) {
                g_mirrorGhost->setPosition(g_ghostTape[0].position);
                g_mirrorGhost->setRotation(g_ghostTape[0].rotation);
            }
        }
    }
    
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        
        if (!g_mirrorGhost && Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot(this);
        }

        if (!g_mirrorGhost) return;

        if (this->m_isPracticeMode && this->m_player1) {
            IconType currentType = getCurrentIconType(this->m_player1);
            int currentID = getIconIdForType(currentType);

            g_ghostTape.push_back({
                this->m_player1->getPosition(),
                this->m_player1->getRotation(),
                currentType,
                currentID
            });
        }
        else if (!this->m_isPracticeMode && !g_ghostTape.empty()) {
            size_t ghostTargetFrame = g_liveFrameCounter + OFFSET_FRAMES;
            
            if (ghostTargetFrame < g_ghostTape.size()) {
                g_mirrorGhost->setVisible(true);
                GhostFrame targetData = g_ghostTape[ghostTargetFrame];
                
                g_mirrorGhost->setPosition(targetData.position);
                g_mirrorGhost->setRotation(targetData.rotation);

                if (targetData.iconType != g_lastGhostType) {
                    g_mirrorGhost->updatePlayerFrame(targetData.iconID, targetData.iconType);
                    
                    if (targetData.iconType == IconType::Robot) g_mirrorGhost->createRobotSprite(targetData.iconID);
                    if (targetData.iconType == IconType::Spider) g_mirrorGhost->createSpiderSprite(targetData.iconID);
                    
                    g_lastGhostType = targetData.iconType;
                }
            } else {
                g_mirrorGhost->setVisible(false);
            }
            g_liveFrameCounter++;
        }
    }

    void storeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::storeCheckpoint(checkpoint);
        if (this->m_isPracticeMode) {
            g_checkpointTapeMarks.push_back(g_ghostTape.size());
        }
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        if (this->m_isPracticeMode && !g_checkpointTapeMarks.empty()) {
            size_t rollbackFrame = g_checkpointTapeMarks.back();
            if (rollbackFrame <= g_ghostTape.size()) {
                g_ghostTape.resize(rollbackFrame);
            }
        }
    }

    void removeCheckpoint(bool p0) {
        PlayLayer::removeCheckpoint(p0);
        if (this->m_isPracticeMode && !g_checkpointTapeMarks.empty()) {
            g_checkpointTapeMarks.pop_back();
        }
    }
};