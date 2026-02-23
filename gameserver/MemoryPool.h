#pragma once
#include <mutex>
#include <queue>
#include <WinSock2.h>

struct OverlappedEx;

struct alignas(16) TaggedPtr
{
    OverlappedEx* ptr;
    uint64_t      tag;
};

static_assert(sizeof(TaggedPtr) == 16, "TaggedPtr must be 16 bytes");
static_assert(alignof(TaggedPtr) == 16, "TaggedPtr must be 16-byte aligned");


class MemoryPool {
public:
    MemoryPool(int capacity); // 선언만!
    ~MemoryPool();            // 선언만!

    OverlappedEx* Pop();      // 선언만!
    void Push(OverlappedEx* obj); // 선언만!

private:
   // std::queue<OverlappedEx*> m_pool;
    //std::mutex m_mutex;


    // 큐와 뮤텍스 대신 원자적 포인터를 사용합니다.
  //  std::atomic<OverlappedEx*> _top;


    //캐쉬 라인 히트를 위해 변경.
   // alignas(16) std::atomic<TaggedPtr> _top;

    alignas(16) TaggedPtr _top;


};


inline TaggedPtr AtomicLoad128(TaggedPtr* addr)
{
    TaggedPtr snapshot;

    while (true)
    {
        snapshot = *addr;

        if (InterlockedCompareExchange128(
            reinterpret_cast<volatile LONG64*>(addr),
            snapshot.tag,
            reinterpret_cast<LONG64>(snapshot.ptr),
            reinterpret_cast<LONG64*>(&snapshot)))
        {
            return snapshot;
        }
    }
}

extern MemoryPool* GMemoryPool;