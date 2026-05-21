#pragma once
#include <arpa/inet.h> 
#include <atomic>
#include "queue.hpp"


class Producer {
    /*
    what do we need? 
    this will run on its own thread, so we can probably have it just poll the network device directly?
    how can we do this without insane context switching?

    basically,
    networking + OS stuff

    lets think about how we are dealing with this buffer
    so we are getting messages in batches with these structs of header + how many bytes
    passing in this array of structs as well to get!
    can fine tune the max number of messages we can pull in one syscall then!

    worse than io_uring for sure, but we will see how this one is!

    the nice thing is that the OS will directly copy the message data into the buffers that we give it!
        so... lowkey only copies are from the NIC to the socket and the socket to the ring buffer!
        then from buffer to socket to nic for outgoing
        so two + two = four total round trip copies
        this is not great, but the best we can do if we arent manipulating where the kernel would have the NIC copy it to

    so, how do we want to design this?
    I think the easiest way is to basically have some ring buffer that we allocate and then...
    we can find the size of the msgStruct and iterate across the pointer locations to update in O(batch)

    so basically we grab in batches so we avoid the syscall, but we still have to provide the thing manually for the thing
    how can we do this better?
    could Alignas and parallelize
    for now single-thread this
    kinda cooked tho

    pass to consumer!

    consumer does:
    parsing
    computation! - setup basic multithreaded computation :)
    edits the order book!
    
    Resource allocation is initialization makes sense here roughly, we just do shared ptr between producer and consumer?
        - ensure that we have destructor(make struct just for that or something if we need alignment), etc if any of the elements in the ring buffer have pointers need to destruct seperately with delete[] as well!
    */

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
        int bufferSize;
        int port;
        struct iovec iovecs[MSG_GLOBALS::MSG_BATCH_SIZE];
        struct mmsghdr msgs[MSG_GLOBALS::MSG_BATCH_SIZE];
        struct timespec timeout;
        struct sockaddr_in addr;
};

inline int Producer::maxPull(int t, int h){ // probably fine to inline
    if (h > t){
        return (h - t) - 1;
    }
    if (h < t){ // awful because this means we genuinely eat the tail :/
        bufferSize - (t - h);
    }
    return bufferSize;
}

template <class T>
void Producer::PollSocket(std::atomic<bool>& terminateFlag, queue<T>& q){ //epoll would only make sense if there were a bunch of sockets! Since we are UDP, we just grab in batches
    
    // recvmmsg
    // allows for pulling of whole queue maybe so less 50-100ns mode switches

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(5, &cpuset); // Bind to core 7
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); 

    // update msg
    int retval;
    int tail;
    size_t i{};
    while(!terminateFlag.load(std::memory_order_relaxed)){ // can use volatile here for intel bc of ordering guarentees I think, but complies to same thing 
        i = 0; // maxPull calcs the number of cells left between tail and head so tail does not eat head!

        // how do we actually need the compiler to order these accesses?
        tail = q.bufferTailIndex.load(std::memory_order_relaxed); // premature optimization for my brain :)
        for (; i < min(MSG_GLOBALS::MSG_BATCH_SIZE, maxPull(tail, q.bufferHeadIndex.load(std::memory_order_relaxed))); i++) { // how do we vectorize this loop? -- optimzation potential with changing to struct of arrays for AVX
            //msgs[i].msg_hdr.msg_iov->iov_base = buffer[bufferFrontIndex+i].message // update where to store messages! put at the tail of ring buffer
            // equivalent line of code is:
            iovecs[i].iov_base = &q.buffer[(tail+i) % q.bufferSize];
            // this is because the msg_iov is just references to the iovecs array and by updating like that, we don't chase pointers and have spatial locailty
            // for this, could use AVX-512 scatter to do basically divide this loop by a lot if it becomes a bottleneck(max linux cap for msg storage is 4096 so probably not)
        }

        // this just tells kernel to write our messages to the buffer, we update where our tail is, and we keep going!
        retval = recvmmsg(sockfd, msgs, i, 0, &timeout); // we only want to give the amount of messages as max that we can add to the ring buffer legally
        if (retval >= 0){
            q.bufferTailIndex.store((retval + tail) % q.bufferSize, std::memory_order_release);  // don't want to fill in the buffer after telling the conusmer we have, release adds a fence here so we dont
        }// else {
        //    throw std::runtime_error("Failed to retrieve messages from kernel");
        //}
        
    }

}