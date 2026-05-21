#pragma once
#include <netinet/in.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <stdexcept>
#include <stdlib.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <string.h>
#include <charconv>
#include <atomic>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <algorithm>

// create concept: T needs to have a generator function!
// T doesn't actually need a constructor? Kinda does need a destructor though!
// arrays will be auto delete[] when go out of scope bs unique_ptr
// concept is that only primitives so no destructor so it is safe when we legit just do the storage trick
// if we had allocated data, when we allocator.deallocate, the delete[] will not get called to free allocated storage pointed to by T objects


template<class T>
concept triviallyDestructable = std::is_trivially_destructible_v<T> == true;

template <class Generator, class Generated>
concept isGenerator = requires(Generated a, Generator gen){
    {gen.generate(a)} -> std::same_as<void>;
};

template<triviallyDestructable T>
class exchangeSimulator {
    public:
        exchangeSimulator(int, int);

        template<isGenerator<T> G>
        void run(std::atomic<bool>&, const struct sockaddr_in*, G& generator);

        ~exchangeSimulator(){
            allocator.deallocate(sendBuffer, batchSize);
        }

        // rule of 5 because we broke RAII and needed a destructor(unique_ptr default initializes objects and we dont want to default construct!)
        exchangeSimulator(const exchangeSimulator&) = delete;
        exchangeSimulator(exchangeSimulator&&) = delete;
        exchangeSimulator& operator=(exchangeSimulator&&) = delete;
        exchangeSimulator& operator=(const exchangeSimulator&) = delete;

    private:
        int sockfd;
        int port;
        int batchSize;
        struct sockaddr_in addr;
        std::unique_ptr<struct iovec[]> iovecs; // heap allocating all of this because batch size could be up to 1024 and these structs are >4 bytes so not lovely on stack T could be quite large 
        std::unique_ptr<struct mmsghdr[]> msgs; // we use unique_ptr to C-style instead of vectors because vectors don't let us get non-const refs to their elements
        std::allocator<T> allocator; // define allocator before so member initializer list goes in the right order!
        T* sendBuffer; // dont want our thing to get default constructed btw so we have to do some tricky things
};


template<triviallyDestructable T>
exchangeSimulator<T>::exchangeSimulator(int port, int batchSize)
         : allocator(), port{port}, batchSize{batchSize}, sendBuffer{allocator.allocate(batchSize)}, msgs{std::make_unique<struct mmsghdr[]>(batchSize)}, iovecs{std::make_unique<struct iovec[]>(batchSize)} {
    
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    memset(&addr, 0, sizeof(addr)); 

    // Filling server information 
    addr.sin_family    = AF_INET; // IPv4 
    addr.sin_addr.s_addr = INADDR_ANY; 
    addr.sin_port = htons(port); 

    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&addr,  
            sizeof(addr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    for (size_t i{0}; i < batchSize; ++i) {
        iovecs[i].iov_base = &sendBuffer[i];
        iovecs[i].iov_len = sizeof(T);
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }

}

// concept that G must have void generator(T&)

template<triviallyDestructable T>
template<isGenerator<T> G>
void exchangeSimulator<T>::run(std::atomic<bool>& terminateFlag, const struct sockaddr_in* dst, G& generator){
        /*
       ssize_t sendto(int socket, const void *message, size_t length,
           int flags, const struct sockaddr *dest_addr,
           socklen_t dest_len);
    */

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(7, &cpuset); // Bind to core 7
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset); // bind to core 7! Should not be interrupted, need this to be our "clock" of sorts

    // connect syscall
   // have to reinterpret cast here because that is how c/linux does polymorphism without classes, top parts of class are still accessible after reinterpret case if shared
    connect(sockfd, reinterpret_cast<const sockaddr*>(dst), sizeof(*dst));

    while(!terminateFlag.load(std::memory_order_relaxed)){ // this is so fast that basically 100% of time is spend blocked by kernel doing I/O.
        // basically between in system, it appears that 300k is just fully on outgoing kernel I/O rather than the incoming processing!

        for(int i{}; i < batchSize; ++i){
            generator.generate(sendBuffer[i]); // pass as reference, in-place construction of type T, do it this way because std::vector makes the object construct its self with .emplace()
        }        
        /*
               int sendmmsg(unsigned int n;
                    int sockfd, struct mmsghdr msgvec[n], unsigned int n,
                    int flags);

        */
        sendmmsg(sockfd, msgs.get(), batchSize, MSG_ZEROCOPY);
    }
}