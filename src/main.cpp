#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <filesystem>

using namespace geode::prelude;

// Structure to capture player state every frame
struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    bool isUpsideDown;
};

// Custom serialization for matjson (Geode v5 strict standard)
template <>
struct matjson::Serialize<GhostFrame> {
    static geode::Result<GhostFrame> fromJson(matjson::Value const& value) {
        GEODE_UNWRAP_INTO(double x, value["x"].asDouble());
        GEODE_UNWRAP_INTO(double y, value["y"].asDouble());
        GEODE_UNWRAP_INTO(double r, value["r"].asDouble());
        GEODE_UNWRAP_INTO(bool u, value["u"].asBool());

        return geode::Ok(GhostFrame {
            .position = cocos2d::CCPoint(static_cast<float>(x), static_cast<float>(y)),
            .rotation = static_cast<float>(r),
            .isUpsideDown = u
        });
    }

    static matjson::Value toJson(GhostFrame const& value) {
        auto obj = matjson::Value();
        obj["x"] = static_cast<double>(value.position.x);
        obj["y"] = static_cast<double>(value.position.y);
        obj["r"] = static_cast<double>(value.rotation);
        obj["u"] = value.isUpsideDown;
        return obj;
    }
};

// Helper function to resolve file paths for each ghost track
std::filesystem::path getGhostPath(int levelID, bool isCoinRun) {
    auto dir = geode::Mod::get()->getSaveDir();
    std::string filename = "ghost_" + std::to_string(levelID) + (isCoinRun ? "_coin.json" : "_normal.json");
    return dir / filename;
}

