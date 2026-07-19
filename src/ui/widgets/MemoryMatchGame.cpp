#include <ui/widgets/MemoryMatchGame.h>
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
#include <cmath>
#include <cstdio>

namespace tsm {
    namespace ui {
        namespace widgets {


            static constexpr int GRID_ROWS = 4;
            static constexpr int GRID_COLS = 4;
            static constexpr float MISMATCH_REVEAL_TIME = 0.8f;

            static const char* kMemoryIconIds[] = {
                "UiEmoteAP01Dance",
                "UiEmoteAP01Juggle",
                "UiEmoteAP01Kiss",
                "UiEmoteAP01Think",
                "UiEmoteAP02Joy",
                "UiEmoteAP02KungFu",
                "UiEmoteAP03Axel",
                "UiEmoteAP03Quiet"
            };

            static constexpr int ICON_COUNT = sizeof(kMemoryIconIds) / sizeof(kMemoryIconIds[0]);


            struct MemoryCard {
                int iconIndex = -1;
                bool matched = false;
                bool faceUp = false;
                float flipAnim = 0.0f;
                float matchAnim = 0.0f;

                void reset() {
                    iconIndex = -1;
                    matched = false;
                    faceUp = false;
                    flipAnim = 0.0f;
                    matchAnim = 0.0f;
                }
            };


            struct MemoryMatchState {
                std::vector<MemoryCard> cards;

                int firstFlipped = -1;
                int secondFlipped = -1;
                float mismatchRevealTimer = 0.0f;

                int totalPairs = 0;
                int matchesFound = 0;
                int moves = 0;
                int bestMoves = 0;

                bool initialized = false;
                bool running = false;
                bool gameOver = false;

                void initialize();
                void reset();
                void update(float deltaTime);
                void handleCardClick(int index);

            private:
                void flipCard(int index);
                void checkMatch();
                void hideUnmatchedCards();
            };

            static MemoryMatchState g_state;
            static bool g_randomSeeded = false;


            static inline void ensureRandomSeeded() {
                if (!g_randomSeeded) {
                    std::srand(static_cast<unsigned int>(std::time(nullptr)));
                    g_randomSeeded = true;
                }
            }

            static inline int getRandomInt(int maxExclusive) {
                return std::rand() % maxExclusive;
            }

            template<typename T>
            static void shuffleVector(std::vector<T>& vec) {
                for (int i = static_cast<int>(vec.size()) - 1; i > 0; --i) {
                    int j = getRandomInt(i + 1);
                    std::swap(vec[i], vec[j]);
                }
            }


            void MemoryMatchState::initialize() {
                ensureRandomSeeded();
                reset();
                initialized = true;
            }

            void MemoryMatchState::reset() {
                const int cardCount = GRID_ROWS * GRID_COLS;
                totalPairs = cardCount / 2;

                firstFlipped = -1;
                secondFlipped = -1;
                mismatchRevealTimer = 0.0f;
                matchesFound = 0;
                moves = 0;
                gameOver = false;
                running = true;

                std::vector<int> iconPool;
                iconPool.reserve(cardCount);

                const int pairsNeeded = std::min(totalPairs, ICON_COUNT);
                for (int i = 0; i < pairsNeeded; ++i) {
                    iconPool.push_back(i);
                    iconPool.push_back(i);
                }

                while (static_cast<int>(iconPool.size()) < cardCount) {
                    iconPool.push_back(iconPool[getRandomInt(pairsNeeded)]);
                }

                shuffleVector(iconPool);

                cards.clear();
                cards.resize(cardCount);
                for (int i = 0; i < cardCount; ++i) {
                    cards[i].iconIndex = iconPool[i];
                }
            }

