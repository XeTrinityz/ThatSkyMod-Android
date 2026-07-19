#include <ui/widgets/BreakoutGame.h>
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
#include <cmath>
#include <cstdio>

namespace tsm {
    namespace ui {
        namespace widgets {

            struct BreakoutPaddle {
                float x = 0.5f;
                float y = 0.9f;
                float width = 0.18f;
                float height = 0.035f;
            };

            struct BreakoutBall {
                float x = 0.5f;
                float y = 0.7f;
                float vx = 0.6f;
                float vy = -0.9f;
                float radius = 0.018f;

                struct TrailPoint {
                    float x, y;
                    float alpha;
                };
                std::vector<TrailPoint> trail;
            };

            struct BreakoutGameState {
                BreakoutPaddle paddle;
                BreakoutBall ball;

                int rows = 5;
                int cols = 8;
                std::vector<int> bricks;

                int level = 1;
                int score = 0;
                int highScore = 0;
                int lives = 3;
                bool initialized = false;
                bool running = false;
                bool gameOver = false;
                bool respawning = false;
                float respawnTimer = 0.0f;

                bool draggingPaddle = false;
                float paddleDragOffset = 0.0f;

                void reset();
                void resetBall();
            };

            static int getBrickHitsForRow(const BreakoutGameState& state, int row);
            static void initBricksForLevel(BreakoutGameState& state);

            static BreakoutGameState g_state;
            static bool g_randomSeeded = false;

            inline void ensureRandomSeeded() {
                if (!g_randomSeeded) {
                    std::srand(static_cast<unsigned int>(std::time(nullptr)));
                    g_randomSeeded = true;
                }
            }

            static int getBrickHitsForRow(const BreakoutGameState& state, int row) {
                int baseHits = 3 - (row / 2);
                if (baseHits < 1) baseHits = 1;

                int levelBonus = (state.level - 1) / 2;
                int hits = baseHits + levelBonus;
                if (hits > 5) hits = 5;
                return hits;
            }

            static void initBricksForLevel(BreakoutGameState& state) {
                const int total = state.rows * state.cols;
                state.bricks.assign(total, 0);

                for (int row = 0; row < state.rows; ++row) {
                    const int maxHitsForRow = getBrickHitsForRow(state, row);
                    for (int col = 0; col < state.cols; ++col) {
                        int index = row * state.cols + col;

                        bool present = true;
                        int pattern = state.level % 3;
                        if (pattern == 1) {
                            present = ((row + col) % 2 == 0);
                        }
                        else if (pattern == 2) {
                            int mid = state.cols / 2;
                            present = std::abs(col - mid) <= (state.cols / 2);
                        }

                        if (present) {
                            state.bricks[index] = maxHitsForRow;
                        }
                    }
                }
            }

            void BreakoutGameState::reset() {
                paddle.x = 0.5f;
                paddle.y = 0.9f;
                paddle.width = 0.18f;
                paddle.height = 0.035f;

                ball.x = 0.5f;
                ball.y = 0.7f;
                ball.vx = 0.6f;
                ball.vy = -0.9f;
                ball.radius = 0.018f;
                ball.trail.clear();

                if (score > highScore) {
                    highScore = score;
                }

                level = 1;
                score = 0;
                lives = 3;
                running = true;
                gameOver = false;
                respawning = false;
                respawnTimer = 0.0f;
                draggingPaddle = false;
                paddleDragOffset = 0.0f;

                initBricksForLevel(*this);
            }

            void BreakoutGameState::resetBall() {
                ball.x = 0.5f;
                ball.y = 0.7f;
                ball.vx = 0.6f * ((std::rand() % 2) ? 1.0f : -1.0f);
                ball.vy = -0.9f;
                ball.trail.clear();

                paddle.x = 0.5f;
                draggingPaddle = false;
                paddleDragOffset = 0.0f;
            }

