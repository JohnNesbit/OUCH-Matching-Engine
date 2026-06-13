#pragma once
#include <cstdint>

namespace globalConfigs {
    constexpr int OuchMaxSize = 48;
    constexpr int senderCore = 13; // phyiscal core 6
    constexpr int producerCore = 15; // physical core 7
    constexpr int consumerCore = 11;// physical core 5
    // RPS on cores 0,1,2,3,4 which is through logical core 9
}

namespace MSG_GLOBALS {
    constexpr int MSG_BATCH_SIZE = 512; // for the producer
    constexpr int MSG_MAX_SIZE = 48;//193; // max size of an OUCH message with all flags is 193 bytes
    constexpr double TIMEOUT = 100000; // 1ms timeout
}

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