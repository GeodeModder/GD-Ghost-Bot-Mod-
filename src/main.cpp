#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <matjson.hpp>
#include <vector>

using namespace geode::prelude;

// ==========================================
// 1. Core Data Structures
// ==========================================
struct GhostFrame {
    cocos2d::CCPoint position;
    float rotation;
    bool isUpsideDown;
};

// ==========================================
// 2. Geode v5 Matjson Serialization
// ==========================================
template <>
struct matjson::Serialize<GhostFrame> {
    static geode::Result<GhostFrame> fromJson(matjson::Value const& value) {
        // Use GEODE_UNWRAP_INTO to safely unwrap the v5 camelCase Result types
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

// ==========================================
// 3. PlayLayer Hook & Bot Logic
// ==========================================
class $modify(PlayLayer) {
    struct Fields {
        std::vector<GhostFrame> m_normalGhostTape;
        std::vector<GhostFrame> m_coinGhostTape;
        std::vector<GhostFrame> m_currentRecording;
    };

    // Example initialization or data loader function where lines 79 & 103 live
    bool init(GJGameLevel* level, bool useReplay, bool dontRunRemover) {
        if (!PlayLayer::init(level, useReplay, dontRunRemover)) return false;
        
        // Assuming you load your string from a file or server somewhere:
        std::string sampleJsonString = "[]"; 
        auto parseResult = matjson::parse(sampleJsonString);

        // Line 79 & 80 Fix: Check directly as boolean, then double unwrap the vector
        if (parseResult) {
            auto tapeRes = parseResult.unwrap().as<std::vector<GhostFrame>>();
            if (tapeRes) {
                m_fields->m_normalGhostTape = tapeRes.unwrap();
            }
        }

        // Line 103 & 104 Fix: Coin tape handling
        if (parseResult) {
            auto tapeRes = parseResult.unwrap().as<std::vector<GhostFrame>>();
            if (tapeRes) {
                m_fields->m_coinGhostTape = tapeRes.unwrap();
            }
        }

        return true;
    }

    // Line 127 Fix: Player update check using m_player1->m_isDead
    void update(float dt) {
        PlayLayer::update(dt);
        
        if (!m_player1 || m_player1->m_isDead) return;

        // Your frame recording or playback logic goes here...
    }

    // Line 186 Fix: Added CheckpointObject* signature and argument forwarding
    void loadFromCheckpoint(CheckpointObject* object) {
        PlayLayer::loadFromCheckpoint(object);
        
        // Your logic to synchronize ghost position back to checkpoint goes here...
    }

    // Line 210 Fix: Added bool first signature and argument forwarding
    void removeCheckpoint(bool first) {
        PlayLayer::removeCheckpoint(first);
        
        // Your logic to pop the latest ghost checkpoint goes here...
    }

    // Line 233 Fix: Works automatically now that matjson knows how to handle GhostFrame vectors!
    void saveRecording() {
        matjson::Value json = m_fields->m_currentRecording;
        
        // Your logic to save the stringified JSON back to a file goes here...
    }
};
