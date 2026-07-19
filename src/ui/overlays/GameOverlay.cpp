#include <ui/overlays/GameOverlay.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/widgets/SnakeGame.h>
#include <ui/widgets/FlappyBirdGame.h>
#include <ui/widgets/PongGame.h>
#include <ui/widgets/BreakoutGame.h>
#include <ui/widgets/MemoryMatchGame.h>
#include <ui/widgets/DoomGame.h>

#include <imgui/imgui.h>

namespace tsm {
    namespace ui {
        namespace overlays {

            static GameOverlayId s_activeGame = GameOverlayId::None;
            static bool s_snakeEnabled = false;
            static bool s_flappyBirdEnabled = false;
            static bool s_pongEnabled = false;
            static bool s_breakoutEnabled = false;
            static bool s_memoryMatchEnabled = false;
            static bool s_doomEnabled = false;

            void SetActiveGame(GameOverlayId id) {
                s_activeGame = id;
                s_snakeEnabled = (id == GameOverlayId::Snake);
                s_flappyBirdEnabled = (id == GameOverlayId::FlappyBird);
                s_pongEnabled = (id == GameOverlayId::Pong);
                s_breakoutEnabled = (id == GameOverlayId::Breakout);
                s_memoryMatchEnabled = (id == GameOverlayId::MemoryMatch);
                s_doomEnabled = (id == GameOverlayId::Doom);
            }

            GameOverlayId GetActiveGame() {
                return s_activeGame;
            }

            void SetSnakeGameEnabled(bool enabled) {
                s_snakeEnabled = enabled;
                s_activeGame = enabled ? GameOverlayId::Snake : GameOverlayId::None;

                if (enabled) {
                    s_flappyBirdEnabled = false;
                    s_pongEnabled = false;
                    s_breakoutEnabled = false;
                    s_memoryMatchEnabled = false;
                    s_doomEnabled = false;
                }
            }

            bool IsSnakeGameEnabled() {
                return s_snakeEnabled;
            }

            void SetFlappyBirdGameEnabled(bool enabled) {
                s_flappyBirdEnabled = enabled;
                s_activeGame = enabled ? GameOverlayId::FlappyBird : GameOverlayId::None;

                if (enabled) {
                    s_snakeEnabled = false;
                    s_pongEnabled = false;
                    s_breakoutEnabled = false;
                    s_memoryMatchEnabled = false;
                    s_doomEnabled = false;
                }
            }

            bool IsFlappyBirdGameEnabled() {
                return s_flappyBirdEnabled;
            }

            void SetPongGameEnabled(bool enabled) {
                s_pongEnabled = enabled;
                s_activeGame = enabled ? GameOverlayId::Pong : GameOverlayId::None;

                if (enabled) {
                    s_snakeEnabled = false;
                    s_flappyBirdEnabled = false;
                    s_breakoutEnabled = false;
                    s_memoryMatchEnabled = false;
                    s_doomEnabled = false;
                }
            }

            bool IsPongGameEnabled() {
                return s_pongEnabled;
            }

            void SetBreakoutGameEnabled(bool enabled) {
                s_breakoutEnabled = enabled;
                s_activeGame = enabled ? GameOverlayId::Breakout : GameOverlayId::None;

                if (enabled) {
                    s_snakeEnabled = false;
                    s_flappyBirdEnabled = false;
                    s_pongEnabled = false;
                    s_memoryMatchEnabled = false;
                    s_doomEnabled = false;
                }
            }

            bool IsBreakoutGameEnabled() {
                return s_breakoutEnabled;
            }

            void SetMemoryMatchGameEnabled(bool enabled) {
                s_memoryMatchEnabled = enabled;
                s_activeGame = enabled ? GameOverlayId::MemoryMatch : GameOverlayId::None;

                if (enabled) {
                    s_snakeEnabled = false;
                    s_flappyBirdEnabled = false;
                    s_pongEnabled = false;
                    s_breakoutEnabled = false;
                    s_doomEnabled = false;
                }
            }

            bool IsMemoryMatchGameEnabled() {
                return s_memoryMatchEnabled;
            }

            void SetDoomGameEnabled(bool enabled) {
                s_doomEnabled = enabled;
                s_activeGame = enabled ? GameOverlayId::Doom : GameOverlayId::None;

                if (enabled) {
                    s_snakeEnabled = false;
                    s_flappyBirdEnabled = false;
                    s_pongEnabled = false;
                    s_breakoutEnabled = false;
                    s_memoryMatchEnabled = false;
                }
            }

            bool IsDoomGameEnabled() {
                return s_doomEnabled;
            }

            void RenderGameOverlay() {
                if (s_activeGame == GameOverlayId::None)
                    return;

                if (s_activeGame == GameOverlayId::Snake) {
                    tsm::ui::widgets::DrawSnakeGame();
                }
                else if (s_activeGame == GameOverlayId::FlappyBird) {
                    tsm::ui::widgets::DrawFlappyBirdGame();
                }
                else if (s_activeGame == GameOverlayId::Pong) {
                    tsm::ui::widgets::DrawPongGame();
                }
                else if (s_activeGame == GameOverlayId::Breakout) {
                    tsm::ui::widgets::DrawBreakoutGame();
                }
                else if (s_activeGame == GameOverlayId::MemoryMatch) {
                    tsm::ui::widgets::DrawMemoryMatchGame();
                }
                else if (s_activeGame == GameOverlayId::Doom) {
                    tsm::ui::widgets::DrawDoomGame();
                }
            }

        }
    }
}
