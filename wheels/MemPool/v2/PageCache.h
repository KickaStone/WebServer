#pragma once
#include "Common.h"

class PageCache
{
public:
    static PageCache *GetInstance()
    {
        return &_sInst;
    }
    std::mutex _pageMtx;

    // pc申请k页span接口
    Span *NewSpan(size_t k);

    // 根据ptr找到对应的span
    Span *MapObjectToSpan(void *obj);
    // 将span还给PC
    void ReleaseSpanToPageCache(Span *span);

private:
    PageCache() {}
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;

private:
    static PageCache _sInst;
    SpanList _spanLists[PAGE_NUM];
    std::unordered_map<PageId, Span *> _idSpanMap; // 记录pageId和span的映射关系，避免每次都要遍历spanList

    // 在NewSpan中分配出去的时候记录pageId和span的映射关系
};
