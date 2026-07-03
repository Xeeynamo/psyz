#include <SDL3/SDL.h>
#include <psyz.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef NO_LOGS
LOG_LEVEL psyz_logLevel = LOG_LEVEL_D;
void psyz_log(unsigned int level, const char* file, unsigned int line,
              const char* func, const char* fmt, ...) {
    static const char levels[] = "DIWE";
    static const SDL_LogPriority priorities[] = {
        SDL_LOG_PRIORITY_DEBUG, SDL_LOG_PRIORITY_INFO, SDL_LOG_PRIORITY_WARN,
        SDL_LOG_PRIORITY_ERROR};
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

        // filtering is done with psyz_logLevel, don't let SDL filter again
        static bool sdl_log_configured = false;
        if (!sdl_log_configured) {
            SDL_SetLogPriority(
                SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
            sdl_log_configured = true;
        }
        SDL_LogMessage(
            SDL_LOG_CATEGORY_APPLICATION, priorities[level],
            "[%c][%s:%d][%s] %s", levels[level], file, line, func, buf);
    }
    va_end(args);
}
#endif
