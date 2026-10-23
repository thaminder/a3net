// log.h - einfache, threadsichere Protokollierung in a3net.log
#pragma once
#include <string>

namespace a3 {

// Pfad leer -> Ausgabe auf stderr (Tests). enabled=false -> keine Ausgabe.
void logInit(const std::string& path, bool enabled);
void logf(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
std::string hexdump(const void* data, size_t len, size_t max = 32);

} // namespace a3
