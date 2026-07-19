#include <ui/widgets/DoomGame.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <ui/overlays/GameOverlay.h>
#include <imgui/imgui.h>
#include <iconloader/IconLoader.h>
#include <game/interop/LuaHelpers.h>

#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <queue>
#include <utility>

namespace tsm {
    namespace ui {
        namespace widgets {


            static constexpr int MAP_WIDTH = 24;
            static constexpr int MAP_HEIGHT = 24;
            static constexpr float FOV = 60.0f * 3.14159f / 180.0f;
            static constexpr float MOVE_SPEED = 3.5f;
            static constexpr float ROTATE_SPEED = 2.5f;
            static constexpr int NUM_RAYS = 240;
            static constexpr float MAX_DEPTH = 20.0f;
            static constexpr int MAX_ENEMIES = 15;
            static constexpr int MAX_PROJECTILES = 30;
            static constexpr int MAX_PARTICLES = 50;
            static constexpr int MAX_PICKUPS = 8;
            static constexpr float PROJECTILE_SPEED = 10.0f;
            static constexpr float ENEMY_SPEED = 1.8f;
            static constexpr float SHOOT_COOLDOWN = 0.22f;


            struct Vec2 {
                float x, y;
                Vec2(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
                Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
                Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
                Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
                float length() const { return std::sqrt(x * x + y * y); }
                Vec2 normalized() const { float l = length(); return l > 0 ? Vec2(x / l, y / l) : Vec2(0, 0); }
            };

            enum class EnemyType {
                GRUNT = 0,
                FAST = 1,
                TANK = 2
            };

            struct Enemy {
                Vec2 pos;
                bool active;
                int health;
                float animTime;
                EnemyType type;
                float attackCooldown;

                int getMaxHealth() const {
                    switch (type) {
                    case EnemyType::GRUNT: return 2;
                    case EnemyType::FAST: return 1;
                    case EnemyType::TANK: return 5;
                    default: return 2;
                    }
                }

                float getSpeed() const {
                    switch (type) {
                    case EnemyType::GRUNT: return ENEMY_SPEED;
                    case EnemyType::FAST: return ENEMY_SPEED * 1.8f;
                    case EnemyType::TANK: return ENEMY_SPEED * 0.6f;
                    default: return ENEMY_SPEED;
                    }
                }

                ImVec4 getColor() const {
                    switch (type) {
                    case EnemyType::GRUNT: return ImVec4(0.8f, 0.2f, 0.2f, 1);
                    case EnemyType::FAST: return ImVec4(1.0f, 0.5f, 0.0f, 1);
                    case EnemyType::TANK: return ImVec4(0.5f, 0.1f, 0.5f, 1);
                    default: return ImVec4(0.8f, 0.2f, 0.2f, 1);
                    }
                }

                float getSize() const {
                    switch (type) {
                    case EnemyType::GRUNT: return 1.0f;
                    case EnemyType::FAST: return 0.8f;
                    case EnemyType::TANK: return 1.4f;
                    default: return 1.0f;
                    }
                }
            };

            struct Projectile {
                Vec2 pos;
                Vec2 dir;
                bool active;
                float lifetime;
            };

            struct Particle {
                Vec2 pos;
                Vec2 vel;
                ImVec4 color;
                float lifetime;
                float maxLifetime;
                bool active;
            };

            enum class PickupType {
                HEALTH,
                AMMO
            };

            struct Pickup {
                Vec2 pos;
                PickupType type;
                bool active;
                float animTime;
            };

            struct DoomState {
                int map[MAP_HEIGHT][MAP_WIDTH];
                Vec2 playerPos;
                float playerAngle;
                int playerHealth;
                int maxHealth;
                int ammo;
                int maxAmmo;
                float shootCooldown;
                std::vector<Enemy> enemies;
                std::vector<Projectile> projectiles;
                std::vector<Particle> particles;
                std::vector<Pickup> pickups;
                int kills;
                int wave;
                bool gameOver;
                bool victory;
                bool initialized;
                float damageFlash;
                float muzzleFlash;
                int score;
                int highScore;

                Vec2 joystickCenter;
                Vec2 joystickCurrent;
                bool joystickActive;
                Vec2 lookStartPos;
                bool lookActive;
                float lookSensitivity;
                bool shootButtonPressed;

                void init();
                void reset();
                void update(float dt);
                void spawnWave(int waveNum);
                void spawnPickup(Vec2 pos, PickupType type);
                void createParticles(Vec2 pos, ImVec4 color, int count);
                bool lineOfSight(Vec2 from, Vec2 to);
                void computeReachableCells();
                bool reachable[MAP_HEIGHT][MAP_WIDTH];
            };

            static DoomState g_state;
            static bool g_seeded = false;


            static void ensureSeed() {
                if (!g_seeded) {
                    std::srand(static_cast<unsigned>(std::time(nullptr)));
                    g_seeded = true;
                }
            }

            static int randInt(int min, int max) {
                return min + (std::rand() % (max - min + 1));
            }

