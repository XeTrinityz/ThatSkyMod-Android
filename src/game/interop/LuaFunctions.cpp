#include <game/interop/LuaFunctions.h>
#include <game/interop/LuaScriptQueue.h>
#include <utils/common/obfuscate.h>

namespace tsm { namespace lua { namespace functions {

void InitializeCore() {
    static const char* TSM_Lua_Func = _O(R"(
        function TChangeLevelWithFade(levelName, useFade)
            local e = game:eventBarn():AddEventByMetaName("ChangeLevelWithFade")
            e:levelName(levelName)
            if useFade then
                e:fadeOutDuration(2.0)
                e:fadeInDuration(0.5)
                e:fadeHoldDuration(0.0)
                e:fadeTexture("Black")
            else
                e:fadeOutDuration(0)
                e:fadeInDuration(0)
                e:fadeHoldDuration(0)
            end
            e:isLevelComplete(false)
            e:Start()
        end

        function TAvatarCrawl(toggle)
            local e = game:eventBarn():AddEventByMetaName("AvatarCrawl")
            e:crawlAmount(toggle and 0.9 or 0.0)
            e:Start()
        end

        function TGrantSpell(buffName)
            local e = game:eventBarn():AddEventByMetaName("GrantSpell")
            e:buffName(buffName)
            e:networked(true)
            e:duration(100.0)
            e:Start()
        end

        function TRemoveSpell(buffName)
            local e = game:eventBarn():AddEventByMetaName("RemoveSpell")
            e:buffName(buffName)
            e:Start()
        end

        function TTriggerToyEvent(consumableName)
            local e = game:eventBarn():AddEventByMetaName("TriggerToyEvent")
            e:consumableName(consumableName)
            e:persistent(false)
            e:Start()
        end

        function TGrantSocialRecording()
            local e = game:eventBarn():AddEventByMetaName("GrantSocialRecording")
            e:cantSubmitString("You can't submit a recording right now.")
            e:Start()
        end

        function TSocialMessageStones()
            local e = game:eventBarn():AddEventByMetaName("SocialMessageStones")
            e:Start()
        end

        function TAudienceSettings(showReactionMenu, replaceLocalAvatar, replaceOtherAvatars, visType)
            local e = game:eventBarn():AddEventByMetaName("AudienceSettings")
            e:showReactionMenu(showReactionMenu)
            e:replaceLocalAvatar(replaceLocalAvatar)
            e:replaceOtherAvatars(replaceOtherAvatars)
            if visType ~= nil and visType ~= "" then
                e:visType(visType)
            end
            e:allowHandhold(true)
            e:Start()
        end

        function TOpenURLBST(url)
            local e = game:eventBarn():AddEventByMetaName("OpenURLBST")
            e:url(url)
            e:Start()
        end

        function TAvatarSetVoice(voiceType)
            local e = game:eventBarn():AddEventByMetaName("AvatarSetVoice")
            e:voiceType(voiceType)
            e:fireOnce(true)
            e:npc(nil)
            e:Start()
        end

        function TOpenDanceMenu(mode)
            local e = game:eventBarn():AddEventByMetaName("OpenDanceMenu")
            e:controlsCameraPos(false)
            e:mode(mode)
            e:muteAnims(false)
            e:onMenuClose(nil)
            e:Start()
        end

        function TManualShardFall(pos)
            local e = game:eventBarn():AddEventByMetaName("ManualShardFall")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({pos[1], pos[2], pos[3]})
            marker:scale({1, 1, 1})

            local markerClump = game:clumpBarn():CreateClumpWithSize(1)
            markerClump:SetAt(0, marker)

            e:spawnPositions(markerClump)
            e:Start()
        end

        function TPlacePuzzleKey(pos)
            local e = game:eventBarn():AddEventByMetaName("PlacePuzzleKey")
            e:destroyPlaceableOnDisable(true)
            e:onGrabEvents(nil)
            e:onDropEvents(nil)

            local transform = {
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {pos[1], pos[2], pos[3], 1}
            }

            e:transform(transform)
            e:Start()
        end

        function TPuzzleBallPedestal(pos)
            local e = game:eventBarn():AddEventByMetaName("PuzzleBallPedestal")
            local transform = {
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {pos[1], pos[2], pos[3], 1}
            }

            e:transform(transform)
            e:Start()
        end

        function TSocialToggleAntlerHelmOrb()
            local e = game:eventBarn():AddEventByMetaName("SocialToggleAntlerHelmOrb")
            e:Start()
        end

        function TOpenConstellationShop(shopName, x, y, z)
            local e = game:eventBarn():AddEventByMetaName("OpenConstellationShop")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({x, y, z})
            marker:scale({1, 1, 1})

            e:marker(marker)
            e:shopName(shopName)
            e:Start()

            game:markerBarn():ReleaseMarker(marker)
        end

        function TGenericShopNpcHint(shopName, iconName, x, y, z)
            local e = game:eventBarn():AddEventByMetaName("GenericShopNpcHint")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({x, y, z})
            marker:scale({5, 5, 5})
            local markerClump = game:clumpBarn():CreateClumpWithSize(1)
            markerClump:SetAt(0, marker)
            e:buttonPos(marker)
            if not iconName or iconName == "" then iconName = "UiMiscDiamond" end
            e:iconName(iconName)
            e:shopName(shopName)
            e:region(markerClump)
            e:Start()
        end

        function TOutfitSelect(x, y, z, showAllPurchasedType)
            local e = game:eventBarn():AddEventByMetaName("OutfitSelect")
            local m = game:markerBarn():CreateMarker(0)
            m:pos({x, y, z})
            m:scale({10.0, 10.0, 10.0})

            e:persistPreviewRegion(m)
            e:avatarRegion(m)
            e:onlyOneTab(false)
            e:showAllPurchasedType(showAllPurchasedType)
            e:closet(true)
            e:Start()
        end

        function TAddFloatToStatUsingName(statName, floatToAdd)
            local e = game:eventBarn():AddEventByMetaName("AddFloatToStatUsingName")
            e:statName(statName)
            e:floatToAdd(floatToAdd)
            e:triggerOnResume(true)
            e:Start()
        end
    )");

    queue::Enqueue(TSM_Lua_Func);
}

void InitializeExtended() {
    static const char* TSM_Lua_Func = _O(R"(
        function TAvatarSpeak(message)
            local e = game:eventBarn():AddEventByMetaName("AvatarSpeak")
            e:message(message)
            e:Start()
        end

        function TDisplayText(text, pos, duration)
            local e = game:eventBarn():AddEventByMetaName("DisplayText")
            e:color({1.0, 1.0, 1.0, 1.0})
            e:duration(duration or 3.0)
            e:worldScale(false)
            e:background(false)
            e:speed({0.0, 0.0, 0.0})
            e:creditDistAlphaOffset(0)
            e:text(text)
            e:x(pos[1])
            e:y(pos[2])
            e:vAlign(2)
            e:wrapScreenWidth(0.9)
            e:shadow(true)
            e:lineOfSightGraceDist(3.0)
            e:lineOfSightNeeded(false)
            e:hideDuringMenus(false)
            e:usePixelSizeFont(false)
            e:fadeInTime(0.3)
            e:fadeOutTime(0.3)
            e:isCreditsText(false)
            e:scale(0.1)
            e:delay(0)
            e:Start()
        end

        function TUIQuestBanner()
            local e = game:eventBarn():AddEventByMetaName("UIQuestBanner")
            e:Start()
        end

        function TAvatarSetPos(pos, onGround)
            local e = game:eventBarn():AddEventByMetaName("AvatarSetPos")
            local m = game:markerBarn():CreateMarker(0)
            m:pos({pos[1], pos[2], pos[3]})
            e:marker(m)
            e:onGround(onGround)
            e:breakHandHold(false)
            e:killMomentum(false)
            e:setLevelStartPosition(false)
            e:Start()
            game:markerBarn():ReleaseMarker(m)
        end

        function TSetLocalAudienceJelly()
            local e = game:eventBarn():AddEventByMetaName("SetLocalAudienceJelly")
            e:Start()
        end

        function TSetPersonalityType(personalityTypeFloat)
            local e = game:eventBarn():AddEventByMetaName("SetPersonalityType")
            local f = Float.new()
            f:value(personalityTypeFloat)
            e:personalityTypeFloat(f)
            e:Start()
        end

        function TAvatarPlayEmote(name, speedMultiplier, emoteLoopDuration, level)
            local e = game:eventBarn():AddEventByMetaName("AvatarPlayEmote")
            e:speedMultiplier(speedMultiplier)
            e:emoteLoopDuration(emoteLoopDuration)
            e:name(name)
            e:level(level)
            e:Start()
        end

        function TCameraDesaturationEffect(intensityFloat, filterType)
            local e = game:eventBarn():AddEventByMetaName("CameraDesaturationEffect")
            e:intensityFloat(intensityFloat)
            e:changeTime(0)
            e:filterType(filterType)
            e:Start()
        end

        function TSocialGift()
            local e = game:eventBarn():AddEventByMetaName("SocialGift")
            e:Start()
        end

        function TLocalDoAudienceReact(power, shoutType)
            local e = game:eventBarn():AddEventByMetaName("LocalDoAudienceReact")
            e:power(power)
            e:shoutType(shoutType)
            e:Start()
        end

        function TWingBuffDepositShrineResetHint()
            local e = game:eventBarn():AddEventByMetaName("WingBuffDepositShrineResetHint")
            e:Start()
        end

        function TOpenMenu(menuName)
            local e = game:eventBarn():AddEventByMetaName(menuName)
            e:Start()
        end

        function TAvatarSetOutfitDye(dyeColor, region, slotName, removeDye)
            local e = game:eventBarn():AddEventByMetaName("AvatarSetOutfitDye")
            e:flash(true)
            e:dyeColor(dyeColor)
            e:region(region)
            e:slotName(slotName)
            e:removeDye(removeDye)
            e:Start()
        end

        function TPlaySound2D(sound, volume)
            local e = game:eventBarn():AddEventByMetaName("PlaySound2D")
            e:volume(volume)
            e:sound(sound)
            e:Start()
        end

        function TCollectCollectible(collectible, collectibleType)
            local e = game:eventBarn():AddEventByMetaName("CollectCollectible")
            local c = game:collectibleBarn():CreateCollectible()
            c:type(collectibleType)
            c:name(collectible)
            e:carry(false)
            e:collectible(c)
            e:Start()
        end

        function TTryCompleteQuestOrIncrementStat(questName)
            local e = game:eventBarn():AddEventByMetaName("TryCompleteQuestOrIncrementStat")
            e:questName(questName)
            e:Start()
        end

        function TSocialRandomGame()
            local e = game:eventBarn():AddEventByMetaName("SocialRandomGame")
            e:Start()
        end

        function TSocialFireworks()
            local e = game:eventBarn():AddEventByMetaName("SocialFireworks")
            e:Start()
        end

        function TSocialFireworksMenu()
            local e = game:eventBarn():AddEventByMetaName("SocialFireworksMenu")
            e:Start()
        end

        function TSetAvatarMicroMode()
            local e = game:eventBarn():AddEventByMetaName("SetAvatarMicroMode")
            e:Start()
        end

        function TDisplayTitleCard(mainTitle, subTitle, forceFirstTime)
            local e = game:eventBarn():AddEventByMetaName("DisplayTitleCard")
            e:mainTitle(mainTitle)
            e:subTitle(subTitle)
            e:shadow(true)
            e:forceFirstTime(forceFirstTime)
            e:yOffset(0)
            e:Start()
        end

        function TPlayMusic(music, foreground, volume)
            local e = game:eventBarn():AddEventByMetaName("PlayMusic")
            e:netSync(true)
            e:prevFadeOutTime(0)
            e:fadeTime(3)
            e:music(music)
            e:foreground(foreground)
            e:setVolume(true)
            e:volume(volume)
            e:continueIfSame(true)
            e:Start()
        end

        function TSetMusicKey(key)
            local e = game:eventBarn():AddEventByMetaName("SetMusicKey")
            e:overrideFloat(nil)
            e:value(key)
            e:Start()
        end

        function TClearRecentPlayers()
            local e = game:eventBarn():AddEventByMetaName("ClearRecentPlayers")
            e:Start()
        end

        function TSetRainParams(intensity)
            local e = game:eventBarn():AddEventByMetaName("SetRainParams")
            e:harmful(false)
            e:transitionDuration(1.0)
            e:oscillationDuration(8.0)
            e:intensity(intensity)
            e:oscillationAmount(0.5)
            e:fallRate(intensity)
            e:Start()
        end

        function TSetWindIntensity(intensity)
            local e = game:eventBarn():AddEventByMetaName("SetWindIntensity")
            e:intensity(intensity)
            e:reset(false)
            e:Start()
        end

        function TMusicalMoteTarget(pos)
            local e = game:eventBarn():AddEventByMetaName("MusicalMoteTarget")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({pos[1], pos[2], pos[3]})
            marker:scale({1, 1, 1})
            local markerClump = game:clumpBarn():CreateClumpWithSize(1)
            markerClump:SetAt(0, marker)
            e:markers(markerClump)
            e:soundName("")
            e:parentToAvatarXZ(true)
            e:carryAction(0)
            e:Start()
            game:markerBarn():ReleaseMarker(marker)
        end

        function TWaxPickupSpawnerZone(pos)
            local e = game:eventBarn():AddEventByMetaName("WaxPickupSpawnerZone")

            local currencyTypes = {46, 52, 54, 56, 48, 58, 50, 60, 46, 52}
            local randomCurrencyType = currencyTypes[math.random(1, #currencyTypes)]

            local guid = game:bstGuidBarn():CreateBSTGuid()

            local guidClump = game:clumpBarn():CreateClumpWithSize(1)
            guidClump:SetAt(0, guid)

            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({pos[1], pos[2] - 5, pos[3]})
            marker:scale({10, 10, 10})

            local markerClump = game:clumpBarn():CreateClumpWithSize(1)
            markerClump:SetAt(0, marker)

            e:useSourceMeshOrientation(false)
            e:vertBasedHugGround(false)
            e:networkedPickups(true)
            e:autoCollectWax(false)
            e:requiresActiveSeasonPass(false)
            e:cameraHintWhenPicking(false)
            e:requiresActiveSeason(false)
            e:disablePickupButton(false)
            e:useEmberRegions(false)
            e:pickupSFX("ShellOpen")
            e:jackpotSFX("ShellOpen")
            e:animationOnPickup("Pickup")
            e:spawnRegion(markerClump)
            e:guids(guidClump)
            e:spawnCountMin(1)
            e:spawnCountMax(1)
            e:waxPerSpawnMin(1)
            e:waxPerSpawnMax(1)
            e:waxSpawnMult(1)
            e:jackpotPlayersRequired(1)
            e:jackpotProbability(0.0)
            e:jackpotWax(1)
            e:spawnType(3)
            e:currencyType(randomCurrencyType)
            e:animationTime(2)
            e:bstGuid(2105409868)
            e:slopeStrictnessMult(1)
            e:onEachCollected(nil)
            e:pickupMeshTypes(nil)
            e:jackpotMeshType(nil)
            e:onAllWaxCollected(nil)
            e:pickupMeshTypes(nil)
            e:Start()
        end

        function THubbaBubbaZone(pos, size)
            local e = game:eventBarn():AddEventByMetaName("HubbaBubbaZone")
            local transform = {
                {size, 0, 0, 0},
                {0, size, 0, 0},
                {0, 0, size, 0},
                {pos[1], pos[2], pos[3], 1}
            }
            e:transform(transform)
            e:type(0)
            e:Start()
        end

        function TSpawnCrabs(number, time, pos, radius)
            local e = game:eventBarn():AddEventByMetaName("SpawnCrabs")
            if not e then return end
            e:number(number or 1)
            e:time(time or 0.5)
            local transform = {
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {pos[1], pos[2], pos[3], 1}
            }
            e:SetTransform(transform)
            local params = CrabParams.new()
            if params then
                params:darkCrab(true)
                params:runSpeed(5.0)
                params:walkSpeed(2.5)
                params:radius(radius or 1.0)
                e:params(params)
            end
            e:Start()
        end

        function TShardnado(pos)
            local e = game:eventBarn():AddEventByMetaName("Shardnado")
            local transform = {
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {pos[1], pos[2], pos[3], 1}
            }
            e:SetTransform(transform)
            local params = ShardnadoParams.new()
            if params then
                params:maxOrbitRate(0.6)
                params:maxRadius(10.0)
                params:minRadius(5.0)
                params:numShards(16)
                params:shardScale(1.0)
                params:minOrbitRate(0.3)
                e:params(params)
            end
            e:Start()
        end

        function TSetBulletTime(duration, speed)
            local e = game:eventBarn():AddEventByMetaName("SetBulletTime")
            e:fadeOut(0)
            e:duration(duration)
            e:speed(speed)
            e:fadeIn(0)
            e:Start()
        end

        function THomeDirectoryButton(pos)
            local e = game:eventBarn():AddEventByMetaName("HomeDirectoryButton")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({pos[1], pos[2], pos[3]})
            marker:scale({5.0, 5.0, 5.0})
            e:buttonPos(marker)
            e:Start()
        end

        function TReloadLevel()
            local e = game:eventBarn():AddEventByMetaName("ChangeLevel")
            e:levelName(game:levelName())
            e:Start()
        end

        function TTitleGate()
            local e = game:eventBarn():AddEventByMetaName("TitleGate")
            e:Start()
        end

        function TAvatarRainbowParty()
            local e = game:eventBarn():AddEventByMetaName("AvatarRainbowParty")
            e:useSpecificColor(false)
            e:hueShiftRate(2.0)
            e:innerGlow(100.0)
            e:tintStrength(0.0)
            e:lightIntensity(0.0)
            e:tintLocalPlayer(true)
            e:tintRemotePlayers(false)
            e:Start()
        end

        function TShowHUDInWorld(textureName, isCreditLogo, pos)
            local e = game:eventBarn():AddEventByMetaName("ShowHUDInWorld")
            e:textureName(textureName)
            e:isCreditLogo(isCreditLogo)
            e:x(pos[1])
            e:y(pos[2])
            e:z(pos[3])
            e:Start()
        end

        function TCreatePortal(pos, customLevelName, customHintText)
            portal = game:portalBarn():CreatePortal(getmetatable(portalBarn))
            local transform = {
                {3, 0, 0, 0},
                {0, 3, 0, 0},
                {0, 0, 3, 0},
                {pos[1], pos[2], pos[3], 1}
            }
            portal:SetTransform(transform)
            portal:tryToResume(false)
            portal:visualOnly(false)
            portal:enabled(true)
            portal:resourceBundle("")
            portal:customColor({1.0, 1.0, 1.0})
            portal:portalType(10)
            portal:customLevelName(customLevelName)
            portal:customSoundName("")
            portal:customHintText(customHintText)
            portal:texture("PortalWhite")
            portal:Initialize(game:eventBarn())
        end

        function TAutoCollectCompletedQuest(questName)
            local e = game:eventBarn():AddEventByMetaName("AutoCollectCompletedQuest")
            e:questName(questName)
            e:Start()
        end

        function TTryCompleteQuest(questName)
            local e = game:eventBarn():AddEventByMetaName("TryCompleteQuest")
            e:questName(questName)
            e:Start()
        end

        function TAutoAcceptSmartQuest(questName)
            local e = game:eventBarn():AddEventByMetaName("AutoAcceptSmartQuest")
            e:questName(questName)
            e:Start()
        end

        function TPlaySeasonFinale()
            local e = game:eventBarn():AddEventByMetaName("PlaySeasonFinale")
            e:Start()
        end

        function TUnlock(name, acknowledge)
            local e = game:eventBarn():AddEventByMetaName("Unlock")
            e:acknowledge(acknowledge)
            e:name(name)
            e:onSuccess(nil)
            e:Start()
        end

        function TProtoTrampoline(x, y, z)
            local e = game:eventBarn():AddEventByMetaName("ProtoTrampoline")
            local transform = {
                {1, 0, 0, 0},
                {0, 1, 0, 0},
                {0, 0, 1, 0},
                {x, y, z, 1}
            }
            e:transform(transform)
            e:Start()
        end

        function TBouncyBalloon(x, y, z)
            local e = game:eventBarn():AddEventByMetaName("BouncyBalloon")
            local px = x or 0
            local py = y or 0
            local pz = z or 0
            local transform = {
                {20, 0, 0, 0},
                {0, 20, 0, 0},
                {0, 0, 20, 0},
                {px, py, pz, 1}
            }
            e:transform(transform)
            e:Start()
        end

        function TFarmersMarket(pos)
            local e = game:eventBarn():AddEventByMetaName("FarmersMarket")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({pos[1], pos[2], pos[3]})
            marker:scale({5, 5, 5})
            e:spawnPoint(marker)
            e:Start()
        end

        function TSocialGrapplingHookMenu()
            local e = game:eventBarn():AddEventByMetaName("SocialGrapplingHookMenu")
            e:Start()
        end

        function TStarFallShower(x, y, z)
            local e = game:eventBarn():AddEventByMetaName("StarFallShower")
            local count = 10
            local markerClump = game:clumpBarn():CreateClumpWithSize(count)
            local radius = 10
            for i = 1, count do
                local marker = game:markerBarn():CreateMarker(0)
                local ox = math.random(-radius, radius)
                local oy = 100 + math.random(-2, 2)
                local oz = math.random(-radius, radius)
                marker:pos({x + ox, y + oy, z + oz})
                marker:scale({1, 1, 1})
                markerClump:SetAt(i - 1, marker)
            end
            local r = 0.3 + (0.7 * math.random())
            local g = 0.3 + (0.7 * math.random())
            local b = 0.3 + (0.7 * math.random())
            e:startPositions(markerClump)
            e:randomSeed(0)
            e:color({r, g, b})
            e:speed(30.0)
            e:speedRandRange(0.0)
            e:killOnGroundCollision(true)
            e:intensity(1.0)
            e:killY(-1000.0)
            e:lightIntensityMult(0.1)
            e:dirRandRange(0.0)
            e:flareSize(0.1)
            e:cooldown(1.0)
            e:Start()
        end

        function TPlayPurchaseFX()
            local e = game:eventBarn():AddEventByMetaName("PlayPurchaseFX")
            e:invisibleItem(false)
            e:Start()
        end

        function TGraphicsSettingsMenu()
            local e = game:eventBarn():AddEventByMetaName("GraphicsSettingsMenu")
            e:Start()
        end

        function THideRemotePlayers()
            local e = game:eventBarn():AddEventByMetaName("HideRemotePlayers")
            e:onlyHideGhosts(true)
            e:includeLocal(false)
            e:onlyInCutsceneCamera(false)
            e:Start()
        end

        function TFpsSelector()
            local e = game:eventBarn():AddEventByMetaName("FpsSelector")
            e:Start()
        end

        function TResolutionSelector()
            local e = game:eventBarn():AddEventByMetaName("ResolutionSelector")
            e:Start()
        end

        function TConstellationArea(x, y, z)
            local e = game:eventBarn():AddEventByMetaName("ConstellationArea")
            local marker = game:markerBarn():CreateMarker(0)
            marker:pos({x, y, z})
            marker:scale({50, 50, 50})
            local markerClump = game:clumpBarn():CreateClumpWithSize(1)
            markerClump:SetAt(0, marker)
            e:buttonPos(marker)
            e:region(markerClump)
            e:friendConstellationOffset(0.0)
            e:friendConstellationOnly(false)
            e:Start()
        end
    )");

    queue::Enqueue(TSM_Lua_Func);
}

}}}
