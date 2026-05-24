#pragma once
#include <list>
#include <limits>
#include <optional>
#include <string_view>
#include <atomic>
#include <map>
#include <unordered_map>
#include "OrderSimulator/OUCH.hpp"

// IMPORTANT: THIS OBJECT IS NOT ZERO-COPY, DUE TO ASSUMING THAT THE CALLING CONSUMER CANNOT STD::MOVE ITS OBJECTS DUE TO NO ALLOCATIONS IN HOT PATH, THE CONSUMER PUTS ALLOCATIONS
// ONTO THE CONSUMER DAEMON
// CONSUMER DAEMON WILL CHECK WITHIN A THRESHOLD OF EXPANSION AND PRE-RESERVE SPACE IN STD::VECTORS TO AVOID ALLOCATIONS WITHIN HOT PATH

// critical section is so large that it only makes sense to spawn off the actual update/cancel and firm holdings hashmap accesses
// easiest way to do this is copying into a ring buffer with a fullfillment daemon
// this is roughly one cache access?

// would need to test both approaches, for now lets just have the consumer do it all


namespace OrderBookConstants
{
    constexpr int PriceRange = 4096;
    constexpr int openingCrossPrice = 1500000; // four decimals
    constexpr int tokenLength = 14;
}

struct OuchOrderWrapper {
    OuchEnterOrder e;
    long id;
};

class OrderBook {
    public:
        OrderBook();

        // handle placing O in orderbook, U and X cia orderMap
        // will need to update smallest buyside and largest sellside atomically wit daemonProcess!
        // for cancellation, just set side to some out of bounds value, daemon can check!
        void consume(const OuchEnterOrder&); 
        void daemonProcess(); // this just iterates between smallest buyside and largest sellside
        void updateMaxMin(int, int);
        void doTrade(OuchOrderWrapper& buyOrder, OuchOrderWrapper& sellOrder);
        std::ptrdiff_t convertPriceToIndex(int);
        long long checkFlowValid();
        long long checkSharesValid();
        std::unordered_map<std::string, long long>& getFirmShares(){return firmShares;}
        std::unordered_map<std::string, long long>& getFirmFlows(){return firmFlows;}
        long getCounter(){return OrderBook::counter;}
        std::list<OuchOrderWrapper>* getBook(){return book;};

        ~OrderBook(){
            

        }


    private:
        // O(1) add to book amortized using vector arraylist implementation
        //std::atomic<int> currentMinSellPrice{std::numeric_limits<int>::max()};
        //std::atomic<int> currentMaxBuyPrice{};

        // use std::ptrdiff_t here because we are directly accessing containers with this so we want implicit std::size_t casts while also being signed?
        // no, these are only 1,500,000 at most, fine to static cast later.
        int currentMinSellPrice{std::numeric_limits<int>::max()};
        int currentMaxBuyPrice{};

        // std::vector is the incorrect took here. we need fast tail and head accesses, but don't care about random access
        // we need dynamic memory allocation
        //
        std::list<OuchOrderWrapper> book[OrderBookConstants::PriceRange]; // hold all the orders for that level of granularity! I.e. for that 4th place
        // need to mantain atomics for orderbook so we don't get weird stuff
        // taring IS a concern because our struct is larger than the cache line size :(
        // less than cache line size
        // 4096 size

        // string view to char[4]
        std::unordered_map<std::string, long long> firmShares; // hold shares in each company
        std::unordered_map<std::string, long long> firmFlows; // hold how much owed/owe
        // hold company inflow and outflow

        // O(1) cancellation and replacement access
        // could use std:ref here, but unorderded maps dont delete on erasure so this is fine as long as we remove from here when ever we pop()
        std::unordered_map<std::string, OuchOrderWrapper*> tokenMap; // just keep a map of token to order so that the daemon can deal with them when executing trades
        // lower hot-path overhead for the consumer threads!

        long counter{0};


};