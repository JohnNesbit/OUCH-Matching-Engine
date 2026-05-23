#include <sys/types.h>
#include <arpa/inet.h> 
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include "Producer.hpp"
#include "SPSC.hpp"


// how are we doing this batched SPSC?
// we add to ring buffer in batches to minimize kernel mode switch overhead, fill in min(batch size, space left) chunks!

// we dont want somethign to read while we write so lets set a "read boundary" and then we write
// dont want to overwrite while we read(could happen with looping around) so need "write boundary"

// do this with atomics basically, don't actually need locks since only one thread is writing each, only need fences to make sure we release memory when its okay to

// so basically, we check lock status of things to find the boundary and then fill in up to that!
// we have two atomics then: the tail and the head


// structure: should probably have a "SPSC queue" class which owns the buffer, producer and consumer should not own the buffer
// that structure makes the atomics

Producer::Producer (int port, int bufferSize)
                         : port{port}, bufferSize{bufferSize} {
            
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

    timeout.tv_sec = MSG_GLOBALS::TIMEOUT;
    timeout.tv_nsec = 0;

    // zero these
    std::memset(iovecs, 0, sizeof(iovec)*MSG_GLOBALS::MSG_BATCH_SIZE);
    std::memset(msgs, 0, sizeof(mmsghdr)*MSG_GLOBALS::MSG_BATCH_SIZE);

    // initalize msg
    for (size_t i{0}; i < MSG_GLOBALS::MSG_BATCH_SIZE; ++i) {
        //iovecs[i].iov_base = q.buffer[i].message; // done later now, unneccesary to do this because overridden later
        iovecs[i].iov_len = MSG_GLOBALS::MSG_MAX_SIZE;
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }
}