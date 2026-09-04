#include <psyz.h>
#include <psyz/log.h>
#include <stdarg.h>
#include <stdio.h>

void psyz_log(unsigned int level, const char* file, unsigned int line,
              const char* func, const char* fmt, ...) {
    static const char levels[] = "DIWE";
    va_list args;

    va_start(args, fmt);
    if (level < sizeof(levels) - 1) {
        fprintf(
            stderr, "[%c][%s:%d][%s] ", levels[level], file, (int)line, func);
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }
    va_end(args);
}
