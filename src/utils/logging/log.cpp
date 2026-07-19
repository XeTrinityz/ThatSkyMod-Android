#include <utils/logging/log.h>

#include <cstdarg>
#include <string>
#include <cstdio>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace tsm::log {

static std::string g_tag = "TSM";
static bool g_enabled = true;

void init(const char* tag) {
    if (tag && *tag) g_tag = tag;
}

static void vlog(int prio, const char* fmt, va_list ap) {
    if (!g_enabled || !fmt) return;
#ifdef __ANDROID__
    __android_log_vprint(prio, g_tag.c_str(), fmt, ap);
#else
    vprintf(fmt, ap);
    printf("\n");
#endif
}

void v(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(ANDROID_LOG_VERBOSE, fmt, ap); va_end(ap); }
void d(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(ANDROID_LOG_DEBUG, fmt, ap); va_end(ap); }
void i(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(ANDROID_LOG_INFO, fmt, ap); va_end(ap); }
void w(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(ANDROID_LOG_WARN, fmt, ap); va_end(ap); }
void e(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vlog(ANDROID_LOG_ERROR, fmt, ap); va_end(ap); }

void set_enabled(bool enabled) { g_enabled = enabled; }
bool is_enabled() { return g_enabled; }

}
