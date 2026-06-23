#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(GhostBotLayer, PlayLayer) {
    struct Fields {
        PlayerObject* m_ghostBot = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;

        m_fields->m_ghostBot = nullptr;
        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot();
        }
        return true;
    }

    void onExit() {
        PlayLayer::onExit();
        m_fields->m_ghostBot = nullptr;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot();
            if (m_fields->m_ghostBot && this->m_player1) {
                m_fields->m_ghostBot->setPosition(this->m_player1->getPosition());
                m_fields->m_ghostBot->setVisible(true);
            }
        }
    }

    // Immortal Hook: The ghost ignores death
    void destroyPlayer(PlayerObject* p1, GameObject* p2) {
        if (p1 == m_fields->m_ghostBot) return; 
        PlayLayer::destroyPlayer(p1, p2);
    }

    void spawnGhostBot() {
        if (!m_fields->m_ghostBot) {
            // FIX 1: Changed 5th argument to 'false' so it isn't shackled by Player 2 dual physics
            auto ghost = PlayerObject::create(1, 2, this, this->m_objectLayer, false);
            if (ghost) {
                m_fields->m_ghostBot = ghost;
                ghost->setScale(this->m_player1->getScale()); 
                ghost->setOpacity(130);
                ghost->setColor({0, 255, 255});
                ghost->setVisible(true);
                ghost->setEnableCollisions(true); 
                this->addChild(ghost, 999); 
            }
        }
    }

    void update(float dt) {
        PlayLayer::update(dt);
        if (this->m_isPaused || !Mod::get()->getSettingValue<bool>("ghost-enabled")) return;

        if (!m_fields->m_ghostBot) spawnGhostBot();

        if (m_fields->m_ghostBot && this->m_player1) {
            m_fields->m_ghostBot->setVisible(true);
            
            if (m_fields->m_ghostBot->m_isShip != this->m_player1->m_isShip) {
                m_fields->m_ghostBot->toggleFlyMode(this->m_player1->m_isShip);
            }

            // FIX 2: Call update() BEFORE setting the position.
            // This lets the engine do its internal math first, then we forcefully override it.
            m_fields->m_ghostBot->update(dt);

            // Now force the scout 60 units ahead cleanly
            float currentX = this->m_player1->getPositionX();
            float currentY = this->m_player1->getPositionY();
            
            m_fields->m_ghostBot->setPosition({currentX + 60.0f, currentY}); 
            m_fields->m_ghostBot->setRotation(this->m_player1->getRotation()); 
        }
    }
};
