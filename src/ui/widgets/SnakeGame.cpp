#include <ui/widgets/SnakeGame.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <iconloader/IconLoader.h>
#include <imgui/imgui.h>
#include <game/interop/LuaHelpers.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace tsm {
    namespace ui {
        namespace widgets {


            struct SnakePoint {
                int x, y;

                bool operator==(const SnakePoint& other) const {
                    return x == other.x && y == other.y;
                }
            };

            enum class Direction : uint8_t {
                Up, Down, Left, Right
            };

            struct SwipeGesture {
                ImVec2 startPos{ 0, 0 };
                ImVec2 currentPos{ 0, 0 };
                bool active = false;
                float startTime = 0.0f;

                void reset() {
                    startPos = ImVec2(0, 0);
                    currentPos = ImVec2(0, 0);
                    active = false;
                    startTime = 0.0f;
                }
            };

            struct GameState {
                int width = 24;
                int height = 24;

                std::vector<SnakePoint> segments;
                Direction direction = Direction::Right;
                Direction queuedDirection = Direction::Right;

                SnakePoint food{ 0, 0 };

                float tickAccumulator = 0.0f;
                float baseTickInterval = 0.18f;
                float tickInterval = 0.18f;

                int score = 0;
                int highScore = 0;
                bool initialized = false;
                bool running = false;
                bool paused = false;
                bool gameOver = false;

                float growAnimProgress = 0.0f;
                float foodSpawnAnim = 0.0f;

                SwipeGesture swipe;

                void reset();
                bool occupiesCell(int x, int y) const;
                SnakePoint wrapPosition(int x, int y) const;
                void updateDifficulty();
            };


            static GameState g_state;
            static bool g_randomSeeded = false;

            inline void ensureRandomSeeded() {
                if (!g_randomSeeded) {
                    std::srand(static_cast<unsigned int>(std::time(nullptr)));
                    g_randomSeeded = true;
                }
            }

            inline bool isOppositeDirection(Direction a, Direction b) {
                return (a == Direction::Up && b == Direction::Down) ||
                    (a == Direction::Down && b == Direction::Up) ||
                    (a == Direction::Left && b == Direction::Right) ||
                    (a == Direction::Right && b == Direction::Left);
            }

            SnakePoint GameState::wrapPosition(int x, int y) const {
                SnakePoint wrapped;
                wrapped.x = ((x % width) + width) % width;
                wrapped.y = ((y % height) + height) % height;
                return wrapped;
            }

            bool GameState::occupiesCell(int x, int y) const {
                return std::any_of(segments.begin(), segments.end(),
                    [x, y](const SnakePoint& p) { return p.x == x && p.y == y; });
            }

            void GameState::updateDifficulty() {
                const float speedMultiplier = 1.0f - (score * 0.015f);
                tickInterval = baseTickInterval * std::max(0.4f, speedMultiplier);
            }

            void GameState::reset() {
                segments.clear();

                const int cx = width / 2;
                const int cy = height / 2;
                segments = { {cx, cy}, {cx - 1, cy}, {cx - 2, cy} };

                direction = Direction::Right;
                queuedDirection = Direction::Right;

                if (score > highScore) {
                    highScore = score;
                }

                score = 0;
                tickInterval = baseTickInterval;
                tickAccumulator = 0.0f;
                growAnimProgress = 0.0f;
                foodSpawnAnim = 0.0f;
                running = true;
                paused = false;
                gameOver = false;
                swipe.reset();
            }


            static void spawnFood() {
                if (g_state.width <= 0 || g_state.height <= 0) return;

                for (int attempt = 0; attempt < 256; ++attempt) {
                    const int fx = std::rand() % g_state.width;
                    const int fy = std::rand() % g_state.height;

                    if (!g_state.occupiesCell(fx, fy)) {
                        g_state.food = { fx, fy };
                        g_state.foodSpawnAnim = 1.0f;
                        return;
                    }
                }

                g_state.food = { 0, 0 };
                g_state.foodSpawnAnim = 1.0f;
            }

            static void requestDirection(Direction newDir) {
                if (!isOppositeDirection(g_state.direction, newDir) &&
                    newDir != g_state.direction) {
                    g_state.queuedDirection = newDir;
                }
            }

            static SnakePoint getNextHeadPosition(const SnakePoint& head, Direction dir) {
                SnakePoint next = head;

                switch (dir) {
                case Direction::Up:    --next.y; break;
                case Direction::Down:  ++next.y; break;
                case Direction::Left:  --next.x; break;
                case Direction::Right: ++next.x; break;
                }

                return next;
            }

            static void updateGame(float deltaTime) {
                if (g_state.growAnimProgress > 0.0f) {
                    g_state.growAnimProgress = std::max(0.0f, g_state.growAnimProgress - deltaTime * 3.0f);
                }
                if (g_state.foodSpawnAnim > 0.0f) {
                    g_state.foodSpawnAnim = std::max(0.0f, g_state.foodSpawnAnim - deltaTime * 2.0f);
                }

                if (!g_state.running || g_state.paused || g_state.gameOver) return;

                g_state.tickAccumulator += deltaTime;
                if (g_state.tickAccumulator < g_state.tickInterval) return;

                g_state.tickAccumulator -= g_state.tickInterval;
                g_state.direction = g_state.queuedDirection;

                if (g_state.segments.empty()) {
                    g_state.reset();
                    spawnFood();
                    return;
                }

                SnakePoint newHead = getNextHeadPosition(g_state.segments.front(), g_state.direction);

                newHead = g_state.wrapPosition(newHead.x, newHead.y);

                for (size_t i = 0; i < g_state.segments.size() - 1; ++i) {
                    if (g_state.segments[i] == newHead) {
                        if (g_state.score > g_state.highScore) {
                            g_state.highScore = g_state.score;
                        }

                        g_state.gameOver = true;
                        g_state.running = false;
                        return;
                    }
                }

                const bool ateFood = (newHead == g_state.food);

                g_state.segments.insert(g_state.segments.begin(), newHead);

                if (ateFood) {
                    ++g_state.score;
                    tsm::lua::helpers::PlaySound2D("HeartCount");
                    g_state.growAnimProgress = 1.0f;
                    g_state.updateDifficulty();
                    spawnFood();
                }
                else {
                    g_state.segments.pop_back();
                }
            }


            static void processSwipeGesture(const ImVec2& boardPos, const ImVec2& boardSize) {
                const ImGuiIO& io = ImGui::GetIO();
                const ImVec2 mouse = io.MousePos;

                const bool overBoard = (mouse.x >= boardPos.x && mouse.x <= boardPos.x + boardSize.x &&
                    mouse.y >= boardPos.y && mouse.y <= boardPos.y + boardSize.y);

                if (!overBoard) {
                    if (!ImGui::IsMouseDown(0)) {
                        g_state.swipe.reset();
                    }
                    return;
                }

                if (ImGui::IsMouseClicked(0) && overBoard) {
                    g_state.swipe.startPos = mouse;
                    g_state.swipe.currentPos = mouse;
                    g_state.swipe.active = true;
                    g_state.swipe.startTime = static_cast<float>(ImGui::GetTime());
                }

                if (g_state.swipe.active && ImGui::IsMouseDown(0)) {
                    g_state.swipe.currentPos = mouse;
                }

                if (g_state.swipe.active && ImGui::IsMouseReleased(0)) {
                    const float dx = g_state.swipe.currentPos.x - g_state.swipe.startPos.x;
                    const float dy = g_state.swipe.currentPos.y - g_state.swipe.startPos.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    const float duration = static_cast<float>(ImGui::GetTime()) - g_state.swipe.startTime;

                    const float minDistance = DP(20.0f);
                    const float maxDuration = 0.8f;

                    if (distance >= minDistance && duration <= maxDuration && !g_state.gameOver) {
                        const float absDx = std::abs(dx);
                        const float absDy = std::abs(dy);

                        const float threshold = 0.4f;

                        if (absDx > absDy * threshold && absDx > minDistance * 0.5f) {
                            if (dx > 0) {
                                requestDirection(Direction::Right);
                            }
                            else {
                                requestDirection(Direction::Left);
                            }
                        }
                        else if (absDy > absDx * threshold && absDy > minDistance * 0.5f) {
                            if (dy > 0) {
                                requestDirection(Direction::Down);
                            }
                            else {
                                requestDirection(Direction::Up);
                            }
                        }
                        else if (absDx > absDy) {
                            if (dx > 0) {
                                requestDirection(Direction::Right);
                            }
                            else {
                                requestDirection(Direction::Left);
                            }
                        }
                        else {
                            if (dy > 0) {
                                requestDirection(Direction::Down);
                            }
                            else {
                                requestDirection(Direction::Up);
                            }
                        }
                    }

                    g_state.swipe.reset();
                }
            }


            struct RenderContext {
                ImDrawList* drawList;
                ImVec2 boardPos;
                ImVec2 boardSize;
                float cellSize;
                float padding;
                ImU32 headColor;
                ImU32 bodyColor;
                ImU32 foodColor;
                ImU32 gridColor;

                void drawCell(int x, int y, ImU32 color, float scale = 1.0f, float rounding = 2.0f) const {
                    const float actualSize = cellSize - padding * 2.0f;
                    const float scaledSize = actualSize * scale;
                    const float offset = (actualSize - scaledSize) * 0.5f;

                    const ImVec2 min(
                        boardPos.x + cellSize * static_cast<float>(x) + padding + offset,
                        boardPos.y + cellSize * static_cast<float>(y) + padding + offset
                    );
                    const ImVec2 max(
                        min.x + scaledSize,
                        min.y + scaledSize
                    );

                    drawList->AddRectFilled(min, max, color, DP(rounding));
                }

                ImVec2 getCellCenter(int x, int y) const {
                    return ImVec2(
                        boardPos.x + cellSize * (static_cast<float>(x) + 0.5f),
                        boardPos.y + cellSize * (static_cast<float>(y) + 0.5f)
                    );
                }
            };

            static void renderGrid(const RenderContext& ctx) {
                for (int x = 0; x <= g_state.width; ++x) {
                    const float posX = ctx.boardPos.x + ctx.cellSize * static_cast<float>(x);
                    ctx.drawList->AddLine(
                        ImVec2(posX, ctx.boardPos.y),
                        ImVec2(posX, ctx.boardPos.y + ctx.boardSize.y),
                        ctx.gridColor, DP(0.5f)
                    );
                }

                for (int y = 0; y <= g_state.height; ++y) {
                    const float posY = ctx.boardPos.y + ctx.cellSize * static_cast<float>(y);
                    ctx.drawList->AddLine(
                        ImVec2(ctx.boardPos.x, posY),
                        ImVec2(ctx.boardPos.x + ctx.boardSize.x, posY),
                        ctx.gridColor, DP(0.5f)
                    );
                }
            }

            static void renderFood(const RenderContext& ctx) {
                const ImVec2 center = ctx.getCellCenter(g_state.food.x, g_state.food.y);

                const float pulse = 0.9f + 0.1f * std::sin(ImGui::GetTime() * 8.0f);
                const float spawnScale = g_state.foodSpawnAnim > 0.0f ?
                    (1.0f - g_state.foodSpawnAnim) : 1.0f;

                const float iconSize = (ctx.cellSize - ctx.padding * 2.0f) * 1.2f * pulse * spawnScale;
                const ImVec2 iconMin(center.x - iconSize * 0.5f, center.y - iconSize * 0.5f);
                const ImVec2 iconMax(center.x + iconSize * 0.5f, center.y + iconSize * 0.5f);

                UIIcon uiIcon{};
                IconLoader::getUIIcon("UiOutfitPropBirthdayCakeL", &uiIcon);

                if (uiIcon.textureId != IL_NO_TEXTURE) {
                    const ImU32 white = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ctx.drawList->AddImage(uiIcon.textureId, iconMin, iconMax,
                        uiIcon.uv0, uiIcon.uv1, white);
                }
                else {
                    ctx.drawCell(g_state.food.x, g_state.food.y, ctx.foodColor, pulse * spawnScale);
                }
            }

            static void renderSnake(const RenderContext& ctx) {
                for (size_t i = 0; i < g_state.segments.size(); ++i) {
                    const auto& segment = g_state.segments[i];

                    float scale = 1.0f;
                    if (i == g_state.segments.size() - 1 && g_state.growAnimProgress > 0.0f) {
                        scale = 1.0f - g_state.growAnimProgress * 0.5f;
                    }

                    const float alpha = i == 0 ? 1.0f : 0.5f + 0.5f * (1.0f - static_cast<float>(i) / g_state.segments.size());
                    ImVec4 color;

                    if (i == 0) {
                        const ImVec4 accent = tsm::ui::GetAccentColor();
                        color = ImVec4(accent.x, accent.y, accent.z, alpha);
                    }
                    else {
                        const ImVec4 accent = tsm::ui::GetAccentColor();
                        color = ImVec4(accent.x * 0.8f, accent.y * 0.8f, accent.z * 0.8f, alpha * 0.85f);
                    }

                    ctx.drawCell(segment.x, segment.y, ImGui::GetColorU32(color), scale, i == 0 ? 3.0f : 2.0f);
                }
            }

            static void renderSwipeIndicator(const RenderContext& ctx) {
                if (!g_state.swipe.active) return;

                const ImVec2& start = g_state.swipe.startPos;
                const ImVec2& current = g_state.swipe.currentPos;

                const float dx = current.x - start.x;
                const float dy = current.y - start.y;
                const float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < DP(10.0f)) return;

                const ImVec4 accent = tsm::ui::GetAccentColor();
                ctx.drawList->AddLine(
                    start, current,
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.6f)),
                    DP(3.0f)
                );

                const float angle = std::atan2(dy, dx);
                const float arrowSize = DP(12.0f);
                const float arrowAngle = 0.5f;

                const ImVec2 arrow1(
                    current.x - arrowSize * std::cos(angle - arrowAngle),
                    current.y - arrowSize * std::sin(angle - arrowAngle)
                );
                const ImVec2 arrow2(
                    current.x - arrowSize * std::cos(angle + arrowAngle),
                    current.y - arrowSize * std::sin(angle + arrowAngle)
                );

                ctx.drawList->AddTriangleFilled(
                    current, arrow1, arrow2,
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.6f))
                );
            }

            static void renderHUD(const RenderContext& ctx) {
                ImFont* font = ImGui::GetFont();
                const float fontSize = DP(20.0f);
                const float padding = DP(16.0f);
                const float hudFontSize = fontSize * 0.75f;

                const int displayBest = std::max(g_state.score, g_state.highScore);

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d/%d",
                    tsm::ui::i18n::Tr("Score"), g_state.score, displayBest);

                const ImVec2 scoreSize = font->CalcTextSizeA(hudFontSize, FLT_MAX, 0.0f, scoreBuffer);

                if (g_state.score > 0) {
                    const float speedPercent = (1.0f - (g_state.tickInterval / g_state.baseTickInterval)) * 100.0f;
                    char speedBuffer[32];
                    std::snprintf(speedBuffer, sizeof(speedBuffer), "%.0f%% %s",
                        speedPercent, tsm::ui::i18n::Tr("faster"));

                    const ImVec2 speedPos(
                        ctx.boardPos.x + padding,
                        ctx.boardPos.y + padding
                    );
                    ctx.drawList->AddText(font, hudFontSize, speedPos,
                        ImGui::GetColorU32(ImVec4(1, 1, 0.3f, 0.8f)), speedBuffer);
                }

                const float baseY = ctx.boardPos.y + padding;
                const float scoreStartX = ctx.boardPos.x + ctx.boardSize.x - scoreSize.x - padding;

                const ImVec2 scorePos(
                    scoreStartX,
                    baseY
                );
                ctx.drawList->AddText(font, hudFontSize, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f)), scoreBuffer);

                if (!g_state.gameOver && g_state.score < 3) {
                    const float hintAlpha = std::max(0.0f, 1.0f - static_cast<float>(ImGui::GetTime()) / 8.0f);
                    if (hintAlpha > 0.0f) {
                        const char* hint = tsm::ui::i18n::Tr("Swipe to move");
                        const ImVec2 hintSize = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, hint);
                        const ImVec2 hintPos(
                            ctx.boardPos.x + (ctx.boardSize.x - hintSize.x) * 0.5f,
                            ctx.boardPos.y + ctx.boardSize.y - fontSize - padding
                        );
                        ctx.drawList->AddText(font, fontSize * 0.85f, hintPos,
                            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.5f * hintAlpha)), hint);
                    }
                }
            }

            static void renderGameOverOverlay(const RenderContext& ctx, const ImGuiIO& io) {
                const char* title = tsm::ui::i18n::Tr("Game Over");
                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d",
                    tsm::ui::i18n::Tr("Final Score"), g_state.score);

                char highScoreBuffer[64];
                const bool newRecord = g_state.score == g_state.highScore && g_state.score > 0;
                if (newRecord) {
                    std::snprintf(highScoreBuffer, sizeof(highScoreBuffer), "%s!",
                        tsm::ui::i18n::Tr("New Record"));
                }
                else {
                    std::snprintf(highScoreBuffer, sizeof(highScoreBuffer), "%s: %d",
                        tsm::ui::i18n::Tr("Best"), g_state.highScore);
                }

                ImFont* font = ImGui::GetFont();
                const float titleSize = DP(28.0f);
                const float textSize = DP(20.0f);

                const ImVec2 titleDim = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
                const ImVec2 scoreDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, scoreBuffer);
                const ImVec2 highScoreDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, highScoreBuffer);

                const float padding = DP(24.0f);
                const float buttonWidth = DP(180.0f);
                const float buttonHeight = DP(48.0f);

                const float boxWidth = std::max({ titleDim.x, scoreDim.x, highScoreDim.x, buttonWidth }) + padding * 2.0f;
                const float boxHeight = titleDim.y + scoreDim.y + highScoreDim.y + buttonHeight + padding * 5.0f;

                const ImVec2 boxMin(
                    ctx.boardPos.x + (ctx.boardSize.x - boxWidth) * 0.5f,
                    ctx.boardPos.y + (ctx.boardSize.y - boxHeight) * 0.5f
                );
                const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);

                const ImVec4 accent = tsm::ui::GetAccentColor();
                const ImVec4 boxBg = ImVec4(0.05f, 0.05f, 0.05f, 0.95f);
                const ImVec4 boxBorder = ImVec4(accent.x, accent.y, accent.z, 0.8f);

                ctx.drawList->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(boxBg), DP(12.0f));
                ctx.drawList->AddRect(boxMin, boxMax, ImGui::GetColorU32(boxBorder), DP(12.0f), 0, DP(2.0f));

                float cursorY = boxMin.y + padding;

                const ImVec2 titlePos(boxMin.x + (boxWidth - titleDim.x) * 0.5f, cursorY);
                ctx.drawList->AddText(font, titleSize, titlePos,
                    ImGui::GetColorU32(ImVec4(1, 0.3f, 0.3f, 1)), title);

                cursorY += titleDim.y + padding;

                const ImVec2 scorePos(boxMin.x + (boxWidth - scoreDim.x) * 0.5f, cursorY);
                ctx.drawList->AddText(font, textSize, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), scoreBuffer);

                cursorY += scoreDim.y + padding * 0.5f;

                const ImVec2 highScorePos(boxMin.x + (boxWidth - highScoreDim.x) * 0.5f, cursorY);
                const ImVec4 highScoreColor = newRecord ?
                    ImVec4(1, 0.85f, 0.2f, 1) : ImVec4(1, 1, 1, 0.7f);
                ctx.drawList->AddText(font, textSize, highScorePos,
                    ImGui::GetColorU32(highScoreColor), highScoreBuffer);

                const ImVec2 btnMin(
                    boxMin.x + (boxWidth - buttonWidth) * 0.5f,
                    boxMax.y - padding - buttonHeight
                );
                const ImVec2 btnMax(btnMin.x + buttonWidth, btnMin.y + buttonHeight);

                const ImVec2 mouse = io.MousePos;
                const bool hovered = (mouse.x >= btnMin.x && mouse.x <= btnMax.x &&
                    mouse.y >= btnMin.y && mouse.y <= btnMax.y);
                const bool pressed = hovered && ImGui::IsMouseDown(0);
                const bool clicked = hovered && ImGui::IsMouseClicked(0);

                const ImVec4 btnBg = hovered ?
                    ImVec4(accent.x * 1.2f, accent.y * 1.2f, accent.z * 1.2f, pressed ? 1.0f : 0.95f) :
                    ImVec4(accent.x, accent.y, accent.z, 0.9f);

                ctx.drawList->AddRectFilled(btnMin, btnMax, ImGui::GetColorU32(btnBg), DP(8.0f));
                ctx.drawList->AddRect(btnMin, btnMax,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? 0.4f : 0.2f)), DP(8.0f), 0, DP(2.0f));

                const char* btnLabel = tsm::ui::i18n::Tr("Play Again");
                const ImVec2 btnLabelSize = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, btnLabel);
                const ImVec2 btnLabelPos(
                    btnMin.x + (buttonWidth - btnLabelSize.x) * 0.5f,
                    btnMin.y + (buttonHeight - btnLabelSize.y) * 0.5f
                );
                ctx.drawList->AddText(font, textSize, btnLabelPos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), btnLabel);

                if (clicked) {
                    g_state.reset();
                    spawnFood();
                }
            }

            static void createInputBlocker(const ImVec2& boardPos, const ImVec2& boardSize) {
                const float margin = DP(60.0f);
                const ImVec2 blockerPos(boardPos.x - margin, boardPos.y - margin * 2.0f);
                const ImVec2 blockerSize(boardSize.x + margin * 2.0f, boardSize.y + margin * 3.0f);

                ImGui::SetNextWindowPos(blockerPos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(blockerSize, ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

                const ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoSavedSettings;

                if (ImGui::Begin("##SnakeInputBlocker", nullptr, flags)) {
                    ImGui::InvisibleButton("##blocker", blockerSize);
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
            }


            void DrawSnakeGame() {
                ensureRandomSeeded();

                if (!g_state.initialized) {
                    g_state.initialized = true;
                    g_state.reset();
                    spawnFood();
                }

                const ImGuiIO& io = ImGui::GetIO();
                updateGame(io.DeltaTime);

                if (g_state.width <= 0 || g_state.height <= 0) return;

                ImGuiViewport* viewport = ImGui::GetMainViewport();

                const float topPadding = DP(80.0f);
                const float bottomPadding = DP(40.0f);
                const float leftPadding = DP(20.0f);
                const float rightMargin = DP(20.0f);

                const float availableWidth = (viewport->Size.x * 0.5f) - leftPadding - rightMargin;
                const float availableHeight = viewport->Size.y - topPadding - bottomPadding;

                const float cellWidth = availableWidth / static_cast<float>(g_state.width);
                const float cellHeight = availableHeight / static_cast<float>(g_state.height);
                const float cellSize = std::min(cellWidth, cellHeight);

                const float boardWidth = cellSize * static_cast<float>(g_state.width);
                const float boardHeight = cellSize * static_cast<float>(g_state.height);

                const ImVec2 boardPos(
                    viewport->Pos.x + leftPadding,
                    viewport->Pos.y + topPadding
                );
                const ImVec2 boardSize(boardWidth, boardHeight);

                const ImVec4 accent = tsm::ui::GetAccentColor();

                RenderContext ctx{
                    ImGui::GetForegroundDrawList(),
                    boardPos,
                    boardSize,
                    cellSize,
                    DP(1.0f),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 1.0f)),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.7f)),
                    ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 1.0f)),
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.05f))
                };

                createInputBlocker(boardPos, boardSize);

                ctx.drawList->AddRectFilled(
                    boardPos,
                    ImVec2(boardPos.x + boardSize.x, boardPos.y + boardSize.y),
                    ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.05f, 0.95f)),
                    DP(8.0f)
                );

                ctx.drawList->AddRect(
                    boardPos,
                    ImVec2(boardPos.x + boardSize.x, boardPos.y + boardSize.y),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.4f)),
                    DP(8.0f),
                    0,
                    DP(2.0f)
                );

                renderGrid(ctx);
                renderFood(ctx);
                renderSnake(ctx);
                renderSwipeIndicator(ctx);
                renderHUD(ctx);

                processSwipeGesture(boardPos, boardSize);

                if (g_state.gameOver) {
                    renderGameOverOverlay(ctx, io);
                }
            }

        }
    }
}
