#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

using namespace geode::prelude;

// ============================================
// 1. СТРУКТУРЫ ДАННЫХ
// ============================================

struct MacroEvent {
    float time;
    bool player1;
    bool player2;
    bool press;
};

struct FembotMacro {
    std::string name;
    std::string path;
    std::vector<MacroEvent> events;
    bool isLoaded = false;
};

// ============================================
// 2. МЕНЕДЖЕР МАКРОСОВ
// ============================================

class MacroManager {
private:
    static MacroManager* instance;
    FembotMacro m_currentMacro;
    size_t m_nextEventIndex = 0;
    float m_startTime = 0;
    bool m_isPlaying = false;
    std::vector<FembotMacro> m_macroList;
    std::string m_macrosFolder;
    int m_currentMacroIndex = -1;
    
    MacroManager() {
        m_macrosFolder = Mod::get()->getResourcesDir().parent_path().string() + "/macros/";
        
        #ifdef _WIN32
        CreateDirectoryA(m_macrosFolder.c_str(), NULL);
        #endif
        
        scanMacrosFolder();
        loadTestMacro();
        
        if (!m_macroList.empty()) {
            m_currentMacroIndex = 0;
            m_currentMacro = m_macroList[0];
        }
    }
    
    void scanMacrosFolder() {
        m_macroList.clear();
        
        #ifdef _WIN32
        std::string searchPath = m_macrosFolder + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE findHandle = FindFirstFileA(searchPath.c_str(), &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fileName = findData.cFileName;
                    FembotMacro macro;
                    macro.name = fileName;
                    macro.path = m_macrosFolder + "\\" + fileName;
                    macro.isLoaded = false;
                    m_macroList.push_back(macro);
                }
            } while (FindNextFileA(findHandle, &findData) != 0);
            FindClose(findHandle);
        }
        #endif
    }
    
    void loadTestMacro() {
        FembotMacro testMacro;
        testMacro.name = "Test Macro (built-in)";
        testMacro.path = "";
        testMacro.isLoaded = true;
        
        for (int i = 0; i < 20; i++) {
            MacroEvent ev;
            ev.time = i * 0.3f;
            ev.player1 = true;
            ev.player2 = false;
            ev.press = (i % 2 == 0);
            testMacro.events.push_back(ev);
        }
        
        m_macroList.insert(m_macroList.begin(), testMacro);
    }
    
    bool parseXDMacro(FembotMacro& macro) {
        std::ifstream file(macro.path);
        if (!file.is_open()) return false;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            std::istringstream iss(line);
            float time;
            int p1, p2, press;
            
            if (iss >> time >> p1 >> p2 >> press) {
                MacroEvent ev;
                ev.time = time;
                ev.player1 = p1 == 1;
                ev.player2 = p2 == 1;
                ev.press = press == 1;
                macro.events.push_back(ev);
            }
        }
        
        file.close();
        macro.isLoaded = !macro.events.empty();
        return macro.isLoaded;
    }
    
