#pragma once
#include <arpa/inet.h> 
#include <atomic>
#include <iostream>
#include <errno.h>
#include "queue.hpp"
#include "config.hpp"

class Producer {

    public:
        Producer (int port, int bufferSize);

        template <class T>
        void PollSocket(std::atomic<bool>&, queue<T>& q);

        int min(int a, int b) {return (a < b) ? a : b;}
        auto maxPull(int t, int h) -> int;
        const struct sockaddr_in* getAddr(){return &addr;}

        // don't need a destructor or copy/move funcs so no rule of 5
        /*
        ~Producer() = default;
        Producer(const Producer&) = delete;
        Producer(Producer&&) = delete;
        Producer& operator=(Producer&&) = delete;
        Producer& operator=(const Producer&) = delete;
        */

    private:
        int sockfd; 
        int port;
        int bufferSize;
        struct iovec iovecs[MSG_GLOBALS::MSG_BATCH_SIZE];
        struct mmsghdr msgs[MSG_GLOBALS::MSG_BATCH_SIZE];
        struct timespec timeout;
        struct sockaddr_in addr;
};

inline int Producer::maxPull(int t, int h){ // probably fine to inline
    return (h - t - 1 + bufferSize) % bufferSize;
}

template <class T>
__attribute__((noinline))
void Producer::PollSocket(std::atomic<bool>& terminateFlag, queue<T>& q){ //epoll would only make sense if there were a bunch of sockets! Since we are UDP, we just grab in batches
    
    // recvmmsg
    // allows for pulling of whole queue maybe so less 50-100ns mode switches
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(globalConfigs::producerCore, &cpuset); // Bind to core 2(no hyperthreads)
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); 
    
    // update msg
    int retval;
    int tail = q.bufferTailIndex.load(std::memory_order_relaxed);
    long producerRecieved{};
    int i{};
    while(!terminateFlag.load(std::memory_order_relaxed)){ // can use volatile here for intel bc of ordering guarentees; complies to same thing 
        i = 0; // maxPull calcs the number of cells left between tail and head so tail does not eat head!
        
        // how do we actually need the compiler to order these accesses?
         // premature optimization for my brain :)
        for (; i < min(MSG_GLOBALS::MSG_BATCH_SIZE, maxPull(tail, q.bufferHeadIndex.load(std::memory_order_relaxed))); i++){//min( ,maxPull(tail, q.bufferHeadIndex.load(std::memory_order_relaxed))); i++) { // how do we vectorize this loop? -- optimzation potential with changing to struct of arrays for AVX
            //msgs[i].msg_hdr.msg_iov->iov_base = &q.buffer[tail+i % q.bufferSize] // update where to store messages! put at the tail of ring buffer
            // equivalent line of code is:
            iovecs[i].iov_base = &q.buffer[(tail+i) % q.bufferSize]; // doesnt actually do access so no false sharing
            // this is because the msg_iov is just references to the iovecs array and by updating like that, we don't chase pointers and have spatial locailty
            // for this, could use AVX-512 scatter to do basically divide this loop by a lot if it becomes a bottleneck(max linux cap for msg storage is 4096 so probably not)
        }
        
        // this just tells kernel to write our messages to the buffer, we update where our tail is, and we keep going!
        retval = recvmmsg(sockfd, msgs, i, 0, &timeout); // we only want to give the amount of messages as max that we can add to the ring buffer legally
        if (retval >= 0){
            producerRecieved += retval;
            tail = (retval + tail) % q.bufferSize;
            q.bufferTailIndex.store(tail, std::memory_order_release);  // don't want to fill in the buffer after telling the conusmer we have, release adds a fence here so we dont
        } else{
            std::cout << retval << std::endl;
        }
    }
    std::cout << producerRecieved << std::endl;
}