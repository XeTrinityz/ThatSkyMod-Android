#pragma once

#include <string>

namespace tsm::game::hooks {

bool Install();

void UninstallAll();

const std::string& GetCapturedUserId();
const std::string& GetCapturedSessionId();
const std::string& GetCapturedUserAgent();

std::uintptr_t GetWingBuffBarn();
std::uintptr_t GetFirstWing();
int* GetWingCountPtr();

std::uintptr_t GetRadianceBarn();
std::uintptr_t GetFirstDye();
int* GetDyeCountPtr();

std::uintptr_t GetCameraSystem();

bool InstallCameraSystemHook();

std::uintptr_t GetPlayerShout();

}
