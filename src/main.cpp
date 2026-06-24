    // --- 1. INITIALIZATION (Smart Auto-Detect) ---
    bool init(GJGameLevel* level, bool useReplay, bool dontCheat) {
        if (!PlayLayer::init(level, useReplay, dontCheat)) return false;

        m_fields->m_ghostTape.clear();
        m_fields->m_playbackIndex = 0;
        m_fields->m_hasDied = false;
        
        // Load whatever tape is saved for this level
        this->loadGhostData(level->m_levelID);
        
        // LOGIC FIX: If we have data, PLAY IT. If we don't, RECORD a new one.
        if (!m_fields->m_ghostTape.empty()) {
            m_fields->m_isRecording = false;
            log::info("Ghost: Loaded {} frames. Mode: PLAYBACK", m_fields->m_ghostTape.size());
        } else {
            m_fields->m_isRecording = true;
            log::info("Ghost: No saved run found. Mode: RECORDING");
        }

        if (Mod::get()->getSettingValue<bool>("ghost-enabled")) {
            this->createGhostPlayer();
        }

        return true;
    }

    // --- 2. THE DEATH TRAP ---
    void destroyPlayer(PlayerObject* p0, GameObject* p1) {
        m_fields->m_hasDied = true;
        PlayLayer::destroyPlayer(p0, p1);
    }

    // --- 3. RESET LOGIC (No More Self-Erasing) ---
    void resetLevel() {
        PlayLayer::resetLevel();
        
        // BUG FIX: ONLY clear the tape if we died WHILE recording a new run.
        // If we are just playing back, leave the tape alone!
        if (m_fields->m_hasDied && m_fields->m_isRecording) {
            log::info("Ghost: Died during recording. Clearing corrupt tape.");
            m_fields->m_ghostTape.clear();
        }
        
        m_fields->m_hasDied = false;
        m_fields->m_playbackIndex = 0; // Rewind the ghost to the start
        
        if (m_fields->m_ghostPlayer) {
            m_fields->m_ghostPlayer->setVisible(!m_fields->m_isRecording && !m_fields->m_ghostTape.empty());
            m_fields->m_ghostPlayer->updatePlayerFrame(GameManager::sharedState()->getPlayerFrame(), IconType::Cube);
        }
    }

    // --- 4. THE HEARTBEAT ---
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!this->m_player1) return;

        // Recording Phase
        if (m_fields->m_isRecording && !m_fields->m_hasDied) {
            GhostFrame frame;
            frame.position = this->m_player1->getPosition();
            frame.rotation = this->m_player1->getRotation();
            frame.isHolding = m_fields->m_isHoldingInput;
            frame.iconID = GameManager::sharedState()->getPlayerFrame();
            frame.iconType = (int)IconType::Cube;
            m_fields->m_ghostTape.push_back(frame);
        }
        // Playback Phase
        else if (!m_fields->m_isRecording && !m_fields->m_ghostTape.empty()) {
            if (m_fields->m_playbackIndex < m_fields->m_ghostTape.size()) {
                auto& frame = m_fields->m_ghostTape[m_fields->m_playbackIndex];
                if (m_fields->m_ghostPlayer) {
                    m_fields->m_ghostPlayer->setVisible(true);
                    m_fields->m_ghostPlayer->setPosition(frame.position);
                    m_fields->m_ghostPlayer->setRotation(frame.rotation);
                    m_fields->m_ghostPlayer->setScale(this->m_player1->getScale());
                }
                m_fields->m_playbackIndex++;
            } else {
                if (m_fields->m_ghostPlayer) m_fields->m_ghostPlayer->setVisible(false);
            }
        }
    }