public:
    static MacroManager* get() {
        if (!instance) instance = new MacroManager();
        return instance;
    }
    
    std::vector<FembotMacro>& getMacroList() { return m_macroList; }
    std::string getMacrosFolder() { return m_macrosFolder; }
    int getCurrentMacroIndex() { return m_currentMacroIndex; }
    std::string getCurrentMacroName() { 
        if (m_currentMacroIndex >= 0 && m_currentMacroIndex < m_macroList.size()) {
            return m_macroList[m_currentMacroIndex].name;
        }
        return "None";
    }
    
    void refreshMacros() {
        m_macroList.clear();
        loadTestMacro();
        scanMacrosFolder();
        if (!m_macroList.empty()) {
            m_currentMacroIndex = 0;
            m_currentMacro = m_macroList[0];
        }
    }
    
    bool loadMacroFromFile(FembotMacro& macro) {
        std::string ext;
        size_t dotPos = macro.path.find_last_of(".");
        if (dotPos != std::string::npos) {
            ext = macro.path.substr(dotPos);
        }
        
        if (ext == ".xd") {
            return parseXDMacro(macro);
        }
        
        if (ext == ".gdr" || ext == ".gdm" || ext == ".json" || ext == ".xml" || ext == ".txt") {
            std::ifstream file(macro.path);
            if (file.is_open()) {
                macro.isLoaded = true;
                file.close();
                return true;
            }
        }
        
        return false;
    }
    
    bool loadMacroByName(const std::string& name) {
        for (size_t i = 0; i < m_macroList.size(); i++) {
            if (m_macroList[i].name == name) {
                if (!m_macroList[i].isLoaded && !m_macroList[i].path.empty()) {
                    loadMacroFromFile(m_macroList[i]);
                }
                m_currentMacroIndex = i;
                m_currentMacro = m_macroList[i];
                return m_macroList[i].isLoaded;
            }
        }
        return false;
    }
    
    void nextMacro() {
        if (m_macroList.empty()) {
            auto alert = FLAlertLayer::create("Fembot", "💔 No macros available!", "OK");
            alert->show();
            return;
        }
        
        if (m_isPlaying) stop();
        
        m_currentMacroIndex++;
        if (m_currentMacroIndex >= m_macroList.size()) {
            m_currentMacroIndex = 0;
        }
        
        auto& macro = m_macroList[m_currentMacroIndex];
        if (!macro.isLoaded && !macro.path.empty()) {
            loadMacroFromFile(macro);
        }
        m_currentMacro = macro;
        
        auto alert = FLAlertLayer::create("Fembot", ("🌸 " + macro.name).c_str(), "OK");
        alert->show();
    }
    
    void previousMacro() {
        if (m_macroList.empty()) {
            auto alert = FLAlertLayer::create("Fembot", "💔 No macros available!", "OK");
            alert->show();
            return;
        }
        
        if (m_isPlaying) stop();
        
        m_currentMacroIndex--;
        if (m_currentMacroIndex < 0) {
            m_currentMacroIndex = m_macroList.size() - 1;
        }
        
        auto& macro = m_macroList[m_currentMacroIndex];
        if (!macro.isLoaded && !macro.path.empty()) {
            loadMacroFromFile(macro);
        }
        m_currentMacro = macro;
        
        auto alert = FLAlertLayer::create("Fembot", ("🌸 " + macro.name).c_str(), "OK");
        alert->show();
    }
    
    void play() {
        if (!m_currentMacro.isLoaded || m_currentMacro.events.empty()) {
            auto alert = FLAlertLayer::create("Fembot", "💔 No macro loaded!", "OK");
            alert->show();
            return;
        }
        
        auto playLayer = PlayLayer::get();
        if (!playLayer) {
            auto alert = FLAlertLayer::create("Fembot", "💔 Not in a level!", "OK");
            alert->show();
            return;
        }
        
        m_nextEventIndex = 0;
        m_startTime = playLayer->m_time;
        m_isPlaying = true;
    }
    
    void stop() {
        m_isPlaying = false;
        m_nextEventIndex = 0;
    }
    
    void update(float currentTime) {
        if (!m_isPlaying || !m_currentMacro.isLoaded) return;
        
        float elapsed = currentTime - m_startTime;
        auto* playLayer = PlayLayer::get();
        if (!playLayer) {
            stop();
            return;
        }
        
        while (m_nextEventIndex < m_currentMacro.events.size()) {
            auto& event = m_currentMacro.events[m_nextEventIndex];
            if (event.time > elapsed) break;
            
            if (event.player1) {
                playLayer->pushButton(PlayerButton::Jump, event.press);
            }
            
            m_nextEventIndex++;
        }
        
        if (m_nextEventIndex >= m_currentMacro.events.size()) {
            stop();
            auto alert = FLAlertLayer::create("Fembot", "✅ Macro finished!", "OK");
            alert->show();
        }
    }
    
    bool isPlaying() const { return m_isPlaying; }
    FembotMacro& getCurrentMacro() { return m_currentMacro; }
};

MacroManager* MacroManager::instance = nullptr;

// ============================================
// 3. ХУК НА PLAYLAYER
// ============================================

class $modify(MyPlayLayer, PlayLayer) {
    void update(float delta) {
        PlayLayer::update(delta);
        MacroManager::get()->update(this->m_time);
    }
    
    void onExit() {
        MacroManager::get()->stop();
        PlayLayer::onExit();
    }
};

// ============================================
// 4. ГЛАВНОЕ ОКНО
// ============================================

class FembotPopup : public FLAlertLayer {
protected:
    CCMenuItemSpriteExtra* m_playBtn = nullptr;
    CCMenuItemSpriteExtra* m_stopBtn = nullptr;
    CCMenuItemSpriteExtra* m_refreshBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    CCLabelBMFont* m_statusLabel = nullptr;
    CCLabelBMFont* m_currentMacroLabel = nullptr;
    std::vector<CCMenuItemToggler*> m_macroButtons;
    int m_selectedIndex = -1;
    
