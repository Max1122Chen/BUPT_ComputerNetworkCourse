#include "platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

static LARGE_INTEGER s_freq;
static LARGE_INTEGER s_start;
static int s_timer_ok;

int platform_init(void)
{
    WSADATA wsa;
    int r;

    r = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (r != 0)
    {
        return -1;
    }

    s_timer_ok = QueryPerformanceFrequency(&s_freq) && QueryPerformanceCounter(&s_start);
    return 0;
}

void platform_cleanup(void)
{
    WSACleanup();
}

uint64_t platform_monotonic_ms(void)
{
    LARGE_INTEGER now;

    if (!s_timer_ok)
    {
        return (uint64_t)GetTickCount64();
    }

    QueryPerformanceCounter(&now);
    return (uint64_t)((now.QuadPart - s_start.QuadPart) * 1000 / s_freq.QuadPart);
}