            static void updateGame(float deltaTime) {
                if (!g_state.running || g_state.gameOver) return;

                if (g_state.respawning) {
                    g_state.respawnTimer -= deltaTime;
                    if (g_state.respawnTimer <= 0.0f) {
                        g_state.respawning = false;
                        g_state.resetBall();
                    }
                    return;
                }

                BreakoutBall& ball = g_state.ball;

                ball.trail.push_back({ ball.x, ball.y, 1.0f });
                if (ball.trail.size() > 6) {
                    ball.trail.erase(ball.trail.begin());
                }
                for (auto& point : ball.trail) {
                    point.alpha -= deltaTime * 3.0f;
                }

                ball.x += ball.vx * deltaTime;
                ball.y += ball.vy * deltaTime;

                if (ball.x - ball.radius <= 0.0f) {
                    ball.x = ball.radius;
                    ball.vx = std::abs(ball.vx);
                }
                else if (ball.x + ball.radius >= 1.0f) {
                    ball.x = 1.0f - ball.radius;
                    ball.vx = -std::abs(ball.vx);
                }

                if (ball.y - ball.radius <= 0.0f) {
                    ball.y = ball.radius;
                    ball.vy = std::abs(ball.vy);
                }

                if (ball.y - ball.radius > 1.0f) {
                    --g_state.lives;

                    if (g_state.lives <= 0) {
                        if (g_state.score > g_state.highScore) {
                            g_state.highScore = g_state.score;
                        }

                        g_state.gameOver = true;
                        g_state.running = false;
                    }
                    else {
                        g_state.respawning = true;
                        g_state.respawnTimer = 1.0f;
                    }
                    return;
                }

                const float paddleLeft = g_state.paddle.x - g_state.paddle.width * 0.5f;
                const float paddleRight = g_state.paddle.x + g_state.paddle.width * 0.5f;
                const float paddleTop = g_state.paddle.y - g_state.paddle.height * 0.5f;
                const float paddleBottom = g_state.paddle.y + g_state.paddle.height * 0.5f;

                const float ballLeft = ball.x - ball.radius;
                const float ballRight = ball.x + ball.radius;
                const float ballTop = ball.y - ball.radius;
                const float ballBottom = ball.y + ball.radius;

                if (ballBottom >= paddleTop && ballTop <= paddleBottom &&
                    ballRight >= paddleLeft && ballLeft <= paddleRight &&
                    ball.vy > 0.0f) {
                    const char* paddleSounds[] = { "StartVideo", "tapDebugClick", "TurnDial" };
                    const int soundIndex = std::rand() % 3;
                    tsm::lua::helpers::PlaySound2D(paddleSounds[soundIndex], 5.0f);
                    const float paddleCenter = g_state.paddle.x;
                    const float hitOffset = (ball.x - paddleCenter) / (g_state.paddle.width * 0.5f);
                    ball.y = paddleTop - ball.radius;
                    ball.vy = -std::abs(ball.vy);
                    ball.vx += hitOffset * 0.8f;
                    ball.vx = std::max(-1.5f, std::min(1.5f, ball.vx));
                }

                const float bricksTop = 0.12f;
                const float bricksHeight = 0.4f;
                const float marginX = 0.05f;

                const float rowHeight = bricksHeight / static_cast<float>(g_state.rows);
                const float colWidth = (1.0f - marginX * 2.0f) / static_cast<float>(g_state.cols);
                const float brickHeight = rowHeight * 0.8f;
                const float brickWidth = colWidth * 0.9f;

                bool hitBrick = false;
                int bricksLeft = 0;

                for (int row = 0; row < g_state.rows; ++row) {
                    for (int col = 0; col < g_state.cols; ++col) {
                        const int index = row * g_state.cols + col;
                        int& hp = g_state.bricks[index];
                        if (hp <= 0) continue;

                        if (!hitBrick) {
                            const float brickLeft = marginX + col * colWidth + (colWidth - brickWidth) * 0.5f;
                            const float brickRight = brickLeft + brickWidth;
                            const float brickTop = bricksTop + row * rowHeight;
                            const float brickBottom = brickTop + brickHeight;

                            if (ballRight >= brickLeft && ballLeft <= brickRight &&
                                ballBottom >= brickTop && ballTop <= brickBottom) {
                                hitBrick = true;

                                --hp;

                                if (hp <= 0) {
                                    tsm::lua::helpers::PlaySound2D("ObjectSelect", 5.0f);
                                }

                                const int rowScore = (g_state.rows - row) * 10;
                                g_state.score += rowScore;

                                if (std::abs((ball.y - ball.vy * deltaTime) - brickTop) < std::abs((ball.y - ball.vy * deltaTime) - brickBottom)) {
                                    if (ball.vy > 0.0f) ball.y = brickTop - ball.radius;
                                    else ball.y = brickBottom + ball.radius;
                                    ball.vy = -ball.vy;
                                }
                                else {
                                    if (ball.vx > 0.0f) ball.x = brickLeft - ball.radius;
                                    else ball.x = brickRight + ball.radius;
                                    ball.vx = -ball.vx;
                                }
                            }
                        }

                        if (hp > 0) {
                            ++bricksLeft;
                        }
                    }
                }

                if (bricksLeft == 0) {
                    ++g_state.level;

                    float speedScale = 1.0f + 0.08f * static_cast<float>(g_state.level - 1);

                    g_state.ball.x = 0.5f;
                    g_state.ball.y = 0.7f;
                    g_state.ball.vx = 0.6f * speedScale * ((std::rand() % 2) ? 1.0f : -1.0f);
                    g_state.ball.vy = -0.9f * speedScale;
                    g_state.ball.trail.clear();

                    g_state.paddle.x = 0.5f;
                    g_state.draggingPaddle = false;
                    g_state.paddleDragOffset = 0.0f;

                    initBricksForLevel(g_state);

                    return;
                }
            }