            static float randFloat(float min, float max) {
                return min + (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * (max - min);
            }


            void DoomState::init() {
                ensureSeed();

                for (int y = 0; y < MAP_HEIGHT; y++) {
                    for (int x = 0; x < MAP_WIDTH; x++) {
                        map[y][x] = (x == 0 || y == 0 || x == MAP_WIDTH - 1 || y == MAP_HEIGHT - 1) ? 1 : 0;
                    }
                }

                for (int i = 0; i < 35; i++) {
                    int x = randInt(2, MAP_WIDTH - 3);
                    int y = randInt(2, MAP_HEIGHT - 3);
                    int len = randInt(2, 7);
                    bool horiz = randInt(0, 1) == 0;

                    for (int j = 0; j < len; j++) {
                        int wx = horiz ? x + j : x;
                        int wy = horiz ? y : y + j;
                        if (wx > 0 && wx < MAP_WIDTH - 1 && wy > 0 && wy < MAP_HEIGHT - 1) {
                            if (std::abs(wx - MAP_WIDTH / 2) > 3 || std::abs(wy - MAP_HEIGHT / 2) > 3) {
                                map[wy][wx] = 1;
                            }
                        }
                    }
                }

                lookSensitivity = 0.01f;
                initialized = true;
                computeReachableCells();
                reset();
            }

            void DoomState::computeReachableCells() {
                for (int y = 0; y < MAP_HEIGHT; y++) {
                    for (int x = 0; x < MAP_WIDTH; x++) {
                        reachable[y][x] = false;
                    }
                }
                int startX = MAP_WIDTH / 2;
                int startY = MAP_HEIGHT / 2;
                if (map[startY][startX] != 0) return;

                std::queue<std::pair<int, int>> q;
                q.push({startX, startY});
                reachable[startY][startX] = true;

                static const int dx[] = {0, 1, 0, -1};
                static const int dy[] = {-1, 0, 1, 0};

                while (!q.empty()) {
                    auto [x, y] = q.front();
                    q.pop();

                    for (int i = 0; i < 4; i++) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        if (nx >= 1 && nx < MAP_WIDTH - 1 && ny >= 1 && ny < MAP_HEIGHT - 1 &&
                            map[ny][nx] == 0 && !reachable[ny][nx]) {
                            reachable[ny][nx] = true;
                            q.push({nx, ny});
                        }
                    }
                }
            }

            void DoomState::reset() {
                playerPos = Vec2(MAP_WIDTH / 2.0f, MAP_HEIGHT / 2.0f);
                playerAngle = 0;
                maxHealth = 100;
                playerHealth = maxHealth;
                maxAmmo = 100;
                ammo = 50;
                shootCooldown = 0;
                kills = 0;
                wave = 1;
                gameOver = false;
                victory = false;
                damageFlash = 0;
                muzzleFlash = 0;
                score = 0;

                joystickActive = false;
                lookActive = false;
                shootButtonPressed = false;

                enemies.clear();
                projectiles.clear();
                particles.clear();
                pickups.clear();

                projectiles.resize(MAX_PROJECTILES);
                particles.resize(MAX_PARTICLES);
                pickups.resize(MAX_PICKUPS);

                spawnWave(wave);
            }

            void DoomState::spawnWave(int waveNum) {
                enemies.clear();
                int numEnemies = std::min(MAX_ENEMIES, 6 + waveNum * 2);
                enemies.resize(numEnemies);

                for (int i = 0; i < numEnemies; i++) {
                    enemies[i].active = false;

                    for (int attempt = 0; attempt < 100; attempt++) {
                        float x = randFloat(2, MAP_WIDTH - 2);
                        float y = randFloat(2, MAP_HEIGHT - 2);
                        Vec2 toPlayer = playerPos - Vec2(x, y);

                        int ix = static_cast<int>(x);
                        int iy = static_cast<int>(y);

                        if (toPlayer.length() > 6 && map[iy][ix] == 0 && reachable[iy][ix]) {
                            int typeRoll = randInt(0, 100);
                            EnemyType type = EnemyType::GRUNT;

                            if (waveNum >= 2 && typeRoll < 30) {
                                type = EnemyType::FAST;
                            }
                            else if (waveNum >= 3 && typeRoll >= 30 && typeRoll < 50) {
                                type = EnemyType::TANK;
                            }

                            enemies[i] = { Vec2(x, y), true, 0, 0, type, 0 };
                            enemies[i].health = enemies[i].getMaxHealth();
                            break;
                        }
                    }
                }

                int numPickups = randInt(2, 4);
                for (int i = 0; i < numPickups && i < MAX_PICKUPS; i++) {
                    for (int attempt = 0; attempt < 50; attempt++) {
                        float x = randFloat(3, MAP_WIDTH - 3);
                        float y = randFloat(3, MAP_HEIGHT - 3);
                        int ix = static_cast<int>(x);
                        int iy = static_cast<int>(y);

                        if (map[iy][ix] == 0 && reachable[iy][ix] && (playerPos - Vec2(x, y)).length() > 4) {
                            PickupType type = (i % 2 == 0) ? PickupType::HEALTH : PickupType::AMMO;
                            pickups[i] = { Vec2(x, y), type, true, 0 };
                            break;
                        }
                    }
                }
            }

            void DoomState::spawnPickup(Vec2 pos, PickupType type) {
                for (auto& p : pickups) {
                    if (!p.active) {
                        p.pos = pos;
                        p.type = type;
                        p.active = true;
                        p.animTime = 0;
                        break;
                    }
                }
            }

