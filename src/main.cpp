#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/binding/GameManager.hpp>
#include <vector>

using namespace geode::prelude;

/**
 * --- STRUCTURE: GhostFrame ---
 * A granular definition of a single moment in time.
 */
struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    bool isHolding;
    int iconID;
    int iconType;
};

/**
 * --- CLASS: PlayLayer (The Fortress Architecture) ---
 * We are moving all ghost logic INTO the PlayLayer so the 
 * memory lifecycle matches the game's actual lifecycle.
 */
class $modify(RobustGhostLayer, PlayLayer) {
    
    // --- INSTANCE VARIABLES ---
    // These belong to the specific level instance. When the level is destroyed, 
    // these are cleaned up automatically by the game.
    std::vector<GhostFrame> m_ghostTape;
    size_t m_playbackIndex = 0;
    bool m_isRecording = false;
    bool m_hasDied = false;
    bool m_isHoldingInput = false;
    SimplePlayer* m_ghostPlayer = nullptr;

    // --- A. THE DEATH TRAP ---
    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        m_hasDied = true;
        log::debug("Death event triggered. Stopping recording.");
        PlayLayer::destroyPlayer(p0, p1);
    }

    // --- B. THE INPUT CAPTURE ---
    void handleButton(bool down, int button, bool isPlayer1) {
        if (isPlayer1) {
            m_isHoldingInput = down;
        }
        PlayLayer::handleButton(down, button, isPlayer1);
    }

    // --- C. INITIALIZATION ---
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;

        // Reset state for this specific instance
        m_ghostTape.clear();
        m_playbackIndex = 0;
        m_hasDied = false;
        m_isRecording = !useReplay;
        
        // Load data from persistent storage
        loadFromStorage(level->m_levelID);

        // Spawn Ghost Player
        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhost();
        }

        return true;
    }

    // --- D. GHOST SPAWNER ---
    void spawnGhost() {
        if (m_ghostPlayer) return;

        auto gm = GameManager::sharedState();
        m_ghostPlayer = SimplePlayer::create(gm->getPlayerFrame());
        
        if (m_ghostPlayer) {
            m_ghostPlayer->setOpacity(130);
            m_ghostPlayer->setColor(cocos2d::ccColor3B{0, 255, 255});
            m_ghostPlayer->setVisible(false); // Hide until playback starts
            this->m_objectLayer->addChild(m_ghostPlayer, 999);
            log::debug("Ghost player spawned.");
        }
    }

    // --- E. PERSISTENCE LAYER ---
    void saveToStorage(int levelID) {
        std::vector<matjson::Value> arr;
        for (const auto& frame : m_ghostTape) {
            arr.push_back(matjson::makeObject({
                {"x", frame.position.x},
                {"y", frame.position.y},
                {"rot", frame.rotation},
                {"hold", frame.isHolding},
                {"id", frame.iconID}
            }));
        }
        std::string key = "ghost_tape_" + std::to_string(levelID);
        Mod::get()->setSavedValue(key, matjson::Value(arr));
        log::debug("Saved {} frames to disk.", m_ghostTape.size());
    }

    void loadFromStorage(int levelID) {
        std::string key = "ghost_tape_" + std::to_string(levelID);
        auto data = Mod::get()->getSavedValue<matjson::Value>(key);
        
        m_ghostTape.clear();
        if (data.isArray()) {
            for (auto& item : data.asArray().unwrap()) {
                m_ghostTape.push_back({
                    {(float)item["x"].asDouble().unwrap(), (float)item["y"].asDouble().unwrap()},
                    (float)item["rot"].asDouble().unwrap(),
                    item["hold"].asBool().unwrap(),
                    (int)item["id"].asInt().unwrap(),
                    (int)IconType::Cube // Defaulting
                });
            }
            log::debug("Loaded {} frames from disk.", m_ghostTape.size());
        }
    }

    // --- F. THE HEARTBEAT ---
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!this->m_player1) return;

        // Recording Logic
        if (m_isRecording && !m_hasDied) {
            m_ghostTape.push_back({
                this->m_player1->getPosition(),
                this->m_player1->getRotation(),
                m_isHoldingInput,
                GameManager::sharedState()->getPlayerFrame(),
                (int)IconType::Cube
            });
        }
        // Playback Logic
        else if (!m_isRecording && !m_ghostTape.empty()) {
            if (m_playbackIndex < m_ghostTape.size()) {
                auto& frame = m_ghostTape[m_playbackIndex];
                
                if (m_ghostPlayer) {
                    m_ghostPlayer->setVisible(true);
                    m_ghostPlayer->setPosition(frame.position);
                    m_ghostPlayer->setRotation(frame.rotation);
                    m_ghostPlayer->setScale(this->m_player1->getScale());
                }
                m_playbackIndex++;
            } else {
                if (m_ghostPlayer) m_ghostPlayer->setVisible(false);
            }
        }
    }

    // --- G. RESET LOGIC ---
    void resetLevel() {
        PlayLayer::resetLevel();
        
        if (m_hasDied) {
            m_ghostTape.clear();
            log::debug("Death occurred. Tape flushed.");
        }
        
        m_hasDied = false;
        m_playbackIndex = 0;
        
        if (m_ghostPlayer) {
            m_ghostPlayer->setVisible(true);
            m_ghostPlayer->updatePlayerFrame(GameManager::sharedState()->getPlayerFrame(), IconType::Cube);
        }
    }

    void onExit() {
        if (m_isRecording && !m_hasDied) {
            saveToStorage(this->m_level->m_levelID);
        }
        PlayLayer::onExit();
    }
};

/**
 * --- CHECKPOINT HANDLING ---
 */
class $modify(CheckpointHandler, PlayLayer) {
    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        // Ensure death flag is cleared on respawn
        static_cast<RobustGhostLayer*>(this)->m_hasDied = false;
    }
};
