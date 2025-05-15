#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
/*
multi-thread producer multi-thread consumer
*/

// 生产者方法
void producer(unsigned int id, const size_t &count, std::queue<int> &queue, std::mutex &mutex, std::condition_variable &condition){
    for(size_t item = 0; item < count; item++){
        std::unique_lock<std::mutex> lock(mutex);
        queue.push(item);
        std::cout << "Produce " << id << " " << "Produced: " << item << std::endl;
        lock.unlock();
        condition.notify_one();
    }
}

// 消费者方法
void consumer(unsigned int id, const size_t &count, std::queue<int> &queue, std::mutex &mutex, std::condition_variable &condition){
    size_t item = 0;
    while(true){
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]{return !queue.empty();});
        int item = queue.front();
        queue.pop();
        std::cout << "Consume " << id << " " << "Consumed: " << item << std::endl;
        lock.unlock();
        item++;
        if(item >= count){
            break;
        }
    }
}

int main(){
    std::queue<int> queue;
    std::mutex mutex;
    std::condition_variable condition;
    unsigned int producer_count = 10;
    unsigned int consumer_count = 10;

    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;


    for(unsigned int i = 0; i < producer_count; i++){
        producer_threads.push_back(std::thread(producer, i, 10, std::ref(queue), std::ref(mutex), std::ref(condition)));
    }

    for(unsigned int i = 0; i < consumer_count; i++){
        consumer_threads.push_back(std::thread(consumer, i, 10, std::ref(queue), std::ref(mutex), std::ref(condition)));
    }

    for(unsigned int i = 0; i < producer_count; i++){
        producer_threads[i].join();
    }
    for(unsigned int i = 0; i < consumer_count; i++){
        consumer_threads[i].join();
    }
}