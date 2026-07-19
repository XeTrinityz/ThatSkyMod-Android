cmake_minimum_required(VERSION 3.19)

# ThatSkyMod-AndroidV2 - base CMake
project(ThatSkyModV2 LANGUAGES C CXX)

# Defaults suitable for Android toolchain; Gradle or CLI may override
if(NOT DEFINED ANDROID_ABI)
  set(ANDROID_ABI "arm64-v8a" CACHE STRING "Android ABI")
endif()
if(NOT DEFINED ANDROID_PLATFORM)
  set(ANDROID_PLATFORM "android-26" CACHE STRING "Min Android API level (minSdk)")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Default to Release for CLI builds
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

  # -----------------------------
  # Sources
  # -----------------------------
  set(SRCS
    # Core
    src/core/main.cpp
    src/core/InitManager.cpp
    src/core/UpdateChecker.cpp
    
    # Game Integration - Memory System
    src/game/memory/Address.cpp
    src/game/memory/Memory.cpp
    src/game/memory/Patch.cpp
    src/game/memory/api.cpp
    src/game/hooks/HookManager.cpp
    src/game/hooks/AccountHooks.cpp
    src/game/hooks/ProgressionHooks.cpp
    src/game/hooks/ShoutHook.cpp
    src/game/hooks/LuaHook.cpp
    src/game/hooks/MusicKeyHook.cpp
    src/game/hooks/PianoFrameHook.cpp
    src/game/hooks/MusicRecordHook.cpp
    src/game/interop/lua_bridge.cpp
    src/game/interop/LuaScriptQueue.cpp
    src/game/interop/LuaFunctions.cpp
    src/game/interop/LuaHelpers.cpp
    
    # Features
    src/features/manager/FeatureManager.cpp
    src/features/camera/CameraFeatures.cpp
    src/features/music/InstrumentManager.cpp
    src/features/world/WorldFeatures.cpp
    src/features/unlocks/UnlockFeatures.cpp
    
    # State Management
    src/state/ModState.cpp
    
    # UI
    src/ui/core/App.cpp
    src/ui/core/metrics.cpp
    src/ui/core/ThemeManager.cpp
    src/ui/core/WindowManager.cpp
    src/ui/core/TabBar.cpp
    src/ui/core/Localization.cpp
    src/ui/widgets/Layout.cpp
    src/ui/widgets/Cards.cpp
    src/ui/widgets/Controls.cpp
    src/ui/widgets/Buttons.cpp
    src/ui/widgets/Icons.cpp
    src/ui/widgets/Popups.cpp
    src/ui/widgets/SnakeGame.cpp
    src/ui/widgets/DoomGame.cpp
    src/ui/widgets/FlappyBirdGame.cpp
    src/ui/widgets/PongGame.cpp
    src/ui/widgets/BreakoutGame.cpp
    src/ui/widgets/MemoryMatchGame.cpp
    src/ui/widgets/MusicPlayback.cpp
    src/ui/helpers/Animations.cpp
    src/ui/helpers/VisualEffects.cpp
    src/ui/helpers/Toast.cpp
    src/ui/overlays/SkyClockOverlay.cpp
    src/ui/overlays/ESPOverlay.cpp
    src/ui/overlays/DebugOverlay.cpp
    src/ui/overlays/QuickTeleportOverlay.cpp
    src/ui/overlays/FavouritesOverlay.cpp
    src/ui/overlays/GameOverlay.cpp
    src/ui/overlays/POIOverlay.cpp
    src/ui/overlays/MusicPlaybackOverlay.cpp
    src/ui/panels/PlayerPanel.cpp
    src/ui/panels/SocialPanel.cpp
    src/ui/panels/TeleportPanel.cpp
    src/ui/panels/OutfitsPanel.cpp
    src/ui/panels/UnlocksPanel.cpp
    src/ui/panels/ProgressionPanel.cpp
    src/ui/panels/WorldPanel.cpp
    src/ui/panels/MusicPanel.cpp
    src/ui/panels/SettingsPanel.cpp
    
    # Utils
    src/utils/logging/log.cpp
    src/utils/hooking/dobby_wrapper.cpp
    src/utils/common/esp.cpp
    src/utils/common/AccountHelper.cpp
    src/utils/common/DataExportImport.cpp
    
    # Data
    src/data/DataManager.cpp
    src/data/loaders/AssetLoader.cpp
    src/data/loaders/LevelLoader.cpp
    
    # Network
    src/network/HttpClient.cpp
    src/network/ApiClient.cpp
    src/network/SocialManager.cpp
    src/network/WingBuffManager.cpp
    src/network/EdenAutomation.cpp
    src/network/QuestApi.cpp
    src/network/CollectiblesApi.cpp
    src/network/ShopApi.cpp
    src/network/helpers/WorldQuestHelper.cpp
    src/network/helpers/DailyQuestHelper.cpp
    
    # Progression
    src/progression/PatchManager.cpp
    src/progression/AutoWaxTools.cpp
    src/progression/WaxRunner.cpp
    src/iconloader/CustomIconLoader.cpp
    src/progression/CandleRunner.cpp
    
    # Third Party Implementation
    third_party/include/stb/stb_image.cpp
  )

