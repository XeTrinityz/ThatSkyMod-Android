#pragma once

#include <cstdint>
#include <utils/common/obfuscate.h>

namespace tsm { namespace game { namespace Offsets {

	constexpr std::uintptr_t Game = 0x2F086F0;
	constexpr std::uintptr_t AudienceBarn = 0x2C9D7B0;
	constexpr std::uintptr_t CandleBarn = 0x2CC6968;

	constexpr std::uintptr_t kLuaState = 0x20;
	constexpr std::uintptr_t kAvatarBarn = 0x310;
	constexpr std::uintptr_t kNetPlayerBarnPtr = 0x1C8;
	constexpr std::uintptr_t kNetPlayerBarnOffset = 0x20;

	constexpr std::uintptr_t kFirstPlayerIdOffset = 0x10;
	constexpr std::uintptr_t kFirstPlayerUuidOffset = 0x15;

	constexpr std::uintptr_t kPlayerIdStride = 0x5220;
	constexpr std::uintptr_t kAccountBarn = 0x8;
	constexpr std::uintptr_t kAvatarOutfit = 0x58;
	constexpr std::uintptr_t kAvatarLocalSlot = 0x30;
	constexpr std::uintptr_t kAvatarSlotStride = 0x10A30;
	constexpr std::uintptr_t kAvatarPosition = 0x18;
	constexpr std::uintptr_t kAvatarShout = 0x60;

	constexpr std::uintptr_t kLevelName = 0x910;

	constexpr std::uintptr_t kLoginType = 0x142C;
	constexpr std::uintptr_t kLoadingType = 0x1420;
	constexpr std::uintptr_t kShouldRestart = 0x1430;

	constexpr std::uintptr_t kGameSpeedBarn = 0x30;
	constexpr std::uintptr_t kGameSpeedDelta = 0x28;

	constexpr std::uintptr_t kCameraSystem = 0x70;
	constexpr std::uintptr_t kCameraIntermediate = 0x158;
	constexpr std::uintptr_t kViewProjectionMatrix = 0x230;
	constexpr std::uintptr_t kJitterFullHalf = 0x60;
	constexpr std::uintptr_t kWhiskerCamera = 0x10D0;
	constexpr std::uintptr_t kCameraAngleX = 0x70C;
	constexpr std::uintptr_t kCameraAngleY = 0x710;
	constexpr std::uintptr_t kCameraRotation = 0x714;
	constexpr std::uintptr_t kCameraFOV = 0x6CC;
	constexpr std::uintptr_t kCameraZoom = 0x700;

	constexpr std::uintptr_t kHeight = 0x18DC;
	constexpr std::uintptr_t kScale = 0x18E0;
	constexpr std::uintptr_t kVoice = 0x1A50;
	constexpr std::uintptr_t kStance = 0x1A51;
	constexpr std::uintptr_t kBody = 0x144;
	constexpr std::uintptr_t kWing = 0x384;
	constexpr std::uintptr_t kHair = 0x5C4;
	constexpr std::uintptr_t kMask = 0x804;
	constexpr std::uintptr_t kNeck = 0xA44;
	constexpr std::uintptr_t kFeet = 0xC84;
	constexpr std::uintptr_t kHorn = 0xEC4;
	constexpr std::uintptr_t kFace = 0x1104;
	constexpr std::uintptr_t kProp = 0x1344;
	constexpr std::uintptr_t kHat = 0x1584;

	constexpr std::uintptr_t kLuaDebugDoString = 0x2594F48;
	constexpr std::uintptr_t kLuaPushLightUserData = 0xBF1E04;
	constexpr std::uintptr_t kWingBuffUpdate = 0x251360C;
	constexpr std::uintptr_t kRadianceBarn = 0x133086C;
	constexpr std::uintptr_t kAccountServerSetSession = 0xD80784;
	constexpr std::uintptr_t kHttpClientSetUserAgent = 0x257B7A8;
	constexpr std::uintptr_t kShouldDisplay = 0xF9B1D8;
	constexpr std::uintptr_t kDoShout = 0x109C8D4;
	constexpr std::uintptr_t kSetJoinableFunction = 0xDB2430;
	constexpr std::uintptr_t kPlayMusicKey = 0x212054C;
	constexpr std::uintptr_t kStopNote = 0x2133344;
	constexpr std::uintptr_t kPianoFrame = 0x211DEA8;
	constexpr std::uintptr_t kSharedMemoryPlayPianoSound = 0x1A14C90;

