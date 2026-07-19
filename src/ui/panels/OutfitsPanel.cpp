#include <ui/panels/OutfitsPanel.h>
#include <ui/widgets/Layout.h>
#include <ui/widgets/Cards.h>
#include <ui/widgets/Controls.h>
#include <ui/widgets/Buttons.h>
#include <ui/widgets/Icons.h>
#include <ui/widgets/Popups.h>
#include <ui/helpers/ImGuiHelpers.h>
#include <ui/helpers/SubTabRenderer.h>
#include <ui/helpers/Toast.h>
#include <imgui/imgui.h>
#include <ui/core/App.h>
#include <ui/core/metrics.h>
#include <ui/core/Localization.h>
#include <iconloader/IconLoader.h>
#include <data/DataManager.h>
#include <game/interop/LuaScriptQueue.h>
#include <game/interop/LuaHelpers.h>
#include <game/memory/Address.h>
#include <game/memory/Memory.h>
#include <game/memory/Patch.h>
#include <game/memory/offsets.h>
#include <game/memory/api.h>
#include <game/data/OutfitData.h>
#include <network/ApiClient.h>
#include <utils/strings/StringUtils.h>
#include <utils/strings/OutfitNameFormatter.h>
#include <Cipher/CipherUtils.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <sstream>
#include <cmath>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

namespace tsm { namespace ui { namespace tabs {

using namespace tsm::game::Signatures;

static int s_outfitsSubIndex = 0;
static int s_prevOutfitsSubIndex = -1;
static std::string s_outfitTypeFilter;

static float s_cachedScale = 1.0f;
static float s_cachedHeight = 1.0f;
static bool s_toolsCacheValid = false;

static bool s_removeCape = false;
static std::vector<tsm::game::memory::Patch> s_removeCapePatches;

void DrawOutfitsTab()
{
    using namespace tsm::ui::widgets;
    using namespace tsm::ui::helpers;
    using namespace tsm::utils;

    static const char* kSubIcons[] = {
        "UiMenuCollectMask",
        "UiMenuDye",
        "UiMenuDevCorner",
        "UiMenuSaveDisk"
    };
    constexpr int sub_count = 4;

    DrawSubTabs(kSubIcons, sub_count, s_outfitsSubIndex);

    if (s_prevOutfitsSubIndex != s_outfitsSubIndex) {
        if (s_outfitsSubIndex == 2) {
            auto avatarInfo = tsm::game::api::GetAvatarInfoByIndex(0);
            if (avatarInfo.avatarScale && avatarInfo.avatarHeight) {
                s_cachedScale = *avatarInfo.avatarScale;
                s_cachedHeight = *avatarInfo.avatarHeight;
                s_toolsCacheValid = true;
            }
        }
        s_prevOutfitsSubIndex = s_outfitsSubIndex;
    }

    const float base_d = DP(44.0f);
    const ImVec4 acc = tsm::ui::GetAccentColor();
    const ImU32  bg_sel  = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.28f));
    const ImU32  brd_sel = ImGui::GetColorU32(ImVec4(acc.x, acc.y, acc.z, 0.70f));
    const ImU32  bg_nrm  = ImGui::GetColorU32(ImVec4(1,1,1,0.10f));
    const ImU32  brd_nrm = ImGui::GetColorU32(ImVec4(1,1,1,0.18f));