// PlayLayer Hooking & Ghost Logic Management
struct $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<GhostFrame> m_currentRecording;
        std::vector<GhostFrame> m_normalGhostTape;
        std::vector<GhostFrame> m_coinGhostTape;

        cocos2d::CCSprite* m_normalGhostSprite = nullptr;
        cocos2d::CCSprite* m_coinGhostSprite = nullptr;

        // Tracks the frame index AND the coin pickup status at the time a checkpoint is dropped
        std::map<CheckpointObject*, std::pair<size_t, bool>> m_practiceCheckpoints; 

        bool m_collectedCoinThisRun = false;
        int m_levelID = 0;
    };

    bool init(GJGameLevel* level, bool usePractice, bool isPlatformer) {
        if (!PlayLayer::init(level, usePractice, isPlatformer)) return false;

        m_fields->m_levelID = level->m_levelID;
        m_fields->m_collectedCoinThisRun = false;

        // 1. Load the Normal Ghost Track
        auto normalPath = getGhostPath(m_fields->m_levelID, false);
        if (std::filesystem::exists(normalPath)) {
            try {
                std::ifstream file(normalPath);
                std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                auto parseResult = matjson::parse(str);
                if (parseResult) {
                    auto tapeRes = parseResult.unwrap().as<std::vector<GhostFrame>>();
                    if (tapeRes) {
                        m_fields->m_normalGhostTape = tapeRes.unwrap();
                        
                        // Instantiate Normal Ghost Visuals (Cyan)
                        m_fields->m_normalGhostSprite = CCSprite::createWithSpriteFrameName("player_01_001.png");
                        if (m_fields->m_normalGhostSprite) {
                            m_fields->m_normalGhostSprite->setColor({0, 255, 255});
                            m_fields->m_normalGhostSprite->setOpacity(120);
                            m_fields->m_normalGhostSprite->setVisible(false);
                            if (m_objectLayer) {
                                m_objectLayer->addChild(m_fields->m_normalGhostSprite, 100);
                            }
                        }
                    }
                }
            } catch (...) {
                log::error("Failed to parse normal ghost tape data.");
            }
        }

        // 2. Load the Coin Ghost Track
        auto coinPath = getGhostPath(m_fields->m_levelID, true);
        if (std::filesystem::exists(coinPath)) {
            try {
                std::ifstream file(coinPath);
                std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                auto parseResult = matjson::parse(str);
                if (parseResult) {
                    auto tapeRes = parseResult.unwrap().as<std::vector<GhostFrame>>();
                    if (tapeRes) {
                        m_fields->m_coinGhostTape = tapeRes.unwrap();
                        
                        // Instantiate Coin Ghost Visuals (Gold/Yellow)
                        m_fields->m_coinGhostSprite = CCSprite::createWithSpriteFrameName("player_01_001.png");
                        if (m_fields->m_coinGhostSprite) {
                            m_fields->m_coinGhostSprite->setColor({255, 215, 0});
                            m_fields->m_coinGhostSprite->setOpacity(150);
                            m_fields->m_coinGhostSprite->setVisible(false);
                            if (m_objectLayer) {
                                m_objectLayer->addChild(m_fields->m_coinGhostSprite, 100);
                            }
                        }
                    }
                }
            } catch (...) {
                log::error("Failed to parse coin ghost tape data.");
            }
        }

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        // Don't record or manipulate visuals if the player object doesn't exist or is dead
        if (!m_player1 || m_player1->m_isDead) return;

        // Use the current size of the recording vector as our sync clock frame index
        size_t currentIndex = m_fields->m_currentRecording.size();

        // Record the player's real-time kinematics
        GhostFrame frame;
        frame.position = m_player1->getPosition();
        frame.rotation = m_player1->getRotation();
        frame.isUpsideDown = m_player1->m_isUpsideDown;
        m_fields->m_currentRecording.push_back(frame);

        // Handle Normal Ghost Playback
        if (m_fields->m_normalGhostSprite && currentIndex < m_fields->m_normalGhostTape.size()) {
            auto const& ghostFrame = m_fields->m_normalGhostTape[currentIndex];
            m_fields->m_normalGhostSprite->setPosition(ghostFrame.position);
            m_fields->m_normalGhostSprite->setRotation(ghostFrame.rotation);
            m_fields->m_normalGhostSprite->setScaleY(ghostFrame.isUpsideDown ? -1.0f : 1.0f);
            m_fields->m_normalGhostSprite->setVisible(true);
        } else if (m_fields->m_normalGhostSprite) {
            m_fields->m_normalGhostSprite->setVisible(false);
        }

        // Handle Coin Ghost Playback
        if (m_fields->m_coinGhostSprite && currentIndex < m_fields->m_coinGhostTape.size()) {
            auto const& ghostFrame = m_fields->m_coinGhostTape[currentIndex];
            m_fields->m_coinGhostSprite->setPosition(ghostFrame.position);
            m_fields->m_coinGhostSprite->setRotation(ghostFrame.rotation);
            m_fields->m_coinGhostSprite->setScaleY(ghostFrame.isUpsideDown ? -1.0f : 1.0f);
            m_fields->m_coinGhostSprite->setVisible(true);
        } else if (m_fields->m_coinGhostSprite) {
            m_fields->m_coinGhostSprite->setVisible(false);
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        // Clean up data for fresh non-practice runs or runs with no active checkpoints
        if (!m_isPracticeMode || m_fields->m_practiceCheckpoints.empty()) {
            m_fields->m_currentRecording.clear();
            m_fields->m_collectedCoinThisRun = false;
        }
    }

    void createCheckpoint() {
        PlayLayer::createCheckpoint();
        
        if (m_checkpointArray && m_checkpointArray->count() > 0) {
            auto checkpoint = static_cast<CheckpointObject*>(m_checkpointArray->lastObject());
            if (checkpoint) {
                size_t frameIndex = m_fields->m_currentRecording.size();
                // Take a snapshot of the tape index and coin state at this point
                m_fields->m_practiceCheckpoints[checkpoint] = { frameIndex, m_fields->m_collectedCoinThisRun };
            }
        }
    }

    void loadFromCheckpoint(CheckpointObject* object) {
        PlayLayer::loadFromCheckpoint(object);
        
        if (object && m_fields->m_practiceCheckpoints.contains(object)) {
            auto const& [frameIndex, coinFlag] = m_fields->m_practiceCheckpoints[object];
            
            // Splice out any corrupted frames recorded past this checkpoint
            if (frameIndex <= m_fields->m_currentRecording.size()) {
                m_fields->m_currentRecording.resize(frameIndex);
            }
            // Revert coin state back to what it was when checkpoint was saved
            m_fields->m_collectedCoinThisRun = coinFlag;
        }
    }

    void removeCheckpoint(bool first) {
        if (m_checkpointArray && m_checkpointArray->count() > 0) {
            auto checkpoint = static_cast<CheckpointObject*>(m_checkpointArray->lastObject());
            if (checkpoint) {
                m_fields->m_practiceCheckpoints.erase(checkpoint);
            }
        }
        PlayLayer::removeCheckpoint(first);
    }

    void destroyObject(GameObject* obj) {
        PlayLayer::destroyObject(obj);
        
        if (obj) {
            int type = static_cast<int>(obj->m_objectType);
            // 22 handles Normal Secret Coins, 37 handles Custom/User Coins
            if (type == 22 || type == 37) {
                m_fields->m_collectedCoinThisRun = true;
            }
        }
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        
        // Persist tape files locally upon a legitimate complete run
        if (!m_fields->m_currentRecording.empty()) {
            bool isCoinRun = m_fields->m_collectedCoinThisRun;
            auto path = getGhostPath(m_fields->m_levelID, isCoinRun);
            
            matjson::Value json = m_fields->m_currentRecording;
            std::ofstream file(path);
            file << json.dump();
        }
    }
};
