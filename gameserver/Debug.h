#pragma once

#ifdef _DEBUG
#define LOG(fmt, ...) DebugLog(fmt, __VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

void DebugLog(const char* format, ...);

