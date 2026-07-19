#pragma once

#include <string>

namespace tsm { namespace utils {

constexpr const char* kExportBaseDir = "/sdcard/Download/ThatSkyMod-Export";

bool ExportAllToFolder(std::string* outError = nullptr);

bool ImportAllFromFolder(const std::string& folderPath, std::string* outError = nullptr);

}}
