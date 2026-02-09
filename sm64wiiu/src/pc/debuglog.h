#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <stdio.h>

#define LOG_DEBUG(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_INFO(...)  do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_ERROR(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_LUA(...)   do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_LUA_LINE(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)
#define LOG_CONSOLE(...) do { printf(__VA_ARGS__); printf("\n"); } while (0)

#endif
