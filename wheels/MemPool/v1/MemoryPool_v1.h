#ifndef MEMORYPOOL_V1_H
#define MEMORYPOOL_V1_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <assert.h>

namespace MemoryPool_V1
{

#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

    // 具体内存池的slot大小无法确定，因为每个内存池的slot大小不同（8的倍数），
    // 所以这个槽结构体的sizeof不是实际的槽大小 
    struct Slot
    {
        Slot* next;
    };

    class MemoryPool
    {
    public:
        MemoryPool(size_t BlockSize = 4096);
        ~MemoryPool();

        void init(size_t);
        void *allocate();
        void deallocate(void *);

    private:
        void allocateNewBlock();
        size_t padPointer(char *p, size_t align);

    private:
        int BlockSize_;               // 内存块大小
        int SlotSize_;                // 槽大小
        Slot *firstBlock_;            // 指向内存池管理的首个实际内存块
        Slot *curSlot_;               // 指向当前未被使用过的槽
        Slot *freeList_;              // 指向空闲槽（被使用过后又被释放的槽）
        Slot *lastSlot_;              // 指向当前内存块中最后能够存放元素的位置表示（超过需要申请新内存块）
        std::mutex mutexForFreeList_; // 保证freeList_的线程安全
        std::mutex mutexForBlock_;    // 避免多线程情况下避免不必要的重复开辟内存导致的浪费行为
    };

    class HashBucket
    {
 
    public:
        static void initMemoryPool();
        static MemoryPool &getMemoryPool(int index); // 单例模式内存池 

        static void *useMemory(size_t size)
        {
            if (size <= 0)
                return nullptr;
            if (size > MAX_SLOT_SIZE)
                return operator new(size); // 大于最大槽大小，使用new分配

            return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
        }

        static void freeMemory(void *ptr, size_t size)
        {
            if (!ptr)
                return;
            if (size > MAX_SLOT_SIZE)
            { // 大于最大槽大小，使用delete释放
                operator delete(ptr);
                return;
            }

            getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
        }

        template <typename T, typename... Args>
        friend T *newElement(Args &&...args);

        template <typename T>
        friend void deleteElement(T *p);
    };

    template <typename T, typename... Args>
    T *newElement(Args &&...args)
    {
        T *p = nullptr;
        // 根据元素大小选择合适的分配器
        if ((p = reinterpret_cast<T *>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        {
            new (p) T(std::forward<Args>(args)...); // 在p指向的内存上构造T对象, construct()函数的底层实现
        }
        return p;
    }

    template <typename T>
    void deleteElement(T *p)
    {
        if (p)
        {
            p->~T();
            HashBucket::freeMemory(reinterpret_cast<void *>(p), sizeof(T));
        }
    }

}
#endif // MEMORYPOOL_V1_H