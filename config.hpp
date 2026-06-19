#pragma once
#include <cstdint>
#include <chrono>

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
