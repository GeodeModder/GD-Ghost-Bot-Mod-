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
        // Absolute teardown of the old node to stop the zombie clone glitch at (0,0)
        if (m_fields->m_ghostBot) {
            m_fields->m_ghostBot->removeFromParent();
            m_fields->m_ghostBot = nullptr;
        }

        PlayLayer::resetLevel();

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot();
            if (m_fields->m_ghostBot && this->m_player1) {
                m_fields->m_ghostBot->setPosition(this->m_player1->getPosition());
            }
        }
    }

    // Immortal Hook: Bypasses standard hazard destruction routines
    void destroyPlayer(PlayerObject* p1, GameObject* p2) {
        if (m_fields->m_ghostBot && p1 == m_fields->m_ghostBot) return; 
        PlayLayer::destroyPlayer(p1, p2);
    }

    void spawnGhostBot() {
        // Pass nullptr as the gameLayer argument to prevent the engine from registering 
        // the ghost into PlayLayer's internal input/update array!
        if (!m_fields->m_ghostBot && this->m_objectLayer) {
            auto ghost = PlayerObject::create(1, 2, nullptr, this->m_objectLayer, true);
            if (ghost) {
                m_fields->m_ghostBot = ghost;
                
                ghost->unscheduleUpdate();
                
                // Visual setup
                ghost->setScale(this->m_player1->getScale()); 
                ghost->setOpacity(130);
                ghost->setColor({0, 255, 255}); 
                ghost->setVisible(true);

                this->addChild(ghost, 999); 
                syncGhostGamemode(ghost, this->m_player1);
            }
        }
    }

    void syncGhostGamemode(PlayerObject* ghost, PlayerObject* player) {
        if (!ghost || !player) return;
        
        if (ghost->m_isShip != player->m_isShip) ghost->toggleFlyMode(player->m_isShip, true);
        if (ghost->m_isBall != player->m_isBall) ghost->toggleRollMode(player->m_isBall, true);
        if (ghost->m_isBird != player->m_isBird) ghost->toggleBirdMode(player->m_isBird, true);
        if (ghost->m_isDart != player->m_isDart) ghost->toggleDartMode(player->m_isDart, true);
        if (ghost->m_isRobot != player->m_isRobot) ghost->toggleRobotMode(player->m_isRobot, true);
        if (ghost->m_isSpider != player->m_isSpider) ghost->toggleSpiderMode(player->m_isSpider, true);
        if (ghost->m_isSwing != player->m_isSwing) ghost->toggleSwingMode(player->m_isSwing, true);
        
        ghost->setScale(player->getScale());
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (this->m_isPaused) return;

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            if (!m_fields->m_ghostBot) spawnGhostBot();

            auto ghost = m_fields->m_ghostBot;
            auto player = this->m_player1;

            if (ghost && player) {
                ghost->setVisible(true);
                syncGhostGamemode(ghost, player);

                // ZERO OUT ENGINE VELOCITY: Paralyzes internal physics computation loops
                // Even if the engine tries to update it, it physically cannot move on its own.
                ghost->m_p1 = 0;
                ghost->m_p2 = 0;

                // Force layout translation match
                float currentX = player->getPositionX();
                float currentY = player->getPositionY();
                
                ghost->setPosition({currentX + 60.0f, currentY}); 
                ghost->setRotation(player->getRotation()); 
                
                // Manually handle visual sprite animations only
                ghost->update(dt);
            }
        } else {
            if (m_fields->m_ghostBot) {
                m_fields->m_ghostBot->setVisible(false);
            }
        }
    }
};
