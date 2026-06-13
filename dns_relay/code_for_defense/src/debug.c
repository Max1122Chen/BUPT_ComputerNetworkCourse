#include "debug.h"

#include <stdarg.h>
#include <stdio.h>

static int s_debug_level;

void debug_init(int level)
{
    s_debug_level = level;
}

int debug_get_level(void)
{
    return s_debug_level;
}

void debug_log(int level, const char *fmt, ...)
{
    va_list ap;

    if (level > s_debug_level)
    {
        return;
    }

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
