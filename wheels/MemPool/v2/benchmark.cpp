/**
 * benchmark for mempool
 * 
 */

#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ConcurrentAlloc.h"



void BenchmarkMalloc(int ntimes, size_t nworks, size_t rounds){
    std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime(0);
    std::atomic<size_t> free_costtime(0);

    // nworks
    for(size_t k = 0; k < nworks; ++k){
        vthread[k] = std::thread([&, k](){
            std::vector<void*> v;
            v.reserve(ntimes);

            for(size_t i = 0; i < rounds; ++i){
                size_t begin1 = clock();

                for(size_t j = 0; j < ntimes; ++j){
                    v.push_back(malloc(16));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for(size_t j = 0; j < ntimes; ++j){
                    free(v[j]);
                }

                size_t end2 = clock();

                malloc_costtime += end1 - begin1;
                free_costtime += end2 - begin2;
                
                // Clear the vector after each round
                v.clear();
            }
        });
    }

    for(auto &t : vthread){
        t.join();
    }

    printf("%zu threads || %zu rounds || %zu malloc : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load());
    printf("%zu threads || %zu rounds || %zu free : cost %zu ms\n", nworks, rounds, ntimes, free_costtime.load());
    printf("%zu threads || %zu rounds || %zu malloc&free : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load() + free_costtime.load());
}

void BenchmarkConcurrentAlloc(int ntimes, size_t nworks, size_t rounds){
 std::vector<std::thread> vthread(nworks);
    std::atomic<size_t> malloc_costtime(0);
    std::atomic<size_t> free_costtime(0);

    // nworks
    for(size_t k = 0; k < nworks; ++k){
        vthread[k] = std::thread([&, k](){
            std::vector<void*> v;
            v.reserve(ntimes);

            for(size_t i = 0; i < rounds; ++i){
                size_t begin1 = clock();

                for(size_t j = 0; j < ntimes; ++j){
                    v.push_back(ConcurrentAlloc(16));
                }
                size_t end1 = clock();

                size_t begin2 = clock();
                for(size_t j = 0; j < ntimes; ++j){
                    ConcurrentFree(v[j]);
                }

                size_t end2 = clock();

                malloc_costtime += end1 - begin1;
                free_costtime += end2 - begin2;
                
                // Clear the vector after each round
                v.clear();
            }
        });
    }

    for(auto &t : vthread){
        t.join();
    }

    printf("%zu threads || %zu rounds || %zu malloc : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load());
    printf("%zu threads || %zu rounds || %zu free : cost %zu ms\n", nworks, rounds, ntimes, free_costtime.load());
    printf("%zu threads || %zu rounds || %zu malloc&free : cost %zu ms\n", nworks, rounds, ntimes, malloc_costtime.load() + free_costtime.load());
}

int main(){
    size_t n = 10000;
    cout << "================================================" << endl;
    BenchmarkMalloc(n, 4, 10);
    cout << endl << endl;

    BenchmarkConcurrentAlloc(n, 4, 10);

    cout << "================================================" << endl;
    return 0;
}