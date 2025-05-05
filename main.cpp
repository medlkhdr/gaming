#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>

// Game states
enum GameState { MENU, APROPOS, LOADING, GAME, HIT, FINISH };

//
// Helper: Resets the player sprite position based on the lane.
//
void resetPlayer(sf::Sprite &player, const sf::Texture &roadTexture, int playerLane,
                 float padLeft, float padRight, int LANES, const sf::RenderWindow &window)
{
    float rw = static_cast<float>(roadTexture.getSize().x);
    float roadLeft = (window.getSize().x - rw) / 2.f;
    float leftPad = padLeft * rw;
    float inner = rw - (padLeft + padRight) * rw;
    float laneW = inner / LANES;
    float pw = player.getGlobalBounds().width;
    float playerTargetX = roadLeft + leftPad + laneW * (playerLane + 0.5f) - pw / 2.f;
    float py = window.getSize().y - player.getGlobalBounds().height - 10.f;
    player.setPosition(playerTargetX, py);
}

//
// Helper: Spawns a bottle if it does not overlap obstacles or coins.
// Adjust the bottle's size with setScale below.
//
void spawnBottle(std::vector<sf::Sprite>& bottles, const sf::Texture &bottleTex,
                 float roadTextureWidth, float padLeft, float padRight, int LANES,
                 sf::RenderWindow &window, const std::vector<sf::Sprite>& obstacles,
                 const std::vector<sf::Sprite>& coins)
{
    if (std::rand() % 1000 < 5)
    {
        int lane = std::rand() % LANES;
        sf::Sprite b(bottleTex);
        b.setScale(0.23f, 0.23f);  // Adjust bottle size as needed.
        float rw = roadTextureWidth;
        float roadL = (window.getSize().x - rw) / 2.f;
        float lp = padLeft * rw;
        float inner = rw - (padLeft + padRight) * rw;
        float laneW = inner / LANES;
        float bx = roadL + lp + laneW * (lane + 0.5f) - b.getGlobalBounds().width / 2.f;
        float by = -b.getGlobalBounds().height - (std::rand() % 100);
        b.setPosition(bx, by);
        for (const auto &existingBottle : bottles) {
            if (std::abs(existingBottle.getPosition().y - b.getPosition().y) < 100.f)
                return;
        }
        for (const auto &obs : obstacles) {
            if (b.getGlobalBounds().intersects(obs.getGlobalBounds()))
                return;
        }
        for (const auto &coin : coins) {
            if (b.getGlobalBounds().intersects(coin.getGlobalBounds()))
                return;
        }
        bottles.push_back(b);
    }
}

//
// Helper: Spawns a coin collectible (score item) if it does not overlap obstacles, bottles, or coins.
// The coin's size is controlled here with c.setScale(0.16f, 0.16f).
//
void spawnScoreCoin(std::vector<sf::Sprite>& coins, const sf::Texture &coinTex,
                    float roadTextureWidth, float padLeft, float padRight, int LANES,
                    sf::RenderWindow &window, const std::vector<sf::Sprite>& obstacles,
                    const std::vector<sf::Sprite>& bottles) {
    if (std::rand() % 1000 < 4) {
        int lane = std::rand() % LANES;
        sf::Sprite c(coinTex);
        c.setScale(0.16f, 0.16f);  // Control the coin's size here.
        float rw = roadTextureWidth;
        float roadL = (window.getSize().x - rw) / 2.f;
        float lp = padLeft * rw;
        float inner = rw - (padLeft + padRight) * rw;
        float laneW = inner / LANES;
        float cx = roadL + lp + laneW * (lane + 0.5f) - c.getGlobalBounds().width / 2.f;
        float cy = -c.getGlobalBounds().height - (std::rand() % 150);
        c.setPosition(cx, cy);
        for (const auto &existingCoin : coins) {
            if (std::abs(existingCoin.getPosition().y - c.getPosition().y) < 100.f)
                return;
        }
        for (const auto &obs : obstacles) {
            if (c.getGlobalBounds().intersects(obs.getGlobalBounds()))
                return;
        }
        for (const auto &bottle : bottles) {
            if (c.getGlobalBounds().intersects(bottle.getGlobalBounds()))
                return;
        }
        coins.push_back(c);
    }
}

//
// Helper: Updates bottles movement and handles player collection.
// When collected, increases stamina and plays the drink sound.
void updateBottles(std::vector<sf::Sprite>& bottles, float worldSpeed, sf::RenderWindow &window,
                   sf::Sprite &player, float &stamina, const float MAX_STAMINA,
                   const float BOTTLE_STAMINA, sf::Sound &drinkSound) {
    for (auto it = bottles.begin(); it != bottles.end(); ) {
        it->move(0, worldSpeed);
        if (player.getGlobalBounds().intersects(it->getGlobalBounds())) {
            drinkSound.play();
            stamina = std::min(MAX_STAMINA, stamina + BOTTLE_STAMINA);
            it = bottles.erase(it);
        } else if (it->getPosition().y > window.getSize().y) {
            it = bottles.erase(it);
        } else {
            window.draw(*it);
            ++it;
        }
    }
}

