#pragma once
#include <cstdint>
#include <chrono>


// explainations:
// we pair best offer and buy going down the list at pairtime and execute the order at the midpoint between them
// this creates a unique inscentive AGAINST movement in the stock price since if a trade is aggressive, it will get a poor deal
// resting fair trades will receive large bonuses since trades are executed at a better price than that offer
// basically, since "resting offers" have no distinction from aggressive offers other than where the old spread was
// we are going to execute every trade at the bidder's price which causes a price asymmetry, creating pools of liquidity in the ask direction and a race to the bottom
// the race to the bottom then attracts buyers who contrain their edge loss via their bid
// send ACKs to client on recieving - have msg structs already made
// acknowledge that actual exchanges use TCP + send ACKs, which actually makes sense, but this is basically for fun and faster so more things come up!
// we verify packets are all making it through with a debug accumulator which records packet number with a debug sim and finds no gaps
// this is expected for not using a real interface

// run io_uring, talk to claude about it, write justification with perf trace
// expiriment runs + make graph with claude(graph.py)
// we are in-order from the packet hitting NIC, so we just need to provide latency measures for customers.


// 6 core bare-metal vultr
namespace globalConfigs {
    
    using namespace std::chrono_literals;
    constexpr int OuchMaxSize = 48;
    inline int senderCore2 = -1;// physical core 2
    inline int senderCore = -1; // phyiscal core 3
    inline int producerCore = -1; // physical core 4
    inline int consumerCore = -1;// physical core 5
    inline std::chrono::steady_clock::duration interval = 100ms;
}


namespace MSG_GLOBALS {
    constexpr int MSG_BATCH_SIZE = 2; // for the producer
    constexpr int MSG_MAX_SIZE = 48;//193; // max size of an OUCH message with all flags is 193 bytes
    constexpr double TIMEOUT = 10000; // 1ms timeout
}

#pragma pack(push, 1)

struct OuchEnterOrderO {
    char type;             
    char token[14];
    char side;             
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint32_t tif;
    std::uint32_t firm;
    char display;
    char capacity;
    char iso;
    uint32_t min_qty;
    char cross_type;
};

struct OuchCancelOrderO {
    char type;             
    char token[14];
    uint32_t shares;
};

struct OuchReplaceOrderO {
    char type;             
    char existing_token[14];
    char replacement_token[14];
    uint32_t shares;
    uint32_t price;
    uint32_t tif;
    char display;
    char iso;
    uint32_t min_qty;
};
#pragma pack(pop)
