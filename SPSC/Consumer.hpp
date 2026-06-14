#pragma once
#include <atomic>
#include <vector>
#include "queue.hpp"
#include <x86intrin.h>
#include "config.hpp"

// we need maxPull for the templated function so we just inline it so the compiler can generate the function template instantiations before linktime
inline int maxPullc(int t, int h, int bufferSize){
    return (t - h + bufferSize) % bufferSize;
}

template <class T, class A>
requires isConsumerOf<T, A>
__attribute__((noinline))
void ConsumeQueue(std::atomic<bool>& terminateFlag, int batchSize, queue<T>& q, A& accumulator){
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(globalConfigs::consumerCore, &cpuset); // Bind to core 3, not hyperthreaded with producer or sim
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); 

    int tail;
    int head = q.bufferHeadIndex.load(std::memory_order_relaxed);;
    int fetch;
    while(!terminateFlag.load(std::memory_order_relaxed)){ 
        
        tail = q.bufferTailIndex.load(std::memory_order_relaxed);
        
        if (tail != head){ // if we havent caught up yet, consoom
            fetch = min(batchSize, maxPullc(tail, head, q.bufferSize));
            for(int i{}; i < fetch; ++i){ //batch!
                accumulator.consume(q.buffer[(head + i) % q.bufferSize]); // because the message is just a pointer to the aligned raw bytes in the buffer, pass by reference!
            } 
            head = (head + fetch) % q.bufferSize;
            q.bufferHeadIndex.store(head, std::memory_order_release);
        }
    }
}