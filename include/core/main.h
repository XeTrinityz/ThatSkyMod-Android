#ifndef TSM_V2_ENTRY_H
#define TSM_V2_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define EXPORT __attribute__((visibility("default")))
#else
#  define EXPORT
#endif

	typedef void (*func)(bool*);

	void Menu(bool* menu_open);
	void Init();
	EXPORT void InitLate();

	EXPORT func Start();

#ifdef __cplusplus
}
#endif

#endif