add_library(${PROJECT_NAME} SHARED ${SRCS})
message(STATUS "Building target: ${PROJECT_NAME}")

# Enable string obfuscation
target_compile_definitions(${PROJECT_NAME} PRIVATE __USE_OBFUSCATOR__)

# Hide symbols by default (only export what's needed)
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  C_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

# Additional compiler flags for symbol obfuscation
target_compile_options(${PROJECT_NAME} PRIVATE
  -fvisibility=hidden
  -fvisibility-inlines-hidden
)

# -----------------------------
# Set output .so name from config/config.json
# -----------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(CONFIG_JSON_PATH "${CMAKE_SOURCE_DIR}/config/config.json")
  if(EXISTS "${CONFIG_JSON_PATH}")
    file(READ "${CONFIG_JSON_PATH}" _CFG_JSON)
    # Expecting a field: { "name": "libTSM.so", ... }
    string(JSON _CFG_LIB_NAME GET "${_CFG_JSON}" name)
    if(_CFG_LIB_NAME)
      # Derive OUTPUT_NAME by stripping leading 'lib' and trailing '.so'
      set(_OUT_NAME "${_CFG_LIB_NAME}")
      string(REGEX REPLACE "^lib(.*)\\.so$" "\\1" _OUT_NAME "${_OUT_NAME}")
      if(NOT _OUT_NAME STREQUAL _CFG_LIB_NAME)
        set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${_OUT_NAME}")
        message(STATUS "Output name set from config: ${_CFG_LIB_NAME} (OUTPUT_NAME=${_OUT_NAME})")
      else()
        message(WARNING "config.json:name='${_CFG_LIB_NAME}' is not of form lib<name>.so; keeping default output name")
      endif()
    endif()
  endif()
endif()

# -----------------------------
# Includes
# -----------------------------
  target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/third_party/include"
    "${CMAKE_SOURCE_DIR}"
  )

# -----------------------------
# Android NDK/system libs
# -----------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  find_library(LOG_LIB log)
  find_library(GLESv3_LIB GLESv3)
  find_library(EGL_LIB EGL)
  find_library(ZLIB_LIB z)
  find_library(DL_LIB dl)

  target_link_libraries(${PROJECT_NAME} PRIVATE
    ${LOG_LIB}
    ${GLESv3_LIB}
    ${EGL_LIB}
    ${ZLIB_LIB}
    ${DL_LIB}
  )
endif()