//
// Helper: Updates coin collectibles movement and handles player collection.
// When collected, increases the score by 100 and plays a coin sound.
void updateCoins(std::vector<sf::Sprite>& coins, float worldSpeed, sf::RenderWindow &window,
                 sf::Sprite &player, int &score, sf::Sound &coinSound) {
    for (auto it = coins.begin(); it != coins.end(); ) {
        it->move(0, worldSpeed);
        if (player.getGlobalBounds().intersects(it->getGlobalBounds())) {
            coinSound.play();
            score += 100;
            it = coins.erase(it);
        } else if (it->getPosition().y > window.getSize().y) {
            it = coins.erase(it);
        } else {
            window.draw(*it);
            ++it;
        }
    }
}

//
// Main function with game loop and helper functions.
//
int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode(800, 600), "Bike Game", sf::Style::Default);
    window.setFramerateLimit(60);
    
    GameState gameState = MENU;

    // — ASSETS —
    sf::Font font;
    if (!font.loadFromFile("resources/fonts/Pixelite.ttf")) {
        std::cerr << "Failed to load font\n";
        return -1;
    }
    
    sf::Texture finishLineTexture;
    if (!finishLineTexture.loadFromFile("resources/images/finish.png")) {
        std::cerr << "Failed to load finish line texture\n";
        // Gérer l'erreur ici
    }

    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile("resources/images/bgmenu.jpg")) {
        std::cerr << "Failed to load bgmenu.jpg\n";
        return -1;
    }
    sf::Sprite bgSprite(bgTexture);
    
    sf::Music bgMusic;
    if (bgMusic.openFromFile("resources/audios/bgmenu.ogg")) {
        bgMusic.setLoop(true);
        bgMusic.setVolume(25.f);
        bgMusic.play();
    }
    
    sf::SoundBuffer clickBuf, crashBuf, drinkBuf, coinBuf, finishBuf, tiredBuf;
    if (!clickBuf.loadFromFile("resources/audios/click.wav") ||
        !crashBuf.loadFromFile("resources/audios/crash.wav") ||
        !drinkBuf.loadFromFile("resources/audios/drink.wav") ||
        !coinBuf.loadFromFile("resources/audios/coin.wav") ||
        !tiredBuf.loadFromFile("resources/audios/tired.wav") ||
        !finishBuf.loadFromFile("resources/audios/finish.wav"))
    {
        std::cerr << "Failed to load one or more sound files\n";
        return -1;
    }

    sf::Sound clickSound(clickBuf), crashSound(crashBuf), drinkSound(drinkBuf), coinSound(coinBuf), finishSound(finishBuf), tiredSound(tiredBuf);
    
    sf::Texture roadTexture;
    if (!roadTexture.loadFromFile("resources/images/road.png")) {
        std::cerr << "Failed to load road texture\n";
        return -1;
    }
    // --- MOD 2 - ADD THESE LINES ---
    sf::Sprite finishLine;          // Separate finish line sprite
    const float TILE_H       = static_cast<float>(roadTexture.getSize().y);
    const int   NUM_TILES    = 10;
    const float RACE_DISTANCE = TILE_H * NUM_TILES;
    bool finishLineSpawned = false; // Control finish line appearance
    bool raceFinished = false;      // Prevent duplicate triggers
    float FINISH_SPAWN_AT = RACE_DISTANCE - 500.f; // Adjust as needed
    bool finishTriggered = false;
    sf::Clock finishTriggerClock;
    // how many seconds to wait after crossing the line before showing “FÉLICITATIONS!”
    const float FINISH_DELAY = 2.0f;



    // — MENU TEXTS —
    std::string labels[4] = {"Jouer", "A propos", "Quitter", "RETOUR"};
    sf::Text menu[4], shadow[4];
    for (int i = 0; i < 4; ++i) {
        menu[i].setFont(font);
        menu[i].setString(labels[i]);
        menu[i].setCharacterSize(32);
        menu[i].setFillColor(sf::Color::White);
        shadow[i] = menu[i];
        shadow[i].setFillColor(sf::Color::Black);
    }
    int selected = 0;

    sf::Text staminaLabel;
    staminaLabel.setFont(font);
    staminaLabel.setCharacterSize(24);
    staminaLabel.setFillColor(sf::Color::White);
    
    // Put each letter on its own line:
    staminaLabel.setString("S\nT\nA\nM\nI\nN\nA");
    // Optional: adjust line spacing if needed
    staminaLabel.setLineSpacing(1.0f);
    

    sf::Text positionLabel;
    positionLabel.setFont(font);
    positionLabel.setCharacterSize(24);
    positionLabel.setFillColor(sf::Color::White);
    positionLabel.setString("VOTRE POSITION :");

    // after loading font…
    sf::Text finishTitle("", font, 64);
    finishTitle.setFillColor(sf::Color::Yellow);

    sf::Text finishScore("", font, 32);
    finishScore.setFillColor(sf::Color::White);

    sf::Text returnBtn("RETOUR AU MENU", font, 28);
    returnBtn.setFillColor(sf::Color::White);

    
    // — “A PROPOS” SCROLLING TEXT —
    std::vector<std::vector<std::string>> aproposTexts = {{
        "Bienvenue dans notre projet de mini-jeu de velo",
        "Realise par Mahmoud Moukouch & Mohamed Lakhdar",
        "Encadre par Professeur Rachida Hannane",
        "Dans notre filiere IAPS4 a l'Universite FSSM Marrakech",
        "Ce jeu est conçu pour offrir une experience immersive",
        "Avec des graphismes futuristes et un gameplay dynamique",
        "Le but est de collecter des objets tout en evitant des obstacles",
        "- Collecte de bouteilles pour gagner des points",
        "- Evitez les autres velos sur la route",
        "- Profitez de l'adrenaline d'une course a grande vitesse",
        "- Compteur de score pour suivre vos progres",
        "- Limite de temps pour rendre le defi encore plus excitant",
        "Nous esperons que vous apprecierez ce jeu innovant!",
        "Merci de jouer et bonne chance!"
    }};
    sf::Text aproposText("", font, 28), aproposShadow("", font, 28);
    aproposText.setFillColor(sf::Color::White);
    aproposShadow.setFillColor(sf::Color::Black);
    size_t currentTextIndex = 0;
    
    // — CLOCKS —
    sf::Clock clock;               // For pulsing alpha
    sf::Clock aproposScrollClock;  // For "A Propos" scrolling
    sf::Clock loadingClock;        // For loading screen
    sf::Clock fadeClock;           // For hit blink effect
    sf::Clock deltaClock;          // For frame-rate independent dt
    
    // — GAME ASSETS & STATE —
    sf::Texture playerTexture, grassTexture;
    std::vector<sf::Texture> treeTextures(5), eplayerTextures(5);
    sf::Texture bottleTex, coinTex;
    bool assetsLoaded = false;
    
    std::vector<sf::Sprite> roadTiles;
    std::vector<sf::Sprite> trees;
    std::vector<sf::Sprite> obstacles;
    std::vector<sf::Sprite> bottles;
    std::vector<sf::Sprite> coins;
    
    sf::Sprite player;
    int lives = 3, score = 0;

    // Variables pour la course
    
    
    // — STAMINA & COLLECTIBLE PARAMETERS —
    const float MAX_STAMINA = 5.f;
    float stamina = MAX_STAMINA;
    const float STAMINA_DRAIN = 3.f;
    const float STAMINA_REGEN = 0.5f;
    const float BOTTLE_STAMINA = 1.f;
    const float MIN_STAMINA_TO_BOOST = 0.5f;    // you can tweak this (0.5 stamina units)

    // — MOVEMENT & SPEED PARAMETERS —
    const float DEFAULT_SPEED = 4.f, MAX_SPEED = 12.f;
    const float ACCEL = 0.2f, BRAKE_FORCE = 0.5f;
    // Separate speeds: one for your world and one for obstacles.
    float playerWorldSpeed = DEFAULT_SPEED;  
    const float obstacleSpeed = DEFAULT_SPEED;
    
    // — LANE & ROAD GEOMETRY —
    const float padLeft = 0.15f, padRight = 0.15f;
    const int LANES = 4;
    int playerLane = 1;
    float grassOffset = 0.f;
    int roadTileCount = 0;  // how many road sprites to cover the window
    float tileH = 0.f;  // will be set once roadTexture is loaded

    auto rebuildRoad = [&](sf::RenderWindow &win, std::vector<sf::Sprite> &roadTiles,const sf::Texture &roadTexture,int &roadTileCount,float &tileH)
    {
    // compute strip height
    tileH = static_cast<float>(roadTexture.getSize().y);
    float winH = static_cast<float>(win.getSize().y);
    roadTileCount = static_cast<int>(std::ceil(winH / tileH)) + 1;
    float startY = winH - roadTileCount * tileH;
    
    roadTiles.clear();
    for (int i = 0; i < roadTileCount; ++i) {
    sf::Sprite tile(roadTexture);
    tile.setPosition(0.f, startY + i * tileH);
    roadTiles.push_back(tile);
    }
    };

    if (!assetsLoaded)
    {
        // — Load & prepare textures —
        roadTexture.loadFromFile("resources/images/road.png");
        playerTexture.loadFromFile("resources/images/player.png");
        grassTexture.loadFromFile("resources/images/grass.png");
        grassTexture.setRepeated(true);
        roadTexture.setRepeated(true);
    
        // — LOAD ENVIRONMENT SPRITES (trees) —
        for (int i = 1; i < 5; ++i)
        {
            std::string treePath = "resources/images/trees/tree" + std::to_string(i) + ".png";
            if (!treeTextures[i].loadFromFile(treePath))
            {
                std::cerr << "Failed to load " << treePath << "\n";
                return -1;
            }
        }
    
        // — LOAD OBSTACLES (eplayers) —
        for (int i = 0; i < 5; ++i) {
            std::string eplPath = "resources/images/obstacles/eplayer" + std::to_string(i+1) + ".png";
            if (!eplayerTextures[i].loadFromFile(eplPath))
            {
                std::cerr << "Failed to load " << eplPath << "\n";
                return -1;
            }
        }
    
        // — LOAD COLLECTIBLES (bottle and score coins) —
        if (!bottleTex.loadFromFile("resources/images/coins/bottle.png"))
        {
            std::cerr << "Failed to load resources/images/coins/bottle.png\n";
            return -1;
        }
        if (!coinTex.loadFromFile("resources/images/coins/score.png"))
        {
            std::cerr << "Failed to load resources/images/coins/score.png\n";
            return -1;
        }
    
        // — Compute tile height & count AFTER loading —
        tileH = static_cast<float>(roadTexture.getSize().y);
        float winH  = static_cast<float>(window.getSize().y);
        roadTileCount = static_cast<int>(std::ceil(winH / tileH)) + 1;
    
        // — Stack the road sprites so bottom is covered immediately —
        float startY = winH - roadTileCount * tileH;
        roadTiles.clear();
        for (int i = 0; i < roadTileCount; ++i)
        {
            sf::Sprite tile(roadTexture);
            tile.setPosition(0.f, startY + i * tileH);
            roadTiles.push_back(tile);
        }
    
        // — Prepare finish line sprite —
        float scale = float(roadTexture.getSize().x) / finishLineTexture.getSize().x;
        finishLine.setTexture(finishLineTexture);
        finishLine.setScale(scale, scale);
        finishLine.setPosition(0, -NUM_TILES * TILE_H);
    
        // — Player setup, etc. —
        player.setTexture(playerTexture);
        player.setScale(0.25f, 0.25f);
    
        assetsLoaded = true;
    }
    
    // — GAME LOOP —
    float       distanceTraveled = 0.f;                         // your progress counter


    sf::Sprite playerShadow(player);
    playerShadow.setScale(0.20f, 0.20f);
    playerShadow.setColor(sf::Color(0, 0, 0, 150));
    resetPlayer(player, roadTexture, playerLane, padLeft, padRight, LANES, window);
    
    while (window.isOpen())
    {
        float dt = deltaClock.restart().asSeconds();
        // — inside your main loop —  
        sf::Event ev;
        while (window.pollEvent(ev))
        {
            // 1) Handle window close
            if (ev.type == sf::Event::Closed)
            {    window.close();
            // 2) Handle resize
            }
            else if (ev.type == sf::Event::Resized) {
        // adjust view to new window size
        sf::FloatRect visibleArea(0, 0, ev.size.width, ev.size.height);
        window.setView(sf::View(visibleArea));

        // reposition the player in its lane
        resetPlayer(player, roadTexture, playerLane, padLeft, padRight, LANES, window);

        // rebuild the vertical stack of road tiles
        rebuildRoad(window, roadTiles, roadTexture, roadTileCount, tileH);
    }
    // 3) Keyboard input
    else if (ev.type == sf::Event::KeyPressed) {
        if (gameState == MENU) {
            if (ev.key.code == sf::Keyboard::Up)
                selected = (selected + 2) % 3;
            else if (ev.key.code == sf::Keyboard::Down)
                selected = (selected + 1) % 3;
            else if (ev.key.code == sf::Keyboard::Enter) {
                clickSound.play();
                if (selected == 0) {
                    // Reset everything so the race truly restarts
                    finishLineSpawned = false;
                    raceFinished      = false;
                    finishTriggered   = false;
                    // Optionally restart the clock now or rely on the next trigger:
                    // finishTriggerClock.restart();
        
                    gameState = LOADING;
                    loadingClock.restart();
                }
                else if (selected == 1) {
                    gameState = APROPOS;
                    aproposScrollClock.restart();
                    currentTextIndex = 0;
                }
                else if (selected == 2) {
                    window.close();
                }
            }
        }
        
        else if (gameState == APROPOS) {
            if (ev.key.code == sf::Keyboard::Enter) {
                clickSound.play();
                gameState = MENU;
                selected = 0;
            }
        }
        else if (gameState == GAME || gameState == HIT) {
            if ((ev.key.code == sf::Keyboard::A || ev.key.code == sf::Keyboard::Left) && playerLane > 0) {
                playerLane--;
                resetPlayer(player, roadTexture, playerLane, padLeft, padRight, LANES, window);
            }
            else if ((ev.key.code == sf::Keyboard::D || ev.key.code == sf::Keyboard::Right) && playerLane < LANES - 1) {
                playerLane++;
                resetPlayer(player, roadTexture, playerLane, padLeft, padRight, LANES, window);
            }
        }
    }
    // you can add other event types (mouse clicks, etc.) here as else if …
} // End of event polling

    
        // Clear the window at the beginning of each frame.
        window.clear();
    
        // RESCALE the background each frame.
        {
            sf::FloatRect bgBounds = bgSprite.getLocalBounds();
            float scaleX = window.getSize().x / bgBounds.width;
            float scaleY = window.getSize().y / bgBounds.height;
            float scale = std::max(scaleX, scaleY);
            bgSprite.setScale(scale, scale);
        }
    
        // Get a pulsating alpha value for menus.
        float time = clock.getElapsedTime().asSeconds();
        int alpha = static_cast<int>(127.5f * (std::sin(time * 2 * 3.1415f) + 1));
    
        // --- STATE HANDLING ---
    
        // LOADING STATE:
        if (gameState == LOADING) {
            // Display a simple loading screen.
            sf::RectangleShape blk({ static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y) });
            blk.setFillColor(sf::Color::Black);
            window.draw(blk);
            sf::Text txt("Chargement en cours...", font, 30);
            txt.setFillColor(sf::Color(255, 255, 0, alpha));
            txt.setPosition(window.getSize().x/2.f - txt.getGlobalBounds().width/2.f,
                            window.getSize().y/2.f);
            window.draw(txt);
            window.display();
            float lt = loadingClock.getElapsedTime().asSeconds();
            if (lt > 3.f) {
                // Reset game state variables.
                lives = 3;
                score = 0;
                finishLineSpawned = false;
                raceFinished = false;
                finishTriggered = false;
                trees.clear();
                obstacles.clear();
                bottles.clear();
                coins.clear();
                playerLane = 1;
                playerWorldSpeed = DEFAULT_SPEED;
                stamina = MAX_STAMINA;
                resetPlayer(player, roadTexture, playerLane, padLeft, padRight, LANES, window);
                fadeClock.restart();
                gameState = GAME;
            }
            continue; // Skip the rest of the frame.
        }
        
        // MENU STATE:
        if (gameState == MENU) {
            // Draw the background.
            window.draw(bgSprite);
            // Render the 3 menu items.
            float centerX = window.getSize().x / 2.f;
            float startY = window.getSize().y / 2.f - 80.f;
            for (int i = 0; i < 3; ++i) {
                sf::FloatRect bounds = menu[i].getLocalBounds();
                float x = centerX - (bounds.width / 2.f + bounds.left);
                float y = startY + i * 60.f - bounds.top;
                shadow[i].setPosition(x + 2, y + 2);
                menu[i].setPosition(x, y);
                if (i == selected) {
                    menu[i].setFillColor(sf::Color(255, 255, 0, alpha));
                    shadow[i].setFillColor(sf::Color(0, 0, 0, alpha));
                } else {
                    menu[i].setFillColor(sf::Color::White);
                    shadow[i].setFillColor(sf::Color::Black);
                }
                window.draw(shadow[i]);
                window.draw(menu[i]);
            }
            window.display();
            continue; // Skip further game processing.
        }
        
        // APROPOS STATE (About Screen):
        else if (gameState == APROPOS) {
            // Draw background.
            window.draw(bgSprite);
            
            // Scroll the about text upward.
            // (Assumes aproposTexts is a vector of vector of strings; currentTextIndex indexes the current page.)
            float scrollY = window.getSize().y + 40.f - aproposScrollClock.getElapsedTime().asSeconds() * 60.f;
            float cx = window.getSize().x / 2.f;
            for (size_t i = 0; i < aproposTexts[currentTextIndex].size(); ++i) {
                aproposText.setString(aproposTexts[currentTextIndex][i]);
                float px = cx - aproposText.getGlobalBounds().width / 2.f;
                float py = scrollY + i * 40.f;
                aproposShadow.setString(aproposTexts[currentTextIndex][i]);
                aproposShadow.setPosition(px + 2, py + 2);
                aproposText.setPosition(px, py);
                if (py > -50 && py < window.getSize().y - 80) { // Only draw if visible.
                    window.draw(aproposShadow);
                    window.draw(aproposText);
                }
            }
            // If text scrolled past threshold, show next page.
            if (scrollY + aproposTexts[currentTextIndex].size() * 40.f < -100.f) {
                currentTextIndex = (currentTextIndex + 1) % aproposTexts.size();
                aproposScrollClock.restart();
            }
            // Draw a fixed "RETOUR AU MENU" button.
            sf::FloatRect rb = menu[3].getLocalBounds();
            float rx = window.getSize().x / 2.f - (rb.width / 2.f + rb.left);
            float ry = window.getSize().y - 60.f;
            shadow[3].setPosition(rx + 2, ry + 2);
            menu[3].setPosition(rx, ry);
            menu[3].setFillColor(sf::Color(255, 255, 0, alpha));
            shadow[3].setFillColor(sf::Color(0, 0, 0, alpha));
            window.draw(shadow[3]);
            window.draw(menu[3]);
            window.display();
            continue;
        }
        
        // FINISH STATE:
    // ===== FINISH STATE =====
    // ===== FINISH STATE =====
    else if (gameState == FINISH) {
        // Real‑time finish‑screen loop with pulsating "RETOUR AU MENU"
        sf::Clock finishClock;
        // Main text + shadow
        sf::Text returnBtn("RETOUR AU MENU", font, 28);
        sf::Text returnBtnShadow = returnBtn;

        // Center both texts once:
        auto updateReturnBtnPos = [&]() {
            auto bb = returnBtn.getLocalBounds();
            sf::Vector2f pos(
                window.getSize().x/2.f - (bb.width/2.f + bb.left),
                window.getSize().y*0.6f + 250.f
            );
            returnBtn.setPosition(pos);
            returnBtnShadow.setPosition(pos + sf::Vector2f(2.f, 2.f));
        };
        updateReturnBtnPos();

        while (window.isOpen() && gameState == FINISH) {
            float dt = finishClock.restart().asSeconds();
            sf::Event ev;
            while (window.pollEvent(ev)) {
                if (ev.type == sf::Event::Closed) {
                    window.close();
                    break;
                }
                if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::Enter) {
                    clickSound.play();
                    gameState = MENU; selected = 0; distanceTraveled = 0.f; score = 0; lives = 3;
                }
                if (ev.type == sf::Event::MouseButtonPressed &&
                    returnBtn.getGlobalBounds().contains(
                        static_cast<float>(ev.mouseButton.x),
                        static_cast<float>(ev.mouseButton.y)
                    ))
                {
                    clickSound.play();
                    gameState = MENU; selected = 0; distanceTraveled = 0.f; score = 0; lives = 3;
                }
            }
            if (!window.isOpen()) break;

            // —— Pulsating alpha exactly like your menu buttons ——
            float pulseTime = clock.getElapsedTime().asSeconds();
            sf::Uint8 alpha = static_cast<sf::Uint8>(
                127.5f * (std::sin(pulseTime * 2 * 3.14159265f) + 1)
            );
            // Yellow text + black shadow, both fading in/out
            returnBtn.setFillColor(sf::Color(255, 255, 0, alpha));
            returnBtnShadow.setFillColor(sf::Color(0,   0,   0, alpha));

            // —— Draw everything every frame ——
            window.clear();

            // Title
            sf::Text finishTitle("FELICITATIONS!", font, 64);
            finishTitle.setFillColor(sf::Color::Yellow);
            {
                auto bb = finishTitle.getLocalBounds();
                finishTitle.setPosition(
                    window.getSize().x/2.f - (bb.width/2.f + bb.left),
                    window.getSize().y*0.2f
                );
            }
            window.draw(finishTitle);

            // Score
            sf::Text finishScore("Votre score est " + std::to_string(score), font, 32);
            finishScore.setFillColor(sf::Color::White);
            {
                auto bb = finishScore.getLocalBounds();
                finishScore.setPosition(
                    window.getSize().x/2.f - (bb.width/2.f + bb.left),
                    window.getSize().y*0.4f + 80.f
                );
            }
            window.draw(finishScore);

            // Return button with shadow
            window.draw(returnBtnShadow);
            window.draw(returnBtn);

            window.display();
        }

        // If window was closed inside that loop, bail out of main loop too
        if (!window.isOpen()) break;
        continue;
    }
    // ===== GAME / HIT STATE =====


    // ===== GAME / HIT STATE =====
    else {
        // Process input for boosting/braking
        static bool triedWhileExhausted = false;
    
        bool boosting = false;
        bool braking  = false;
    
        // Check if boost key is down
        bool boostKeyDown =
               sf::Keyboard::isKeyPressed(sf::Keyboard::W) ||
               sf::Keyboard::isKeyPressed(sf::Keyboard::Up) ||
               sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
    
        if (boostKeyDown) {
            if (stamina >= MIN_STAMINA_TO_BOOST) {
                // Allowed to boost
                boosting = true;
                stamina  = std::max(0.f, stamina - STAMINA_DRAIN * dt);
                triedWhileExhausted = false;   // reset flag once we successfully boost
            } else {
                // Not enough stamina—play tired sound one time
                if (!triedWhileExhausted) {
                    tiredSound.play();
                    triedWhileExhausted = true;
                }
            }
        } else {
            // Once the player releases the boost key, allow the sound to trigger again next time
            triedWhileExhausted = false;
        }
    
        // Brake logic unchanged
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
        {
            braking = true;
        }
    
        // Regenerate stamina when not boosting
        if (!boosting) {
            stamina = std::min(MAX_STAMINA, stamina + STAMINA_REGEN * dt);
        }
    
        if (!raceFinished) distanceTraveled += playerWorldSpeed;
        stamina = std::min(MAX_STAMINA, stamina + STAMINA_REGEN * dt);

        // Update world speed
        if (boosting)        playerWorldSpeed = std::min(playerWorldSpeed + ACCEL, MAX_SPEED);
        else if (braking)    playerWorldSpeed = 0.f;
        else {
            if (playerWorldSpeed < DEFAULT_SPEED)
                playerWorldSpeed = std::min(playerWorldSpeed + ACCEL, DEFAULT_SPEED);
            else if (playerWorldSpeed > DEFAULT_SPEED)
                playerWorldSpeed = std::max(playerWorldSpeed - BRAKE_FORCE, DEFAULT_SPEED);
        }

        // Determine obstacle speed
        float actualObstacleSpeed = braking ? -obstacleSpeed : obstacleSpeed;

        // Draw grass margins
        float rw = static_cast<float>(roadTexture.getSize().x);
        float roadLeft = (window.getSize().x - rw) / 2.f;
        grassOffset -= playerWorldSpeed;
        if (grassOffset < 0.f)
            grassOffset += static_cast<float>(grassTexture.getSize().y);
        int winH = static_cast<int>(window.getSize().y);
        int iRoadLeft = static_cast<int>(roadLeft);
        sf::RectangleShape gL({ roadLeft, static_cast<float>(winH) });
        sf::RectangleShape gR({ roadLeft, static_cast<float>(winH) });
        gL.setPosition(0, 0);
        gR.setPosition(roadLeft + rw, 0);
        gL.setTexture(&grassTexture);
        gR.setTexture(&grassTexture);
        gL.setTextureRect({ 0, static_cast<int>(grassOffset), iRoadLeft, winH });
        gR.setTextureRect({ 0, static_cast<int>(grassOffset), iRoadLeft, winH });
        window.draw(gL);
        window.draw(gR);

        // Update and draw road tiles
       // — Draw & wrap road tiles —
for (auto &tile : roadTiles) {
    // scroll
    tile.move(0, playerWorldSpeed);

    // if it moves off bottom, jump it back up by the total stacked height
    if (tile.getPosition().y >= window.getSize().y) {
        tile.setPosition(
            tile.getPosition().x,
            tile.getPosition().y - roadTileCount * tileH
        );
    }

    // re‑center X
    float roadLeft = (window.getSize().x - static_cast<float>(roadTexture.getSize().x)) / 2.f;
    tile.setPosition(roadLeft, tile.getPosition().y);

    // draw
    window.draw(tile);
}

        
        

        // Spawn & move finish line
        // — Spawn & move finish line —
if (!finishLineSpawned && distanceTraveled >= FINISH_SPAWN_AT) {
    finishLine.setTexture(finishLineTexture);
    float scale = rw / static_cast<float>(finishLineTexture.getSize().x);
    finishLine.setScale(scale, scale);
    finishLine.setPosition(roadLeft, -finishLine.getGlobalBounds().height);
    finishLineSpawned = true;
}

if (finishLineSpawned) {
    finishLine.move(0, playerWorldSpeed);
    window.draw(finishLine);

    // Trigger on first contact
    if (!finishTriggered && player.getGlobalBounds().intersects(finishLine.getGlobalBounds())) {
        finishTriggered = true;
        finishTriggerClock.restart();
        finishSound.play();
    }

    // After 2s, switch to FINISH state
if (finishTriggered && finishTriggerClock.getElapsedTime().asSeconds() >= FINISH_DELAY) {
    gameState    = FINISH;
    raceFinished = true;
}

}


        // Spawn & update trees
        if (std::rand() % 100 < 2 && (trees.empty() || trees.back().getPosition().y > 200)) {
            sf::Sprite tr(treeTextures[std::rand() % 5]);
            float tw = tr.getGlobalBounds().width, th = tr.getGlobalBounds().height;
            bool leftSide = (std::rand() % 2) == 0;
            float tx = leftSide
                     ? (roadLeft > tw ? std::rand() % static_cast<int>(roadLeft - tw + 1) : 0)
                     : (window.getSize().x - (roadLeft + rw) > tw
                        ? roadLeft + rw + std::rand() % static_cast<int>(window.getSize().x - roadLeft - rw - tw + 1)
                        : window.getSize().x - tw);
            tr.setPosition(tx, -th);
            trees.push_back(tr);
        }
        for (auto it = trees.begin(); it != trees.end(); ) {
            it->move(0, playerWorldSpeed);
            if (it->getPosition().y > window.getSize().y) it = trees.erase(it);
            else { window.draw(*it); ++it; }
        }

        // Spawn & update obstacles
        if (std::rand() % 100 < 10 && (obstacles.empty() || obstacles.back().getPosition().y > 150.f)) {
            int lane = std::rand() % LANES;
            sf::Sprite obs(eplayerTextures[std::rand() % 5]);
            obs.setScale(0.20f, 0.20f);
            float laneW = (rw - (padLeft + padRight) * rw) / LANES;
            float cx_lane = roadLeft + padLeft * rw + laneW * (lane + 0.5f);
            obs.setPosition(
                cx_lane - obs.getGlobalBounds().width / 2.f,
                -obs.getGlobalBounds().height - (std::rand() % 101 + 50)
            );
            obstacles.push_back(obs);
        }
        for (auto it = obstacles.begin(); it != obstacles.end(); ) {
            it->move(0, (braking ? -obstacleSpeed : obstacleSpeed));
            sf::FloatRect pb = player.getGlobalBounds();
            pb.left += pb.width * 0.25f; pb.width *= 0.5f;
            sf::FloatRect ob = it->getGlobalBounds();
            ob.left += ob.width * 0.25f; ob.width *= 0.5f;
            if (pb.intersects(ob) && gameState == GAME) {
                crashSound.play(); lives--;
                if (lives <= 0) gameState = MENU; else { gameState = HIT; fadeClock.restart(); }
                it = obstacles.erase(it);
            } else if (it->getPosition().y > window.getSize().y) {
                it = obstacles.erase(it);
                score += 10;
            } else {
                sf::Sprite sh = *it;
                sh.move(5.f, 5.f);
                sh.setColor(sf::Color(0, 0, 0, 150));
                window.draw(sh);
                window.draw(*it);
                ++it;
            }
        }

        // Smooth lane movement
        {
            float laneW = (rw - (padLeft + padRight) * rw) / LANES;
            float pw = player.getGlobalBounds().width;
            float playerTargetX = roadLeft + padLeft * rw + laneW * (playerLane + 0.5f) - pw / 2.f;
            float bx = player.getPosition().x;
            if (bx + 5.f < playerTargetX) player.move(5.f, 0.f);
            else if (bx - 5.f > playerTargetX) player.move(-5.f, 0.f);
            else player.setPosition(playerTargetX, player.getPosition().y);
        }

        // Hit blink effect
        if (gameState == HIT) {
            float ht = fadeClock.getElapsedTime().asSeconds();
            if (ht < 2.f) {
                uint8_t a = static_cast<uint8_t>(255 * std::abs(std::sin(ht * 10.f)));
                player.setColor(sf::Color(255, 255, 255, a));
            } else {
                gameState = GAME;
                player.setColor(sf::Color::White);
            }
        }

        // Draw player and shadow
        playerShadow.setPosition(player.getPosition().x + 5.f, player.getPosition().y + 5.f);
        window.draw(playerShadow);
        window.draw(player);

        // Spawn and update collectibles
        spawnBottle(bottles, bottleTex, rw, padLeft, padRight, LANES, window, obstacles, coins);
        updateBottles(bottles, playerWorldSpeed, window, player, stamina, MAX_STAMINA, BOTTLE_STAMINA, drinkSound);
        spawnScoreCoin(coins, coinTex, rw, padLeft, padRight, LANES, window, obstacles, bottles);
        updateCoins(coins, playerWorldSpeed, window, player, score, coinSound);

        // HUD: Score and Lives
        sf::Text hud;
        hud.setFont(font);
        hud.setCharacterSize(24);
        hud.setFillColor(sf::Color::White);
        hud.setString("Score: " + std::to_string(score) + "  Lives: " + std::to_string(lives));
        hud.setPosition(20.f, 20.f);
        window.draw(hud);

       

            // Draw stamina bar
    const float BAR_W = 20.f, BAR_H = 150.f;
    float barX = window.getSize().x - BAR_W - 20.f;
    float barY = (window.getSize().y - BAR_H) / 2.f;

    // ← STAMINA label
    staminaLabel.setPosition(
        barX - staminaLabel.getGlobalBounds().width - 10.f,  // to the left of the bar
        barY - staminaLabel.getCharacterSize()               // just above it
    );
    window.draw(staminaLabel);

    sf::RectangleShape barBg(sf::Vector2f(BAR_W, BAR_H));
    barBg.setPosition(barX, barY);
    barBg.setFillColor(sf::Color(50, 50, 50, 200));
    window.draw(barBg);

    float fillH = (stamina / MAX_STAMINA) * BAR_H;
    sf::RectangleShape barFill(sf::Vector2f(BAR_W, fillH));
    barFill.setPosition(barX, barY + (BAR_H - fillH));
    barFill.setFillColor(sf::Color(100, 100, 255, 200));
    window.draw(barFill);


    // Draw race progress bar
    const float PB_W = 300.f, PB_H = 15.f;
    float progress = std::min(1.f, distanceTraveled / RACE_DISTANCE);
    float pbX = (window.getSize().x - PB_W) / 2.f;
    float pbY = window.getSize().y - PB_H - 10.f;

    // ← VOTRE POSITION label
    positionLabel.setPosition(
        pbX,                                                  // align left edge to bar
        pbY - positionLabel.getCharacterSize() - 5.f          // just above bar
    );
    window.draw(positionLabel);

    sf::RectangleShape progressBg(sf::Vector2f(PB_W, PB_H));
    progressBg.setPosition(pbX, pbY);
    progressBg.setFillColor(sf::Color(50, 50, 50, 200));
    window.draw(progressBg);

    sf::RectangleShape progressFill(sf::Vector2f(PB_W * progress, PB_H));
    progressFill.setPosition(pbX, pbY);
    progressFill.setFillColor(sf::Color(100, 255, 100, 220));
    window.draw(progressFill);

    window.display();

    } // End GAME/HIT state

    // Loop back to start of main while
} // End while(window.isOpen())

return 0;
} // End main