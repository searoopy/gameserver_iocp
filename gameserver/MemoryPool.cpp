#include "MemoryPool.h"
#include "Common.h" // 여기서 비로소 OverlappedEx의 정의를 읽음
#include <winsock2.h> // memset 등을 위해 필요
#include <atomic>

MemoryPool* GMemoryPool = nullptr;

struct SendPacket;
struct OverlappedEx;
struct Session;
struct OverlappedEx;

MemoryPool::MemoryPool(int capacity) 
{
    OverlappedEx* head = nullptr;

    for (int i = 0; i < capacity; ++i)
    {
        OverlappedEx* node = new OverlappedEx();
        node->next = head;
        head = node;
    }

    _top.ptr = head;
    _top.tag = 0;
}

OverlappedEx* MemoryPool::Pop() 
{
    /*std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool.empty()) return new OverlappedEx();

    OverlappedEx* obj = m_pool.front();
    m_pool.pop();
    return obj;*/

    while (true)
    {
        TaggedPtr oldTop = AtomicLoad128(&_top);

        if (oldTop.ptr == nullptr)
            return new OverlappedEx();

        TaggedPtr newTop;
        newTop.ptr = oldTop.ptr->next;
        newTop.tag = oldTop.tag + 1;

        if (InterlockedCompareExchange128(
            reinterpret_cast<volatile LONG64*>(&_top),
            newTop.tag,
            reinterpret_cast<LONG64>(newTop.ptr),
            reinterpret_cast<LONG64*>(&oldTop)))
        {
            oldTop.ptr->next = nullptr;
            return oldTop.ptr;
        }
    }

}

void MemoryPool::Push(OverlappedEx* obj) {
    //std::lock_guard<std::mutex> lock(m_mutex);
    //// 이제 OverlappedEx의 크기를 알므로 memset이 가능합니다.
    //memset(obj, 0, sizeof(OverlappedEx));
    //m_pool.push(obj);

    if (!obj) return;

    while (true)
    {
        TaggedPtr oldTop = AtomicLoad128(&_top);

        obj->next = oldTop.ptr;

        TaggedPtr newTop;
        newTop.ptr = obj;
        newTop.tag = oldTop.tag + 1;

        if (InterlockedCompareExchange128(
            reinterpret_cast<volatile LONG64*>(&_top),
            newTop.tag,
            reinterpret_cast<LONG64>(newTop.ptr),
            reinterpret_cast<LONG64*>(&oldTop)))
        {
            return;
        }
    }


}

MemoryPool::~MemoryPool() {
   /* while (!m_pool.empty()) {
        delete m_pool.front();
        m_pool.pop();
    }*/

    TaggedPtr top = AtomicLoad128(&_top);

    OverlappedEx* current = top.ptr;

    while (current)
    {
        OverlappedEx* next = current->next;
        delete current;
        current = next;
    }

}