	constexpr std::uintptr_t kInvincibility = 0x2CA5FD8;
	constexpr std::uintptr_t kAutoCharge = 0x2CA5FD4;
	constexpr std::uintptr_t kDisableRainDrain = 0x2CA5FE8;
	constexpr std::uintptr_t kDarkCreatureTame = 0x2CDE074;
	constexpr std::uintptr_t kAllowAfk = 0x2AD84F8;
	constexpr std::uintptr_t kRunSpeed = 0x2B785DC;
	constexpr std::uintptr_t kSuperSlidey = 0x2FE07A4;
	constexpr std::uintptr_t kAutoCollectAllFragments = 0x2FCA610;
	constexpr std::uintptr_t kHideHudExceptForStarFragments = 0x2FCA614;
	constexpr std::uintptr_t kAutoFragmentWarp = 0x2CE13C4;
	constexpr std::uintptr_t kFastBurn = 0x2CDECA8;
	constexpr std::uintptr_t kDyeDebug = 0x2CECF50;
	constexpr std::uintptr_t kEnableAllRelationshipAbilities = 0x2FA68BC;
	constexpr std::uintptr_t kFakeCapeLevelEnabled = 0x2FCE810;
	constexpr std::uintptr_t kFakeCapeLevel = 0x2B78BFC;
	constexpr std::uintptr_t kAllowOverride = 0x2C2FED8;
	constexpr std::uintptr_t kSunMoonXPosition = 0x2C2FEE8;
	constexpr std::uintptr_t kSunMoonYPosition = 0x29B77DC;
	constexpr std::uintptr_t kSunMoon = 0x2C2FEEC;
	constexpr std::uintptr_t kMoonPhase = 0x2C2FEF0;
	constexpr std::uintptr_t kSunMoonSize = 0x29B77E0;
	constexpr std::uintptr_t kExposure = 0x29B77E8;
	constexpr std::uintptr_t kFlameToCandleScale = 0x2AEC7E4;
	constexpr std::uintptr_t kFlowerHeight = 0x2B37020;
	constexpr std::uintptr_t kFlowerSize = 0x2B37024;
	constexpr std::uintptr_t kEnableGameCamSnap = 0x2B8CD74;
	constexpr std::uintptr_t kAvatarCharcoaling = 0x2CA4018;
	constexpr std::uintptr_t kAllNpcsHaveRadar = 0x2CA401E;
	constexpr std::uintptr_t kForceEthereal = 0x2CA4004;
	constexpr std::uintptr_t kRevealPlayers = 0x1000C50;
	constexpr std::uintptr_t kEnableMultiplayer = 0x2B4C128;
	constexpr std::uintptr_t kDisableGates = 0x2CD6814;
	constexpr std::uintptr_t kFastHome = 0x1B6DCE8;
	constexpr std::uintptr_t kFreezeKrills = 0x131A224;
	constexpr std::uintptr_t kBirthdayKrills = 0x131B634;
	constexpr std::uintptr_t kTguiPauseAnimation = 0x2FDB2B4;
	constexpr std::uintptr_t kUiShowHierarchy = 0x2FDB2BC;
	constexpr std::uintptr_t kDebugShowSpiritLocations = 0x2CC1140;
	constexpr std::uintptr_t kShowRadarForPreviousWingBuffs = 0x3052A40;
	constexpr std::uintptr_t kEnableShrineRadar = 0x30045F0;
	constexpr std::uintptr_t kShowAllFeedback = 0x30045F4;
	constexpr std::uintptr_t kMapShrineRadar = 0x3004620;
	constexpr std::uintptr_t kFishSchoolDebug = 0x2CFCDD8;
	constexpr std::uintptr_t kTvDebugUi = 0x2FD7170;
	constexpr std::uintptr_t kDisableWindWall = 0x145ECF4;
	constexpr std::uintptr_t kDisableLevelChangeEvents = 0x2CC823C;
	constexpr std::uintptr_t kDisableObjectCollision = 0x2558D00;
	constexpr std::uintptr_t kDisableAllCollision = 0x2556F80;
	constexpr std::uintptr_t kDisableTerrain = 0x2CD3BF1;
	constexpr std::uintptr_t kDisableAvatars = 0x2CD3BF4;
	constexpr std::uintptr_t kDisableObjects = 0x2CD3BF5;
	constexpr std::uintptr_t kDisableObjectSkirts = 0x2CD3BF6;
	constexpr std::uintptr_t kDisableModels = 0x2CD3BF7;
	constexpr std::uintptr_t kEnableGravity = 0x2B7859C;
	constexpr std::uintptr_t kEnableClouds = 0x2AEE980;
	constexpr std::uintptr_t kEnableWater = 0x2B52ADC;
	constexpr std::uintptr_t kEnableOcean = 0x2B52AE0;
	constexpr std::uintptr_t kDisableLights = 0x2F46EC4;
	constexpr std::uintptr_t kAutoCompleteQuests = 0x1CDAA80;
	constexpr std::uintptr_t kSuperLaunch = 0x2B78C00;
	constexpr std::uintptr_t kSpellEmitter = 0xFBCA48;
	constexpr std::uintptr_t kScooterMode = 0x101F300;
	constexpr std::uintptr_t kRainbowGlow = 0x2FA26A8;
	constexpr std::uintptr_t kBubbleTrails = 0x2CA93C4;
	constexpr std::uintptr_t kRainbowTrails = 0x2CA93C8;
	constexpr std::uintptr_t kEnableReverb = 0x2FA834C;
	constexpr std::uintptr_t kInstrumentAutoPlaySheets = 0x2FE5910;
	constexpr std::uintptr_t kInstrumentEasyMode = 0x2FE5920;
	constexpr std::uintptr_t kInstrumentRadialLayout = 0x2FE593C;
	constexpr std::uintptr_t kDisableRemoteOutfitCache = 0x2CAD1B8;
	constexpr std::uintptr_t kIOSHeadphones = 0x1003F5C;
	constexpr std::uintptr_t kFireworksCooldown = 0x1B77BD8;
	constexpr std::uintptr_t kFastFlap = 0x1F6735C;
	constexpr std::uintptr_t kReadTableMessages = 0x12138A8;
	constexpr std::uintptr_t kStarwatchAuth = 0x1F263C0;

