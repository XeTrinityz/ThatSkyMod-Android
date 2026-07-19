#pragma once

#include <string>
#include <utils/common/vec3.h>

namespace tsm { namespace lua { namespace helpers {


void ChangeLevel(const std::string& levelName, bool useFade = true);
void ReloadLevel();
void CreatePortalAt(const vec3& pos, const std::string& levelName, const std::string& hintText = "");

void SetPlayerPosition(const vec3& pos, bool alignToGround = false);
void EnableCrawl(bool enable);

void GrantSpell(const std::string& spellId);
void RemoveSpell(const std::string& spellId);

void TriggerToy(const std::string& toyId);

void PlayEmote(const std::string& emoteName, float speedMultiplier = 1.0f, float loopDuration = 0.0f, int level = 1);
void PlayAnimation(const std::string& animationName);

void SetVoice(const std::string& voiceType);

void TriggerSocialRecording();
void TriggerMessageStones();
void TriggerToy(const std::string& toyName);
void SocialGift();
void SocialRandomGame();
void SocialFireworks();
void SocialFireworksMenu();

void SetOutfitDye(const std::string& dyeColor, int region, const std::string& slotName, bool removeDye = false);
void RandomizeOutfitDyes();

void SetAudienceSettings(bool showReactionMenu, bool replaceLocalAvatar, bool replaceOtherAvatars, const std::string& visType = "");
void SetMicroMode();

void AvatarSpeak(const std::string& message);

void OpenMenu(const std::string& menuName);
void OpenURL(const std::string& url);

void DisplayText(const std::string& text, const vec3& screenPos, float duration);
void SetCameraDesaturation(float intensity, int filterType);
void SetRainIntensity(float intensity);
void SetWindIntensity(float intensity);
void SetBulletTime(float duration, float speed);
void DisplayTitleCard(const std::string& mainTitle, const std::string& subTitle, bool forceFirstTime = false);

void PlaySound2D(const std::string& soundName, float volume = 1.0f);
void PlayMusic(const std::string& musicName, bool foreground = true, float volume = 1.0f);
void SetMusicKey(float key);

void Unlock(const std::string& name, bool acknowledge = true);
void CollectCollectible(const std::string& name, const std::string& collectibleType);
void TryCompleteQuestOrIncrementStat(const std::string& questName);
void TryCompleteQuest(const std::string& questName);
void AutoCollectCompletedQuest(const std::string& questName);
void AutoAcceptSmartQuest(const std::string& questName);
void AddFloatToStat(const std::string& statName, float value);
void PlaySeasonFinale();

void OpenConstellationShop(const std::string& shopName, const vec3& position);
void GenericShopNpcHint(const std::string& shopName, const std::string& iconName, const vec3& position);

void SpawnShardnado(const vec3& position);
void ManualShardFall(const vec3& position);
void PlacePuzzleKey(const vec3& position);
void PlacePuzzleBallPedestal(const vec3& position);

void LocalDoAudienceReact(float power, int shoutType);

void ClearRecentPlayers();
void RainbowParty();
void WingBuffDepositShrineResetHint();

}}}
