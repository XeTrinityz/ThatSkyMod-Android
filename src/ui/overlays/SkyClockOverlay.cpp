#include <ui/overlays/SkyClockOverlay.h>
#include <ui/core/metrics.h>
#include <ui/core/App.h>
#include <ui/core/Localization.h>
#include <state/ModState.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <imgui/imgui.h>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <array>

namespace tsm { namespace ui { namespace overlays {

static constexpr int MIN_PER_HR = 60;
static constexpr int MIN_PER_DAY = 24 * MIN_PER_HR;
static constexpr int UPCOMING_THRESHOLD_MIN = 15;

struct SkyEvent {
    const char* name;
    const char* icon;
    int minuteOfHour;
    int durationMin;
    bool daily;
    int dailyHour;
    ImVec4 color;

    bool isActive = false;
    int minutesUntil = 0;
    int remainingMinutes = 0;
    int startHour = 0;
    int startMinute = 0;
};

static std::array<SkyEvent, 5> s_skyEvents = {{
    { "Geyser",          "UiMenuStageVolcano",      5, 10, false, 0, ImVec4(0.9f, 0.4f, 0.2f, 1.0f) },
    { "Grandma",         "UiEmoteAP01Think",       35, 10, false, 0, ImVec4(1.0f, 0.8f, 0.2f, 1.0f) },
    { "Turtle",          "UiEmoteSocialHeart",     50, 10, false, 0, ImVec4(0.3f, 0.8f, 0.5f, 1.0f) },
    { "Aurora Concert",  "UiMiscMusicNote",        10, 50, false, 0, ImVec4(0.6f, 0.4f, 0.9f, 1.0f) },
    { "Daily Reset",     "UiMenuBuffDoubleWax",     0,  0, true,  0, ImVec4(0.2f, 0.8f, 0.9f, 1.0f) }
}};

static bool IsPDT(const std::tm& localTm) {
    auto nthSunday = [](int year, int month0, int nth) -> int {
        std::tm tm{};
        tm.tm_year = year - 1900;
        tm.tm_mon = month0;
        tm.tm_mday = 1;
        std::mktime(&tm);
        int w = tm.tm_wday;
        int firstSunday = (w == 0) ? 1 : (8 - w);
        return firstSunday + 7 * (nth - 1);
    };

    const int year = localTm.tm_year + 1900;
    const int march2ndSunday = nthSunday(year, 2, 2);
    const int nov1stSunday = nthSunday(year, 10, 1);

    return (localTm.tm_mon > 2 && localTm.tm_mon < 10) ||
           (localTm.tm_mon == 2 && localTm.tm_mday >= march2ndSunday) ||
           (localTm.tm_mon == 10 && localTm.tm_mday < nov1stSunday);
}

static void CenterText(const char* text) {
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float textWidth = ImGui::CalcTextSize(text).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f + ImGui::GetCursorPosX());
    ImGui::TextUnformatted(text);
}

static void ComputeEventTimings(const std::tm& pacificTm) {
    const int curH = pacificTm.tm_hour;
    const int curM = pacificTm.tm_min;
    const int curTotalMin = curH * MIN_PER_HR + curM;
    const bool isEvenHour = (curH % 2 == 0);

    for (auto& e : s_skyEvents) {
        e.isActive = false;
        e.minutesUntil = 0;
        e.remainingMinutes = 0;

        if (e.daily) {
            const int resetMin = e.dailyHour * MIN_PER_HR + e.minuteOfHour;
            e.startHour = e.dailyHour;
            e.startMinute = e.minuteOfHour;

            if (e.durationMin == 0) {
                if (curH == e.dailyHour && curM == e.minuteOfHour) {
                    e.isActive = true;
                } else if (curTotalMin < resetMin) {
                    e.minutesUntil = resetMin - curTotalMin;
                } else {
                    e.minutesUntil = MIN_PER_DAY - curTotalMin + resetMin;
                }
            } else {
                const int endMin = resetMin + e.durationMin;
                if (curTotalMin >= resetMin && curTotalMin < endMin) {
                    e.isActive = true;
                    e.remainingMinutes = endMin - curTotalMin;
                } else if (curTotalMin < resetMin) {
                    e.minutesUntil = resetMin - curTotalMin;
                } else {
                    e.minutesUntil = MIN_PER_DAY - curTotalMin + resetMin;
                }
            }
        } else {
            e.startHour = isEvenHour ? curH : (curH + 1) % 24;
            e.startMinute = e.minuteOfHour;

            if (isEvenHour) {
                const int eventStart = e.minuteOfHour;
                const int eventEnd = eventStart + e.durationMin;

                if (curM >= eventStart && curM < eventEnd) {
                    e.isActive = true;
                    e.remainingMinutes = eventEnd - curM;
                } else if (curM < eventStart) {
                    e.minutesUntil = eventStart - curM;
                } else {
                    e.minutesUntil = 2 * MIN_PER_HR - curM + eventStart;
                    e.startHour = (curH + 2) % 24;
                }
            } else {
                e.minutesUntil = MIN_PER_HR - curM + e.minuteOfHour;
                e.startHour = (curH + 1) % 24;
            }
        }
    }

    std::sort(s_skyEvents.begin(), s_skyEvents.end(), [](const SkyEvent& a, const SkyEvent& b) {
        if (a.isActive != b.isActive) return a.isActive;
        return a.minutesUntil < b.minutesUntil;
    });
}

void RenderSkyClock() {
    using namespace tsm::ui::widgets;

    auto& ms = tsm::state::ModState::Get();
    if (!ms.debug.showSkyClock) return;

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowUtc = std::chrono::system_clock::to_time_t(now);

    std::tm localTm{};
    #ifdef _WIN32
        localtime_s(&localTm, &nowUtc);
    #else
        localtime_r(&nowUtc, &localTm);
    #endif

    const bool isPDT = IsPDT(localTm);
    const int pacificOffsetSec = isPDT ? -7 * 3600 : -8 * 3600;
    const std::time_t pacific = nowUtc + pacificOffsetSec;

    std::tm pacificTm{};
    #ifdef _WIN32
        gmtime_s(&pacificTm, &pacific);
    #else
        gmtime_r(&pacific, &pacificTm);
    #endif

    ComputeEventTimings(pacificTm);

    static constexpr ImGuiWindowFlags FLAGS =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const float pad = DP(12.0f);

    ImVec2 pos, pivot;
    switch (ms.debug.skyClockPosition) {
        case 0:
            pos = ImVec2(pad, pad);
            pivot = ImVec2(0.0f, 0.0f);
            break;
        case 1:
            pos = ImVec2(ds.x * 0.5f, pad);
            pivot = ImVec2(0.5f, 0.0f);
            break;
        case 2:
            pos = ImVec2(ds.x - pad, pad);
            pivot = ImVec2(1.0f, 0.0f);
            break;
        case 3:
            pos = ImVec2(pad, ds.y - pad);
            pivot = ImVec2(0.0f, 1.0f);
            break;
        case 4:
            pos = ImVec2(ds.x * 0.5f, ds.y - pad);
            pivot = ImVec2(0.5f, 1.0f);
            break;
        case 5:
            pos = ImVec2(ds.x - pad, ds.y - pad);
            pivot = ImVec2(1.0f, 1.0f);
            break;
        default:
            pos = ImVec2(ds.x * 0.5f, pad);
            pivot = ImVec2(0.5f, 0.0f);
            break;
    }

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);

    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(16.0f), DP(12.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, DP(12.0f));