            void MemoryMatchState::update(float deltaTime) {
                if (!running || gameOver) return;

                for (auto& card : cards) {
                    if (card.flipAnim > 0.0f) {
                        card.flipAnim = std::max(0.0f, card.flipAnim - deltaTime * 4.0f);
                    }
                    if (card.matchAnim > 0.0f) {
                        card.matchAnim = std::max(0.0f, card.matchAnim - deltaTime * 2.0f);
                    }
                }

                if (mismatchRevealTimer > 0.0f) {
                    mismatchRevealTimer -= deltaTime;
                    if (mismatchRevealTimer <= 0.0f) {
                        hideUnmatchedCards();
                    }
                }

                if (matchesFound >= totalPairs && totalPairs > 0) {
                    if (moves > 0 && (bestMoves == 0 || moves < bestMoves)) {
                        bestMoves = moves;
                    }
                    gameOver = true;
                    running = false;
                }
            }

            void MemoryMatchState::handleCardClick(int index) {
                if (!running || gameOver || mismatchRevealTimer > 0.0f) return;
                if (index < 0 || index >= static_cast<int>(cards.size())) return;

                MemoryCard& card = cards[index];
                if (card.matched || card.faceUp) return;

                flipCard(index);
            }

            void MemoryMatchState::flipCard(int index) {
                cards[index].faceUp = true;
                cards[index].flipAnim = 1.0f;

                if (firstFlipped < 0) {
                    firstFlipped = index;
                }
                else if (secondFlipped < 0) {
                    secondFlipped = index;
                    ++moves;
                    checkMatch();
                }
            }

            void MemoryMatchState::checkMatch() {
                const MemoryCard& first = cards[firstFlipped];
                const MemoryCard& second = cards[secondFlipped];

                if (first.iconIndex == second.iconIndex) {
                    cards[firstFlipped].matched = true;
                    cards[secondFlipped].matched = true;
                    cards[firstFlipped].matchAnim = 1.0f;
                    cards[secondFlipped].matchAnim = 1.0f;
                    ++matchesFound;
                    firstFlipped = -1;
                    secondFlipped = -1;
                }
                else {
                    mismatchRevealTimer = MISMATCH_REVEAL_TIME;
                }
            }

            void MemoryMatchState::hideUnmatchedCards() {
                if (firstFlipped >= 0 && secondFlipped >= 0) {
                    cards[firstFlipped].faceUp = false;
                    cards[secondFlipped].faceUp = false;
                }
                firstFlipped = -1;
                secondFlipped = -1;
                mismatchRevealTimer = 0.0f;
            }


            struct CardLayout {
                ImVec2 position;
                ImVec2 size;
                float padding;
                float rounding;
            };

            struct BoardLayout {
                ImVec2 position;
                ImVec2 size;
                float cardSize;
                float spacing;
                float margin;

                CardLayout getCardLayout(int row, int col) const;
            };

            BoardLayout calculateBoardLayout(const ImVec2& availablePos, const ImVec2& availableSize) {
                BoardLayout layout;

                const float minSpacing = DP(8.0f);

                const float availableWidth = availableSize.x;
                const float availableHeight = availableSize.y;

                const float maxCardWidth = (availableWidth - minSpacing * (GRID_COLS + 1)) / GRID_COLS;
                const float maxCardHeight = (availableHeight - minSpacing * (GRID_ROWS + 1)) / GRID_ROWS;

                layout.cardSize = std::min(maxCardWidth, maxCardHeight);
                layout.spacing = minSpacing;

                const float boardWidth = GRID_COLS * layout.cardSize + (GRID_COLS + 1) * layout.spacing;
                const float boardHeight = GRID_ROWS * layout.cardSize + (GRID_ROWS + 1) * layout.spacing;

                layout.position = availablePos;
                layout.size = ImVec2(boardWidth, boardHeight);
                layout.margin = 0;

                return layout;
            }

            CardLayout BoardLayout::getCardLayout(int row, int col) const {
                CardLayout card;

                card.size = ImVec2(cardSize, cardSize);
                card.position = ImVec2(
                    position.x + spacing + col * (cardSize + spacing),
                    position.y + spacing + row * (cardSize + spacing)
                );
                card.padding = DP(4.0f);
                card.rounding = DP(6.0f);

                return card;
            }


