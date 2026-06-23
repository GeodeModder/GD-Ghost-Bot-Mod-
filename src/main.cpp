#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Global tracking pointer for our ghost bot
static inline PlayerObject* g_ghostBot = nullptr;

class $modify(GhostPlayerObject, PlayerObject) {
    // 1. FREEZE HORIZONTAL MOVEMENT: Stops the engine from giving it auto-speed
    void playerMoveX(float amt) {
        if (this == g_ghostBot) return;
        PlayerObject::playerMoveX(amt);
    }

    // 2. PARALYZE NATIVE PHYSICS: Stops it from executing independent gravity/velocity
    void update(float dt) {
        if (this == g_ghostBot) return;
        PlayerObject::update(dt);
    }

    // 3. BLOCK CLICKS: Stops it from jumping when you tap the screen
    void pushButton(PlayerButton btn) {
        if (this == g_ghostBot) return;
        PlayerObject::pushButton(btn);
    }

    void releaseButton(PlayerButton btn) {
        if (this == g_ghostBot) return;
        PlayerObject::releaseButton(btn);
    }
};

class $modify(GhostBotLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;
        g_ghostBot = nullptr;

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot();
        }
        return true;
    }

    void onExit() {
        PlayLayer::onExit();
        g_ghostBot = nullptr;
    }

    void resetLevel() {
        if (g_ghostBot) {
            g_ghostBot->removeFromParent();
            g_ghostBot = nullptr;
        }

        PlayLayer::resetLevel();

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            spawnGhostBot();
            if (g_ghostBot && this->m_player1) {
                g_ghostBot->setPosition(this->m_player1->getPosition());
            }
        }
    }

    void destroyPlayer(PlayerObject* p1, GameObject* p2) {
        if (g_ghostBot && p1 == g_ghostBot) return; 
        PlayLayer::destroyPlayer(p1, p2);
    }

    void spawnGhostBot() {
        if (!g_ghostBot && this->m_objectLayer) {
            // FIXED: Passing nullptr for the 4th argument isolates it from the engine's automated physics loops
            auto ghost = PlayerObject::create(1, 2, nullptr, nullptr, false);
            if (ghost) {
                g_ghostBot = ghost;
                
                ghost->unscheduleUpdate();
                
                // Hide all native trail arrays completely
                if (ghost->m_regularTrail) ghost->m_regularTrail->setVisible(false);
                if (ghost->m_shipStreak) ghost->m_shipStreak->setVisible(false);
                if (ghost->m_waveTrail) ghost->m_waveTrail->setVisible(false);
                
                ghost->setScale(this->m_player1->getScale()); 
                ghost->setOpacity(130);
                ghost->setColor({0, 255, 255}); 
                ghost->setVisible(true);

                // Manually add it to the scene layer so it renders visually
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
            if (!g_ghostBot) spawnGhostBot();

            auto ghost = g_ghostBot;
            auto player = this->m_player1;

            if (ghost && player) {
                ghost->setVisible(true);
                syncGhostGamemode(ghost, player);

                if (ghost->m_regularTrail) ghost->m_regularTrail->setVisible(false);
                if (ghost->m_shipStreak) ghost->m_shipStreak->setVisible(false);
                if (ghost->m_waveTrail) ghost->m_waveTrail->setVisible(false);

                float currentX = player->getPositionX();
                float currentY = player->getPositionY();
                
                // POSITION CONTROL:
                // Changing "+ 60.0f" to "+ 0.0f" makes it overlap exactly on top of you like a standard replay ghost.
                ghost->setPosition({currentX + 60.0f, currentY}); 
                ghost->setRotation(player->getRotation()); 
            }
        } else {
            if (g_ghostBot) {
                g_ghostBot->setVisible(false);
            }
        }
    }
};
