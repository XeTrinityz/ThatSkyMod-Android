#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <utility>
#include <progression/RunConfig.h>

namespace tsm {
    namespace progression {

        class CandleRunner {
        public:
            [[nodiscard]] static CandleRunner& Get();

            CandleRunner(const CandleRunner&) = delete;
            CandleRunner& operator=(const CandleRunner&) = delete;


            void RefreshCache(bool forced = false);

            void ClearCache();

            [[nodiscard]] std::vector<CandlePoint> GetRegularCoords() const;

            [[nodiscard]] std::vector<CandlePoint> GetNamedCoords() const;

            [[nodiscard]] int GetActiveCandleCount() const { return active_candle_count_; }

        private:
            CandleRunner() = default;
            ~CandleRunner() = default;


            void RebuildCache(const std::string& level, int rawCount, double now);

            int  CountActiveCandles(int rawCount) const;

            void LoadEmbeddedDatabase();

            [[nodiscard]] std::pair<std::vector<CandlePoint>, int>
                ScanAndCluster(const std::string& level, int count) const;


            mutable std::mutex mutex_;

            std::string current_map_;
            double      last_refresh_time_{ 0.0 };
            int         cached_raw_count_{ -1 };
            int         active_candle_count_{ 0 };

            std::vector<CandlePoint> embedded_candles_;

            std::vector<CandlePoint> regular_coords_;
            std::vector<CandlePoint> named_coords_;
        };

    }
}