# -----------------------------
# Optional local prebuilt third-party libs
# -----------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(LOCAL_LIB_DIR "${CMAKE_SOURCE_DIR}/third_party/lib/android/${ANDROID_ABI}")

  # libciphered.so (from mod loader)
  if(EXISTS "${LOCAL_LIB_DIR}/libciphered.so")
    add_library(ciphered SHARED IMPORTED)
    set_target_properties(ciphered PROPERTIES IMPORTED_LOCATION "${LOCAL_LIB_DIR}/libciphered.so")
    target_link_libraries(${PROJECT_NAME} PRIVATE ciphered)
  endif()

  # Dobby static (optional)
  if(EXISTS "${LOCAL_LIB_DIR}/libdobby.a")
    # Link Dobby with whole-archive to ensure symbols are included even if only referenced weakly
    target_link_libraries(${PROJECT_NAME} PRIVATE -Wl,--whole-archive "${LOCAL_LIB_DIR}/libdobby.a" -Wl,--no-whole-archive)
    target_compile_definitions(${PROJECT_NAME} PRIVATE TSM_HAVE_DOBBY=1)
  endif()

  # OpenSSL static pair (optional)
  if(EXISTS "${LOCAL_LIB_DIR}/libssl.a" AND EXISTS "${LOCAL_LIB_DIR}/libcrypto.a")
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--start-group)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${LOCAL_LIB_DIR}/libssl.a" "${LOCAL_LIB_DIR}/libcrypto.a")
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--end-group)
  endif()
else()
  message(STATUS "Non-Android platform (${CMAKE_SYSTEM_NAME}): skipping Android prebuilt libs import.")
endif()

# -----------------------------
# Post-build: embed .config and .icon sections
# -----------------------------
if(NOT CMAKE_OBJCOPY)
  find_program(CMAKE_OBJCOPY NAMES llvm-objcopy objcopy)
endif()

if(CMAKE_OBJCOPY AND EXISTS "${CMAKE_OBJCOPY}")
  set(CONFIG_FILE "${CMAKE_SOURCE_DIR}/config/config.json")
  if(EXISTS "${CONFIG_FILE}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_OBJCOPY} --add-section .config=${CONFIG_FILE} --set-section-flags .config=readonly,contents,data $<TARGET_FILE:${PROJECT_NAME}>
      COMMENT "Embedding config.json into ${PROJECT_NAME}"
    )
  endif()

  set(ICON_PNG "${CMAKE_SOURCE_DIR}/resources/icons/icon.png")
  set(ICON_BMP "${CMAKE_SOURCE_DIR}/resources/icons/icon.bmp")
  if(EXISTS "${ICON_PNG}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_OBJCOPY} --add-section .icon=${ICON_PNG} --set-section-flags .icon=readonly,contents,data $<TARGET_FILE:${PROJECT_NAME}>
      COMMENT "Embedding icon.png into ${PROJECT_NAME}"
    )
  elseif(EXISTS "${ICON_BMP}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_OBJCOPY} --add-section .icon=${ICON_BMP} --set-section-flags .icon=readonly,contents,data $<TARGET_FILE:${PROJECT_NAME}>
      COMMENT "Embedding icon.bmp into ${PROJECT_NAME}"
    )
  endif()
else()
  message(STATUS "llvm-objcopy/objcopy not found; skipping binary section embedding.")
endif()

# -----------------------------
# Optimization and strip (Release)
# -----------------------------
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Oz -g0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Oz -g0")

if(NOT CMAKE_STRIP)
  find_program(CMAKE_STRIP NAMES llvm-strip strip)
endif()
if(CMAKE_STRIP AND CMAKE_BUILD_TYPE STREQUAL "Release")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_STRIP} --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping symbols from $<TARGET_FILE:${PROJECT_NAME}> (Release)"
  )
endif()

# -----------------------------
# Summary
# -----------------------------
message(STATUS "Summary:")
message(STATUS "  Project name: ${PROJECT_NAME}")
message(STATUS "  ABI: ${ANDROID_ABI}")
message(STATUS "  ANDROID_PLATFORM (minSdk): ${ANDROID_PLATFORM}")