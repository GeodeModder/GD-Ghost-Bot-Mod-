#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <vector>

using namespace geode::prelude;

// --- Data Structures ---
struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    bool isShip;
    bool isBall;
    bool isBird;
    bool isDart;
    bool isRobot;
    bool isSpider;
    bool isSwing;
};

// --- Global State Management ---
static inline std::vector<GhostFrame> g_goldenMacro; 
static inline std::vector<size_t> g_checkpointTapeMarks; 
static inline PlayerObject* g_mirrorGhost = nullptr;
static inline size_t g_liveFrameCounter = 0;
static inline GJGameLevel* g_recordedLevel = nullptr;

// 120 FPS Optimization: ~80 frames ahead is roughly 0.66 seconds.
static const size_t OFFSET_FRAMES = 80; 

// --- Forward Declarations of Helpers ---
void spawnGhostBot(PlayLayer* playLayer);
void syncGhostState(PlayerObject* ghost, const GhostFrame& frame);

// --- 1. THE INPUT WALL ---
class $modify(GhostPlayerObject, PlayerObject) {
    void pushButton(PlayerButton btn) {
        if (this == g_mirrorGhost) return; // Ignore live clicks completely
        PlayerObject::pushButton(btn);
    }

    void releaseButton(PlayerButton btn) {
        if (this == g_mirrorGhost) return; // Ignore live releases completely
        PlayerObject::releaseButton(btn);
    }
    // playerMoveX deleted here to satisfy the compiler bindings!
};

// --- 2. THE PERFORMANCE MIRROR CORE ENGINE ---
class $modify(MirrorLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;
        
        g_mirrorGhost = nullptr;
        g_liveFrameCounter = 0;

        // If switching to a completely different level, wipe the macro library clean
        if (g_recordedLevel != level) {
            g_goldenMacro.clear();
            g_checkpointTapeMarks.clear();
            g_recordedLevel = level;
        }

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot(this);
        }

        return true;
    }

    void resetLevel() {
        // Clear old ghost instance safely before resetting the engine
        if (g_mirrorGhost) {
            g_mirrorGhost->removeFromParent();
            g_mirrorGhost = nullptr;
        }

        PlayLayer::resetLevel();
        
        // Reset our playback clock back to the start line
        g_liveFrameCounter = 0; 

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot(this);
        }
    }

    // --- PRACTICE MODE CHECKPOINT SLICING ---
    
    // Fixed: Added CheckpointObject* parameter to match the original Geode signature
    void storeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::storeCheckpoint(checkpoint);
        if (this->m_isPracticeMode) {
            g_checkpointTapeMarks.push_back(g_goldenMacro.size());
        }
    }

    // Chop off the garbage frames when you crash and go back
    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        
        if (this->m_isPracticeMode && !g_checkpointTapeMarks.empty()) {
            size_t safeTapeSize = g_checkpointTapeMarks.back();
            g_goldenMacro.resize(safeTapeSize); 
        }
    }

    // Handle manual checkpoint deletion
    void removeCheckpoint(bool p0) {
        PlayLayer::removeCheckpoint(p0);
        if (this->m_isPracticeMode && !g_checkpointTapeMarks.empty()) {
            g_checkpointTapeMarks.pop_back();
        }
    }

    // --- EVERY-FRAME PROCESSING ---
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (this->m_isPaused || !this->m_player1) return;

        // MODE A: RECORDING THE TRACKS (Practice Mode)
        if (this->m_isPracticeMode) {
            GhostFrame currentFrame;
            currentFrame.position = this->m_player1->getPosition();
            currentFrame.rotation = this->m_player1->getRotation();
            
            // Capture all physical game mode states
            currentFrame.isShip = this->m_player1->m_isShip;
            currentFrame.isBall = this->m_player1->m_isBall;
            currentFrame.isBird = this->m_player1->m_isBird;
            currentFrame.isDart = this->m_player1->m_isDart;
            currentFrame.isRobot = this->m_player1->m_isRobot;
            currentFrame.isSpider = this->m_player1->m_isSpider;
            currentFrame.isSwing = this->m_player1->m_isSwing;

            g_goldenMacro.push_back(currentFrame);
        }
        
        // MODE B: TIME-TRAVEL PLAYBACK (Normal Mode / Testing your Run)
        else if (!this->m_isPracticeMode && !g_goldenMacro.empty() && g_mirrorGhost) {
            size_t ghostTargetFrame = g_liveFrameCounter + OFFSET_FRAMES;

            if (ghostTargetFrame < g_goldenMacro.size()) {
                GhostFrame futureFrame = g_goldenMacro[ghostTargetFrame];
                
                // Directly control the puppet strings
                g_mirrorGhost->setPosition(futureFrame.position);
                g_mirrorGhost->setRotation(futureFrame.rotation);
                
                // Keep gamemode visuals aligned
                syncGhostState(g_mirrorGhost, futureFrame);
                g_mirrorGhost->setVisible(true);
            } else {
                g_mirrorGhost->setVisible(false);
            }

            g_liveFrameCounter++;
        }
    }

    void onExit() {
        PlayLayer::onExit();
        g_mirrorGhost = nullptr; 
    }
};

// --- 3. GLOBAL HELPER FUNCTIONS ---
// Fixed: Extracted out of Fields to live happily in global file scope
void spawnGhostBot(PlayLayer* playLayer) {
    if (!g_mirrorGhost && playLayer->m_objectLayer && playLayer->m_player1) {
        auto ghost = PlayerObject::create(1, 2, playLayer, nullptr, false);
        if (ghost) {
            g_mirrorGhost = ghost;
            
            ghost->setPosition(playLayer->m_player1->getPosition());
            ghost->setScale(playLayer->m_player1->getScale()); 
            
            ghost->setOpacity(120);
            ghost->setColor({0, 255, 255}); 
            ghost->setVisible(false); 

            if (ghost->m_regularTrail) ghost->m_regularTrail->setVisible(false);
            if (ghost->m_shipStreak) ghost->m_shipStreak->setVisible(false);
            if (ghost->m_waveTrail) ghost->m_waveTrail->setVisible(false);

            playLayer->addChild(ghost, 999); 
        }
    }
}

void syncGhostState(PlayerObject* ghost, const GhostFrame& frame) {
    if (ghost->m_isShip != frame.isShip) ghost->toggleFlyMode(frame.isShip, true);
    if (ghost->m_isBall != frame.isBall) ghost->toggleRollMode(frame.isBall, true);
    if (ghost->m_isBird != frame.isBird) ghost->toggleBirdMode(frame.isBird, true);
    if (ghost->m_isDart != frame.isDart) ghost->toggleDartMode(frame.isDart, true);
    if (ghost->m_isRobot != frame.isRobot) ghost->toggleRobotMode(frame.isRobot, true);
    if (ghost->m_isSpider != frame.isSpider) ghost->toggleSpiderMode(frame.isSpider, true);
    if (ghost->m_isSwing != frame.isSwing) ghost->toggleSwingMode(frame.isSwing, true);
}