    bool init() {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        this->setContentSize(CCSize(480, 400));
        this->setPosition(winSize / 2);
        
        auto bg = CCSprite::create("GJ_square01.png");
        bg->setScaleX(480 / bg->getContentWidth());
        bg->setScaleY(400 / bg->getContentHeight());
        bg->setColor(ccc3(255, 182, 193));
        bg->setOpacity(230);
        bg->setPosition(this->getContentSize() / 2);
        this->addChild(bg);
        
        auto border = CCSprite::create("GJ_square02.png");
        border->setScaleX(480 / border->getContentWidth());
        border->setScaleY(400 / border->getContentHeight());
        border->setColor(ccc3(255, 105, 180));
        border->setPosition(this->getContentSize() / 2);
        this->addChild(border);
        
        auto title = CCLabelBMFont::create("🌸 Fembot 🌸", "bigFont.fnt");
        title->setColor(ccc3(255, 20, 147));
        title->setScale(0.7f);
        title->setPosition(240, 365);
        this->addChild(title);
        
        auto menu = CCMenu::create();
        menu->setPosition(0, 0);
        this->addChild(menu);
        
        std::string currentName = MacroManager::get()->getCurrentMacroName();
        m_currentMacroLabel = CCLabelBMFont::create(("Current: " + currentName).c_str(), "bigFont.fnt");
        m_currentMacroLabel->setColor(ccc3(255, 20, 147));
        m_currentMacroLabel->setScale(0.4f);
        m_currentMacroLabel->setPosition(240, 335);
        this->addChild(m_currentMacroLabel);
        
        auto prevSpr = ButtonSprite::create("◀", "bigFont.fnt", "GJ_button_01.png", 0.5f);
        prevSpr->setColor(ccc3(200, 100, 180));
        m_prevBtn = CCMenuItemSpriteExtra::create(
            prevSpr, this, menu_selector(FembotPopup::onPrevMacro)
        );
        m_prevBtn->setPosition(140, 335);
        menu->addChild(m_prevBtn);
        
        auto nextSpr = ButtonSprite::create("▶", "bigFont.fnt", "GJ_button_01.png", 0.5f);
        nextSpr->setColor(ccc3(200, 100, 180));
        m_nextBtn = CCMenuItemSpriteExtra::create(
            nextSpr, this, menu_selector(FembotPopup::onNextMacro)
        );
        m_nextBtn->setPosition(340, 335);
        menu->addChild(m_nextBtn);
        
        auto listBg = CCSprite::create("GJ_square01.png");
        listBg->setScaleX(440 / listBg->getContentWidth());
        listBg->setScaleY(190 / listBg->getContentHeight());
        listBg->setColor(ccc3(255, 240, 245));
        listBg->setOpacity(200);
        listBg->setPosition(240, 185);
        this->addChild(listBg);
        
        auto listTitle = CCLabelBMFont::create("📁 Macros", "bigFont.fnt");
        listTitle->setColor(ccc3(200, 50, 150));
        listTitle->setScale(0.5f);
        listTitle->setPosition(240, 275);
        this->addChild(listTitle);
        
        auto& macros = MacroManager::get()->getMacroList();
        float yPos = 260;
        int currentIndex = MacroManager::get()->getCurrentMacroIndex();
        
        for (size_t i = 0; i < macros.size() && i < 8; i++) {
            auto& macro = macros[i];
            
            auto btn = CCMenuItemToggler::create(
                CCSprite::createWithSpriteFrameName("GJ_checkBox_001.png"),
                CCSprite::createWithSpriteFrameName("GJ_checkBox_002.png"),
                this,
                menu_selector(FembotPopup::onMacroSelect)
            );
            btn->setTag(i);
            btn->setPosition(35, yPos);
            
            if (i == currentIndex) {
                btn->toggle(true);
            }
            
            menu->addChild(btn);
            m_macroButtons.push_back(btn);
            
            std::string displayName = macro.name;
            if (i == currentIndex) {
                displayName = "👉 " + displayName;
            }
            
            auto nameLabel = CCLabelBMFont::create(displayName.c_str(), "bigFont.fnt");
            nameLabel->setColor(i == currentIndex ? ccc3(255, 20, 147) : ccc3(100, 50, 100));
            nameLabel->setScale(0.35f);
            nameLabel->setAnchorPoint(ccp(0, 0.5f));
            nameLabel->setPosition(55, yPos);
            this->addChild(nameLabel);
            
            auto statusLabel = CCLabelBMFont::create(
                macro.isLoaded ? "✅" : "❌",
                "bigFont.fnt"
            );
            statusLabel->setScale(0.5f);
            statusLabel->setPosition(440, yPos);
            this->addChild(statusLabel);
            
            yPos -= 25;
        }
        
        if (macros.empty()) {
            auto noMacros = CCLabelBMFont::create(
                ("💔 No macros found!\nPlace .xd files in:\n" + MacroManager::get()->getMacrosFolder()).c_str(),
                "bigFont.fnt"
            );
            noMacros->setColor(ccc3(200, 50, 150));
            noMacros->setScale(0.3f);
            noMacros->setPosition(240, 185);
            this->addChild(noMacros);
        }
        
        auto playSpr = ButtonSprite::create("▶ Play", "bigFont.fnt", "GJ_button_01.png", 0.5f);
        playSpr->setColor(ccc3(255, 105, 180));
        m_playBtn = CCMenuItemSpriteExtra::create(
            playSpr, this, menu_selector(FembotPopup::onPlay)
        );
        m_playBtn->setPosition(170, 60);
        menu->addChild(m_playBtn);
        
        auto stopSpr = ButtonSprite::create("⏹ Stop", "bigFont.fnt", "GJ_button_01.png", 0.5f);
        stopSpr->setColor(ccc3(255, 20, 147));
        m_stopBtn = CCMenuItemSpriteExtra::create(
            stopSpr, this, menu_selector(FembotPopup::onStop)
        );
        m_stopBtn->setPosition(310, 60);
        menu->addChild(m_stopBtn);
        
        auto refreshSpr = ButtonSprite::create("🔄 Refresh", "bigFont.fnt", "GJ_button_01.png", 0.4f);
        refreshSpr->setColor(ccc3(200, 100, 180));
        m_refreshBtn = CCMenuItemSpriteExtra::create(
            refreshSpr, this, menu_selector(FembotPopup::onRefresh)
        );
        m_refreshBtn->setPosition(240, 25);
        menu->addChild(m_refreshBtn);
        
        m_statusLabel = CCLabelBMFont::create("💕 Ready", "bigFont.fnt");
        m_statusLabel->setColor(ccc3(255, 20, 147));
        m_statusLabel->setScale(0.4f);
        m_statusLabel->setPosition(240, 105);
        this->addChild(m_statusLabel);
        
        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        closeSpr->setColor(ccc3(255, 105, 180));
        auto closeBtn = CCMenuItemSpriteExtra::create(
            closeSpr, this, menu_selector(FembotPopup::onClose)
        );
        closeBtn->setPosition(460, 380);
        menu->addChild(closeBtn);
        
        return true;
    }
    
public:
    static FembotPopup* create() {
        auto ret = new FembotPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
    
    void onMacroSelect(CCObject* sender) {
        auto btn = static_cast<CCMenuItemToggler*>(sender);
        int index = btn->getTag();
        m_selectedIndex = index;
        
        auto& macros = MacroManager::get()->getMacroList();
        if (index >= 0 && index < macros.size()) {
            auto& macro = macros[index];
            
            if (!macro.isLoaded) {
                if (MacroManager::get()->loadMacroByName(macro.name)) {
                    m_statusLabel->setString(("🌸 " + macro.name).c_str());
                } else {
                    m_statusLabel->setString(("❌ Failed: " + macro.name).c_str());
                }
            } else {
                m_statusLabel->setString(("🌸 " + macro.name).c_str());
            }
            
            MacroManager::get()->loadMacroByName(macro.name);
            
            for (auto& b : m_macroButtons) {
                b->toggle(false);
            }
            btn->toggle(true);
            
            this->onRefresh(nullptr);
        }
    }
    
    void onPlay(CCObject*) {
        MacroManager::get()->play();
        this->onClose(nullptr);
    }
    
    void onStop(CCObject*) {
        MacroManager::get()->stop();
        m_statusLabel->setString("⏸ Stopped");
    }
    
    void onRefresh(CCObject*) {
        MacroManager::get()->refreshMacros();
        this->onClose(nullptr);
        FembotPopup::create()->show();
    }
    
    void onNextMacro(CCObject*) {
        MacroManager::get()->nextMacro();
        this->onRefresh(nullptr);
    }
    
    void onPrevMacro(CCObject*) {
        MacroManager::get()->previousMacro();
        this->onRefresh(nullptr);
    }
};

// ============================================
// 5. КЛАВИША END
// ============================================

class $modify(MyKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isRepeat) {
        if (key == KEY_End && down && !isRepeat) {
            FembotPopup::create()->show();
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isRepeat);
    }
};

// ============================================
// 6. ЗАГРУЗКА
// ============================================

$on_mod(Loaded) {
    log::info("🌸 Fembot loaded!");
}