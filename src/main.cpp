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

        std::map<CheckpointObject*, std::pair<size_t, bool>> m_practiceCheckpoints; 

        bool m_collectedCoinThisRun = false;
        bool m_hasSavedThisRun = false;
        int m_levelID = 0;
    };

    bool init(GJGameLevel* level, bool usePractice, bool isPlatformer) {
        if (!PlayLayer::init(level, usePractice, isPlatformer)) return false;

        m_fields->m_levelID = level->m_levelID;
        m_fields->m_collectedCoinThisRun = false;
        m_fields->m_hasSavedThisRun = false;

        // Helper lambda to safely instantiate ghost visuals with a robust fallback system
        auto createGhostVisual = [&](cocos2d::ccColor3B color, GLubyte opacity) -> cocos2d::CCSprite* {
            cocos2d::CCSprite* sprite = cocos2d::CCSprite::createWithSpriteFrameName("player_square_01_001.png");
            if (!sprite) sprite = cocos2d::CCSprite::createWithSpriteFrameName("player_01_001.png");
            if (!sprite) sprite = cocos2d::CCSprite::createWithSpriteFrameName("checkpoint_01_001.png"); // Unfailing asset fallback
            
            if (sprite) {
                sprite->setColor(color);
                sprite->setOpacity(opacity);
                sprite->setVisible(false);
            }
            return sprite;
        };

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
                        m_fields->m_normalGhostSprite = createGhostVisual({0, 255, 255}, 130); // Cyan
                        if (m_fields->m_normalGhostSprite && m_objectLayer) {
                            m_objectLayer->addChild(m_fields->m_normalGhostSprite, 1000); // Higher Z-Order
                        }
                    }
                }
            } catch (...) {}
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
                        m_fields->m_coinGhostSprite = createGhostVisual({255, 215, 0}, 160); // Gold
                        if (m_fields->m_coinGhostSprite && m_objectLayer) {
                            m_objectLayer->addChild(m_fields->m_coinGhostSprite, 1000);
                        }
                    }
                }
            } catch (...) {}
        }

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (!m_player1 || m_player1->m_isDead) return;

        // CRITICAL UPDATE: Detects finish line cross instantly in BOTH Normal & Practice mode
        if (m_player1->getPositionX() >= m_levelLength && !m_fields->m_hasSavedThisRun) {
            m_fields->m_hasSavedThisRun = true;
            if (!m_fields->m_currentRecording.empty()) {
                // Practice runs can never collect coins, so force normal-route sorting
                bool isCoinRun = !m_isPracticeMode && m_fields->m_collectedCoinThisRun;
                auto path = getGhostPath(m_fields->m_levelID, isCoinRun);
                
                matjson::Value json = m_fields->m_currentRecording;
                std::ofstream file(path);
                file << json.dump();
            }
        }

        size_t currentIndex = m_fields->m_currentRecording.size();

        // Record player kinematics
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
        m_fields->m_hasSavedThisRun = false;
        
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
                m_fields->m_practiceCheckpoints[checkpoint] = { frameIndex, m_fields->m_collectedCoinThisRun };
            }
        }
    }

    void loadFromCheckpoint(CheckpointObject* object) {
        PlayLayer::loadFromCheckpoint(object);
        if (object && m_fields->m_practiceCheckpoints.contains(object)) {
            auto const& [frameIndex, coinFlag] = m_fields->m_practiceCheckpoints[object];
            if (frameIndex <= m_fields->m_currentRecording.size()) {
                m_fields->m_currentRecording.resize(frameIndex);
            }
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
            if (type == 22 || type == 37) {
                m_fields->m_collectedCoinThisRun = true;
            }
        }
    }
};
