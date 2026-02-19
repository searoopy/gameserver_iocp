#include "MemoryPool.h"
#include "Common.h" // 여기서 비로소 OverlappedEx의 정의를 읽음
#include <winsock2.h> // memset 등을 위해 필요

MemoryPool* GMemoryPool = nullptr;

struct SendPacket;
struct OverlappedEx;
struct Session;
struct OverlappedEx;

MemoryPool::MemoryPool(int capacity) {
    for (int i = 0; i < capacity; ++i) {
        // 이제 OverlappedEx의 크기를 알 수 있으므로 new가 가능합니다.
        m_pool.push(new OverlappedEx());
    }
}

OverlappedEx* MemoryPool::Pop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool.empty()) return new OverlappedEx();

    OverlappedEx* obj = m_pool.front();
    m_pool.pop();
    return obj;
}

void MemoryPool::Push(OverlappedEx* obj) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // 이제 OverlappedEx의 크기를 알므로 memset이 가능합니다.
    memset(obj, 0, sizeof(OverlappedEx));
    m_pool.push(obj);
}

MemoryPool::~MemoryPool() {
    while (!m_pool.empty()) {
        delete m_pool.front();
        m_pool.pop();
    }
}


void SendPacket(Session* session, OverlappedEx* sendOv);