            struct RenderContext {
                ImDrawList* drawList;
                ImVec2 boardPos;
                ImVec2 boardSize;
                ImU32 paddleColor;
                ImU32 ballColor;
                ImU32 brickColor;
                ImU32 borderColor;

                ImVec2 toScreen(float normalizedX, float normalizedY) const {
                    return ImVec2(
                        boardPos.x + normalizedX * boardSize.x,
                        boardPos.y + normalizedY * boardSize.y
                    );
                }

                float toScreenX(float normalized) const {
                    return normalized * boardSize.x;
                }
            };

            static void renderBackground(const RenderContext& ctx) {
                ctx.drawList->AddRectFilled(
                    ctx.boardPos,
                    ImVec2(ctx.boardPos.x + ctx.boardSize.x, ctx.boardPos.y + ctx.boardSize.y),
                    ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.08f, 0.95f)),
                    DP(8.0f)
                );
            }

            static void renderBricks(const RenderContext& ctx) {
                const float bricksTop = 0.12f;
                const float bricksHeight = 0.4f;
                const float marginX = 0.05f;

                const float rowHeight = bricksHeight / static_cast<float>(g_state.rows);
                const float colWidth = (1.0f - marginX * 2.0f) / static_cast<float>(g_state.cols);
                const float brickHeight = rowHeight * 0.8f;
                const float brickWidth = colWidth * 0.9f;

                const ImVec4 accent = tsm::ui::GetAccentColor();

                for (int row = 0; row < g_state.rows; ++row) {
                    const int maxHitsForRow = getBrickHitsForRow(g_state, row);

                    for (int col = 0; col < g_state.cols; ++col) {
                        const int index = row * g_state.cols + col;
                        int hp = g_state.bricks[index];
                        if (hp <= 0) continue;

                        float t = (maxHitsForRow > 0) ? (static_cast<float>(hp) / static_cast<float>(maxHitsForRow)) : 0.0f;
                        float brightness = 0.5f + 0.5f * t;

                        ImVec4 color = ImVec4(accent.x * brightness, accent.y * brightness, accent.z * brightness, 0.9f);
                        ImU32 fillColor = ImGui::GetColorU32(color);

                        const float brickLeft = marginX + col * colWidth + (colWidth - brickWidth) * 0.5f;
                        const float brickRight = brickLeft + brickWidth;
                        const float brickTop = bricksTop + row * rowHeight;
                        const float brickBottom = brickTop + brickHeight;

                        const ImVec2 min = ctx.toScreen(brickLeft, brickTop);
                        const ImVec2 max = ctx.toScreen(brickRight, brickBottom);

                        if (brightness > 0.7f) {
                            const ImVec2 glowMin(min.x - DP(2.0f), min.y - DP(2.0f));
                            const ImVec2 glowMax(max.x + DP(2.0f), max.y + DP(2.0f));
                            ctx.drawList->AddRectFilled(glowMin, glowMax,
                                ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.15f)), DP(5.0f));
                        }

                        ctx.drawList->AddRectFilled(min, max, fillColor, DP(4.0f));
                        ctx.drawList->AddRect(min, max, ctx.borderColor, DP(4.0f), 0, DP(1.5f));
                    }
                }
            }

            static void renderPaddle(const RenderContext& ctx) {
                const float left = g_state.paddle.x - g_state.paddle.width * 0.5f;
                const float right = g_state.paddle.x + g_state.paddle.width * 0.5f;
                const float top = g_state.paddle.y - g_state.paddle.height * 0.5f;
                const float bottom = g_state.paddle.y + g_state.paddle.height * 0.5f;

                const ImVec2 min = ctx.toScreen(left, top);
                const ImVec2 max = ctx.toScreen(right, bottom);

                const ImVec4 accent = tsm::ui::GetAccentColor();
                const ImVec2 glowMin(min.x - DP(3.0f), min.y - DP(3.0f));
                const ImVec2 glowMax(max.x + DP(3.0f), max.y + DP(3.0f));
                ctx.drawList->AddRectFilled(glowMin, glowMax,
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.2f)), DP(8.0f));

                ctx.drawList->AddRectFilled(min, max, ctx.paddleColor, DP(6.0f));
            }

            static void renderBall(const RenderContext& ctx) {
                if (g_state.respawning) return;

                for (size_t i = 0; i < g_state.ball.trail.size(); ++i) {
                    const auto& point = g_state.ball.trail[i];
                    if (point.alpha <= 0.0f) continue;

                    const ImVec2 center = ctx.toScreen(point.x, point.y);
                    const float radius = ctx.toScreenX(g_state.ball.radius) * 0.6f;
                    const float sizeRatio = static_cast<float>(i) / g_state.ball.trail.size();

                    ctx.drawList->AddCircleFilled(center, radius * sizeRatio,
                        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, point.alpha * 0.3f)), 16);
                }

                const ImVec2 center = ctx.toScreen(g_state.ball.x, g_state.ball.y);
                const float radius = ctx.toScreenX(g_state.ball.radius);

                ctx.drawList->AddCircleFilled(center, radius * 1.5f,
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.2f)), 32);

                ctx.drawList->AddCircleFilled(center, radius, ctx.ballColor, 32);
            }

            static void renderHUD(const RenderContext& ctx) {
                ImFont* font = ImGui::GetFont();
                const float fontSize = DP(20.0f);
                const float padding = DP(16.0f);

                const int displayBest = std::max(g_state.score, g_state.highScore);

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d/%d",
                    tsm::ui::i18n::Tr("Score"),
                    g_state.score,
                    displayBest);

                const ImVec2 scorePos(ctx.boardPos.x + padding, ctx.boardPos.y + padding);
                ctx.drawList->AddText(font, fontSize * 0.75f, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)), scoreBuffer);

                const float heartSize = DP(24.0f);
                const float heartSpacing = DP(8.0f);
                const float totalWidth = g_state.lives * heartSize + (g_state.lives - 1) * heartSpacing;

                float heartX = ctx.boardPos.x + ctx.boardSize.x - padding - totalWidth;
                const float heartY = ctx.boardPos.y + padding;

                UIIcon heartIcon{};
                IconLoader::getUIIcon("UiMiscHeart", &heartIcon);

                for (int i = 0; i < g_state.lives; ++i) {
                    const ImVec2 iconMin(heartX, heartY);
                    const ImVec2 iconMax(heartX + heartSize, heartY + heartSize);

                    if (heartIcon.textureId != IL_NO_TEXTURE) {
                        const ImU32 red = ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                        ctx.drawList->AddImage(heartIcon.textureId, iconMin, iconMax,
                            heartIcon.uv0, heartIcon.uv1, red);
                    }
                    else {
                        const ImVec2 center(heartX + heartSize * 0.5f, heartY + heartSize * 0.5f);
                        ctx.drawList->AddCircleFilled(center, heartSize * 0.4f,
                            ImGui::GetColorU32(ImVec4(1.0f, 0.2f, 0.2f, 1.0f)), 16);
                    }

                    heartX += heartSize + heartSpacing;
                }

                char levelBuffer[32];
                std::snprintf(levelBuffer, sizeof(levelBuffer), "%s: %d",
                    tsm::ui::i18n::Tr("Level"), g_state.level);

                const ImVec2 levelPos(
                    ctx.boardPos.x + padding,
                    ctx.boardPos.y + padding + fontSize * 0.75f + DP(4.0f)
                );
                ctx.drawList->AddText(font, fontSize * 0.75f, levelPos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.7f)), levelBuffer);

                if (g_state.respawning) {
                    const char* msg = tsm::ui::i18n::Tr("Get Ready!");
                    const ImVec2 msgSize = font->CalcTextSizeA(fontSize * 1.2f, FLT_MAX, 0.0f, msg);
                    const ImVec2 msgPos(
                        ctx.boardPos.x + (ctx.boardSize.x - msgSize.x) * 0.5f,
                        ctx.boardPos.y + ctx.boardSize.y * 0.5f
                    );

                    ctx.drawList->AddText(font, fontSize * 1.2f,
                        ImVec2(msgPos.x + 2, msgPos.y + 2),
                        ImGui::GetColorU32(ImVec4(0, 0, 0, 0.8f)), msg);
                    ctx.drawList->AddText(font, fontSize * 1.2f, msgPos,
                        ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), msg);
                }

                if (!g_state.gameOver && g_state.score == 0 && !g_state.respawning) {
                    const float hintAlpha = std::max(0.0f, 1.0f - static_cast<float>(ImGui::GetTime()) / 6.0f);
                    if (hintAlpha > 0.0f) {
                        const char* hint = tsm::ui::i18n::Tr("Drag paddle to move");
                        const ImVec2 hintSize = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, hint);
                        const ImVec2 hintPos(
                            ctx.boardPos.x + (ctx.boardSize.x - hintSize.x) * 0.5f,
                            ctx.boardPos.y + ctx.boardSize.y * 0.75f
                        );

                        ctx.drawList->AddText(font, fontSize * 0.85f,
                            ImVec2(hintPos.x + 2, hintPos.y + 2),
                            ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f * hintAlpha)), hint);
                        ctx.drawList->AddText(font, fontSize * 0.85f, hintPos,
                            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f * hintAlpha)), hint);
                    }
                }
            }

            static void renderGameOverOverlay(const RenderContext& ctx, const ImGuiIO& io) {
                const char* title = tsm::ui::i18n::Tr("Game Over");

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d",
                    tsm::ui::i18n::Tr("Final Score"), g_state.score);

                char levelBuffer[64];
                std::snprintf(levelBuffer, sizeof(levelBuffer), "%s: %d",
                    tsm::ui::i18n::Tr("Level Reached"), g_state.level);

                char highScoreBuffer[64];
                const bool newRecord = g_state.score == g_state.highScore && g_state.highScore > 0;
                if (newRecord) {
                    std::snprintf(highScoreBuffer, sizeof(highScoreBuffer), "%s!",
                        tsm::ui::i18n::Tr("New Record"));
                }
                else if (g_state.highScore > 0) {
                    std::snprintf(highScoreBuffer, sizeof(highScoreBuffer), "%s: %d",
                        tsm::ui::i18n::Tr("Best"), g_state.highScore);
                }
                else {
                    highScoreBuffer[0] = '\0';
                }

                ImFont* font = ImGui::GetFont();
                const float titleSize = DP(28.0f);
                const float textSize = DP(20.0f);

                const ImVec2 titleDim = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
                const ImVec2 scoreDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, scoreBuffer);
                const ImVec2 levelDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, levelBuffer);
                const ImVec2 highScoreDim = highScoreBuffer[0] ?
                    font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, highScoreBuffer) : ImVec2(0, 0);

                const float padding = DP(24.0f);
                const float buttonWidth = DP(180.0f);
                const float buttonHeight = DP(48.0f);

                const float boxWidth = std::max({ titleDim.x, scoreDim.x, levelDim.x, highScoreDim.x, buttonWidth }) + padding * 2.0f;
                const float boxHeight = titleDim.y + scoreDim.y + levelDim.y +
                    (highScoreBuffer[0] ? highScoreDim.y + padding * 0.5f : 0.0f) +
                    buttonHeight + padding * 5.0f;

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

                const ImVec2 levelPos(boxMin.x + (boxWidth - levelDim.x) * 0.5f, cursorY);
                ctx.drawList->AddText(font, textSize, levelPos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.9f)), levelBuffer);

                cursorY += levelDim.y + padding * 0.5f;

                if (highScoreBuffer[0]) {
                    const ImVec2 highScorePos(boxMin.x + (boxWidth - highScoreDim.x) * 0.5f, cursorY);
                    const ImVec4 highScoreColor = newRecord ?
                        ImVec4(1, 0.85f, 0.2f, 1) : ImVec4(1, 1, 1, 0.7f);
                    ctx.drawList->AddText(font, textSize, highScorePos,
                        ImGui::GetColorU32(highScoreColor), highScoreBuffer);
                    cursorY += highScoreDim.y + padding * 0.5f;
                }

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
                }
            }

            static void createInputBlocker(const ImVec2& boardPos, const ImVec2& boardSize) {
                const ImGuiIO& io = ImGui::GetIO();
                const ImVec2 mousePos = io.MousePos;

                const float margin = DP(60.0f);
                const ImVec2 blockerPos(boardPos.x - margin, boardPos.y - margin);
                const ImVec2 blockerSize(boardSize.x + margin * 2.0f, boardSize.y + margin * 2.0f);

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

                if (ImGui::Begin("##BreakoutInputBlocker", nullptr, flags)) {
                    ImGui::InvisibleButton("##blocker", blockerSize);

                    const bool pointerDown = ImGui::IsMouseDown(0);
                    const float paddleCenterX = boardPos.x + g_state.paddle.x * boardSize.x;
                    const float paddleCenterY = boardPos.y + g_state.paddle.y * boardSize.y;
                    const float paddleWidth = g_state.paddle.width * boardSize.x;
                    const float paddleHeight = g_state.paddle.height * boardSize.y;

                    const float paddleLeft = paddleCenterX - paddleWidth * 0.5f;
                    const float paddleRight = paddleCenterX + paddleWidth * 0.5f;
                    const float paddleTop = paddleCenterY - paddleHeight * 0.5f;
                    const float paddleBottom = paddleCenterY + paddleHeight * 0.5f;

                    if (pointerDown && !g_state.gameOver) {
                        if (!g_state.draggingPaddle) {
                            const bool overPaddle = (mousePos.x >= paddleLeft && mousePos.x <= paddleRight &&
                                mousePos.y >= paddleTop && mousePos.y <= paddleBottom);
                            const bool overBottomHalf = (mousePos.x >= boardPos.x &&
                                mousePos.x <= boardPos.x + boardSize.x &&
                                mousePos.y >= boardPos.y + boardSize.y * 0.5f &&
                                mousePos.y <= boardPos.y + boardSize.y);

                            if (overPaddle || overBottomHalf) {
                                g_state.draggingPaddle = true;
                                if (overPaddle) {
                                    g_state.paddleDragOffset = mousePos.x - paddleCenterX;
                                }
                                else {
                                    g_state.paddleDragOffset = 0.0f;
                                }
                            }
                        }
                    }
                    else {
                        g_state.draggingPaddle = false;
                    }

                    if (g_state.draggingPaddle) {
                        float targetCenterX = mousePos.x - g_state.paddleDragOffset;
                        float normalizedX = (targetCenterX - boardPos.x) / boardSize.x;

                        const float minX = g_state.paddle.width * 0.5f;
                        const float maxX = 1.0f - g_state.paddle.width * 0.5f;
                        normalizedX = std::max(minX, std::min(maxX, normalizedX));

                        g_state.paddle.x = normalizedX;
                    }
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
            }

            void DrawBreakoutGame() {
                ensureRandomSeeded();

                if (!g_state.initialized) {
                    g_state.initialized = true;
                    g_state.reset();
                }

                const ImGuiIO& io = ImGui::GetIO();
                updateGame(io.DeltaTime);

                ImGuiViewport* viewport = ImGui::GetMainViewport();

                const float topPadding = DP(80.0f);
                const float bottomPadding = DP(40.0f);
                const float leftPadding = DP(20.0f);
                const float rightMargin = DP(20.0f);

                const float availableWidth = (viewport->Size.x * 0.5f) - leftPadding - rightMargin;
                const float availableHeight = viewport->Size.y - topPadding - bottomPadding;

                const float boardWidth = availableWidth;
                const float boardHeight = availableHeight;

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
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.95f)),
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.95f)),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.85f)),
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f))
                };

                createInputBlocker(boardPos, boardSize);

                renderBackground(ctx);

                ctx.drawList->AddRect(
                    boardPos,
                    ImVec2(boardPos.x + boardSize.x, boardPos.y + boardSize.y),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.4f)),
                    DP(8.0f),
                    0,
                    DP(2.0f)
                );

                renderBricks(ctx);
                renderPaddle(ctx);
                renderBall(ctx);
                renderHUD(ctx);

                if (g_state.gameOver) {
                    renderGameOverOverlay(ctx, io);
                }
            }

        }
    }
}
