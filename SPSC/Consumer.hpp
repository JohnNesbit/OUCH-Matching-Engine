#pragma once
#include <atomic>
#include <vector>
#include "queue.hpp"
//#include <immintrin.h>
#include <x86intrin.h>
// cpp concept to ensure accumulator has a consume function, T type is the type we are assuming we pull from the UDP messages
// put templated function in header file since it can violate ODR

// we need maxPull for the templated function so we just inline it so the compiler can generate the function template instantiations before linktime
inline int maxPullc(int t, int h, int bufferSize){ // fix this.
    //std::cout << "Head:"
    return (t - h + bufferSize) % bufferSize;
    /*if (h > t){
        return ((bufferSize - h) + (t - 1))  % bufferSize;
        //return bufferSize - (h - t);
    }
    if (h < t){ // awful because this means we genuinely eat the tail :/
        return (t - h) - 1;
    }
    return -1;*/
}

template <class T, class A>
requires isConsumerOf<T, A>
__attribute__((noinline))
void ConsumeQueue(std::atomic<bool>& terminateFlag, int batchSize, queue<T>& q, A& accumulator){
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(3, &cpuset); // Bind to core 7
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); 

    int tail;// = q.bufferTailIndex.load(std::memory_order_relaxed);
    int head = q.bufferHeadIndex.load(std::memory_order_relaxed);;
    int fetch;
    while(!terminateFlag.load(std::memory_order_relaxed)){ // question of if batching these makes sense even

        //while(q.bufferTailIndex.load(std::memory_order_relaxed) == tail && tail != q.bufferSize-1){ // would be effectively the same thing anyway, this stops contention maybe?
            // contention does actually NOT seem to be a problem for producer though... it also seems like consumer is busy-waiting like half the time??
        //    _mm_pause(); // busy wait and check diff?
        //}
        tail = q.bufferTailIndex.load(std::memory_order_relaxed);
        fetch = min(batchSize, maxPullc(tail, head, q.bufferSize));
        if (fetch > 0){ // if we havent caught up yet, consoom
            //_mm_lfence(); // ensure we aren't speculatively false sharing the buffer
            for(int i{}; i < fetch; ++i){ //batch!
                accumulator.consume(q.buffer[(head + i) % q.bufferSize]); // because the message is just a pointer to the aligned raw bytes in the buffer, pass by reference!
            } 
            head = (head + fetch) % q.bufferSize;
            q.bufferHeadIndex.store(head, std::memory_order_release);
        } else{

        }
    }
}