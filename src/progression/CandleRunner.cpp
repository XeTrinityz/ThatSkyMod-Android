#include <progression/CandleRunner.h>
#include <game/memory/api.h>
#include <data/DataManager.h>
#include <nlohmann/json.hpp>
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <utility>

#include "../../resources/data/candles_json.h"

namespace tsm {
    namespace progression {


        namespace {

            constexpr double    kRefreshInterval = 2.0;
            constexpr float     kClusterRadius = 1.5f;
            constexpr uintptr_t kCandleStride = 0x70;

            constexpr int kTypeRed = 1;
            constexpr int kTypeSeason = 6;
            constexpr int kTypeCake = 8;

            [[nodiscard]] float Distance3D(const Position3D& a, const Position3D& b) noexcept {
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                const float dz = b.z - a.z;
                return std::sqrt(dx * dx + dy * dy + dz * dz);
            }

            [[nodiscard]] std::vector<CandlePoint> ClusterPositions(
                const std::vector<Position3D>& positions,
                float                          radius,
                const std::string& typeName,
                const std::string& levelName)
            {
                const size_t n = positions.size();
                std::vector<CandlePoint> result;
                result.reserve(n);

                std::vector<bool> visited(n, false);

                for (size_t i = 0; i < n; ++i) {
                    if (visited[i]) continue;
                    visited[i] = true;

                    float sumX = positions[i].x;
                    float sumY = positions[i].y;
                    float sumZ = positions[i].z;
                    int   count = 1;

                    for (size_t j = i + 1; j < n; ++j) {
                        if (!visited[j] && Distance3D(positions[i], positions[j]) <= radius) {
                            visited[j] = true;
                            sumX += positions[j].x;
                            sumY += positions[j].y;
                            sumZ += positions[j].z;
                            ++count;
                        }
                    }

                    const float inv = 1.0f / static_cast<float>(count);
                    CandlePoint cp;
                    cp.name = typeName;
                    cp.map = levelName;
                    cp.position = { sumX * inv, sumY * inv, sumZ * inv };
                    result.push_back(std::move(cp));
                }

                result.shrink_to_fit();
                return result;
            }

        }


        CandleRunner& CandleRunner::Get() {
            static CandleRunner instance;
            return instance;
        }


        void CandleRunner::RefreshCache(bool forced) {
            std::lock_guard<std::mutex> lock(mutex_);

            const char* lvlRaw = tsm::game::api::LevelName();
            const std::string lvl = (lvlRaw && *lvlRaw) ? lvlRaw : std::string{};
            const double      now = ImGui::GetTime();
            const int    rawCount = tsm::game::api::CandleCount();

            const bool levelChanged = (lvl != current_map_);
            const bool countChanged = (rawCount != cached_raw_count_);
            const bool stale = (now - last_refresh_time_) >= kRefreshInterval;

            if (!forced && !levelChanged && !countChanged && !stale) return;

            if (!forced && !levelChanged && !countChanged && stale) {
                if (CountActiveCandles(rawCount) == active_candle_count_) {
                    last_refresh_time_ = now;
                    return;
                }
            }

            RebuildCache(lvl, rawCount, now);
        }

        void CandleRunner::ClearCache() {
            std::lock_guard<std::mutex> lock(mutex_);
            embedded_candles_.clear();
            regular_coords_.clear();
            named_coords_.clear();
            current_map_.clear();
            last_refresh_time_ = 0.0;
            cached_raw_count_ = -1;
            active_candle_count_ = 0;
        }

        [[nodiscard]] std::vector<CandlePoint> CandleRunner::GetRegularCoords() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return regular_coords_;
        }

        [[nodiscard]] std::vector<CandlePoint> CandleRunner::GetNamedCoords() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return named_coords_;
        }


        void CandleRunner::RebuildCache(const std::string& level, int rawCount, double now) {
            current_map_ = level;
            cached_raw_count_ = rawCount;
            last_refresh_time_ = now;

            LoadEmbeddedDatabase();

            std::vector<CandlePoint> plants;
            named_coords_.clear();

            for (const auto& cp : embedded_candles_) {
                if (cp.map != level) continue;
                if (cp.name == "Plant") {
                    plants.push_back(cp);
                }
                else if (cp.name != "Candle" && cp.name != "Season Candle") {
                    named_coords_.push_back(cp);
                }
            }

            auto [clusters, activeCount] = ScanAndCluster(level, rawCount);
            active_candle_count_ = activeCount;

            regular_coords_.clear();
            regular_coords_.reserve(clusters.size() + plants.size());
            for (auto& c : clusters) regular_coords_.push_back(std::move(c));
            for (auto& p : plants)   regular_coords_.push_back(std::move(p));
        }

        int CandleRunner::CountActiveCandles(int rawCount) const {
            int count = 0;
            for (int i = 0; i < rawCount; ++i) {
                if (!tsm::game::api::CandleIsActive(i)) continue;
                const int type = tsm::game::api::CandleTypeIndex(i);
                if (type == kTypeRed || type == kTypeSeason || type == kTypeCake) ++count;
            }
            return count;
        }

        void CandleRunner::LoadEmbeddedDatabase() {
            if (!embedded_candles_.empty()) return;

            try {
                const auto* begin = reinterpret_cast<const char*>(kCandlesData);
                const auto* end = begin + sizeof(kCandlesData);
                const auto  arr = nlohmann::json::parse(begin, end);

                if (!arr.is_array()) return;

                embedded_candles_.reserve(arr.size());
                for (const auto& item : arr) {
                    if (!item.is_object()) continue;

                    CandlePoint cp;
                    cp.name = item.value("name", std::string{});
                    cp.map = item.value("map", std::string{});
                    cp.position.x = item.value("x", 0.0f);
                    cp.position.y = item.value("y", 0.0f);
                    cp.position.z = item.value("z", 0.0f);

                    if (!cp.map.empty()) {
                        embedded_candles_.emplace_back(std::move(cp));
                    }
                }
                embedded_candles_.shrink_to_fit();

            }
            catch (const std::exception&) {
                embedded_candles_.clear();
            }
        }

        std::pair<std::vector<CandlePoint>, int>
            CandleRunner::ScanAndCluster(const std::string& level, int count) const {
            const uintptr_t base = tsm::game::api::FirstCandle();
            if (count <= 0 || base == 0) return { {}, 0 };

            std::vector<Position3D> reds, seasons;
            reds.reserve(static_cast<size_t>(count));
            seasons.reserve(static_cast<size_t>(count));
            int activeCount = 0;

            for (int i = 0; i < count; ++i) {
                if (!tsm::game::api::CandleIsActive(i)) continue;

                const int type = tsm::game::api::CandleTypeIndex(i);
                if (type != kTypeRed && type != kTypeSeason && type != kTypeCake) continue;

                ++activeCount;

                const auto* v = reinterpret_cast<const float*>(base + static_cast<uintptr_t>(i) * kCandleStride);
                const Position3D pos{ v[0], v[1], v[2] };

                (type == kTypeSeason ? seasons : reds).push_back(pos);
            }

            auto clusters = ClusterPositions(reds, kClusterRadius, "Candle", level);
            auto sc = ClusterPositions(seasons, kClusterRadius, "Season Candle", level);

            clusters.reserve(clusters.size() + sc.size());
            for (auto& s : sc) clusters.push_back(std::move(s));

            return { std::move(clusters), activeCount };
        }

    }
}