            void DoomState::createParticles(Vec2 pos, ImVec4 color, int count) {
                for (int i = 0; i < count; i++) {
                    for (auto& p : particles) {
                        if (!p.active) {
                            float angle = randFloat(0, 6.28f);
                            float speed = randFloat(1.0f, 3.0f);
                            p.pos = pos;
                            p.vel = Vec2(std::cos(angle) * speed, std::sin(angle) * speed);
                            p.color = color;
                            p.maxLifetime = randFloat(0.3f, 0.8f);
                            p.lifetime = p.maxLifetime;
                            p.active = true;
                            break;
                        }
                    }
                }
            }

            bool DoomState::lineOfSight(Vec2 from, Vec2 to) {
                Vec2 dir = (to - from).normalized();
                float dist = (to - from).length();

                for (float d = 0; d < dist; d += 0.2f) {
                    Vec2 check = from + dir * d;
                    int cx = static_cast<int>(check.x);
                    int cy = static_cast<int>(check.y);

                    if (cx < 0 || cx >= MAP_WIDTH || cy < 0 || cy >= MAP_HEIGHT || map[cy][cx] != 0) {
                        return false;
                    }
                }
                return true;
            }

            void DoomState::update(float dt) {
                if (gameOver) return;

                if (damageFlash > 0) damageFlash -= dt * 3.0f;
                if (muzzleFlash > 0) muzzleFlash -= dt * 4.0f;
                if (shootCooldown > 0) shootCooldown -= dt;

                float strafeX = 0, moveY = 0;

                if (joystickActive) {
                    Vec2 delta = joystickCurrent - joystickCenter;
                    float dist = delta.length();

                    if (dist > 5.0f) {
                        float maxDist = DP(60.0f);
                        if (dist > maxDist) {
                            delta = delta.normalized() * maxDist;
                            dist = maxDist;
                        }

                        strafeX = delta.x / maxDist;
                        moveY = -delta.y / maxDist;
                    }
                }

                if (moveY != 0) {
                    Vec2 forward(std::cos(playerAngle), std::sin(playerAngle));
                    Vec2 move = forward * moveY * MOVE_SPEED * dt;

                    Vec2 newPos = playerPos + move;
                    int nx = static_cast<int>(newPos.x);
                    int ny = static_cast<int>(newPos.y);

                    if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT && map[ny][nx] == 0) {
                        playerPos = newPos;
                    }
                }

                if (strafeX != 0) {
                    Vec2 right(std::cos(playerAngle + 3.14159f / 2.0f), std::sin(playerAngle + 3.14159f / 2.0f));
                    Vec2 move = right * strafeX * MOVE_SPEED * dt;

                    Vec2 newPos = playerPos + move;
                    int nx = static_cast<int>(newPos.x);
                    int ny = static_cast<int>(newPos.y);

                    if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT && map[ny][nx] == 0) {
                        playerPos = newPos;
                    }
                }

                for (auto& p : projectiles) {
                    if (!p.active) continue;
                    p.lifetime -= dt;
                    if (p.lifetime <= 0) {
                        p.active = false;
                        continue;
                    }

                    Vec2 newP = p.pos + p.dir * PROJECTILE_SPEED * dt;
                    int px = static_cast<int>(newP.x);
                    int py = static_cast<int>(newP.y);

                    if (px < 0 || px >= MAP_WIDTH || py < 0 || py >= MAP_HEIGHT || map[py][px] != 0) {
                        p.active = false;
                        createParticles(p.pos, ImVec4(1, 1, 0, 1), 5);
                    }
                    else {
                        p.pos = newP;
                    }
                }

                for (auto& p : particles) {
                    if (!p.active) continue;
                    p.lifetime -= dt;
                    if (p.lifetime <= 0) {
                        p.active = false;
                        continue;
                    }
                    p.pos = p.pos + p.vel * dt;
                    p.vel = p.vel * 0.95f;
                }

                for (auto& pickup : pickups) {
                    if (!pickup.active) continue;
                    pickup.animTime += dt;

                    if ((playerPos - pickup.pos).length() < 0.6f) {
                        if (pickup.type == PickupType::HEALTH && playerHealth < maxHealth) {
                            playerHealth = std::min(maxHealth, playerHealth + 25);
                            pickup.active = false;
                            createParticles(pickup.pos, ImVec4(0, 1, 0, 1), 10);
                        }
                        else if (pickup.type == PickupType::AMMO && ammo < maxAmmo) {
                            ammo = std::min(maxAmmo, ammo + 20);
                            pickup.active = false;
                            createParticles(pickup.pos, ImVec4(1, 1, 0, 1), 10);
                        }
                    }
                }

