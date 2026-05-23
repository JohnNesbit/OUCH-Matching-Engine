#pragma once
#include <atomic>
#include <vector>
#include "queue.hpp"


// cpp concept to ensure accumulator has a consume function, T type is the type we are assuming we pull from the UDP messages
// put templated function in header file since it can violate ODR

// we need maxPull for the templated function so we just inline it so the compiler can generate the function template instantiations before linktime
inline int maxPull(int t, int h, int bufferSize){ // fix this.
    //std::cout << "Head:"
    if (h > t){
        return ((bufferSize - h) + (t - 1))  % bufferSize;
        
    }
    if (h < t){ // awful because this means we genuinely eat the tail :/
        return (t - h) - 1;
    }
    return 0;
}

template <class T, class A>
requires isConsumerOf<T, A>
void ConsumeQueue(std::atomic<bool>& terminateFlag, int batchSize, queue<T>& q, A& accumulator){
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(6, &cpuset); // Bind to core 7
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); 

    int tail;
    int head;
    int fetch;
    while(!terminateFlag.load(std::memory_order_relaxed)){ // question of if batching these makes sense even
        tail = q.bufferTailIndex.load(std::memory_order_relaxed);
        head = q.bufferHeadIndex.load(std::memory_order_relaxed);
        fetch = min(batchSize, maxPull(tail, head, q.bufferSize));
        if (head != tail){ // if we havent caught up yet, consoom
            for(int i{}; i < fetch; ++i){ //batch!
                accumulator.consume(q.buffer[(head + i) % q.bufferSize]); // because the message is just a pointer to the aligned raw bytes in the buffer, pass by reference!
            } 
            q.bufferHeadIndex.store((head + fetch) % q.bufferSize, std::memory_order_release);
        }   
    }
}