    if (!ImGui::Begin("##SkyClockOverlay", nullptr, FLAGS)) {
        ImGui::PopStyleVar(2);
        ImGui::End();
        return;
    }

    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &pacificTm);

    ImVec4 acc = tsm::ui::GetAccentColor();
    ImGui::PushStyleColor(ImGuiCol_Text, acc);
    ImGui::SetWindowFontScale(1.3f);

    const char* timezone = isPDT ? tsm::ui::i18n::Tr("PDT") : tsm::ui::i18n::Tr("PST");
    char clockText[48];
    std::snprintf(clockText, sizeof(clockText), "%s %s", timeBuf, timezone);
    CenterText(clockText);

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    bool anyNearby = false;
    for (const auto& e : s_skyEvents) {
        if (e.isActive || (e.minutesUntil > 0 && e.minutesUntil <= UPCOMING_THRESHOLD_MIN)) {
            anyNearby = true;
            break;
        }
    }

    ImGui::Separator();

    if (anyNearby) {
        for (const auto& e : s_skyEvents) {
            if (!(e.isActive || (e.minutesUntil > 0 && e.minutesUntil <= UPCOMING_THRESHOLD_MIN))) continue;

            ImGui::PushStyleColor(ImGuiCol_Text, e.color);
            if (e.isActive) {
                const char* eventName = tsm::ui::i18n::Tr(e.name);
                if (e.durationMin == 0) {
                    char buf[64];
                    const char* fmt = tsm::ui::i18n::Tr("%s - NOW!");
                    std::snprintf(buf, sizeof(buf), fmt, eventName);
                    CenterText(buf);
                } else {
                    char buf[64];
                    const char* fmt = tsm::ui::i18n::Tr("%s - Active (%dm left)");
                    std::snprintf(buf, sizeof(buf), fmt, eventName, e.remainingMinutes);
                    CenterText(buf);
                }
            } else {
                const char* eventName = tsm::ui::i18n::Tr(e.name);
                char buf[64];
                const char* fmt = tsm::ui::i18n::Tr("%s - In %dm");
                std::snprintf(buf, sizeof(buf), fmt, eventName, e.minutesUntil);
                CenterText(buf);
            }
            ImGui::PopStyleColor();
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        CenterText(tsm::ui::i18n::Tr("No events starting soon"));
        ImGui::PopStyleColor();
    }

    const bool hovered = ImGui::IsWindowHovered();

    if (hovered) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::BeginTable("##SkyEvents", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn(tsm::ui::i18n::Tr("Event"), ImGuiTableColumnFlags_WidthFixed, DP(140.0f));
            ImGui::TableSetupColumn(tsm::ui::i18n::Tr("Time"), ImGuiTableColumnFlags_WidthFixed, DP(60.0f));
            ImGui::TableSetupColumn(tsm::ui::i18n::Tr("Status"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (const auto& e : s_skyEvents) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, e.color);
                ImGui::TextUnformatted(tsm::ui::i18n::Tr(e.name));
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                char timeStr[16];
                std::snprintf(timeStr, sizeof(timeStr), "%02d:%02d", e.startHour, e.startMinute);
                ImGui::TextUnformatted(timeStr);

                ImGui::TableSetColumnIndex(2);
                if (e.isActive) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                    if (e.durationMin == 0) {
                        ImGui::TextUnformatted(tsm::ui::i18n::Tr("Happening now!"));
                    } else {
                        char buf[64];
                        const char* fmt = tsm::ui::i18n::Tr("Active (%dm left)");
                        std::snprintf(buf, sizeof(buf), fmt, e.remainingMinutes);
                        ImGui::TextUnformatted(buf);
                    }
                    ImGui::PopStyleColor();
                } else if (e.minutesUntil == 0) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                    ImGui::TextUnformatted(tsm::ui::i18n::Tr("Starting now!"));
                    ImGui::PopStyleColor();
                } else if (e.minutesUntil <= UPCOMING_THRESHOLD_MIN) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
                    char buf[64];
                    const char* fmt = tsm::ui::i18n::Tr("In %d minutes");
                    std::snprintf(buf, sizeof(buf), fmt, e.minutesUntil);
                    ImGui::TextUnformatted(buf);
                    ImGui::PopStyleColor();
                } else if (e.minutesUntil < 60) {
                    char buf[64];
                    const char* fmt = tsm::ui::i18n::Tr("In %d minutes");
                    std::snprintf(buf, sizeof(buf), fmt, e.minutesUntil);
                    ImGui::TextUnformatted(buf);
                } else {
                    const int h = e.minutesUntil / 60;
                    const int m = e.minutesUntil % 60;
                    char buf[64];
                    const char* fmt = tsm::ui::i18n::Tr("In %dh %02dm");
                    std::snprintf(buf, sizeof(buf), fmt, h, m);
                    ImGui::TextUnformatted(buf);
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::PopStyleVar(2);
    ImGui::End();
}

}}}
