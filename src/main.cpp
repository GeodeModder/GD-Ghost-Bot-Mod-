#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <vector>

using namespace geode::prelude;

// --- 1. ROBUST DATA STRUCTURES ---
// We record exactly what the player did (Input) AND where they were (State).
// This ensures we can recover even if a frame drops.
struct FrameAction {
    cocos2d::CCPoint position;
    float rotation;
    bool isHolding; // The actual "Macro" component
    IconType iconType;
    int iconID;
};

// --- 2. MANAGERS (Explicitly managed) ---
static inline std::vector<FrameAction> g_masterTape;
static inline size_t g_playbackIndex = 0;
static inline bool g_isRecording = false;
static inline bool g_hasFailed = false;

// We use a dedicated pointer for our ghost.
static inline SimplePlayer* g_ghostInstance = nullptr;

// --- 3. EXPLICIT RECORDING ENGINE ---
void recordFrame(PlayLayer* layer) {
    if (g_hasFailed || !layer->m_player1) return;

    auto player = layer->m_player1;
    
    // Determine current input state (Are we holding?)
    bool holding = player->m_isHolding; 
    
    // Create the snapshot
    FrameAction frame = {
        player->getPosition(),
        player->getRotation(),
        holding,
        // (Icon logic omitted for brevity, but can be added here)
        IconType::Cube, 
        GameManager::sharedState()->getPlayerFrame()
    };
    
    g_masterTape.push_back(frame);
}

// --- 4. THE FORTRESS HOOKS ---
class $modify(RobustPlayLayer, PlayLayer) {

    // --- A. THE DEATH TRAP (100% Reliability) ---
    // Instead of checking if we are dead, we wait for the game to tell us.
    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        g_hasFailed = true; 
        PlayLayer::destroyPlayer(p0, p1);
    }

    // --- B. THE INPUT CAPTURE ---
    // We hook the buttons. This is the core of a "Macro."
    void handleButton(bool down, int button, bool isPlayer1) {
        PlayLayer::handleButton(down, button, isPlayer1);
        // We ensure we only capture inputs if we are in the recording phase
        if (g_isRecording && isPlayer1) {
            // We could log the input here if we wanted a pure macro
        }
    }

    // --- C. INITIALIZATION ---
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;

        // Total reset of the system
        g_masterTape.clear();
        g_hasFailed = false;
        g_playbackIndex = 0;
        
        // Decide if we are recording or playing
        g_isRecording = !useReplay;

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            // Manual spawning of ghost
            auto gm = GameManager::sharedState();
            g_ghostInstance = SimplePlayer::create(gm->getPlayerFrame());
            g_ghostInstance->setOpacity(130);
            this->m_objectLayer->addChild(g_ghostInstance, 999);
        }

        return true;
    }

    // --- D. THE HEARTBEAT (Update Loop) ---
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!this->m_player1) return;

        if (g_isRecording) {
            // Keep writing to the tape
            recordFrame(this);
        } else {
            // Playback mode: Move the ghost to the frame
            if (g_playbackIndex < g_masterTape.size()) {
                auto& frame = g_masterTape[g_playbackIndex];
                if (g_ghostInstance) {
                    g_ghostInstance->setPosition(frame.position);
                    g_ghostInstance->setRotation(frame.rotation);
                    g_ghostInstance->setVisible(true);
                }
                g_playbackIndex++;
            } else {
                if (g_ghostInstance) g_ghostInstance->setVisible(false);
            }
        }
    }

    // --- E. RESET LOGIC ---
    void resetLevel() {
        PlayLayer::resetLevel();
        
        // On death/reset, we flush the current buffer if we were recording a "failed" run
        if (g_hasFailed) {
            g_masterTape.clear();
        }
        
        g_hasFailed = false;
        g_playbackIndex = 0;
    }
};
