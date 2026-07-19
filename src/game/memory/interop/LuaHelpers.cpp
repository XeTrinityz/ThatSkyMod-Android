#include <game/interop/LuaHelpers.h>
#include <game/interop/LuaScriptQueue.h>
#include <string>
#include <sstream>

namespace tsm { namespace lua { namespace helpers {

namespace {
    std::string EscapeLuaString(const std::string& s) {
        std::string result;
        result.reserve(s.size() + 8);
        for (char c : s) {
            if (c == '\\' || c == '"') result += '\\';
            result += c;
        }
        return result;
    }
}

void ChangeLevel(const std::string& levelName, bool useFade) {
    std::string script = "TChangeLevelWithFade(\"" + EscapeLuaString(levelName) + "\", " +
                        (useFade ? "true" : "false") + ")";
    queue::Enqueue(script);
}

void ReloadLevel() {
    queue::Enqueue("TReloadLevel()");
}

void CreatePortalAt(const vec3& pos, const std::string& levelName, const std::string& hintText) {
    std::string hint = hintText.empty() ? levelName : hintText;
    std::ostringstream script;
    script << "TCreatePortal({" << pos.x << ", " << pos.y << ", " << pos.z << "}, \""
           << EscapeLuaString(levelName) << "\", \"" << EscapeLuaString(hint) << "\")";
    queue::Enqueue(script.str());
}

void SetPlayerPosition(const vec3& pos, bool alignToGround) {
    std::ostringstream script;
    script << "TAvatarSetPos({" << pos.x << ", " << pos.y << ", " << pos.z << "}, "
           << (alignToGround ? "true" : "false") << ")";
    queue::Enqueue(script.str());
}

void EnableCrawl(bool enable) {
    std::string script = "TAvatarCrawl(" + std::string(enable ? "true" : "false") + ")";
    queue::Enqueue(script);
}

void GrantSpell(const std::string& spellId) {
    queue::Enqueue("TGrantSpell(\"" + EscapeLuaString(spellId) + "\")");
}

void RemoveSpell(const std::string& spellId) {
    queue::Enqueue("TRemoveSpell(\"" + EscapeLuaString(spellId) + "\")");
}

void TriggerToy(const std::string& toyId) {
    queue::Enqueue("TTriggerToyEvent(\"" + EscapeLuaString(toyId) + "\")");
}

void PlayEmote(const std::string& emoteName, float speedMultiplier, float loopDuration, int level) {
    if (speedMultiplier <= 0.0f) speedMultiplier = 1.0f;
    if (loopDuration < 0.0f) loopDuration = 0.0f;

    std::ostringstream script;
    script << "TAvatarPlayEmote(\"" << EscapeLuaString(emoteName) << "\", "
           << speedMultiplier << ", " << loopDuration << ", " << level << ")";
    queue::Enqueue(script.str());
}

void PlayAnimation(const std::string& animationName) {
    queue::Enqueue("game:avatarBarn():DEBUG_PreviewAnimationSequence(game, \"" +
                  EscapeLuaString(animationName) + "\")");
}

void SetVoice(const std::string& voiceType) {
    queue::Enqueue("TAvatarSetVoice(\"" + EscapeLuaString(voiceType) + "\")");
}

void TriggerSocialRecording() { queue::Enqueue("TGrantSocialRecording()"); }
void TriggerMessageStones() { queue::Enqueue("TSocialMessageStones()"); }
void SocialGift() { queue::Enqueue("TSocialGift()"); }
void SocialRandomGame() { queue::Enqueue("TSocialRandomGame()"); }
void SocialFireworks() { queue::Enqueue("TSocialFireworks()"); }
void SocialFireworksMenu() { queue::Enqueue("TSocialFireworksMenu()"); }

void SetOutfitDye(const std::string& dyeColor, int region, const std::string& slotName, bool removeDye) {
    std::ostringstream script;
    script << "TAvatarSetOutfitDye(\"" << EscapeLuaString(dyeColor) << "\", "
           << region << ", \"" << EscapeLuaString(slotName) << "\", "
           << (removeDye ? "true" : "false") << ")";
    queue::Enqueue(script.str());
}

void RandomizeOutfitDyes() {
    queue::Enqueue("RandomizeOutfitDyes(game)");
}

void SetAudienceSettings(bool showReactionMenu, bool replaceLocalAvatar, bool replaceOtherAvatars, const std::string& visType) {
    std::string visArg = visType.empty() ? "nil" : ("\"" + visType + "\"");
    std::ostringstream script;
    script << "TAudienceSettings(" << (showReactionMenu ? "true" : "false") << ", "
           << (replaceLocalAvatar ? "true" : "false") << ", "
           << (replaceOtherAvatars ? "true" : "false") << ", " << visArg << ")";
    queue::Enqueue(script.str());
}

void SetMicroMode() {
    queue::Enqueue("TSetAvatarMicroMode()");
}

void AvatarSpeak(const std::string& message) {
    queue::Enqueue("TAvatarSpeak(\"" + EscapeLuaString(message) + "\")");
}

void OpenMenu(const std::string& menuName) {
    queue::Enqueue("TOpenMenu(\"" + EscapeLuaString(menuName) + "\")");
}

void OpenURL(const std::string& url) {
    queue::Enqueue("TOpenURLBST(\"" + EscapeLuaString(url) + "\")");
}

void DisplayText(const std::string& text, const vec3& screenPos, float duration) {
    std::ostringstream script;
    script << "TDisplayText(\"" << EscapeLuaString(text) << "\", {"
           << screenPos.x << ", " << screenPos.y << ", " << screenPos.z << "}, " << duration << ")";
    queue::Enqueue(script.str());
}

void SetCameraDesaturation(float intensity, int filterType) {
    std::ostringstream script;
    script << "TCameraDesaturationEffect(" << intensity << ", " << filterType << ")";
    queue::Enqueue(script.str());
}

void SetRainIntensity(float intensity) {
    std::ostringstream script;
    script << "TSetRainParams(" << intensity << ")";
    queue::Enqueue(script.str());
}

void SetWindIntensity(float intensity) {
    std::ostringstream script;
    script << "TSetWindIntensity(" << intensity << ")";
    queue::Enqueue(script.str());
}

void SetBulletTime(float duration, float speed) {
    std::ostringstream script;
    script << "TSetBulletTime(" << duration << ", " << speed << ")";
    queue::Enqueue(script.str());
}

void DisplayTitleCard(const std::string& mainTitle, const std::string& subTitle, bool forceFirstTime) {
    std::ostringstream script;
    script << "TDisplayTitleCard(\"" << EscapeLuaString(mainTitle) << "\", \""
           << EscapeLuaString(subTitle) << "\", " << (forceFirstTime ? "true" : "false") << ")";
    queue::Enqueue(script.str());
}

void PlaySound2D(const std::string& soundName, float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    std::ostringstream script;
    script << "TPlaySound2D(\"" << EscapeLuaString(soundName) << "\", " << volume << ")";
    queue::Enqueue(script.str());
}

void PlayMusic(const std::string& musicName, bool foreground, float volume) {
    std::ostringstream script;
    script << "TPlayMusic(\"" << EscapeLuaString(musicName) << "\", "
           << (foreground ? "true" : "false") << ", " << volume << ")";
    queue::Enqueue(script.str());
}

void SetMusicKey(float key) {
    std::ostringstream script;
    script << "TSetMusicKey(" << key << ")";
    queue::Enqueue(script.str());
}

void CollectCollectible(const std::string& name, const std::string& collectibleType) {
    queue::Enqueue("TCollectCollectible(\"" + EscapeLuaString(name) + "\", \"" +
                  EscapeLuaString(collectibleType) + "\")");
}

void TryCompleteQuestOrIncrementStat(const std::string& questName) {
    queue::Enqueue("TTryCompleteQuestOrIncrementStat(\"" + EscapeLuaString(questName) + "\")");
}

void AutoCollectCompletedQuest(const std::string& questName) {
    queue::Enqueue("TAutoCollectCompletedQuest(\"" + EscapeLuaString(questName) + "\")");
}

void TryCompleteQuest(const std::string& questName) {
    queue::Enqueue("TTryCompleteQuest(\"" + EscapeLuaString(questName) + "\")");
}

void AutoAcceptSmartQuest(const std::string& questName) {
    queue::Enqueue("TAutoAcceptSmartQuest(\"" + EscapeLuaString(questName) + "\")");
}

void AddFloatToStat(const std::string& statName, float value) {
    std::ostringstream script;
    script << "TAddFloatToStatUsingName(\"" << EscapeLuaString(statName) << "\", " << value << ")";
    queue::Enqueue(script.str());
}

void OpenConstellationShop(const std::string& shopName, const vec3& position) {
    std::ostringstream script;
    script << "TOpenConstellationShop(\"" << EscapeLuaString(shopName) << "\", "
           << position.x << ", " << position.y << ", " << position.z << ")";
    queue::Enqueue(script.str());
}

void SpawnShardnado(const vec3& position) {
    std::ostringstream script;
    script << "TShardnado({" << position.x << ", " << position.y << ", " << position.z << "})";
    queue::Enqueue(script.str());
}

void ManualShardFall(const vec3& position) {
    std::ostringstream script;
    script << "TManualShardFall({" << position.x << ", " << position.y << ", " << position.z << "})";
    queue::Enqueue(script.str());
}

void PlacePuzzleKey(const vec3& position) {
    std::ostringstream script;
    script << "TPlacePuzzleKey({" << position.x << ", " << position.y << ", " << position.z << "})";
    queue::Enqueue(script.str());
}

void PlacePuzzleBallPedestal(const vec3& position) {
    std::ostringstream script;
    script << "TPuzzleBallPedestal({" << position.x << ", " << position.y << ", " << position.z << "})";
    queue::Enqueue(script.str());
}

void LocalDoAudienceReact(float power, int shoutType) {
    std::ostringstream script;
    script << "TLocalDoAudienceReact(" << power << ", " << shoutType << ")";
    queue::Enqueue(script.str());
}

void ClearRecentPlayers() {
    queue::Enqueue("TClearRecentPlayers()");
}

void RainbowParty() {
    queue::Enqueue("TAvatarRainbowParty()");
}

void WingBuffDepositShrineResetHint() {
    queue::Enqueue("TWingBuffDepositShrineResetHint()");
}

}}}
