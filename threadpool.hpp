
#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

using namespace std;

class ThreadPool {
private:
    vector<thread> workers;
    queue<function<void()>> tasks;
    
    mutex queue_mutex;
    condition_variable condition;
    bool stop;

public:
    ThreadPool(size_t num_threads) : stop(false) {
        // Spin up the worker threads
        for(size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                for(;;) {
                    function<void()> task;
                    {
                        // Worker waits for the Boss to add a task to the queue
                        unique_lock<mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
                        
                        if(this->stop && this->tasks.empty()) return;
                        
                        task = move(this->tasks.front());
                        this->tasks.pop();
                    }
                    // Worker executes the HTTP task
                    task();
                }
            });
        }
    }

    // Boss Thread uses this to add a new client socket to the queue
    void enqueue(function<void()> task) {
        {
            unique_lock<mutex> lock(queue_mutex);
            tasks.emplace(move(task));
        }
        condition.notify_one(); // Wake up one sleeping worker
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(thread &worker: workers) {
            worker.join();
        }
    }
};