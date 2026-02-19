#pragma once

#include <WinSock2.h>


void WorkerThread(HANDLE hIOCP, SOCKET listenSocket);