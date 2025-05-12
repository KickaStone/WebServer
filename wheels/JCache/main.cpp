#include "JLruCache.h"
#include <iostream>

using namespace JCache;

int main(){
    JLruCache<int, int> cache(3);
    cache.put(1, 1);
    cache.put(2, 2);
    cache.put(3, 3);
    cache.put(4, 4);
    cache.put(5, 5);
    cache.put(6, 6);
    cache.put(7, 7);
    cache.put(8, 8);
    cache.remove(1);
    cache.remove(2);
    cache.remove(3);
    cache.remove(4);
    cache.remove(5);
    cache.remove(6);
    std::cout << cache.get(7) << std::endl;
    std::cout << cache.get(8) << std::endl;
}