            static void renderCardBackground(ImDrawList* drawList, const CardLayout& layout, const MemoryCard& card) {
                const ImVec2 min = layout.position;
                const ImVec2 max = ImVec2(min.x + layout.size.x, min.y + layout.size.y);

                float scale = 1.0f;
                if (card.matchAnim > 0.0f) {
                    scale = 1.0f + card.matchAnim * 0.15f;
                }

                ImVec2 scaledMin = min;
                ImVec2 scaledMax = max;
                if (scale != 1.0f) {
                    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
                    const float halfWidth = (max.x - min.x) * 0.5f * scale;
                    const float halfHeight = (max.y - min.y) * 0.5f * scale;
                    scaledMin = ImVec2(center.x - halfWidth, center.y - halfHeight);
                    scaledMax = ImVec2(center.x + halfWidth, center.y + halfHeight);
                }

                const ImVec4 accent = tsm::ui::GetAccentColor();
                ImVec4 bgColor;
                if (card.matched) {
                    bgColor = ImVec4(0.2f, 0.8f, 0.3f, 0.95f);
                }
                else if (card.faceUp) {
                    bgColor = ImVec4(accent.x, accent.y, accent.z, 0.9f);
                }
                else {
                    bgColor = ImVec4(0.15f, 0.15f, 0.18f, 0.95f);
                }

                drawList->AddRectFilled(scaledMin, scaledMax, ImGui::GetColorU32(bgColor), layout.rounding);
                drawList->AddRect(scaledMin, scaledMax, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.4f)),
                    layout.rounding, 0, DP(1.5f));
            }

