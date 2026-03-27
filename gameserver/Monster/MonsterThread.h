#pragma once

#include <atomic>
#include <thread>


#include "..\Debug.h"

extern std::atomic<bool> g_MonsterThreadRun;

extern std::thread g_MonsterThread;

void MonsterThread();

void StartMonsterThread();

void CleanMonsterThread();
