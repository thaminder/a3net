#include "log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace a3 {

static std::mutex g_logMutex;
static FILE* g_logFile = nullptr;
static bool g_logEnabled = true;
static bool g_toStderr = true;

void logInit(const std::string& path, bool enabled) {
    std::lock_guard<std::mutex> lk(g_logMutex);
    g_logEnabled = enabled;
    if (g_logFile) { std::fclose(g_logFile); g_logFile = nullptr; }
    g_toStderr = path.empty();
    if (enabled && !path.empty()) g_logFile = std::fopen(path.c_str(), "w");
}

void logf(const char* fmt, ...) {
    if (!g_logEnabled) return;
    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    int ms = (int)(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
    std::lock_guard<std::mutex> lk(g_logMutex);
    std::tm tmv{};
    if (std::tm* lt = std::localtime(&t)) tmv = *lt;   // unter g_logMutex, daher unkritisch
    FILE* f = g_logFile ? g_logFile : (g_toStderr ? stderr : nullptr);
    if (!f) return;
    std::fprintf(f, "%02d:%02d:%02d.%03d %s\n", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms, msg);
    std::fflush(f);
}

std::string hexdump(const void* data, size_t len, size_t max) {
    static const char* hx = "0123456789abcdef";
    std::string s;
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < len && i < max; ++i) {
        s += hx[p[i] >> 4];
        s += hx[p[i] & 15];
        s += ' ';
    }
    if (len > max) s += "...";
    return s;
}

} // namespace a3
