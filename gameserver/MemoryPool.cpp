#include "MemoryPool.h"
#include "Common.h" // 여기서 비로소 OverlappedEx의 정의를 읽음
#include <winsock2.h> // memset 등을 위해 필요
#include <atomic>

MemoryPool* GMemoryPool = nullptr;
SLIST_HEADER g_poolHeader;


struct SendPacket;
struct OverlappedEx;
struct Session;

MemoryPool::MemoryPool(int capacity) 
{

    // 초기화 (생성자)
    InitializeSListHead(&g_poolHeader);

    for (int i = 0; i < capacity; ++i)
    {
        OverlappedEx* node = new OverlappedEx();
        // SList에 직접 푸시 (이렇게 해야 Pop이 작동함)
        InterlockedPushEntrySList(&g_poolHeader, reinterpret_cast<PSLIST_ENTRY>(node));
    }


    /*
    OverlappedEx* head = nullptr;

    for (int i = 0; i < capacity; ++i)
    {
        OverlappedEx* node = new OverlappedEx();
        node->next = head;
        head = node;
    }

    _top.ptr = head;
    _top.tag = 0;
    */
}

OverlappedEx* MemoryPool::Pop() 
{
    /*std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pool.empty()) return new OverlappedEx();

    OverlappedEx* obj = m_pool.front();
    m_pool.pop();
    return obj;*/

    /*
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
    */

    PSLIST_ENTRY entry = InterlockedPopEntrySList(&g_poolHeader);

    if (entry == nullptr) {
        OverlappedEx* newObj = new OverlappedEx();
        newObj->Init(); // 초기화 함수 호출 잊지 마세요!
        return newObj;
    }

    OverlappedEx* obj = reinterpret_cast<OverlappedEx*>(entry);
    obj->Init(); // 꺼낸 객체는 항상 깨끗하게 초기화
    return obj;

}

void MemoryPool::Push(OverlappedEx* obj) {
    //std::lock_guard<std::mutex> lock(m_mutex);
    //// 이제 OverlappedEx의 크기를 알므로 memset이 가능합니다.
    //memset(obj, 0, sizeof(OverlappedEx));
    //m_pool.push(obj);


    /*
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
    */

    if (obj == nullptr) return;

    // 다시 풀에 반납
    InterlockedPushEntrySList(&g_poolHeader, reinterpret_cast<PSLIST_ENTRY>(obj));
    //InterlockedPushEntrySList(&g_poolHeader, reinterpret_cast<PSLIST_ENTRY>(obj));

}

MemoryPool::~MemoryPool() {
   /* while (!m_pool.empty()) {
        delete m_pool.front();
        m_pool.pop();
    }*/

    /*
    TaggedPtr top = AtomicLoad128(&_top);

    OverlappedEx* current = top.ptr;

    while (current)
    {
        OverlappedEx* next = current->next;
        delete current;
        current = next;
    }*/

    while (PSLIST_ENTRY entry = InterlockedPopEntrySList(&g_poolHeader)) {
        delete reinterpret_cast<OverlappedEx*>(entry);
    }


}