                int aliveCount = 0;
                for (auto& e : enemies) {
                    if (!e.active) continue;
                    aliveCount++;

                    e.animTime += dt;
                    if (e.attackCooldown > 0) e.attackCooldown -= dt;

                    Vec2 toPlayer = playerPos - e.pos;
                    float dist = toPlayer.length();

                    if (dist > 0.5f && dist < 15.0f && lineOfSight(e.pos, playerPos)) {
                        Vec2 dir = toPlayer.normalized();
                        Vec2 newE = e.pos + dir * e.getSpeed() * dt;
                        int ex = static_cast<int>(newE.x);
                        int ey = static_cast<int>(newE.y);

                        if (ex >= 0 && ex < MAP_WIDTH && ey >= 0 && ey < MAP_HEIGHT && map[ey][ex] == 0) {
                            e.pos = newE;
                        }
                    }

                    if (dist < 0.8f && e.attackCooldown <= 0) {
                        int damage = 10;
                        if (e.type == EnemyType::TANK) damage = 20;
                        else if (e.type == EnemyType::FAST) damage = 5;

                        playerHealth -= damage;
                        damageFlash = 1.0f;
                        e.attackCooldown = 1.0f;

                        if (playerHealth <= 0) {
                            playerHealth = 0;
                            if (score > highScore) {
                                highScore = score;
                            }
                            gameOver = true;
                        }
                    }

                    for (auto& p : projectiles) {
                        if (!p.active) continue;
                        if ((e.pos - p.pos).length() < 0.5f) {
                            e.health--;
                            p.active = false;
                            createParticles(e.pos, e.getColor(), 8);

                            if (e.health <= 0) {
                                e.active = false;
                                kills++;
                                score += 100 * wave;
                                createParticles(e.pos, e.getColor(), 15);

                                if (randInt(0, 100) < 30) {
                                    PickupType type = (randInt(0, 1) == 0) ? PickupType::HEALTH : PickupType::AMMO;
                                    spawnPickup(e.pos, type);
                                }
                            }
                            break;
                        }
                    }
                }

                if (aliveCount == 0) {
                    wave++;
                    score += 500 * wave;
                    spawnWave(wave);

                    playerHealth = std::min(maxHealth, playerHealth + 20);
                }
            }


            static ImU32 shadeColor(const ImVec4& color, float brightness) {
                return ImGui::GetColorU32(ImVec4(
                    color.x * brightness,
                    color.y * brightness,
                    color.z * brightness,
                    color.w
                ));
            }

            static const char* GetEnemyIconName(EnemyType type) {
                switch (type) {
                case EnemyType::GRUNT: return "UiEmoteAngry";
                case EnemyType::FAST:  return "UiEmoteAP06Scare";
                case EnemyType::TANK:  return "UiEmoteAP13EvilLaugh0";
                default:               return "UiEmoteAngry";
                }
            }

            static const char* GetPickupIconName(PickupType type) {
                switch (type) {
                case PickupType::HEALTH: return "UiMiscHeart";
                case PickupType::AMMO:   return "UiMenuBattery03";
                default:                 return "UiMiscQuestion";
                }
            }

            void DrawDoomGame() {
                if (!g_state.initialized) {
                    g_state.init();
                }

                ImGuiIO& io = ImGui::GetIO();
                g_state.update(io.DeltaTime);

                ImGuiViewport* vp = ImGui::GetMainViewport();

                const float topPadding = DP(80.0f);
                const float bottomPadding = DP(40.0f);
                const float leftPadding = DP(20.0f);
                const float rightMargin = DP(20.0f);

                const float gameWidth = (vp->Size.x * 0.5f) - leftPadding - rightMargin;
                const float gameHeight = vp->Size.y - topPadding - bottomPadding;

                ImVec2 screenPos(vp->Pos.x + leftPadding, vp->Pos.y + topPadding);
                ImVec2 screenSize(gameWidth, gameHeight);

                ImDrawList* draw = ImGui::GetForegroundDrawList();

                ImGui::SetNextWindowPos(ImVec2(screenPos.x - DP(20.0f), screenPos.y), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(screenSize.x + DP(40.0f), screenSize.y), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

                ImGuiWindowFlags inputFlags =
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoSavedSettings;

                if (ImGui::Begin("##DoomGameInputCapture", nullptr, inputFlags)) {
                    ImVec2 size = ImGui::GetWindowSize();
                    ImGui::SetCursorPos(ImVec2(0, 0));
                    ImGui::InvisibleButton("##DoomGameInputRegion", size);

                    ImVec2 touchPos = io.MousePos;

                    if (io.MouseDown[0]) {
                        if (touchPos.x >= screenPos.x && touchPos.x < screenPos.x + screenSize.x * 0.35f &&
                            touchPos.y >= screenPos.y + screenSize.y * 0.5f) {
                            if (!g_state.joystickActive) {
                                g_state.joystickCenter = Vec2(touchPos.x, touchPos.y);
                                g_state.joystickActive = true;
                            }
                            g_state.joystickCurrent = Vec2(touchPos.x, touchPos.y);
                        }
                        else if (touchPos.x >= screenPos.x + screenSize.x * 0.4f &&
                            touchPos.x < screenPos.x + screenSize.x &&
                            touchPos.y < screenPos.y + screenSize.y - DP(120.0f)) {
                            if (!g_state.lookActive) {
                                g_state.lookStartPos = Vec2(touchPos.x, touchPos.y);
                                g_state.lookActive = true;
                            }

                            Vec2 lookDelta = Vec2(touchPos.x, touchPos.y) - g_state.lookStartPos;
                            g_state.playerAngle += lookDelta.x * g_state.lookSensitivity;
                            g_state.lookStartPos = Vec2(touchPos.x, touchPos.y);
                        }
                    }
                    else {
                        g_state.joystickActive = false;
                        g_state.lookActive = false;
                        g_state.shootButtonPressed = false;
                    }
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);

                const ImVec4 accent = tsm::ui::GetAccentColor();
                draw->AddRectFilled(screenPos, ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                    ImGui::GetColorU32(ImVec4(0, 0, 0, 1)));

                draw->AddRect(screenPos, ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.4f)), DP(8.0f), 0, DP(2.0f));

