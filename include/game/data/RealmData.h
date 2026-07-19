#pragma once

#include <vector>
#include <string>
#include <unordered_set>

namespace tsm { namespace game { namespace data {

inline const std::vector<std::string> kRealmNames = {
    "DawnStatueForm",
    "DayStatueForm",
    "RainStatueForm",
    "SunsetStatueForm",
    "DuskStatueForm",
    "NightStatueForm",
    "StormStatueForm"
};

inline const std::unordered_set<std::string> kIgnoreLevels = {
    "Storm", "MainStreetFlyingIntro", "MainStreet_ShopOutfits", "MainStreet_ShopSpells", "MainStreet_ShopProps",
    "MainStreet_Apartment", "MainStreet_Cafe", "MainStreet_Cafe_Wonderland", "MainStreet_ConcertHall", "MainStreet_Soundbath", "Night_InfiniteDesert",
    "Prairie_WildLifePark", "SunsetRace", "Sunset_Theater", "SunsetVillage_MusicShop", "SunsetEnd",
    "Event_Arr_Runaway", "Event_Arr_SoftInside", "Event_Arr_Warrior", "Event_Arr_TheSeed", "Event_Arr_EyesOfAChild",
    "Event_Arr_ExhaleInhale", "Event_Cinema", "Nintendo_CandleSpace", "VoidSharedSpace", "Event_DaysOfMischief", "Dusk_TriangleEnd",
    "NightEnd", "NightDesert_Planets", "Night_IPHallway", "Night_ValleyForest", "Night_ValleyHouse", "Season24Void1",
    "Night_StoryBook", "StormEvent_VoidSpace", "StormStart", "Storm", "StormEnd", "OrbitMid", "OrbitEnd", "CandleSpaceEnd", "Credits", "TGCOffice",
    "Night_ValleyHouseSky", "ECWaxTest", "Rain_BlueBirdTheater", "DuskMid_Past", "DuskMid_PastMarket", "DuskEnd_Past", "Dusk_HiddenTemple", "Event_DaysOfMoonlight_HugeMoon"
};

inline const std::vector<std::string> kRealmOrder = {
    "dawn", "prairie", "rain", "sunset", "dusk", "night", "storm"
};

}}}
