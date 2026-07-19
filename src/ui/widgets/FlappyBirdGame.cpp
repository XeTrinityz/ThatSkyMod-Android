#include <ui/widgets/FlappyBirdGame.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <iconloader/IconLoader.h>
#include <imgui/imgui.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace tsm {
    namespace ui {
        namespace widgets {


            struct FlappyPipe {
                float x;
                float gapY;
                bool scored;
            };

            struct FlappyBirdState {
                float birdY = 0.5f;
                float birdVelocity = 0.0f;
                float birdSize = 0.07f;

                float gravity = 1.6f;
                float flapStrength = -0.55f;
                float pipeSpeed = 0.3f;
                float pipeWidth = 0.14f;
                float pipeGapSize = 0.28f;
                float pipeSpacing = 0.65f;

                std::vector<FlappyPipe> pipes;
                float nextPipeSpawn = 1.0f;

                int score = 0;
                int highScore = 0;
                bool initialized = false;
                bool running = false;
                bool gameOver = false;

                float flapAnimProgress = 0.0f;
                float deathAnimProgress = 0.0f;

                void reset();
                void spawnPipe();
                bool checkCollision() const;
            };


            static FlappyBirdState g_state;
            static bool g_randomSeeded = false;

            inline void ensureRandomSeeded() {
                if (!g_randomSeeded) {
                    std::srand(static_cast<unsigned int>(std::time(nullptr)));
                    g_randomSeeded = true;
                }
            }

            void FlappyBirdState::reset() {
                birdY = 0.5f;
                birdVelocity = 0.0f;

                if (score > highScore) {
                    highScore = score;
                }

                score = 0;
                pipes.clear();
                nextPipeSpawn = 1.2f;
                running = true;
                gameOver = false;
                flapAnimProgress = 0.0f;
                deathAnimProgress = 0.0f;
            }

            void FlappyBirdState::spawnPipe() {
                const float minGapY = 0.3f;
                const float maxGapY = 0.7f;
                const float gapY = minGapY + (maxGapY - minGapY) * (std::rand() / static_cast<float>(RAND_MAX));

                pipes.push_back({ 1.0f + pipeWidth * 0.5f, gapY, false });
                nextPipeSpawn = pipeSpacing;
            }

            bool FlappyBirdState::checkCollision() const {
                const float birdLeft = 0.2f - birdSize * 0.5f;
                const float birdRight = 0.2f + birdSize * 0.5f;
                const float birdTop = birdY - birdSize * 0.5f;
                const float birdBottom = birdY + birdSize * 0.5f;

                if (birdTop <= 0.0f || birdBottom >= 1.0f) {
                    return true;
                }

                for (const auto& pipe : pipes) {
                    const float pipeLeft = pipe.x - pipeWidth * 0.5f;
                    const float pipeRight = pipe.x + pipeWidth * 0.5f;

                    if (birdRight > pipeLeft && birdLeft < pipeRight) {
                        const float gapTop = pipe.gapY - pipeGapSize * 0.5f;
                        const float gapBottom = pipe.gapY + pipeGapSize * 0.5f;

                        if (birdTop < gapTop || birdBottom > gapBottom) {
                            return true;
                        }
                    }
                }

                return false;
            }


            static void updateGame(float deltaTime) {
                if (g_state.flapAnimProgress > 0.0f) {
                    g_state.flapAnimProgress = std::max(0.0f, g_state.flapAnimProgress - deltaTime * 4.0f);
                }

                if (g_state.gameOver && g_state.deathAnimProgress < 1.0f) {
                    g_state.deathAnimProgress = std::min(1.0f, g_state.deathAnimProgress + deltaTime * 2.0f);
                }

                if (!g_state.running || g_state.gameOver) return;

                g_state.birdVelocity += g_state.gravity * deltaTime;
                g_state.birdY += g_state.birdVelocity * deltaTime;

                g_state.nextPipeSpawn -= g_state.pipeSpeed * deltaTime;

                if (g_state.nextPipeSpawn <= 0.0f) {
                    g_state.spawnPipe();
                }

                for (auto it = g_state.pipes.begin(); it != g_state.pipes.end();) {
                    it->x -= g_state.pipeSpeed * deltaTime;

                    if (!it->scored && it->x < 0.2f) {
                        it->scored = true;
                        ++g_state.score;
                    }

                    if (it->x < (0.0f - g_state.pipeWidth * 0.5f)) {
                        it = g_state.pipes.erase(it);
                    }
                    else {
                        ++it;
                    }
                }

                if (g_state.checkCollision()) {
                    if (g_state.score > g_state.highScore) {
                        g_state.highScore = g_state.score;
                    }

                    g_state.gameOver = true;
                    g_state.running = false;
                    g_state.deathAnimProgress = 0.0f;
                }
            }


            struct RenderContext {
                ImDrawList* drawList;
                ImVec2 boardPos;
                ImVec2 boardSize;
                ImU32 birdColor;
                ImU32 pipeColor;
                ImU32 backgroundColor;
                ImU32 groundColor;

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
                const ImVec4 topColor = ImVec4(0.53f, 0.81f, 0.92f, 1.0f);
                const ImVec4 bottomColor = ImVec4(0.68f, 0.85f, 0.90f, 1.0f);

                ctx.drawList->AddRectFilledMultiColor(
                    ctx.boardPos,
                    ImVec2(ctx.boardPos.x + ctx.boardSize.x, ctx.boardPos.y + ctx.boardSize.y),
                    ImGui::GetColorU32(topColor),
                    ImGui::GetColorU32(topColor),
                    ImGui::GetColorU32(bottomColor),
                    ImGui::GetColorU32(bottomColor)
                );

                const float groundHeight = ctx.toScreenY(0.08f);
                const ImVec2 groundMin(ctx.boardPos.x, ctx.boardPos.y + ctx.boardSize.y - groundHeight);
                const ImVec2 groundMax(ctx.boardPos.x + ctx.boardSize.x, ctx.boardPos.y + ctx.boardSize.y);

                ctx.drawList->AddRectFilled(groundMin, groundMax, ctx.groundColor);
                ctx.drawList->AddLine(
                    ImVec2(groundMin.x, groundMin.y),
                    ImVec2(groundMax.x, groundMin.y),
                    ImGui::GetColorU32(ImVec4(0.4f, 0.6f, 0.3f, 1.0f)),
                    DP(3.0f)
                );
            }

            static void renderPipes(const RenderContext& ctx) {
                for (const auto& pipe : g_state.pipes) {
                    const float pipeLeft = pipe.x - g_state.pipeWidth * 0.5f;
                    const float pipeRight = pipe.x + g_state.pipeWidth * 0.5f;

                    if (pipeRight < 0.0f || pipeLeft > 1.0f) continue;

                    const float clampedLeft = std::max(0.0f, pipeLeft);
                    const float clampedRight = std::min(1.0f, pipeRight);

                    const float gapTop = pipe.gapY - g_state.pipeGapSize * 0.5f;
                    const float gapBottom = pipe.gapY + g_state.pipeGapSize * 0.5f;

                    const ImVec2 topPipeMin = ctx.toScreen(clampedLeft, 0.0f);
                    const ImVec2 topPipeMax = ctx.toScreen(clampedRight, gapTop);

                    const ImVec2 bottomPipeMin = ctx.toScreen(clampedLeft, gapBottom);
                    const ImVec2 bottomPipeMax = ctx.toScreen(clampedRight, 0.92f);

                    const ImVec4 accent = tsm::ui::GetAccentColor();
                    const ImVec4 darkerAccent = ImVec4(accent.x * 0.6f, accent.y * 0.6f, accent.z * 0.6f, accent.w);

                    const ImU32 pipeBorder = ImGui::GetColorU32(darkerAccent);

                    ctx.drawList->AddRectFilled(topPipeMin, topPipeMax, ctx.pipeColor, DP(4.0f));
                    ctx.drawList->AddRect(topPipeMin, topPipeMax, pipeBorder, DP(4.0f), 0, DP(2.0f));

                    ctx.drawList->AddRectFilled(bottomPipeMin, bottomPipeMax, ctx.pipeColor, DP(4.0f));
                    ctx.drawList->AddRect(bottomPipeMin, bottomPipeMax, pipeBorder, DP(4.0f), 0, DP(2.0f));

                    const float capLeft = pipe.x - g_state.pipeWidth * 0.575f;
                    const float capRight = pipe.x + g_state.pipeWidth * 0.575f;

                    if (capRight > 0.0f && capLeft < 1.0f) {
                        const float clampedCapLeft = std::max(0.0f, capLeft);
                        const float clampedCapRight = std::min(1.0f, capRight);

                        const ImVec2 topCapMin = ctx.toScreen(clampedCapLeft, gapTop - 0.04f);
                        const ImVec2 topCapMax = ctx.toScreen(clampedCapRight, gapTop);

                        const ImVec2 bottomCapMin = ctx.toScreen(clampedCapLeft, gapBottom);
                        const ImVec2 bottomCapMax = ctx.toScreen(clampedCapRight, gapBottom + 0.04f);

                        ctx.drawList->AddRectFilled(topCapMin, topCapMax, ctx.pipeColor, DP(4.0f));
                        ctx.drawList->AddRect(topCapMin, topCapMax, pipeBorder, DP(4.0f), 0, DP(2.0f));

                        ctx.drawList->AddRectFilled(bottomCapMin, bottomCapMax, ctx.pipeColor, DP(4.0f));
                        ctx.drawList->AddRect(bottomCapMin, bottomCapMax, pipeBorder, DP(4.0f), 0, DP(2.0f));
                    }
                }
            }

            static void renderBird(const RenderContext& ctx) {
                const ImVec2 center = ctx.toScreen(0.2f, g_state.birdY);

                const float bounceScale = 1.0f + g_state.flapAnimProgress * 0.15f;
                const float size = ctx.toScreenX(g_state.birdSize) * bounceScale;

                const ImVec2 iconMin(center.x - size * 0.5f, center.y - size * 0.5f);
                const ImVec2 iconMax(center.x + size * 0.5f, center.y + size * 0.5f);

                UIIcon uiIcon{};
                IconLoader::getUIIcon("UiPersonalityButterfly", &uiIcon);

                if (uiIcon.textureId != IL_NO_TEXTURE) {
                    const float alpha = g_state.gameOver ? 1.0f - g_state.deathAnimProgress * 0.5f : 1.0f;
                    const ImU32 tint = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));

                    ctx.drawList->AddImage(uiIcon.textureId, iconMin, iconMax,
                        uiIcon.uv0, uiIcon.uv1, tint);
                }
                else {
                    const float alpha = g_state.gameOver ? 1.0f - g_state.deathAnimProgress * 0.5f : 1.0f;
                    const ImU32 birdColorWithAlpha = ImGui::GetColorU32(ImVec4(
                        ((ctx.birdColor >> 0) & 0xFF) / 255.0f,
                        ((ctx.birdColor >> 8) & 0xFF) / 255.0f,
                        ((ctx.birdColor >> 16) & 0xFF) / 255.0f,
                        alpha
                    ));

                    ctx.drawList->AddCircleFilled(center, size * 0.5f, birdColorWithAlpha, 32);
                    ctx.drawList->AddCircle(center, size * 0.5f,
                        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha)), 32, DP(2.0f));
                }
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

                const float baseY = ctx.boardPos.y + padding;
                const float scoreStartX = ctx.boardPos.x + ctx.boardSize.x - scoreSize.x - padding;

                const ImVec2 scorePos(
                    scoreStartX,
                    baseY
                );
                ctx.drawList->AddText(font, hudFontSize, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f)), scoreBuffer);

                if (!g_state.gameOver && g_state.score < 2) {
                    const float hintAlpha = std::max(0.0f, 1.0f - static_cast<float>(ImGui::GetTime()) / 6.0f);
                    if (hintAlpha > 0.0f) {
                        const char* hint = tsm::ui::i18n::Tr("Tap to flap");
                        const ImVec2 hintSize = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, hint);
                        const ImVec2 hintPos(
                            ctx.boardPos.x + (ctx.boardSize.x - hintSize.x) * 0.5f,
                            ctx.boardPos.y + ctx.boardSize.y * 0.7f
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
                }
            }

            static void createInputBlocker(const ImVec2& boardPos, const ImVec2& boardSize) {
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

                bool tapped = false;
                if (ImGui::Begin("##FlappyInputBlocker", nullptr, flags)) {
                    ImGui::InvisibleButton("##blocker", blockerSize);
                    if (ImGui::IsItemClicked() && !g_state.gameOver) {
                        tapped = true;
                    }
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);

                if (tapped) {
                    g_state.birdVelocity = g_state.flapStrength;
                    g_state.flapAnimProgress = 1.0f;
                }
            }


            void DrawFlappyBirdGame() {
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

                const float boardSize = std::min(availableWidth, availableHeight);

                const ImVec2 boardPos(
                    viewport->Pos.x + leftPadding,
                    viewport->Pos.y + topPadding
                );
                const ImVec2 boardSizeVec(boardSize, boardSize);

                const ImVec4 accent = tsm::ui::GetAccentColor();

                RenderContext ctx{
                    ImGui::GetForegroundDrawList(),
                    boardPos,
                    boardSizeVec,
                    ImGui::GetColorU32(ImVec4(1.0f, 0.84f, 0.0f, 1.0f)),
                    ImGui::GetColorU32(accent),
                    ImGui::GetColorU32(ImVec4(0.53f, 0.81f, 0.92f, 1.0f)),
                    ImGui::GetColorU32(ImVec4(0.56f, 0.93f, 0.56f, 1.0f))
                };

                createInputBlocker(boardPos, boardSizeVec);

                ctx.drawList->AddRect(
                    boardPos,
                    ImVec2(boardPos.x + boardSize, boardPos.y + boardSize),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.4f)),
                    DP(8.0f),
                    0,
                    DP(2.0f)
                );

                renderBackground(ctx);
                renderPipes(ctx);
                renderBird(ctx);
                renderHUD(ctx);

                if (g_state.gameOver) {
                    renderGameOverOverlay(ctx, io);
                }
            }

        }
    }
}
