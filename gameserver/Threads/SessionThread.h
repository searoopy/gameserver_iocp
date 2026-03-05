#pragma once


#include <atomic>
#include <thread>
#include "..\Debug.h"


extern std::atomic<bool> g_SessionThreadRun;

extern std::thread g_SessionThread;


void SessionThread();


void StartSessionThread();

void CleanSessionThread();





