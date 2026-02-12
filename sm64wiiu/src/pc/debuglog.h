#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#ifdef TARGET_WII_U
#include <whb/log.h>
#include <whb/log_console.h>
#define LOG_DEBUG(...) do { WHBLogPrintf(__VA_ARGS__); } while (0)
#define LOG_INFO(...)  do { WHBLogPrintf(__VA_ARGS__); } while (0)
#define LOG_ERROR(...) do { WHBLogPrintf(__VA_ARGS__); } while (0)
#define LOG_LUA(...)   do { WHBLogPrintf(__VA_ARGS__); } while (0)
#define LOG_LUA_LINE(...) do { WHBLogPrintf(__VA_ARGS__); } while (0)
#define LOG_CONSOLE(...) do { WHBLogPrintf(__VA_ARGS__); } while (0)
#else
#include <stdio.h>

#define LOG_DEBUG(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_INFO(...)  do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_ERROR(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_LUA(...)   do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_LUA_LINE(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_CONSOLE(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#endif

#endif