                draw->AddRectFilledMultiColor(
                    screenPos,
                    ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y * 0.5f),
                    ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.15f, 1)),
                    ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.15f, 1)),
                    ImGui::GetColorU32(ImVec4(0.15f, 0.1f, 0.25f, 1)),
                    ImGui::GetColorU32(ImVec4(0.15f, 0.1f, 0.25f, 1))
                );

                draw->AddRectFilledMultiColor(
                    ImVec2(screenPos.x, screenPos.y + screenSize.y * 0.5f),
                    ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                    ImGui::GetColorU32(ImVec4(0.15f, 0.1f, 0.05f, 1)),
                    ImGui::GetColorU32(ImVec4(0.15f, 0.1f, 0.05f, 1)),
                    ImGui::GetColorU32(ImVec4(0.05f, 0.03f, 0.01f, 1)),
                    ImGui::GetColorU32(ImVec4(0.05f, 0.03f, 0.01f, 1))
                );

                std::vector<float> depthBuffer(NUM_RAYS, MAX_DEPTH);

                float stripWidth = screenSize.x / NUM_RAYS;
                for (int i = 0; i < NUM_RAYS; i++) {
                    float rayAngle = g_state.playerAngle - FOV / 2 + (i * FOV / NUM_RAYS);
                    Vec2 rayDir(std::cos(rayAngle), std::sin(rayAngle));

                    float dist = 0;
                    bool hit = false;

                    while (dist < MAX_DEPTH && !hit) {
                        dist += 0.1f;
                        Vec2 test = g_state.playerPos + rayDir * dist;
                        int tx = static_cast<int>(test.x);
                        int ty = static_cast<int>(test.y);

                        if (tx < 0 || tx >= MAP_WIDTH || ty < 0 || ty >= MAP_HEIGHT || g_state.map[ty][tx] != 0) {
                            hit = true;
                        }
                    }

                    depthBuffer[i] = dist;

                    float corrected = dist * std::cos(rayAngle - g_state.playerAngle);
                    float wallHeight = screenSize.y / (corrected + 0.001f);
                    wallHeight = std::min(wallHeight, screenSize.y * 2.0f);

                    float top = (screenSize.y - wallHeight) * 0.5f;
                    float brightness = 1.0f - std::min(corrected / MAX_DEPTH, 0.85f);

                    float x = screenPos.x + i * stripWidth;

                    int wx = static_cast<int>(g_state.playerPos.x + rayDir.x * dist);
                    int wy = static_cast<int>(g_state.playerPos.y + rayDir.y * dist);
                    float pattern = ((wx + wy) % 2 == 0) ? 0.9f : 1.0f;

                    draw->AddRectFilled(
                        ImVec2(x, screenPos.y + top),
                        ImVec2(x + stripWidth + 1, screenPos.y + top + wallHeight),
                        shadeColor(ImVec4(0.4f * pattern, 0.4f * pattern, 0.45f * pattern, 1), brightness)
                    );
                }

                struct SpriteData {
                    Vec2 pos;
                    float dist;
                    ImVec4 color;
                    float size;
                    bool isEnemy;
                    bool isPickup;
                    float animTime;
                    const char* iconName;
                };

                std::vector<SpriteData> sprites;

                for (const auto& enemy : g_state.enemies) {
                    if (!enemy.active) continue;

                    Vec2 toEnemy = enemy.pos - g_state.playerPos;
                    float dist = toEnemy.length();
                    if (dist > MAX_DEPTH) continue;

                    float angle = std::atan2(toEnemy.y, toEnemy.x) - g_state.playerAngle;
                    while (angle < -3.14159f) angle += 2 * 3.14159f;
                    while (angle > 3.14159f) angle -= 2 * 3.14159f;

                    if (std::abs(angle) > FOV / 2) continue;

                    int rayIndex = static_cast<int>((angle / FOV + 0.5f) * NUM_RAYS);
                    rayIndex = std::max(0, std::min(NUM_RAYS - 1, rayIndex));

                    if (dist < depthBuffer[rayIndex] - 0.5f) {
                        const char* iconName = GetEnemyIconName(enemy.type);
                        sprites.push_back({ enemy.pos, dist, enemy.getColor(), enemy.getSize(), true, false, enemy.animTime, iconName });
                    }
                }

                for (const auto& pickup : g_state.pickups) {
                    if (!pickup.active) continue;

                    Vec2 toPickup = pickup.pos - g_state.playerPos;
                    float dist = toPickup.length();
                    if (dist > MAX_DEPTH) continue;

                    float angle = std::atan2(toPickup.y, toPickup.x) - g_state.playerAngle;
                    while (angle < -3.14159f) angle += 2 * 3.14159f;
                    while (angle > 3.14159f) angle -= 2 * 3.14159f;

                    if (std::abs(angle) > FOV / 2) continue;

                    int rayIndex = static_cast<int>((angle / FOV + 0.5f) * NUM_RAYS);
                    rayIndex = std::max(0, std::min(NUM_RAYS - 1, rayIndex));

                    if (dist < depthBuffer[rayIndex] - 0.5f) {
                        ImVec4 color = (pickup.type == PickupType::HEALTH) ?
                            ImVec4(0.0f, 1.0f, 0.3f, 1) : ImVec4(1.0f, 1.0f, 0.0f, 1);
                        const char* iconName = GetPickupIconName(pickup.type);
                        sprites.push_back({ pickup.pos, dist, color, 0.6f, false, true, pickup.animTime, iconName });
                    }
                }

                std::sort(sprites.begin(), sprites.end(), [](const SpriteData& a, const SpriteData& b) {
                    return a.dist > b.dist;
                    });

                for (const auto& sprite : sprites) {
                    Vec2 toSprite = sprite.pos - g_state.playerPos;
                    float angle = std::atan2(toSprite.y, toSprite.x) - g_state.playerAngle;
                    while (angle < -3.14159f) angle += 2 * 3.14159f;
                    while (angle > 3.14159f) angle -= 2 * 3.14159f;

                    float screenX = (angle / FOV + 0.5f) * screenSize.x;
                    float size = (screenSize.y / (sprite.dist + 0.001f)) * sprite.size;
                    size = std::min(size, screenSize.y * 0.5f);

                    float brightness = 1.0f - std::min(sprite.dist / MAX_DEPTH, 0.8f);
                    ImU32 tintColor = shadeColor(sprite.color, brightness);

                    UIIcon uiIcon{};
                    bool hasIcon = false;
                    if (sprite.iconName) {
                        IconLoader::getUIIcon(sprite.iconName, &uiIcon);
                        hasIcon = (uiIcon.textureId != IL_NO_TEXTURE);
                    }

                    float centerY = screenPos.y + screenSize.y * 0.5f;

                    if (sprite.isPickup) {
                        float bob = std::sin(sprite.animTime * 3.0f) * size * 0.1f;
                        centerY += bob;

                        float half = size * 0.3f;
                        ImVec2 min(screenPos.x + screenX - half, centerY - half);
                        ImVec2 max(screenPos.x + screenX + half, centerY + half);

                        if (hasIcon) {
                            draw->AddImage(uiIcon.textureId, min, max, uiIcon.uv0, uiIcon.uv1, tintColor);
                        }
                        else {
                            draw->AddRectFilled(min, max, tintColor);
                        }
                    }
                    else {
                        float halfW = size * 0.3f;
                        float halfH = size * 0.5f;
                        ImVec2 min(screenPos.x + screenX - halfW, centerY - halfH);
                        ImVec2 max(screenPos.x + screenX + halfW, centerY + halfH);

                        if (hasIcon) {
                            draw->AddImage(uiIcon.textureId, min, max, uiIcon.uv0, uiIcon.uv1, tintColor);
                        }
                        else {
                            draw->AddRectFilled(min, max, tintColor);
                        }
                    }
                }

                for (const auto& proj : g_state.projectiles) {
                    if (!proj.active) continue;

                    Vec2 toProj = proj.pos - g_state.playerPos;
                    float dist = toProj.length();
                    if (dist > MAX_DEPTH || dist < 0.3f) continue;

                    float angle = std::atan2(toProj.y, toProj.x) - g_state.playerAngle;
                    while (angle < -3.14159f) angle += 2 * 3.14159f;
                    while (angle > 3.14159f) angle -= 2 * 3.14159f;

                    if (std::abs(angle) > FOV / 2) continue;

                    float screenX = (angle / FOV + 0.5f) * screenSize.x;
                    float size = screenSize.y / (dist + 0.001f) * 0.3f;

                    draw->AddCircleFilled(
                        ImVec2(screenPos.x + screenX, screenPos.y + screenSize.y * 0.5f),
                        size, ImGui::GetColorU32(ImVec4(1, 1, 0, 0.9f))
                    );
                }

                ImFont* font = ImGui::GetFont();
                float fontSize = DP(20.0f);
                float pad = DP(12.0f);

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d/%d",
                    tsm::ui::i18n::Tr("Score"),
                    g_state.score,
                    g_state.highScore);

                draw->AddText(font, fontSize * 0.75f, ImVec2(screenPos.x + pad, screenPos.y + pad),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)), scoreBuffer);

                char waveBuffer[32];
                std::snprintf(waveBuffer, sizeof(waveBuffer), "%s: %d",
                    tsm::ui::i18n::Tr("Wave"), g_state.wave);
                draw->AddText(font, fontSize * 0.75f,
                    ImVec2(screenPos.x + pad, screenPos.y + pad + fontSize * 0.75f + DP(4.0f)),
                    ImGui::GetColorU32(ImVec4(0.5f, 0.8f, 1, 0.7f)), waveBuffer);

                float barWidth = DP(120.0f);
                float barHeight = DP(18.0f);
                ImVec2 barPos(screenPos.x + screenSize.x - barWidth - pad, screenPos.y + pad);

                draw->AddRectFilled(barPos,
                    ImVec2(barPos.x + barWidth, barPos.y + barHeight),
                    ImGui::GetColorU32(ImVec4(0.2f, 0, 0, 0.8f)));

                float healthPct = static_cast<float>(g_state.playerHealth) / g_state.maxHealth;
                draw->AddRectFilled(barPos,
                    ImVec2(barPos.x + barWidth * healthPct, barPos.y + barHeight),
                    ImGui::GetColorU32(ImVec4(1, 0, 0, 0.9f)));

                char hpBuf[32];
                std::snprintf(hpBuf, sizeof(hpBuf), "HP: %d", g_state.playerHealth);
                draw->AddText(font, fontSize * 0.75f, ImVec2(barPos.x + DP(4), barPos.y + DP(2)),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), hpBuf);

                char ammoBuf[32];
                std::snprintf(ammoBuf, sizeof(ammoBuf), "%s: %d", tsm::ui::i18n::Tr("AMMO"), g_state.ammo);
                ImVec2 ammoSize = font->CalcTextSizeA(fontSize * 0.75f, FLT_MAX, 0, ammoBuf);
                draw->AddText(font, fontSize * 0.75f,
                    ImVec2(screenPos.x + screenSize.x - ammoSize.x - pad,
                        barPos.y + barHeight + DP(4.0f)),
                    ImGui::GetColorU32(ImVec4(1, 1, 0, 0.9f)), ammoBuf);

                float minimapSize = DP(120.0f);
                ImVec2 minimapPos(screenPos.x + pad, screenPos.y + screenSize.y - minimapSize - pad);

                draw->AddRectFilled(minimapPos,
                    ImVec2(minimapPos.x + minimapSize, minimapPos.y + minimapSize),
                    ImGui::GetColorU32(ImVec4(0, 0, 0, 0.7f)));
                draw->AddRect(minimapPos,
                    ImVec2(minimapPos.x + minimapSize, minimapPos.y + minimapSize),
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.3f)), 0, 0, DP(1.5f));

                float cellSize = minimapSize / MAP_WIDTH;
                for (int y = 0; y < MAP_HEIGHT; y++) {
                    for (int x = 0; x < MAP_WIDTH; x++) {
                        if (g_state.map[y][x] != 0) {
                            ImVec2 min(minimapPos.x + x * cellSize, minimapPos.y + y * cellSize);
                            ImVec2 max(min.x + cellSize, min.y + cellSize);
                            draw->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1)));
                        }
                    }
                }

                for (const auto& e : g_state.enemies) {
                    if (!e.active) continue;
                    ImVec2 enemyMapPos(minimapPos.x + e.pos.x * cellSize,
                        minimapPos.y + e.pos.y * cellSize);
                    draw->AddCircleFilled(enemyMapPos, cellSize * 0.8f,
                        ImGui::GetColorU32(e.getColor()));
                }

                ImVec2 playerMapPos(minimapPos.x + g_state.playerPos.x * cellSize,
                    minimapPos.y + g_state.playerPos.y * cellSize);

                float dirLength = cellSize * 2.0f;
                ImVec2 dirEnd(playerMapPos.x + std::cos(g_state.playerAngle) * dirLength,
                    playerMapPos.y + std::sin(g_state.playerAngle) * dirLength);
                draw->AddLine(playerMapPos, dirEnd,
                    ImGui::GetColorU32(ImVec4(0, 1, 0, 0.8f)), DP(2.0f));

                draw->AddCircleFilled(playerMapPos, cellSize * 1.2f,
                    ImGui::GetColorU32(ImVec4(0, 1, 0, 1)));
                draw->AddCircle(playerMapPos, cellSize * 1.2f,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)), 0, DP(1.5f));

                float cx = screenPos.x + screenSize.x * 0.5f;
                float cy = screenPos.y + screenSize.y * 0.5f;
                float csize = DP(10.0f);
                ImVec4 crosshairColor = g_state.shootCooldown > 0 ? ImVec4(1, 0, 0, 0.5f) : ImVec4(0, 1, 0, 0.8f);
                draw->AddLine(ImVec2(cx - csize, cy), ImVec2(cx + csize, cy), ImGui::GetColorU32(crosshairColor), DP(2));
                draw->AddLine(ImVec2(cx, cy - csize), ImVec2(cx, cy + csize), ImGui::GetColorU32(crosshairColor), DP(2));
                draw->AddCircle(ImVec2(cx, cy), csize * 0.6f, ImGui::GetColorU32(crosshairColor), 0, DP(2));

                if (g_state.muzzleFlash > 0) {
                    float flashSize = DP(60.0f) * g_state.muzzleFlash;
                    draw->AddCircleFilled(ImVec2(cx, cy + screenSize.y * 0.25f), flashSize,
                        ImGui::GetColorU32(ImVec4(1, 1, 0, g_state.muzzleFlash * 0.7f)));
                }

                if (g_state.damageFlash > 0) {
                    draw->AddRectFilled(screenPos, ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                        ImGui::GetColorU32(ImVec4(1, 0, 0, g_state.damageFlash * 0.3f)));
                }

                float btnSize = DP(70.0f);
                float btnY = screenPos.y + screenSize.y - btnSize - pad;

                ImVec2 shootBtnPos(screenPos.x + screenSize.x - btnSize - pad, btnY);
                bool canShoot = g_state.shootCooldown <= 0 && g_state.ammo > 0;
                ImU32 shootColor = canShoot ?
                    ImGui::GetColorU32(ImVec4(1, 0.2f, 0, 0.7f)) :
                    ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.5f));

                ImVec2 shootBtnCenter(shootBtnPos.x + btnSize / 2, shootBtnPos.y + btnSize / 2);
                draw->AddCircleFilled(shootBtnCenter, btnSize / 2, shootColor);
                draw->AddCircle(shootBtnCenter, btnSize / 2,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)), 0, DP(3));

                UIIcon fireIcon{};
                IconLoader::getUIIcon("UiEmoteFlame4", &fireIcon);
                if (fireIcon.textureId != IL_NO_TEXTURE) {
                    float iconSize = btnSize * 0.5f;
                    ImVec2 iconMin(shootBtnCenter.x - iconSize * 0.5f, shootBtnCenter.y - iconSize * 0.5f);
                    ImVec2 iconMax(shootBtnCenter.x + iconSize * 0.5f, shootBtnCenter.y + iconSize * 0.5f);
                    draw->AddImage(fireIcon.textureId, iconMin, iconMax, fireIcon.uv0, fireIcon.uv1,
                        ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
                }

                if (io.MouseDown[0]) {
                    ImVec2 touchPos = io.MousePos;
                    Vec2 toTouch = Vec2(touchPos.x - shootBtnCenter.x, touchPos.y - shootBtnCenter.y);

                    if (toTouch.length() < btnSize / 2 && canShoot && !g_state.shootButtonPressed) {
                        Vec2 forward(std::cos(g_state.playerAngle), std::sin(g_state.playerAngle));
                        for (auto& p : g_state.projectiles) {
                            if (!p.active) {
                                p.pos = g_state.playerPos + forward * 0.5f;
                                p.dir = forward;
                                p.active = true;
                                p.lifetime = 3.0f;
                                g_state.shootCooldown = SHOOT_COOLDOWN;
                                g_state.ammo--;
                                g_state.muzzleFlash = 1.0f;
                                g_state.shootButtonPressed = true;
                                tsm::lua::helpers::PlaySound2D("Damage8bit");
                                break;
                            }
                        }
                    }
                }

                if (g_state.joystickActive) {
                    float joyRadius = DP(55.0f);
                    draw->AddCircleFilled(ImVec2(g_state.joystickCenter.x, g_state.joystickCenter.y),
                        joyRadius, ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.6f)));
                    draw->AddCircle(ImVec2(g_state.joystickCenter.x, g_state.joystickCenter.y),
                        joyRadius, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.6f)), 0, DP(2));
                    draw->AddCircleFilled(ImVec2(g_state.joystickCurrent.x, g_state.joystickCurrent.y),
                        DP(22.0f), ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)));
                }

                if (g_state.gameOver) {
                    draw->AddRectFilled(screenPos, ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
                        ImGui::GetColorU32(ImVec4(0, 0, 0, 0.85f)));

                    const char* msg = tsm::ui::i18n::Tr("GAME OVER");
                    float msgSize = DP(36.0f);
                    ImVec2 textSize = font->CalcTextSizeA(msgSize, FLT_MAX, 0, msg);
                    draw->AddText(font, msgSize,
                        ImVec2(screenPos.x + (screenSize.x - textSize.x) * 0.5f, screenPos.y + screenSize.y * 0.35f),
                        ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), msg);

                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s %d", tsm::ui::i18n::Tr("Wave"), g_state.wave);
                    ImVec2 waveSize = font->CalcTextSizeA(fontSize * 1.2f, FLT_MAX, 0, buf);
                    draw->AddText(font, fontSize * 1.2f,
                        ImVec2(screenPos.x + (screenSize.x - waveSize.x) * 0.5f, screenPos.y + screenSize.y * 0.45f),
                        ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), buf);

                    std::snprintf(buf, sizeof(buf), "%s: %d", tsm::ui::i18n::Tr("Score"), g_state.score);
                    ImVec2 scoreSize2 = font->CalcTextSizeA(fontSize * 1.2f, FLT_MAX, 0, buf);
                    draw->AddText(font, fontSize * 1.2f,
                        ImVec2(screenPos.x + (screenSize.x - scoreSize2.x) * 0.5f, screenPos.y + screenSize.y * 0.5f),
                        ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), buf);

                    if (g_state.highScore > 0) {
                        const bool newRecord = (g_state.score == g_state.highScore && g_state.score > 0);
                        if (newRecord) {
                            const char* recordMsg = tsm::ui::i18n::Tr("New Record");
                            ImVec2 recordSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0, recordMsg);
                            draw->AddText(font, fontSize,
                                ImVec2(screenPos.x + (screenSize.x - recordSize.x) * 0.5f, screenPos.y + screenSize.y * 0.56f),
                                ImGui::GetColorU32(ImVec4(1, 0.85f, 0.2f, 1)), recordMsg);
                        }
                        else {
                            std::snprintf(buf, sizeof(buf), "%s: %d", tsm::ui::i18n::Tr("Best"), g_state.highScore);
                            ImVec2 bestSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0, buf);
                            draw->AddText(font, fontSize,
                                ImVec2(screenPos.x + (screenSize.x - bestSize.x) * 0.5f, screenPos.y + screenSize.y * 0.56f),
                                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.7f)), buf);
                        }
                    }

                    const char* restart = tsm::ui::i18n::Tr("Tap to Restart");
                    ImVec2 restartSize = font->CalcTextSizeA(fontSize * 1.1f, FLT_MAX, 0, restart);
                    draw->AddText(font, fontSize * 1.1f,
                        ImVec2(screenPos.x + (screenSize.x - restartSize.x) * 0.5f, screenPos.y + screenSize.y * 0.65f),
                        ImGui::GetColorU32(ImVec4(0, 1, 0, 1)), restart);

                    if (io.MouseClicked[0]) {
                        g_state.reset();
                    }
                }
            }

        }
    }
}
