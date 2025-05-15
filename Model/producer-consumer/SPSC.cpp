#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
/*
single-thread producer single-thread consumer
*/

// 生产者方法
void producer(const size_t &count, std::queue<int> &queue, std::mutex &mutex,   std::condition_variable &condition){
    for(size_t i = 0; i < count; i++){
        std::unique_lock<std::mutex> lock(mutex);
        queue.push(i);
        std::cout << "Produced: " << i << std::endl;
        lock.unlock();
        condition.notify_one();
    }
}

// 消费者方法
void consumer(const size_t &count, std::queue<int> &queue, std::mutex &mutex, std::condition_variable &condition){
    size_t i = 0;
    while(true){
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]{return !queue.empty();});
        int item = queue.front();
        queue.pop();
        std::cout << "Consumed: " << item << std::endl;
        lock.unlock();
        i++;
        if(i >= count){
            break;
        }
    }
}


int main(){
    std::queue<int> queue;
    std::mutex mutex;
    std::condition_variable condition;
    std::thread producer_thread(producer, 10, std::ref(queue), std::ref(mutex), std::ref(condition));
    std::thread consumer_thread(consumer, 10, std::ref(queue), std::ref(mutex), std::ref(condition));
    producer_thread.join();
    consumer_thread.join();
}