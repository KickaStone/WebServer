#pragma once

namespace JCache
{

template <typename Key, typename Value>
class JICachePolicy
{
public:
    virtual ~JICachePolicy(){};

    // 插入一个键值对
    virtual void put(Key key, Value value) = 0;
    // 获取一个键值对, key是入参，返回的值通过参数传回，成功返回true，失败返回false
    virtual bool get(Key key, Value& value) = 0;
    // 直接获取key对应value
    virtual Value get(Key key) = 0;
    // 删除一个键值对
    virtual void remove(Key key) = 0;
};

} // namespace JCache