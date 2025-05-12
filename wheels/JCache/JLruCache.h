#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "JICachePolicy.h"

namespace JCache
{

    template <typename Key, typename Value>
    class JLruCache;

    template <typename Key, typename Value>
    class LruNode
    {

    private:
        Key key_;
        Value value_;

        size_t accessCount_;                      // 访问次数
        std::weak_ptr<LruNode<Key, Value>> prev_; // 避免循环引用
        std::shared_ptr<LruNode<Key, Value>> next_;

    public:
        LruNode(Key key, Value value) : key_(key), value_(value), accessCount_(1) {}

        Key getKey() const { return key_; }
        Value getValue() const { return value_; }
        void setValue(Value value) { value_ = value; }
        size_t getAccessCount() const { return accessCount_; }
        void incrementAccessCount() { ++accessCount_; }

        friend class JLruCache<Key, Value>; // 声明友元类: 可以访问LruNode的私有成员和protected成员
    };

    template <typename Key, typename Value>
    class JLruCache : public JICachePolicy<Key, Value>
    {

    public:
        // 类型别名 方便编程
        using LruNodeType = LruNode<Key, Value>;
        using NodePtr = std::shared_ptr<LruNodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        // 构造和析构函数
        JLruCache(size_t maxSize) : capacity_(maxSize) { initializeList(); }
        ~JLruCache() override = default; // 默认析构函数

        void put(Key key, Value value) override;
        bool get(Key key, Value &value) override;
        Value get(Key key) override;

        void remove(Key key); // 删除指定节点
    private:
        void initializeList();                                     // 初始化链表
        void updateExistingNode(NodePtr node, const Value &value); // 更新存在结点的值
        void moveToMostRecent(NodePtr node);                       // 移动到最新的位置
        void removeNode(NodePtr node);                             // 删除节点
        void insertNode(NodePtr node);                             // 在尾部插入节点
        void addNewNode(const Key &key, const Value &value);       // 添加新节点
        void evictLeastRecent();                                   // 淘汰最久未使用的节点

    private:
        int capacity_;      // 缓存容量
        NodeMap nodeMap_;   // 存储节点
        std::mutex mutex_;  // 互斥锁
        NodePtr dummyHead_; // 头节点（虚拟）
        NodePtr dummyTail_; // 尾节点
    };

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::put(Key key, Value value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            // 更新已存在的节点
            updateExistingNode(it->second, value);
            return;
        }
        addNewNode(key, value);
    }

    template <typename Key, typename Value>
    inline bool JLruCache<Key, Value>::get(Key key, Value &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            moveToMostRecent(it->second);
            value = it->second->getValue();
            return true;
        }
        return false;
    }

    template <typename Key, typename Value>
    inline Value JLruCache<Key, Value>::get(Key key)
    {
        Value value{};
        get(key, value);
        return value;
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::remove(Key key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodeMap_.find(key);
        if (it != nodeMap_.end())
        {
            removeNode(it->second);
            nodeMap_.erase(it);
        }
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::initializeList()
    {
        dummyHead_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyTail_ = std::make_shared<LruNodeType>(Key(), Value());
        dummyHead_->next_ = dummyTail_;
        dummyTail_->prev_ = dummyHead_;
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::updateExistingNode(NodePtr node, const Value &value)
    {
        node->setValue(value);
        moveToMostRecent(node);
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::insertNode(NodePtr node)
    {
        // 在尾部插入节点
        node->next_ = dummyTail_;
        node->prev_ = dummyTail_->prev_;
        dummyTail_->prev_.lock()->next_ = node;
        dummyTail_->prev_ = node;
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::removeNode(NodePtr node)
    {
        if (!node->prev_.expired() && node->next_)
        {
            auto prev = node->prev_.lock(); // 使用lock获取shared_ptr
            prev->next_ = node->next_;
            node->next_->prev_ = prev;
            node->next_ = nullptr; // 断开连接
        }
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::addNewNode(const Key &key, const Value &value)
    {
        if (nodeMap_.size() >= capacity_)
        {
            evictLeastRecent();
        }

        auto newNode = std::make_shared<LruNodeType>(key, value);
        insertNode(newNode);
        nodeMap_[key] = newNode;
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::evictLeastRecent()
    {
        NodePtr leastRecent = dummyHead_->next_;
        removeNode(leastRecent);
        nodeMap_.erase(leastRecent->getKey());
    }

    template <typename Key, typename Value>
    inline void JLruCache<Key, Value>::moveToMostRecent(NodePtr node)
    {
        removeNode(node);
    }

    template <typename Key, typename Value>
    class JLruKCache : public JLruCache<Key, Value>
    {
    public:
        JLruKCache(size_t historyCap, size_t k)
            : JLruCache<Key, Value>(historyCap),
              k_(k),
              historyList_(std::make_unique<JLruCache<Key, size_t>>(historyCap)) {}

        ~JLruKCache() override = default;

        void put(Key key, Value value);
        Value get(Key key);

    private:
        size_t k_;                                            // 需要被访问k次才能进入缓存
        std::unique_ptr<JLruCache<Key, size_t>> historyList_; // 访问数据历史记录， value为访问次数
        std::unordered_map<Key, Value> historyValueMap_;      // 数据未达到k次访问的数据值
    };

    template <typename Key, typename Value>
    inline void JLruKCache<Key, Value>::put(Key key, Value value)
    {
        // 检查是否在主缓存中
        Value existingValue{};
        bool inMainCache = JLruCache<Key, Value>::get(key, existingValue);
        if (inMainCache)
        {
            // 在主缓存中，直接更新主缓存中对应值
            JLruCache<Key, Value>::put(key, value);
            return;
        }

        // 获取并更新访问历史
        size_t historyCount = historyList_->get(key);
        historyList_->put(key, ++historyCount);

        // 保存到历史记录映射，后续get可能调用
        historyValueMap_[key] = value;

        if (historyCount >= k_)
        {
            // 达到k次访问，加入主缓存, 并删除历史记录相关的数据
            historyList_->remove(key);
            historyValueMap_.erase(key);
            JLruCache<Key, Value>::put(key, value);
        }
    }

    template <typename Key, typename Value>
    inline Value JLruKCache<Key, Value>::get(Key key)
    {
        // 检查是否在主缓存中
        Value existingValue{};
        bool inMainCache = JLruCache<Key, Value>::get(key, existingValue);

        if (inMainCache)
        {
            return existingValue;
        }

        // 获取并更新访问历史
        size_t historyCount = historyList_->get(key);
        historyList_->put(key, ++historyCount);

        if (historyCount >= k_)
        {
            auto it = historyValueMap_.find(key);
            if (it != historyValueMap_.end())
            {
                // 有历史值，将其加入主缓存
                Value storedHistoryValue = it->second;
                historyList_->remove(key);
                historyValueMap_.erase(key);
                JLruCache<Key, Value>::put(key, storedHistoryValue);
                return storedHistoryValue;
            }
            else
            {
                // 没有历史值，无法添加到缓存？ 返回默认值（其实是不是应该抛出异常了）
                throw std::runtime_error("No history value found for key: " + key);
            }
        }

        // 数据不在缓存且不满足添加条件
        return existingValue;
    }

} // namespace JCache
