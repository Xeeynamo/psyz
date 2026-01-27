#include <psyz.h>
#include <stdarg.h>
#include <stdio.h>

#ifndef NO_LOGS
LOG_LEVEL psyz_logLevel = LOG_LEVEL_D;
void psyz_log(unsigned int level, const char* file, unsigned int line,
              const char* func, const char* fmt, ...) {
    static const char levels[] = "DIWE";
    va_list args;

    va_start(args, fmt);
    if (level >= psyz_logLevel && level < sizeof(levels) - 1) {
        char buf[1024];

        int n = vsnprintf(buf, sizeof(buf), fmt, args);
        if (n < 0) {
            buf[0] = '\0';
        } else if ((size_t)n >= sizeof(buf)) {
            buf[sizeof(buf) - 1] = '\0';
        }

        fprintf(stderr, "[%c][%s:%d][%s] %s\n", levels[level], file, line, func,
                buf);
    }
    va_end(args);
}
#endif
