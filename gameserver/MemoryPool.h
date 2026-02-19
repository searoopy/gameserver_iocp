#pragma once
#include <mutex>
#include <queue>


struct OverlappedEx;

class MemoryPool {
public:
    MemoryPool(int capacity); // 선언만!
    ~MemoryPool();            // 선언만!

    OverlappedEx* Pop();      // 선언만!
    void Push(OverlappedEx* obj); // 선언만!

private:
    std::queue<OverlappedEx*> m_pool;
    std::mutex m_mutex;
};

extern MemoryPool* GMemoryPool;