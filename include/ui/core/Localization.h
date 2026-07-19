#pragma once

namespace tsm { namespace ui { namespace i18n {

enum class Language {
    English = 0,
    Russian = 1
};

void SetLanguage(Language language);
Language GetLanguage();
const char* Tr(const char* key);

struct TranslationPackInfo {
    const char* id;
    const char* name;
    const char* author;
    const char* version;
};

int GetTranslationPackCount();
bool GetTranslationPackInfo(int index, TranslationPackInfo& outInfo);

int GetActiveLanguageIndex();
void SetActiveLanguageIndex(int index);

bool ImportTranslationFromFd(int fd, int* outLanguageIndex, char* outErrorBuf, int errorBufSize);

bool DeleteTranslationPack(int packIndex);

void ReloadTranslations();

}}}
