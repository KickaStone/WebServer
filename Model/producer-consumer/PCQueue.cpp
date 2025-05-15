#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <thread>

using namespace std;

template <class T>
class PCQueue{

public:
    void put(T val){
        unique_lock<mutex> lock(mutex_);
        while(!queue_.empty()){
            condition_.wait(lock);
        }
        queue_.push(val);
        condition_.notify_one();
        cout << "Produced: " << val << endl;
    }

    T get(){
        unique_lock<mutex> lock(mutex_);
        while(queue_.empty()){
            condition_.wait(lock);
        }
        T val = queue_.front();
        queue_.pop();
        condition_.notify_one();
        cout << "Consumed: " << val << endl;
        return val;
    }
private:
    queue<T> queue_;
    mutex mutex_;
    condition_variable condition_;
};

int main()
{
    PCQueue<int> my_queue;

    std::thread t1([&my_queue]() -> void
                   {
        for(int i = 0; i <= 10; i++)
        {
            my_queue.put(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } });

    std::thread t2([&my_queue]() -> void
                   {
        for(int i = 0; i <= 10; i++)
        {
            my_queue.get();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        } });

    t1.join();
    t2.join();
    return 0;
}
