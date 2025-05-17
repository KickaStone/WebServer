#ifndef MEMORYPOOL_H
#define MEMORYPOOL_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#define MEMPOOL_ALIGNMENT 8

using namespace std;

template <typename T>
struct memoryblock
{
    int nsize;
    int nfree;
    int nfirst;
    int nunitsize;
    memoryblock *pnext;
    char adata[]; // 柔性数组，必须是最后一个成员

    memoryblock(int nunitsize, int nunitamount) : nunitsize(nunitsize), nsize(nunitamount * nunitsize), nfree(nunitamount - 1), nfirst(1), pnext(nullptr)
    {
        char *ppdata = adata;
        cout << "存储区首指针 ppdata=" << (int *)ppdata << endl;
        // 第一个单位默认已分配
        for (int i = 1; i < nunitamount; i++)
        {
            // 每个单位的前2个字节存储下一个空闲单位的指针
            (*(unsigned short *)ppdata) = i;
            cout << (int *)ppdata << "下一个可分配单位序号: " << (int)*ppdata << endl;
            ppdata += nunitsize;
        }
        cout << "---------调用内存块构造函数---------" << endl;
    }

    void *operator new(size_t, int nunitsize, int nunitamount)
    {
        cout << "分配内存并创建memoryblock对象" << endl;
        return ::operator new(sizeof(memoryblock) + nunitsize * nunitamount);
    }

    void operator delete(void *p)
    {
        cout << "--------调用内存块析构函数---------" << endl;
        ::operator delete(p);
    }

    void print_status()
    {   
        cout << "-------------------------------------------------------------------------------------------------------------------------" << endl;
        cout << "memblock addr: " << (int*)this << " | nsize: " << nsize << " | nfree: " << nfree << " | nfirst: " << nfirst << " | pnext: " << (int*)pnext << " | adata: " << (int*)adata << " | nunitsize: " << nunitsize << endl;
        if(nfree == 0){
            cout << "memblock is full" << endl;
        }else{
            int next_free = nfirst;
            cout << "free list: ";
            for(int i = 0; i < nfree; i++){
                if(i) cout << "->";
                cout << (int)next_free;
                next_free = (*(unsigned short*)(adata + next_free * nunitsize));
            }
            cout << endl;
        }
        cout << "-------------------------------------------------------------------------------------------------------------------------" << endl;
    }
};

template <typename T>
struct memorypool
{
    int ninitsize;
    int ngrowsize;
    int nunitsize;
    memoryblock<T> *pblock;

    memorypool(int _ngrowsize = 10, int _ninitsize = 3) : ngrowsize(_ngrowsize), ninitsize(_ninitsize), pblock(nullptr)
    {
        cout << "--------调用内存池的构造函数---------" << endl;
        nunitsize = sizeof(T);
        if (sizeof(T) > 4)
        {
            nunitsize = (sizeof(T) + (MEMPOOL_ALIGNMENT - 1)) & ~(MEMPOOL_ALIGNMENT - 1); // 返回值为MEMPOOL_ALIGNMENT的倍数
        }
        else if (sizeof(T) > 2)
        {
            nunitsize = (sizeof(T) + (MEMPOOL_ALIGNMENT - 1)) & ~(MEMPOOL_ALIGNMENT - 1);
        }
        else
        {
            nunitsize = sizeof(T);
        }
    }

    ~memorypool()
    {
        memoryblock<T> *pmyblock = pblock;
        while (pmyblock != nullptr)
        {
            memoryblock<T> *tmp = pmyblock;
            pmyblock = pmyblock->pnext;
            delete tmp;
        }
        cout << "--------调用内存池的析构函数---------" << endl;
    }

    void print_status()
    {
        cout << "\n\n <<<<<<<< MemoryPool Status >>>>>>>>\n\n" << endl;
        cout << "MemoryPool Status:" << endl;
        cout << "ninitsize: " << ninitsize << " | ngrowsize: " << ngrowsize << " | nunitsize: " << nunitsize << " | pblock: " << (int*)pblock << endl;
        memoryblock<T> *pmyblock = pblock;
        while (pmyblock != nullptr)
        {
            pmyblock->print_status();
            pmyblock = pmyblock->pnext;
        }
        cout << "\n\n <<<<<<<< MemoryPool Status >>>>>>>>\n\n" << endl;
    }

    void *allocate(size_t num)
    {
        for (int i = 0; i < num; i++)
        {
            if (pblock == nullptr)
            {
                // 创建第一个内存块
                pblock = (memoryblock<T> *)new (nunitsize, ninitsize) memoryblock<T>(nunitsize, ninitsize);
                return (void *)pblock->adata;
            }

            // 找合适的内存块
            memoryblock<T> *pmyblock = pblock;
            while (pmyblock != NULL && 0 == pmyblock->nfree)
            {
                pmyblock = pmyblock->pnext;
            }

            if (pmyblock == NULL)
            {
                // 没有找到合适的内存块，申请新的内存块
                if (ngrowsize == 0)
                {
                    return nullptr;
                }
                cout << "无空闲块 分配新的内存块" << endl;
                pmyblock = (memoryblock<T> *)new (nunitsize, ngrowsize) memoryblock<T>(nunitsize, ngrowsize);
                if (pmyblock == nullptr)
                    return nullptr; // 失败

                pmyblock->pnext = pblock;
                pblock = pmyblock;
                return (void *)pmyblock->adata;
            }
            else
            {
                // 找到合适的内存块
                cout << "找到合适的内存块" << endl;
                char *pfree = pmyblock->adata + pmyblock->nfirst * nunitsize;
                pmyblock->nfirst = (*(unsigned short *)pfree);
                pmyblock->nfree--;
                return (void *)pfree;
            }
        }
    }

    void free(void *pfree)
    {
        cout << "释放存储单元内存 " << (int *)pfree << endl;
        memoryblock<T> *pmyblock = pblock;
        memoryblock<T> *preblock = nullptr;

        // 找到p所在块 因为链表随机排列，只能用地址不在范围内判断
        while (pmyblock != nullptr && (pfree < pmyblock->adata || pfree >= pmyblock->adata + pmyblock->nsize))
        {
            preblock = pmyblock;
            pmyblock = pmyblock->pnext;
        }

        if (pmyblock != nullptr)
        {
            // 1 修改链表
            *((unsigned short *)pfree) = pmyblock->nfirst;
            // 2. 修改nfirst，把pfree的序号赋值给nfirst（放到最前面）
            pmyblock->nfirst = (unsigned short)((unsigned long)pfree - (unsigned long)pmyblock->adata) / nunitsize;
            pmyblock->nfree++;

            // 3. 判断是否需要释放block的内存
            if (pmyblock->nfree * nunitsize == pmyblock->nsize)
            {
                // 释放block的内存
                // 在链表中移除该结点
                if(preblock == nullptr){
                    pblock = pmyblock->pnext;
                }else{
                    preblock->pnext = pmyblock->pnext;
                }
                delete (pmyblock);
            }
            else
            {
                // 移动pmyblock到链表头部
                if(preblock == nullptr){
                    // 本来就是头 不需要修改
                    return;
                }

                preblock->pnext = pmyblock->pnext;
                pmyblock->pnext = pblock;
                pblock = pmyblock;
            }
        }
    }
};

#endif