    if (BeginCard("##outfits_card", 8.0f, false)) {
        switch (s_outfitsSubIndex) {
            case 0: {
                const auto& defs = tsm::data::DataManager::Get().GetOutfitDefs();

                struct TypeInfo { std::string original; std::string firstIcon; std::string secondIcon; std::string thirdIcon; int count = 0; };
                std::map<std::string, TypeInfo> typeMap;
                if (defs.is_array()) {
                    for (const auto& it : defs) {
                        if (!it.is_object()) continue;
                        const auto* ty = it.contains("type") ? &it["type"] : nullptr;
                        if (!ty || !ty->is_string()) continue;
                        std::string type = ty->get<std::string>();
                        std::string tkey = type;
                        for (auto& ch : tkey) ch = (char)std::tolower((unsigned char)ch);
                        std::string icon = it.contains("icon") && it["icon"].is_string() ? it["icon"].get<std::string>() : std::string();
                        auto it2 = typeMap.find(tkey);
                        if (it2 == typeMap.end()) {
                            TypeInfo ti;
                            ti.original = type;
                            if (!icon.empty()) ti.firstIcon = icon;
                            ti.count = 1;
                            typeMap.emplace(tkey, std::move(ti));
                        } else {
                            TypeInfo& ti = it2->second;
                            ti.count++;
                            if (!icon.empty()) {
                                if (ti.firstIcon.empty()) {
                                    ti.firstIcon = icon;
                                } else if (ti.secondIcon.empty() && icon != ti.firstIcon) {
                                    ti.secondIcon = icon;
                                } else if (ti.thirdIcon.empty() && icon != ti.firstIcon && icon != ti.secondIcon) {
                                    ti.thirdIcon = icon;
                                }
                            }
                        }
                    }
                }

                {
                    CenterSeparator("Categories");
                    struct Entry { std::string keyLower; std::string label; std::string icon; };
                    std::vector<Entry> entries;
                    entries.push_back({ std::string(), std::string("All"), std::string() });
                    static const std::map<std::string, std::string> kTypeIconMap = {
                        {"body", "UiOutfitBodyClassicPants"},
                        {"face", "UiOutfitMaskElderDawn"},
                        {"feet", "UiOutfitFeetSocks"},
                        {"hair", "UiOutfitHairBraidSideSmall"},
                        {"hat",  "UiOutfitHairAP07StrawHat"},
                        {"horn", "UiOutfitHornAP04EarMuffs"},
                        {"mask", "UiOutfitMaskBasic"},
                        {"neck", "UiOutfitNeckAP17Scarf"},
                        {"prop", "UiOutfitPropHarp"},
                        {"wing", "UiOutfitCape"},
                    };
                    for (const auto& kv : typeMap) {
                        std::string lbl = kv.second.original;
                        for (size_t i = 0; i < lbl.size(); ++i) {
                            unsigned char ch = static_cast<unsigned char>(lbl[i]);
                            lbl[i] = (i == 0) ? static_cast<char>(std::toupper(ch)) : static_cast<char>(std::tolower(ch));
                        }
                        std::string iconPick;
                        auto itFix = kTypeIconMap.find(kv.first);
                        if (itFix != kTypeIconMap.end()) iconPick = itFix->second;
                        else iconPick = !kv.second.thirdIcon.empty() ? kv.second.thirdIcon : (!kv.second.secondIcon.empty() ? kv.second.secondIcon : kv.second.firstIcon);
                        entries.push_back({ kv.first, lbl, iconPick });
                    }

                    const int t_count = (int)entries.size();
                    float avail_w2 = ImGui::GetContentRegionAvail().x;
                    float base_gap2 = ImGui::GetStyle().ItemSpacing.x;
                    const float t_base_d = base_d * 0.8f;

                    int target_cols = (t_count <= 1) ? 1 : (t_count + 1) / 2;
                    float denom2 = target_cols * t_base_d + (target_cols - 1) * base_gap2;
                    float s2 = 1.0f;
                    if (denom2 > 0.0f) s2 = std::min(1.0f, avail_w2 / denom2);
                    s2 = std::max(0.6f, s2);

                    float t_slot_d = t_base_d * s2;
                    float t_icon_sz = t_slot_d * 0.62f;
                    float t_gap = base_gap2 * s2;

                    int cols_fit = std::max(1, (int)floorf((avail_w2 + t_gap) / (t_slot_d + t_gap)));
                    int cols = std::min(std::max(1, target_cols), cols_fit);
                    int rows = (t_count + cols - 1) / cols;

                    ImGui::BeginGroup();
                    ImVec2 t_start = ImGui::GetCursorPos();
                    for (int i = 0; i < t_count; ++i) {
                        int row = i / cols;
                        int col = i % cols;
                        int items_in_row = (row == rows - 1) ? (t_count - row * cols) : cols;
                        float row_w = items_in_row * t_slot_d + (items_in_row - 1) * t_gap;
                        float left_pad_row = std::max(0.0f, (avail_w2 - row_w) * 0.5f);

                        ImGui::SetCursorPos(ImVec2(t_start.x + left_pad_row + col * (t_slot_d + t_gap), t_start.y + row * (t_slot_d + t_gap)));
                        ImGui::PushID(i + 1000);
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::InvisibleButton("##typeico", ImVec2(t_slot_d, t_slot_d));
                        bool clicked = ImGui::IsItemClicked();

                        const bool selected = (entries[i].keyLower.empty() && s_outfitTypeFilter.empty()) || (!entries[i].keyLower.empty() && s_outfitTypeFilter == entries[i].keyLower);

                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        float r = t_slot_d * 0.5f;
                        ImVec2 c = ImVec2(p.x + r, p.y + r);
                        dl->AddCircleFilled(c, r, selected ? bg_sel : bg_nrm, 48);
                        dl->AddCircle(c, r, selected ? brd_sel : brd_nrm, 48, 1.0f);

                        if (!entries[i].icon.empty()) {
                            ImVec2 icon_pos = ImVec2(c.x - t_icon_sz * 0.5f, c.y - t_icon_sz * 0.5f);
                            ImGui::SetCursorScreenPos(icon_pos);
                            Icon(entries[i].icon.c_str(), t_icon_sz, ImVec4(1,1,1,1));
                        } else {
                            const char* label = entries[i].label.c_str();
                            ImVec2 ts = ImGui::CalcTextSize(label, label + 1);
                            ImGui::SetCursorScreenPos(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f));
                            ImGui::TextUnformatted(label, label + 1);
                        }

                        if (clicked) {
                            if (entries[i].keyLower.empty()) s_outfitTypeFilter.clear();
                            else s_outfitTypeFilter = entries[i].keyLower;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndGroup();
                    ImGui::Dummy(ImVec2(0, DP(8.0f)));
                }

                CenterSeparator("Outfits");
                static char s_search[128] = "";
                SearchCard("Search...", s_search, sizeof(s_search));
                if (BeginPannableChild("##outfits_scroll")) {
                    std::string q = s_search;
                    for (auto& ch : q) ch = (char)tolower((unsigned char)ch);
                    const bool doFilter = !q.empty();

                    if (defs.is_array()) {
                        struct OutfitRow {
                            std::string name, type, displayName, icon, tval;
                            bool hasIcon;
                            ImVec4 iconColor;
                        };
                        std::vector<OutfitRow> rows;
                        rows.reserve(defs.size());

                        for (const auto& it : defs) {
                            if (!it.is_object()) continue;
                            const auto* ty = it.contains("type") ? &it["type"] : nullptr;
                            if (!ty || !ty->is_string()) continue;
                            std::string type = ty->get<std::string>();
                            std::string tval = type; for (auto& ch : tval) ch = (char)tolower((unsigned char)ch);
                            if (!s_outfitTypeFilter.empty() && tval != s_outfitTypeFilter) continue;

                            std::string name = it.contains("name") && it["name"].is_string() ? it["name"].get<std::string>() : std::string("?");
                            if (name.size() < 10 || name.compare(0, 10, "CharSkyKid") != 0) continue;

                            std::string icon = it.contains("icon") && it["icon"].is_string() ? it["icon"].get<std::string>() : std::string();
                            bool hasIcon = !icon.empty();

                            ImVec4 iconColor(1,1,1,1);
                            if (it.contains("icon_hsv") && it["icon_hsv"].is_array() && it["icon_hsv"].size() >= 3) {
                                float h = it["icon_hsv"][0].get<float>() / 360.0f;
                                float s = it["icon_hsv"][1].get<float>() / 100.0f;
                                float v = it["icon_hsv"][2].get<float>() / 100.0f;
                                float r, g, b;
                                ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
                                iconColor = ImVec4(r, g, b, 1.0f);
                            }

                            std::string displayName = FormatOutfitName(name, type);

                            if (doFilter) {
                                std::string lname = displayName; for (auto& ch : lname) ch = (char)tolower((unsigned char)ch);
                                std::string rid = name; for (auto& ch : rid) ch = (char)tolower((unsigned char)ch);
                                if (lname.find(q) == std::string::npos && rid.find(q) == std::string::npos) continue;
                            }

                            if (tval == "mask") {
                                std::string dlow = displayName; for (auto& ch : dlow) ch = (char)tolower((unsigned char)ch);
                                if (!hasIcon && dlow == std::string("npc")) continue;
                            }

                            rows.push_back(OutfitRow{ name, type, displayName, icon, tval, hasIcon, iconColor });
                        }

                        auto renderRow = [&](const OutfitRow& r){
                            const char* iconLabel = r.hasIcon ? r.icon.c_str() : "UiMiscQuestion";
                            ImGui::PushID(r.name.c_str());
                            bool pressed = ButtonCard(r.displayName.c_str(), nullptr, iconLabel, "WEAR", r.iconColor);
                            ImGui::PopID();
                            if (pressed) {
                                std::string execName = r.name;
                                const char* kPrefix = "CharSkyKid_";
                                size_t prefixLen = std::strlen(kPrefix);
                                if (execName.size() > prefixLen && execName.compare(0, prefixLen, kPrefix) == 0) {
                                    execName = execName.substr(prefixLen);
                                } else {
                                    size_t lastUnderscore = r.name.find_last_of('_');
                                    std::string shortName = (lastUnderscore != std::string::npos && lastUnderscore + 1 < r.name.length()) ? r.name.substr(lastUnderscore + 1) : r.name;
                                    std::string typeCapitalized = r.type; if (!typeCapitalized.empty()) { typeCapitalized[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(typeCapitalized[0]))); }
                                    execName = typeCapitalized + "_" + shortName;
                                }
                                std::string lua = std::string("OutfitSelectSetOutfit(game, \"") + execName + "\")";
                                tsm::lua::queue::Enqueue(lua);
                            }
                        };

                        auto isNone = [](const std::string& s){
                            if (s.size() != 4) return false;
                            return (std::tolower((unsigned char)s[0])=='n' && std::tolower((unsigned char)s[1])=='o' && std::tolower((unsigned char)s[2])=='n' && std::tolower((unsigned char)s[3])=='e');
                        };

                        for (const auto& r : rows) if (isNone(r.displayName)) renderRow(r);
                        for (const auto& r : rows) if (!isNone(r.displayName)) renderRow(r);
                    } else {
                        ImGui::TextDisabled("No OutfitDefs loaded.");
                    }
                }
                EndPannableChild();                break; }
            case 1: {
                static int  s_dyeRegion = 0;
                static int  s_slotIndex = 0;
                static bool s_removeDye = false;

                static const char* kSlotOptions[] = {
                    "body", "face", "feet", "hair", "hat", "horn", "mask", "neck", "prop", "wing"
                };
                static std::vector<const char*> kSlotLabels;
                if (kSlotLabels.empty()) {
                    for (const auto& cat : tsm::game::data::kOutfitCategories) {
                        kSlotLabels.push_back(cat.c_str());
                    }
                }
                const int slotCount = (int)(sizeof(kSlotOptions)/sizeof(kSlotOptions[0]));

                if (BeginPannableChild("##dyes_scroll")) {
                    CenterSeparator("Target");
                    SelectCardIcon("Slot", "Choose outfit slot to dye", "UiMenuDye", &s_slotIndex, kSlotLabels.data(), slotCount);
                    {
                        static const char* kRegionOptions[] = { "Primary", "Secondary" };
                        SelectCardIcon("Region", "Choose dye region", "UiMenuDyeCraftingGradient", &s_dyeRegion, kRegionOptions, 2);
                    }

                    ToggleCardIcon("Remove Dye", "Remove dye from selected slot/region", "UiMenuDyeCraftingOutline", &s_removeDye);

                    CenterSeparator("Randomize");
                    if (StandardButton("Randomize Outfit Dyes")) {
                        tsm::lua::queue::Enqueue("RandomizeOutfitDyes(game)");
                        tsm::ui::helpers::ShowToastSuccess("Randomized outfit dyes");
                    }
                    if (StandardButton("Randomize Placeable Dyes", false)) {
                        tsm::lua::queue::Enqueue("PlaceableDyeRandomize(game)");
                        tsm::ui::helpers::ShowToastSuccess("Randomized placeable dyes");
                    }
                    if (StandardButton("Remove Placeable Dyes", false)) {
                        tsm::lua::queue::Enqueue("PlaceableDyeRemove(game)");
                        tsm::ui::helpers::ShowToastSuccess("Removed placeable dyes");
                    }

                    CenterSeparator("Colors");
                    const auto& arr = tsm::data::DataManager::Get().GetDyeColorDefs();
                    if (arr.is_array()) {

                        float side_padding = DP(2.0f);
                        float avail_w = ImGui::GetContentRegionAvail().x - (side_padding * 2.0f);
                        float gap = ImGui::GetStyle().ItemSpacing.x;
                        float cell_d = DP(36.0f);
                        int cols = (int)std::max(1.0f, floorf((avail_w + gap) / (cell_d + gap)));
                        float row_w = cols * cell_d + (cols - 1) * gap;
                        float left_pad = side_padding + std::max(0.0f, (avail_w - row_w) * 0.5f);

                        ImVec2 start = ImGui::GetCursorPos();
                        int i = 0;
                        for (const auto& it : arr) {
                            if (!it.is_object()) continue;
                            const auto* nameP = it.contains("name") ? &it["name"] : nullptr;
                            const auto* hsvP  = it.contains("hsv")  ? &it["hsv"]  : nullptr;
                            const auto* secP  = it.contains("secondary_dye") ? &it["secondary_dye"] : nullptr;
                            const auto* priP  = it.contains("primary_dye") ? &it["primary_dye"] : nullptr;
                            if (!nameP || !nameP->is_string() || !hsvP || !hsvP->is_array() || hsvP->size() < 3) continue;
                            std::string name = nameP->get<std::string>();

                            float H = 0.0f, S = 0.0f, V = 0.0f;
                            try {
                                H = (*hsvP)[0].get<float>() / 360.0f;
                                S = (*hsvP)[1].get<float>() / 100.0f;
                                V = (*hsvP)[2].get<float>() / 100.0f;
                            } catch (...) { H = S = 0.0f; V = 0.35f; }
                            float r=0,g=0,b=0; ImGui::ColorConvertHSVtoRGB(H, S, V, r, g, b);

                            int col = i % cols;
                            int row = i / cols;
                            ImVec2 pos = ImVec2(start.x + left_pad + col * (cell_d + gap), start.y + row * (cell_d + gap));
                            ImGui::SetCursorPos(pos);
                            ImGui::PushID(name.c_str());

                            bool clicked = ColorCircleButton(ImVec4(r, g, b, 1.0f), 36.0f);

                            if (clicked) {
                                const char* slotName = kSlotOptions[std::clamp(s_slotIndex, 0, slotCount-1)];
                                std::string chosen = name;
                                if (s_dyeRegion == 0) {
                                    if (priP && priP->is_string() && !priP->get<std::string>().empty()) chosen = priP->get<std::string>();
                                } else {
                                    if (secP && secP->is_string() && !secP->get<std::string>().empty()) chosen = secP->get<std::string>();
                                }
                                tsm::lua::helpers::SetOutfitDye(chosen, s_dyeRegion, slotName, s_removeDye);
                            }
                            ImGui::PopID();
                            ++i;
                        }
                        float used_h = (float)((i + cols - 1) / cols) * (cell_d + gap);
                        ImGui::Dummy(ImVec2(0, std::max(0.0f, used_h - (ImGui::GetCursorPosY() - start.y))));
                    } else {
                        PaddedTextDisabled("No DyeColorDefs loaded.");
                        if (StandardButton("Reload")) {
                            tsm::data::DataManager::Get().LoadDyeColorDefs();
                        }

                    }
                }
                EndPannableChild();
                break; }
            case 2: {
                if (BeginPannableChild("##tools_scroll")) {
                    CenterSeparator("Character Adjustments");

                    auto avatarInfo = tsm::game::api::GetAvatarInfoByIndex(0);
                    if (avatarInfo.avatarScale && avatarInfo.avatarHeight) {
                        static float s_currentScale = 1.0f;
                        static float s_currentHeight = 1.0f;
                        static float s_originalScale = 0.0f;
                        static float s_originalHeight = 0.0f;
                        static bool s_initialized = false;

                        s_currentScale = *avatarInfo.avatarScale;
                        s_currentHeight = *avatarInfo.avatarHeight;

                        if (!s_initialized) {
                            s_originalScale = s_currentScale;
                            s_originalHeight = s_currentHeight;
                            s_initialized = true;
                        }

                        if (FloatCardIcon("Scale", "Adjust character size", "UiMenuBuffRerollHeight", &s_currentScale, -3.0f, 10.0f, s_originalScale, "%.2f")) {
                            tsm::game::memory::WriteBytes(avatarInfo.avatarScale, &s_currentScale, sizeof(float));
                        }

                        if (FloatCardIcon("Height", "Adjust character height", "UiMenuBuffRerollHeight", &s_currentHeight, -3.0f, 10.0f, s_originalHeight, "%.2f")) {
                            tsm::game::memory::WriteBytes(avatarInfo.avatarHeight, &s_currentHeight, sizeof(float));
                        }
                    }

                    CenterSeparator("Outfit Modifications");

                    if (ToggleCardIcon("Remove Cape", "Hide the cape from your avatar", "UiOutfitCapeNone", &s_removeCape)) {
                        if (s_removeCape) {
                            tsm::game::memory::EnsureInitialized();
                            s_removeCapePatches.clear();
                            if (tsm::game::memory::CreateNopPatchesForPattern(kRemoveCape, s_removeCapePatches, 1) > 0) {
                                for (auto& p : s_removeCapePatches) p.Apply();
                            } else {
                                s_removeCape = false;
                            }
                        } else {
                            for (auto& p : s_removeCapePatches) if (p.address) p.Restore();
                        }
                    }

                    static bool s_iosHeadphones = false;
                    if (ToggleCardIcon("iOS Headphones", "Toggle iOS headphones accessory", "UiOutfitHornAP04EarMuffs", &s_iosHeadphones)) {
                        tsm::game::memory::EnsureInitialized();
                        void* p = tsm::game::memory::RvaToPtr(tsm::game::Offsets::kIOSHeadphones);
                        if (s_iosHeadphones) {
                            std::uint32_t original = 0x1E249000;
                            tsm::game::memory::WriteBytes(p, &original, 4);
                        } else {
                            std::uint32_t nop = 0xD503201F;
                            tsm::game::memory::WriteBytes(p, &nop, 4);
                        }
                    }

                    CenterSeparator("Quick Actions");

                    if (ButtonCard("Portable Closet", "Creates outfit selection stations around you", "UiMenuCollectMask", "CREATE")) {
                        vec3 position = tsm::game::api::LocalAvatarPosition();
                        float radius = 1.0f;
                        std::vector<int> buttonIndices = { 0, 1, 2, 3, 8 };

                        for (size_t i = 0; i < buttonIndices.size(); ++i) {
                            float angle = (2.0f * 3.14159265358979323846f / buttonIndices.size()) * i;
                            vec3 adjustedPosition = position;
                            adjustedPosition.x += std::cos(angle) * radius;
                            adjustedPosition.z += std::sin(angle) * radius;
                            std::ostringstream luaScript;
                            luaScript << "TOutfitSelect(" << adjustedPosition.x << ", " << adjustedPosition.y << ", " << adjustedPosition.z << ", " << buttonIndices[i] << ")";
                            tsm::lua::queue::Enqueue(luaScript.str());
                        }
                    }

                    if (ButtonCard("Randomize Outfit", "Applies random outfit pieces", "UiMenuRandomGenerator", "RANDOMIZE")) {
                        tsm::lua::queue::Enqueue("Randomize(game)");
                    }

                    CenterSeparator("Presets");

                    if (ButtonCard("Become Austin", "Wear Austin's signature outfit", "UiMenuCollectMask", "APPLY")) {
                        tsm::lua::queue::Enqueue("BecomeAustin(game)");
                    }

                    if (ButtonCard("Become TayLee", "Wear TayLee's signature outfit", "UiMenuCollectMask", "APPLY")) {
                        tsm::lua::queue::Enqueue("BecomeTayLee(game)");
                    }

                    if (ButtonCard("Maxed Outfit", "Wear the maximum tier outfit", "UiMenuWingBuff", "APPLY")) {
                        tsm::lua::queue::Enqueue("WearMaxOutfit(game)");
                    }

                    if (ButtonCard("Clear Outfit", "Remove all outfit pieces", "UiMenuDeleteAll", "CLEAR")) {
                        tsm::lua::queue::Enqueue("ClearOutfit(game)");
                    }
                }
                EndPannableChild();
                break; }
            case 3: {
                struct SavedOutfit {
                    char name[128];
                    nlohmann::json data;
                };

                static std::vector<SavedOutfit> s_savedOutfits;
                static char s_newOutfitName[128] = "";
                static bool s_showSaveModal = false;
                static bool s_showEditModal = false;
                static int s_editOutfitIdx = -1;
                static bool s_outfitsLoaded = false;

                auto GetOutfitsDir = []() -> std::string {
                    const char* cfgDir = CipherUtils::get_ConfigsPath();
                    std::string baseDir = (cfgDir && *cfgDir) ? std::string(cfgDir)
                                                               : std::string("/data/data/git.artdeell.skymodloader/files");
                    return baseDir + "/Outfits";
                };

                auto EnsureDirectoryExists = [](const std::string& path) -> bool {
                    struct stat st;
                    if (stat(path.c_str(), &st) == 0) {
                        return S_ISDIR(st.st_mode);
                    }
                    if (mkdir(path.c_str(), 0755) == 0) {
                        return true;
                    }
                    return (errno == EEXIST);
                };

                auto SaveOutfitsToFile = [&]() -> bool {
                    std::string outfitsDir = GetOutfitsDir();
                    if (!EnsureDirectoryExists(outfitsDir)) {
                        return false;
                    }

                    nlohmann::json j;
                    j["version"] = 1;
                    j["outfits"] = nlohmann::json::array();

                    for (const auto& outfit : s_savedOutfits) {
                        nlohmann::json jOutfit;
                        jOutfit["name"] = std::string(outfit.name);
                        jOutfit["data"] = outfit.data;
                        j["outfits"].push_back(jOutfit);
                    }

                    std::string filePath = outfitsDir + "/saved_outfits.json";
                    try {
                        std::ofstream ofs(filePath, std::ios::binary);
                        if (!ofs.is_open()) return false;
                        ofs << j.dump(2);
                        ofs.close();
                        return true;
                    } catch (...) {
                        return false;
                    }
                };

                auto LoadOutfitsFromFile = [&]() -> bool {
                    std::string filePath = GetOutfitsDir() + "/saved_outfits.json";

                    try {
                        std::ifstream ifs(filePath, std::ios::binary);
                        if (!ifs.is_open()) return false;

                        nlohmann::json j;
                        ifs >> j;
                        ifs.close();

                        if (j.contains("outfits") && j["outfits"].is_array()) {
                            s_savedOutfits.clear();
                            for (const auto& jOutfit : j["outfits"]) {
                                if (!jOutfit.is_object()) continue;

                                SavedOutfit outfit;
                                std::string name = jOutfit.value("name", "");
                                std::strncpy(outfit.name, name.c_str(), sizeof(outfit.name) - 1);
                                outfit.name[sizeof(outfit.name) - 1] = '\0';
                                outfit.data = jOutfit.value("data", nlohmann::json::object());
                                s_savedOutfits.push_back(outfit);
                            }
                        }

                        return true;
                    } catch (...) {
                        return false;
                    }
                };

                auto ApplyOutfit = [](const nlohmann::json& outfitData) {
                    const auto& OUTFIT_CATEGORIES = tsm::game::data::kOutfitCategories;
                    const auto& SPECIAL_ITEM_TYPES = tsm::game::data::kSpecialOutfitTypes;


                    auto applyOutfitItem = [&](const std::string& category, const std::string& lowerCategory, const nlohmann::json& outfitJson) {
                        if (!outfitJson.contains(lowerCategory)) return;
                        const auto& item = outfitJson[lowerCategory];
                        if (!item.is_object()) return;

                        std::string itemName;
                        if (item.contains("name") && item["name"].is_string()) {
                            itemName = item["name"].get<std::string>();
                        } else if (item.contains("id")) {
                            return;
                        } else {
                            return;
                        }

                        std::string execName = itemName;
                        const char* kPrefix = "CharSkyKid_";
                        size_t prefixLen = std::strlen(kPrefix);
                        if (execName.size() > prefixLen && execName.compare(0, prefixLen, kPrefix) == 0) {
                            execName = execName.substr(prefixLen);
                        }

                        std::string lua = std::string("OutfitSelectSetOutfit(game, \"") + execName + "\")";
                        tsm::lua::queue::Enqueue(lua);

                        if (item.contains("dye") && item["dye"].is_string()) {
                            std::string dyeStr = item["dye"].get<std::string>();

                            if (dyeStr.size() > 2 && dyeStr.front() == '(' && dyeStr.back() == ')') {
                                std::string inner = dyeStr.substr(1, dyeStr.size() - 2);
                                size_t commaPos = inner.find(',');

                                if (commaPos != std::string::npos) {
                                    std::string primaryDye = inner.substr(0, commaPos);
                                    std::string secondaryDye = inner.substr(commaPos + 1);

                                    if (primaryDye != "none" && !primaryDye.empty()) {
                                        tsm::lua::helpers::SetOutfitDye(primaryDye, 0, lowerCategory.c_str(), false);
                                    }

                                    if (secondaryDye != "none" && !secondaryDye.empty()) {
                                        tsm::lua::helpers::SetOutfitDye(secondaryDye, 1, lowerCategory.c_str(), false);
                                    }
                                }
                            }
                        }
                    };

                    auto applySpecialOutfitItem = [&](const std::string& itemType, const nlohmann::json& outfitJson) {
                        applyOutfitItem(itemType, itemType, outfitJson);
                    };

                    for (const auto& category : OUTFIT_CATEGORIES) {
                        std::string lua = "OutfitSelectSetOutfit(game, \"" + category + "_None\");";
                        tsm::lua::queue::Enqueue(lua);
                    }

                    for (const auto& category : OUTFIT_CATEGORIES) {
                        std::string lowerCategory = ToLower(category);
                        tsm::lua::helpers::SetOutfitDye("red_red", 0, lowerCategory.c_str(), true);
                        tsm::lua::helpers::SetOutfitDye("red_red", 1, lowerCategory.c_str(), true);
                    }

                    for (const auto& category : OUTFIT_CATEGORIES) {
                        std::string lowerCategory = ToLower(category);
                        if (std::find(SPECIAL_ITEM_TYPES.begin(), SPECIAL_ITEM_TYPES.end(), lowerCategory) != SPECIAL_ITEM_TYPES.end()) {
                            continue;
                        }
                        applyOutfitItem(category, lowerCategory, outfitData);
                    }

                    for (const auto& itemType : SPECIAL_ITEM_TYPES) {
                        applySpecialOutfitItem(itemType, outfitData);
                    }
                };

                if (!s_outfitsLoaded) {
                    LoadOutfitsFromFile();
                    s_outfitsLoaded = true;
                }

                CenterSeparator("Quick Save");

                if (ButtonCard("Save Current Outfit", "Save your current outfit configuration", "UiMiscPlusMedium", "SAVE")) {
                    s_showSaveModal = true;
                    std::snprintf(s_newOutfitName, sizeof(s_newOutfitName), "Outfit %d", (int)s_savedOutfits.size() + 1);
                }

                ImGui::Dummy(ImVec2(0, DP(8.0f)));
                CenterSeparator("Saved Outfits");

                if (BeginPannableChild("##saved_outfits_scroll")) {
                    if (s_savedOutfits.empty()) {
                        PaddedTextDisabled("No saved outfits yet");
                    } else {
                        for (size_t i = 0; i < s_savedOutfits.size(); ++i) {
                            const auto& outfit = s_savedOutfits[i];

                            ImGui::PushID((int)i);

                            auto result = EditableCard(outfit.name, nullptr, "UiMenuCollectMask");

                            if (result.main) {
                                ApplyOutfit(outfit.data);
                                tsm::ui::helpers::ShowToastSuccess("Applied outfit");
                            }

                            if (result.edit) {
                                s_editOutfitIdx = (int)i;
                                s_showEditModal = true;
                                std::strncpy(s_newOutfitName, outfit.name, sizeof(s_newOutfitName) - 1);
                            }

                            ImGui::PopID();
                        }
                    }
                }
                EndPannableChild();

                if (s_showSaveModal) {
                    ImGui::OpenPopup("##SaveOutfitModal");
                    s_showSaveModal = false;
                }

                if (InputPopup("##SaveOutfitModal", "Save Outfit", "Enter outfit name",
                              s_newOutfitName, sizeof(s_newOutfitName), "Save", "Cancel")) {
                    static tsm::network::ApiClient client;
                    auto outfit_info = client.GetCurrentOutfit();

                    nlohmann::json outfitData;
                    for (const auto& [part_name, part] : outfit_info.parts) {
                        outfitData[part_name]["name"] = part.name;
                        if (!part.dye.empty()) outfitData[part_name]["dye"] = part.dye;
                        if (part.mask != 0) outfitData[part_name]["mask"] = part.mask;
                        if (part.pat != 0) outfitData[part_name]["pat"] = part.pat;
                        if (part.tex != 0) outfitData[part_name]["tex"] = part.tex;
                    }
                    if (outfit_info.height != 0.0f) outfitData["height"] = outfit_info.height;
                    if (outfit_info.scale != 0.0f) outfitData["scale"] = outfit_info.scale;
                    if (outfit_info.voice != 0) outfitData["voice"] = outfit_info.voice;

                    if (!outfitData.empty()) {
                        SavedOutfit outfit;
                        std::strncpy(outfit.name, s_newOutfitName, sizeof(outfit.name) - 1);
                        outfit.name[sizeof(outfit.name) - 1] = '\0';
                        outfit.data = outfitData;
                        s_savedOutfits.push_back(outfit);
                        SaveOutfitsToFile();
                        tsm::ui::helpers::ShowToastSuccess("Outfit saved");
                    } else {
                        tsm::ui::helpers::ShowToastError("Failed to retrieve outfit data");
                    }
                }

                if (s_showEditModal) {
                    ImGui::OpenPopup("##EditOutfitModal");
                    s_showEditModal = false;
                }

                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(DP(280.0f), 0), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(DP(12.0f), DP(10.0f)));
                if (ImGui::BeginPopupModal("##EditOutfitModal", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                    if (s_editOutfitIdx >= 0 && s_editOutfitIdx < (int)s_savedOutfits.size()) {
                        const char* title = tsm::ui::i18n::Tr("Edit Outfit");
                        float title_w = ImGui::CalcTextSize(title).x;
                        float window_w = ImGui::GetContentRegionAvail().x;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (window_w - title_w) * 0.5f);
                        ImGui::Text("%s", title);
                        ImGui::Separator();

                        ImGui::Text("%s", tsm::ui::i18n::Tr("Name:"));
                        ImGui::PushItemWidth(-1);
                        InputWithPlaceholder("##editname", s_newOutfitName, sizeof(s_newOutfitName), "Outfit name");
                        ImGui::PopItemWidth();

                        ImGui::Dummy(ImVec2(0, DP(8.0f)));

                        float btn_w = (ImGui::GetContentRegionAvail().x - DP(8.0f)) / 2.0f;

                        if (AccentButton("Save", ImVec2(btn_w, 0))) {
                            std::strncpy(s_savedOutfits[s_editOutfitIdx].name, s_newOutfitName,
                                       sizeof(s_savedOutfits[s_editOutfitIdx].name) - 1);
                            SaveOutfitsToFile();
                            tsm::ui::helpers::ShowToastSuccess("Outfit updated");
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if (SecondaryButton("Cancel", ImVec2(btn_w, 0))) {
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::Dummy(ImVec2(0, DP(4.0f)));
                        ImGui::Separator();
                        ImGui::Dummy(ImVec2(0, DP(4.0f)));

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.3f, 0.3f, 0.9f));
                        const char* delLabel = tsm::ui::i18n::Tr("Delete Outfit");
                        if (ImGui::Button(delLabel, ImVec2(-1, 0))) {
                            s_savedOutfits.erase(s_savedOutfits.begin() + s_editOutfitIdx);
                            SaveOutfitsToFile();
                            tsm::ui::helpers::ShowToastSuccess("Outfit deleted");
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopStyleColor(3);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();

                break; }
        }
        EndCard();
    }
}

}}}