            static void renderCardIcon(ImDrawList* drawList, const CardLayout& layout, const MemoryCard& card) {
                if (!card.faceUp && !card.matched) return;

                const char* iconName = kMemoryIconIds[card.iconIndex % ICON_COUNT];

                UIIcon uiIcon{};
                IconLoader::getUIIcon(iconName, &uiIcon);

                if (uiIcon.textureId != IL_NO_TEXTURE) {
                    const float iconPadding = layout.padding * 3.0f;
                    const ImVec2 iconMin = ImVec2(
                        layout.position.x + iconPadding,
                        layout.position.y + iconPadding
                    );
                    const ImVec2 iconMax = ImVec2(
                        layout.position.x + layout.size.x - iconPadding,
                        layout.position.y + layout.size.y - iconPadding
                    );

                    const float alpha = card.flipAnim > 0.5f ? (1.0f - card.flipAnim) * 2.0f : 1.0f;
                    drawList->AddImage(uiIcon.textureId, iconMin, iconMax, uiIcon.uv0, uiIcon.uv1,
                        ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)));
                }
            }

            static void renderCard(ImDrawList* drawList, const CardLayout& layout, const MemoryCard& card,
                int index, const ImGuiIO& io) {
                renderCardBackground(drawList, layout, card);

                renderCardIcon(drawList, layout, card);

                const ImVec2 min = layout.position;
                const ImVec2 max = ImVec2(min.x + layout.size.x, min.y + layout.size.y);
                const ImVec2 mouse = io.MousePos;

                const bool hovered = (mouse.x >= min.x && mouse.x <= max.x &&
                    mouse.y >= min.y && mouse.y <= max.y);

                if (hovered && ImGui::IsMouseClicked(0)) {
                    g_state.handleCardClick(index);
                }
            }

            static void renderAllCards(const BoardLayout& boardLayout, const ImGuiIO& io) {
                ImDrawList* drawList = ImGui::GetForegroundDrawList();

                for (int row = 0; row < GRID_ROWS; ++row) {
                    for (int col = 0; col < GRID_COLS; ++col) {
                        const int index = row * GRID_COLS + col;
                        if (index >= static_cast<int>(g_state.cards.size())) continue;

                        CardLayout cardLayout = boardLayout.getCardLayout(row, col);
                        renderCard(drawList, cardLayout, g_state.cards[index], index, io);
                    }
                }
            }

            static void renderHUD(const BoardLayout& boardLayout) {
                ImFont* font = ImGui::GetFont();
                const float fontSize = DP(20.0f);
                const float padding = DP(16.0f);

                ImDrawList* drawList = ImGui::GetForegroundDrawList();

                char movesBuffer[64];
                std::snprintf(movesBuffer, sizeof(movesBuffer), "%s: %d/%d",
                    tsm::ui::i18n::Tr("Moves"),
                    g_state.moves,
                    g_state.bestMoves);

                const ImVec2 movesPos(boardLayout.position.x, boardLayout.position.y - DP(40.0f));
                drawList->AddText(font, fontSize * 0.75f, movesPos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f)), movesBuffer);

                char matchesBuffer[32];
                std::snprintf(matchesBuffer, sizeof(matchesBuffer), "%d/%d",
                    g_state.matchesFound, g_state.totalPairs);

                const ImVec2 matchesSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, matchesBuffer);
                const ImVec2 matchesPos(
                    boardLayout.position.x + boardLayout.size.x - matchesSize.x,
                    boardLayout.position.y - DP(40.0f)
                );
                drawList->AddText(font, fontSize, matchesPos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 0.95f)), matchesBuffer);
            }

            static void renderGameOverOverlay(const BoardLayout& boardLayout, const ImGuiIO& io) {
                const char* title = tsm::ui::i18n::Tr("Complete!");

                char scoreBuffer[64];
                std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%s: %d",
                    tsm::ui::i18n::Tr("Final Moves"), g_state.moves);

                char highScoreBuffer[64];
                highScoreBuffer[0] = '\0';

                const bool hasBest = g_state.bestMoves > 0;
                const bool newRecord = hasBest && g_state.moves == g_state.bestMoves && g_state.moves > 0;
                if (newRecord) {
                    std::snprintf(highScoreBuffer, sizeof(highScoreBuffer), "%s!",
                        tsm::ui::i18n::Tr("New Record"));
                }
                else if (hasBest) {
                    std::snprintf(highScoreBuffer, sizeof(highScoreBuffer), "%s: %d",
                        tsm::ui::i18n::Tr("Best"), g_state.bestMoves);
                }

                ImFont* font = ImGui::GetFont();
                const float titleSize = DP(28.0f);
                const float textSize = DP(20.0f);

                const ImVec2 titleDim = font->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title);
                const ImVec2 scoreDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, scoreBuffer);
                ImVec2 highScoreDim(0.0f, 0.0f);
                if (highScoreBuffer[0] != '\0') {
                    highScoreDim = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, highScoreBuffer);
                }

                const float padding = DP(24.0f);
                const float buttonWidth = DP(180.0f);
                const float buttonHeight = DP(48.0f);

                const float boxWidth = std::max({ titleDim.x, scoreDim.x, highScoreDim.x, buttonWidth }) + padding * 2.0f;
                const float boxHeight = titleDim.y + scoreDim.y +
                    (highScoreBuffer[0] ? highScoreDim.y + padding * 0.5f : 0.0f) +
                    buttonHeight + padding * 4.5f;

                const ImVec2 boxMin(
                    boardLayout.position.x + (boardLayout.size.x - boxWidth) * 0.5f,
                    boardLayout.position.y + (boardLayout.size.y - boxHeight) * 0.5f
                );
                const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);

                ImDrawList* drawList = ImGui::GetForegroundDrawList();

                const ImVec4 overlayBg = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
                drawList->AddRectFilled(boardLayout.position,
                    ImVec2(boardLayout.position.x + boardLayout.size.x,
                        boardLayout.position.y + boardLayout.size.y),
                    ImGui::GetColorU32(overlayBg));

                const ImVec4 accent = tsm::ui::GetAccentColor();
                const ImVec4 boxBg = ImVec4(0.05f, 0.05f, 0.05f, 0.95f);
                const ImVec4 boxBorder = ImVec4(accent.x, accent.y, accent.z, 0.8f);

                drawList->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(boxBg), DP(12.0f));
                drawList->AddRect(boxMin, boxMax, ImGui::GetColorU32(boxBorder), DP(12.0f), 0, DP(2.0f));

                float cursorY = boxMin.y + padding;

                const ImVec2 titlePos(boxMin.x + (boxWidth - titleDim.x) * 0.5f, cursorY);
                drawList->AddText(font, titleSize, titlePos,
                    ImGui::GetColorU32(ImVec4(0.3f, 1.0f, 0.3f, 1)), title);

                cursorY += titleDim.y + padding;

                const ImVec2 scorePos(boxMin.x + (boxWidth - scoreDim.x) * 0.5f, cursorY);
                drawList->AddText(font, textSize, scorePos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), scoreBuffer);

                cursorY += scoreDim.y + padding * 0.5f;

                if (highScoreBuffer[0] != '\0') {
                    const ImVec2 highScorePos(boxMin.x + (boxWidth - highScoreDim.x) * 0.5f, cursorY);
                    const ImVec4 highScoreColor = newRecord ?
                        ImVec4(1, 0.85f, 0.2f, 1) : ImVec4(1, 1, 1, 0.7f);
                    drawList->AddText(font, textSize, highScorePos,
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

                drawList->AddRectFilled(btnMin, btnMax, ImGui::GetColorU32(btnBg), DP(8.0f));
                drawList->AddRect(btnMin, btnMax,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? 0.4f : 0.2f)), DP(8.0f), 0, DP(2.0f));

                const char* btnLabel = tsm::ui::i18n::Tr("Play Again");
                const ImVec2 btnLabelSize = font->CalcTextSizeA(textSize, FLT_MAX, 0.0f, btnLabel);
                const ImVec2 btnLabelPos(
                    btnMin.x + (buttonWidth - btnLabelSize.x) * 0.5f,
                    btnMin.y + (buttonHeight - btnLabelSize.y) * 0.5f
                );
                drawList->AddText(font, textSize, btnLabelPos,
                    ImGui::GetColorU32(ImVec4(1, 1, 1, 1)), btnLabel);

                if (clicked) {
                    g_state.reset();
                }
            }

            static void createInputBlocker(const BoardLayout& boardLayout) {
                const float margin = DP(60.0f);
                const ImVec2 blockerPos(boardLayout.position.x - margin, boardLayout.position.y - margin - DP(40.0f));
                const ImVec2 blockerSize(boardLayout.size.x + margin * 2.0f, boardLayout.size.y + margin * 2.0f + DP(40.0f));

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

                if (ImGui::Begin("##MemoryInputBlocker", nullptr, flags)) {
                    ImGui::InvisibleButton("##blocker", blockerSize);
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
            }


            void DrawMemoryMatchGame() {
                if (!g_state.initialized) {
                    g_state.initialize();
                }

                const ImGuiIO& io = ImGui::GetIO();
                g_state.update(io.DeltaTime);

                ImGuiViewport* viewport = ImGui::GetMainViewport();

                const float topPadding = DP(80.0f);
                const float bottomPadding = DP(40.0f);
                const float leftPadding = DP(20.0f);
                const float rightMargin = DP(20.0f);

                const float availableWidth = (viewport->Size.x * 0.5f) - leftPadding - rightMargin;
                const float availableHeight = viewport->Size.y - topPadding - bottomPadding;

                const ImVec2 availablePos(viewport->Pos.x + leftPadding, viewport->Pos.y + topPadding);
                const ImVec2 availableSize(availableWidth, availableHeight);

                BoardLayout boardLayout = calculateBoardLayout(availablePos, availableSize);

                const ImVec4 accent = tsm::ui::GetAccentColor();
                ImDrawList* drawList = ImGui::GetForegroundDrawList();

                drawList->AddRect(
                    boardLayout.position,
                    ImVec2(boardLayout.position.x + boardLayout.size.x, boardLayout.position.y + boardLayout.size.y),
                    ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.4f)),
                    DP(8.0f),
                    0,
                    DP(2.0f)
                );

                createInputBlocker(boardLayout);

                renderAllCards(boardLayout, io);
                renderHUD(boardLayout);

                if (g_state.gameOver) {
                    renderGameOverOverlay(boardLayout, io);
                }
            }

        }
    }
}
