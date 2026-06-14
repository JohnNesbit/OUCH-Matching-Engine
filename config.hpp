#pragma once
#include <cstdint>


/*
namespace globalConfigs {
    constexpr int OuchMaxSize = 48;
    constexpr int senderCore = 3; // phyiscal core 6
    constexpr int producerCore = 1; // physical core 7
    constexpr int consumerCore = 2;// physical core 5
    // RPS on cores 0,1,2,3,4 which is through logical core 9
}


namespace globalConfigs {
    constexpr int OuchMaxSize = 48;
    constexpr int senderCore = 13; // phyiscal core 6
    constexpr int producerCore = 15; // physical core 7
    constexpr int consumerCore = 11;// physical core 5
    // RPS on cores 0,1,2,3,4 which is through logical core 9
}
*/

// 6 core bare-metal vultr
namespace globalConfigs {
    constexpr int OuchMaxSize = 48;
    constexpr int senderCore2 = 2;// physical core 2
    constexpr int senderCore = 3; // phyiscal core 3
    constexpr int producerCore = 4; // physical core 4
    constexpr int consumerCore = 5;// physical core 5
}
// RPS on cores 0,1 which is logicals 0,2,1,6,7,8
// we want to isolate cores 3,4, 5 which is isolcpu=3,4,5,9,10,11
//

// /etc/default/grub
//GRUB_CMDLINE_LINUX_DEFAULT="isolcpus=3,4,5,9,10,11 rcu_nocbs=3,4,5,9,10,11 nohz_full=3,4,5,9,10,11 irqaffinity=0,2,1,6,7,8"
//Sudo reboot


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
