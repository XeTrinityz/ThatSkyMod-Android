#pragma once

namespace tsm {
    namespace ui {
        namespace overlays {

            enum class GameOverlayId {
                None = 0,
                Snake = 1,
                FlappyBird = 2,
                Pong = 3,
                Breakout = 4,
                MemoryMatch = 5,
                Doom = 6
            };

            void RenderGameOverlay();

            void SetActiveGame(GameOverlayId id);
            GameOverlayId GetActiveGame();

            void SetSnakeGameEnabled(bool enabled);
            bool IsSnakeGameEnabled();

            void SetFlappyBirdGameEnabled(bool enabled);
            bool IsFlappyBirdGameEnabled();

            void SetPongGameEnabled(bool enabled);
            bool IsPongGameEnabled();

            void SetBreakoutGameEnabled(bool enabled);
            bool IsBreakoutGameEnabled();

            void SetMemoryMatchGameEnabled(bool enabled);
            bool IsMemoryMatchGameEnabled();

            void SetDoomGameEnabled(bool enabled);
            bool IsDoomGameEnabled();

        }
    }
}