	const std::uintptr_t kAutoBurnPlants1 = 0x1329044;
	const std::uintptr_t kAutoBurnPlants2 = 0x13290B8;
	const std::uintptr_t kAutoBurnPlants3 = 0x1329130;

} } }

namespace tsm { namespace game { namespace Signatures {

	#define kUnlockAllPattern1 _O("?? 00 00 B4 20 00 80 52 F4 4F 42 A9 FD 7B 41 A9 FF C3 00 91 C0 03 5F D6 ?? ?? 43 F9")
	#define kUnlockAllPattern2 _O("?? ?? ?? ?? F8 03 00 AA ?? ?? 00 B5 E8 AA 4B 39")
	#define kEmoteLevelsPattern _O("48 2B 40 B9 ?? ?? 00 14")
	#define kLockCamPositionPattern _O("17 FD FF B5 E1 03 45 AD")
	#define kFreeZoomPattern _O("60 B2 05 BD 20 96 EC 97")
	#define kAutoCollectWaxPattern _O("E8 32 00 36 A9 26 00 90")
	#define kAutoBurnCandlesPattern _O("68 0C 00 34 60 42 41 BD")
	#define kRemoveCape _O("28 01 00 34 89 37 58 39")
	#define kSuperFlight _O("3F 01 00 B9 60 5A 4D BD")

	constexpr std::uint32_t kNopInstruction = 0xD503201Fu;
	constexpr std::uint32_t kRetInstruction = 0xD65F03C0u;
	constexpr std::uint32_t kMovW9Zero = 0x52800009u;
	constexpr std::uint32_t kMovW13Zero = 0x5280000Du;

} } }
