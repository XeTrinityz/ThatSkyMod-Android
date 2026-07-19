#include <ui/widgets/PongGame.h>
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


            struct PongPaddle {
                float y = 0.5f;
                float height = 0.15f;
                float width = 0.025f;
                float speed = 1.2f;
                int score = 0;
            };

            struct PongBall {
                float x = 0.5f;
                float y = 0.5f;
                float vx = 0.0f;
                float vy = 0.0f;
                float radius = 0.018f;
                float baseSpeed = 0.55f;
                float speedMultiplier = 1.0f;
                float collisionCooldown = 0.0f;

                struct TrailPoint {
                    float x, y;
                    float alpha;
                };
                std::vector<TrailPoint> trail;
            };

            struct PongGameState {
                PongPaddle leftPaddle;
                PongPaddle rightPaddle;
                PongBall ball;

                bool initialized = false;
                bool running = false;
                bool gameOver = false;
                bool showingScore = false;
                float scoreDisplayTimer = 0.0f;

                float aiReactionSpeed = 0.48f;
                float aiError = 0.08f;

                bool draggingLeftPaddle = false;
                float paddleDragOffset = 0.0f;

                void reset();
                void resetBall(bool towardsLeft);
                void updateAI(float deltaTime);
                bool checkPaddleCollision(const PongPaddle& paddle);
            };


            static PongGameState g_state;
            static bool g_randomSeeded = false;

            inline void ensureRandomSeeded() {
                if (!g_randomSeeded) {
                    std::srand(static_cast<unsigned int>(std::time(nullptr)));
                    g_randomSeeded = true;
                }
            }

            inline float randomFloat(float min, float max) {
                return min + (max - min) * (std::rand() / static_cast<float>(RAND_MAX));
            }

            void PongGameState::reset() {
                leftPaddle.y = 0.5f;
                leftPaddle.score = 0;
                rightPaddle.y = 0.5f;
                rightPaddle.score = 0;

                running = true;
                gameOver = false;
                showingScore = false;
                scoreDisplayTimer = 0.0f;

                draggingLeftPaddle = false;
                paddleDragOffset = 0.0f;

                resetBall(std::rand() % 2 == 0);
            }

            void PongGameState::resetBall(bool towardsLeft) {
                ball.x = 0.5f;
                ball.y = 0.5f;
                ball.speedMultiplier = 1.0f;
                ball.trail.clear();

                const float angle = randomFloat(-0.7f, 0.7f);
                const float direction = towardsLeft ? -1.0f : 1.0f;

                ball.vx = std::cos(angle) * ball.baseSpeed * direction;
                ball.vy = std::sin(angle) * ball.baseSpeed;
            }

            void PongGameState::updateAI(float deltaTime) {
                if (ball.vx > 0) {
                    const float targetY = ball.y + randomFloat(-aiError, aiError);
                    const float paddleCenter = rightPaddle.y;

                    const float diff = targetY - paddleCenter;
                    const float moveAmount = rightPaddle.speed * deltaTime * aiReactionSpeed;

                    if (std::abs(diff) > 0.02f) {
                        if (diff > 0) {
                            rightPaddle.y = std::min(1.0f - rightPaddle.height * 0.5f,
                                rightPaddle.y + moveAmount);
                        }
                        else {
                            rightPaddle.y = std::max(rightPaddle.height * 0.5f,
                                rightPaddle.y - moveAmount);
                        }
                    }
                }
            }

            bool PongGameState::checkPaddleCollision(const PongPaddle& paddle) {
                const float ballLeft = ball.x - ball.radius;
                const float ballRight = ball.x + ball.radius;
                const float ballTop = ball.y - ball.radius;
                const float ballBottom = ball.y + ball.radius;

                float paddleLeft, paddleRight;
                if (&paddle == &leftPaddle) {
                    paddleLeft = 0.05f;
                    paddleRight = 0.05f + paddle.width;
                }
                else {
                    paddleLeft = 0.95f - paddle.width;
                    paddleRight = 0.95f;
                }

                const float paddleTop = paddle.y - paddle.height * 0.5f;
                const float paddleBottom = paddle.y + paddle.height * 0.5f;

                return (ballRight >= paddleLeft && ballLeft <= paddleRight &&
                    ballBottom >= paddleTop && ballTop <= paddleBottom);
            }


            static void updateGame(float deltaTime) {
                if (!g_state.running || g_state.gameOver) return;

                if (g_state.showingScore) {
                    g_state.scoreDisplayTimer -= deltaTime;
                    if (g_state.scoreDisplayTimer <= 0.0f) {
                        g_state.showingScore = false;
                        const bool leftScored = g_state.ball.x > 0.5f;
                        g_state.resetBall(leftScored);
                    }
                    return;
                }

                if (g_state.ball.collisionCooldown > 0.0f) {
                    g_state.ball.collisionCooldown -= deltaTime;
                }

                g_state.updateAI(deltaTime);

                g_state.ball.trail.push_back({ g_state.ball.x, g_state.ball.y, 1.0f });
                if (g_state.ball.trail.size() > 8) {
                    g_state.ball.trail.erase(g_state.ball.trail.begin());
                }
                for (auto& point : g_state.ball.trail) {
                    point.alpha -= deltaTime * 3.0f;
                }

                g_state.ball.x += g_state.ball.vx * deltaTime * g_state.ball.speedMultiplier;
                g_state.ball.y += g_state.ball.vy * deltaTime * g_state.ball.speedMultiplier;

                if (g_state.ball.y - g_state.ball.radius <= 0.0f) {
                    g_state.ball.y = g_state.ball.radius;
                    g_state.ball.vy = -g_state.ball.vy;
                }
                else if (g_state.ball.y + g_state.ball.radius >= 1.0f) {
                    g_state.ball.y = 1.0f - g_state.ball.radius;
                    g_state.ball.vy = -g_state.ball.vy;
                }

                if (g_state.ball.collisionCooldown <= 0.0f) {
                    bool hitLeftPaddle = g_state.ball.vx < 0 && g_state.checkPaddleCollision(g_state.leftPaddle);
                    bool hitRightPaddle = g_state.ball.vx > 0 && g_state.checkPaddleCollision(g_state.rightPaddle);

                    if (hitLeftPaddle || hitRightPaddle) {
                        const char* paddleSounds[] = { "StartVideo", "tapDebugClick", "TurnDial" };
                        const int soundIndex = std::rand() % 3;
                        tsm::lua::helpers::PlaySound2D(paddleSounds[soundIndex], 5.0f);
                        g_state.ball.vx = -g_state.ball.vx;

                        const PongPaddle& hitPaddle = hitLeftPaddle ? g_state.leftPaddle : g_state.rightPaddle;
                        const float hitOffset = g_state.ball.y - hitPaddle.y;
                        const float normalizedOffset = hitOffset / (hitPaddle.height * 0.5f);
                        g_state.ball.vy += normalizedOffset * 0.3f;

                        g_state.ball.vy = std::max(-1.2f, std::min(1.2f, g_state.ball.vy));

                        g_state.ball.speedMultiplier = std::min(1.8f, g_state.ball.speedMultiplier + 0.08f);

                        g_state.ball.collisionCooldown = 0.1f;

                        if (hitLeftPaddle) {
                            g_state.ball.x = 0.05f + g_state.leftPaddle.width + g_state.ball.radius + 0.005f;
                        }
                        else {
                            g_state.ball.x = 0.95f - g_state.rightPaddle.width - g_state.ball.radius - 0.005f;
                        }
                    }
                }

                if (g_state.ball.x - g_state.ball.radius <= 0.0f) {
                    ++g_state.rightPaddle.score;
                    g_state.showingScore = true;
                    g_state.scoreDisplayTimer = 1.0f;

                    if (g_state.rightPaddle.score >= 5) {
                        g_state.gameOver = true;
                        g_state.running = false;
                    }
                }
                else if (g_state.ball.x + g_state.ball.radius >= 1.0f) {
                    ++g_state.leftPaddle.score;
                    g_state.showingScore = true;
                    g_state.scoreDisplayTimer = 1.0f;

                    if (g_state.leftPaddle.score >= 5) {
                        g_state.gameOver = true;
                        g_state.running = false;
                    }
                }
            }


            struct RenderContext {
                ImDrawList* drawList;
                ImVec2 boardPos;
                ImVec2 boardSize;
                ImU32 paddleColor;
                ImU32 ballColor;
                ImU32 lineColor;

                ImVec2 toScreen(float normalizedX, float normalizedY) const {
                    return ImVec2(
                        boardPos.x + normalizedX * boardSize.x,
                        boardPos.y + normalizedY * boardSize.y
                    );
                }

                float toScreenX(float normalized) const {
                    return normalized * boardSize.x;
                }

                float toScreenY(float normalized) const {
                    return normalized * boardSize.y;
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

            static void renderCenterLine(const RenderContext& ctx) {
                const float centerX = ctx.boardPos.x + ctx.boardSize.x * 0.5f;
                const float dashHeight = ctx.toScreenY(0.04f);
                const float gapHeight = ctx.toScreenY(0.025f);

                float y = ctx.boardPos.y;
                while (y < ctx.boardPos.y + ctx.boardSize.y) {
                    const ImVec2 dashMin(centerX - DP(2.0f), y);
                    const ImVec2 dashMax(centerX + DP(2.0f), std::min(y + dashHeight, ctx.boardPos.y + ctx.boardSize.y));

                    ctx.drawList->AddRectFilled(dashMin, dashMax, ctx.lineColor);
                    y += dashHeight + gapHeight;
                }
            }

            static void renderPaddle(const RenderContext& ctx, const PongPaddle& paddle, bool isLeft) {
                const float x = isLeft ? 0.05f : (0.95f - paddle.width);
                const float top = paddle.y - paddle.height * 0.5f;
                const float bottom = paddle.y + paddle.height * 0.5f;

                const ImVec2 min = ctx.toScreen(x, top);
                const ImVec2 max = ctx.toScreen(x + paddle.width, bottom);

                const ImVec4 accent = tsm::ui::GetAccentColor();
                ctx.drawList->AddRectFilled(min, max,
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.2f)), DP(6.0f));

                const ImVec2 innerMin(min.x + DP(2.0f), min.y + DP(2.0f));
                const ImVec2 innerMax(max.x - DP(2.0f), max.y - DP(2.0f));
                ctx.drawList->AddRectFilled(innerMin, innerMax, ctx.paddleColor, DP(4.0f));
            }

            static void renderBall(const RenderContext& ctx) {
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

                ctx.drawList->AddCircleFilled(center, radius * 1.4f,
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.2f)), 32);

                ctx.drawList->AddCircleFilled(center, radius, ctx.ballColor, 32);
            }

            static void renderHUD(const RenderContext& ctx) {
                ImFont* font = ImGui::GetFont();
                const float fontSize = DP(20.0f);
                const float padding = DP(16.0f);

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%d - %d",
                    g_state.leftPaddle.score,
                    g_state.rightPaddle.score);

                const ImVec2 scorePos(ctx.boardPos.x + padding, ctx.boardPos.y + padding);
                ctx.drawList->AddText(font, fontSize * 0.75f, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)), scoreBuffer);

                if (!g_state.gameOver && g_state.leftPaddle.score == 0 && g_state.rightPaddle.score == 0) {
                    const float hintAlpha = std::max(0.0f, 1.0f - static_cast<float>(ImGui::GetTime()) / 6.0f);
                    if (hintAlpha > 0.0f) {
                        const char* hint = tsm::ui::i18n::Tr("Drag paddle to move");
                        const ImVec2 hintSize = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, hint);
                        const ImVec2 hintPos(
                            ctx.boardPos.x + (ctx.boardSize.x - hintSize.x) * 0.5f,
                            ctx.boardPos.y + ctx.boardSize.y - fontSize * 1.5f - padding
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
                const bool playerWon = g_state.leftPaddle.score >= 5;
                const char* title = playerWon ? tsm::ui::i18n::Tr("You Win!") : tsm::ui::i18n::Tr("AI Wins!");

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d - %d",
                    tsm::ui::i18n::Tr("Final Score"),
                    g_state.leftPaddle.score, g_state.rightPaddle.score);

                ImFont* font = ImGui::GetFont();
                const float titleSize = DP(28.0f);
                const float textSize = DP(20.0f);

                const ImVec2 titleDim = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
                const ImVec2 scoreDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, scoreBuffer);

                const float padding = DP(24.0f);
                const float buttonWidth = DP(180.0f);
                const float buttonHeight = DP(48.0f);

                const float boxWidth = std::max({ titleDim.x, scoreDim.x, buttonWidth }) + padding * 2.0f;
                const float boxHeight = titleDim.y + scoreDim.y +
                    buttonHeight + padding * 4.5f;

                const ImVec2 boxMin(
                    ctx.boardPos.x + (ctx.boardSize.x - boxWidth) * 0.5f,
                    ctx.boardPos.y + (ctx.boardSize.y - boxHeight) * 0.5f
                );
                const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);

                const ImVec4 accent = tsm::ui::GetAccentColor();
                const ImVec4 boxBg = ImVec4(0.05f, 0.05f, 0.05f, 0.95f);
                const ImVec4 boxBorder = ImVec4(accent.x, accent.y, accent.z, 0.8f);

                ctx.drawList->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(boxBg), DP(12.0f));
                ctx.drawList->AddRect(boxMin, boxMax,
                    ImGui::GetColorU32(boxBorder), DP(12.0f), 0, DP(2.0f));

                float cursorY = boxMin.y + padding;

                const ImVec2 titlePos(boxMin.x + (boxWidth - titleDim.x) * 0.5f, cursorY);
                const ImVec4 titleColor = playerWon ?
                    ImVec4(0.3f, 1.0f, 0.3f, 1) : ImVec4(1, 0.3f, 0.3f, 1);
                ctx.drawList->AddText(font, titleSize, titlePos,
                    ImGui::GetColorU32(titleColor), title);

                cursorY += titleDim.y + padding;

                const ImVec2 scorePos(boxMin.x + (boxWidth - scoreDim.x) * 0.5f, cursorY);
                ctx.drawList->AddText(font, textSize, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), scoreBuffer);

                cursorY += scoreDim.y + padding;

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

                if (ImGui::Begin("##PongInputBlocker", nullptr, flags)) {
                    ImGui::InvisibleButton("##blocker", blockerSize);

                    const bool pointerDown = ImGui::IsMouseDown(0);
                    const float paddleScreenLeft = boardPos.x + 0.05f * boardSize.x;
                    const float paddleWidth = g_state.leftPaddle.width * boardSize.x;
                    const float paddleTop = boardPos.y + (g_state.leftPaddle.y - g_state.leftPaddle.height * 0.5f) * boardSize.y;
                    const float paddleBottom = boardPos.y + (g_state.leftPaddle.y + g_state.leftPaddle.height * 0.5f) * boardSize.y;
                    const float paddleRight = paddleScreenLeft + paddleWidth;

                    if (pointerDown && !g_state.gameOver) {
                        if (!g_state.draggingLeftPaddle) {
                            const bool overPaddle = (mousePos.x >= paddleScreenLeft && mousePos.x <= paddleRight &&
                                mousePos.y >= paddleTop && mousePos.y <= paddleBottom);
                            const bool overLeftHalf = (mousePos.x >= boardPos.x &&
                                mousePos.x <= boardPos.x + boardSize.x * 0.5f &&
                                mousePos.y >= boardPos.y &&
                                mousePos.y <= boardPos.y + boardSize.y);

                            if (overPaddle || overLeftHalf) {
                                g_state.draggingLeftPaddle = true;
                                if (overPaddle) {
                                    const float paddleCenterY = (paddleTop + paddleBottom) * 0.5f;
                                    g_state.paddleDragOffset = mousePos.y - paddleCenterY;
                                }
                                else {
                                    g_state.paddleDragOffset = 0.0f;
                                }
                            }
                        }
                    }
                    else {
                        g_state.draggingLeftPaddle = false;
                    }

                    if (g_state.draggingLeftPaddle) {
                        float paddleCenterScreenY = mousePos.y - g_state.paddleDragOffset;
                        float normalizedY = (paddleCenterScreenY - boardPos.y) / boardSize.y;

                        const float minY = g_state.leftPaddle.height * 0.5f;
                        const float maxY = 1.0f - g_state.leftPaddle.height * 0.5f;
                        normalizedY = std::max(minY, std::min(maxY, normalizedY));

                        g_state.leftPaddle.y = normalizedY;
                    }
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
            }


            void DrawPongGame() {
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
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.25f))
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

                renderCenterLine(ctx);
                renderPaddle(ctx, g_state.leftPaddle, true);
                renderPaddle(ctx, g_state.rightPaddle, false);
                renderBall(ctx);
                renderHUD(ctx);

                if (g_state.gameOver) {
                    renderGameOverOverlay(ctx, io);
                }
            }

        }
    }
}
