#pragma once
#include <list>
#include <limits>
#include <optional>
#include <string_view>
#include <atomic>
#include <map>
#include <unordered_map>
#include "OrderSimulator/OUCH.hpp"

// we build this object to assume multiple threads are going to be calling member functions at the same time
// this will contain both some bookkeeping daemon for executing orders
// need O(1) for cancellation and replacing, maybe a hashmap? Test B-tree because of cache locality!

// consume function will be a small part, executed by the consumer while another thread takes care of actual bookkeeping
// basically consume needs to be as inexpensive as possible!

// So.. priorities:
// Consume should be O(1) or O(log n) on orderbook size

// only simulating one ticker, multiple would just be adding one access to a map between ticker and its orderbook object

// actual order list should be sorted on price, since only 4 decimal places, just index directly into array based on price. In reality would need to do multi-level access(probably one indirection?)
// in this, only 4096 possibilities for price so we just allocate an array, store all of the orders 

// IMPORTANT: THIS OBJECT IS NOT ZERO-COPY, DUE TO ASSUMING THAT THE CALLING CONSUMER CANNOT STD::MOVE ITS OBJECTS DUE TO NO ALLOCATIONS IN HOT PATH, THE CONSUMER PUTS ALLOCATIONS
// ONTO THE CONSUMER DAEMON
// CONSUMER DAEMON WILL CHECK WITHIN A THRESHOLD OF EXPANSION AND PRE-RESERVE SPACE IN STD::VECTORS TO AVOID ALLOCATIONS WITHIN HOT PATH

// actualyl screw this, critical section is so large that it only makes sense to spawn off the actual update/cancel and firm holdings hashmap accesses
// easiest way to do this is copying into a ring buffer with a fullfillment daemon
// this is roughly one cache access?

// would need to test both approaches, for now lets just have the consumer do it all


namespace OrderBookConstants
{
    constexpr int PriceRange = 8196;
    constexpr int openingCrossPrice = 1500000; // four decimals
    constexpr int tokenLength = 14;
}

class OrderBook {
    public:
        OrderBook();

        // handle placing O in orderbook, U and X cia orderMap
        // will need to update smallest buyside and largest sellside atomically wit daemonProcess!
        // for cancellation, just set side to some out of bounds value, daemon can check!
        void consume(const OuchEnterOrder&); 
        void daemonProcess(); // this just iterates between smallest buyside and largest sellside
        void updateMaxMin(int, int);
        void doTrade(OuchEnterOrder& buyOrder, OuchEnterOrder& sellOrder);
        std::ptrdiff_t convertPriceToIndex(int);
        int checkValid();
        long long getProfit(){return profit;}
        long getCounter(){return OrderBook::counter;}
        std::list<OuchEnterOrder>* getBook(){return book;};

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
        std::list<OuchEnterOrder> book[OrderBookConstants::PriceRange]; // hold all the orders for that level of granularity! I.e. for that 4th place
        // need to mantain atomics for orderbook so we don't get weird stuff
        // taring IS a concern because our struct is larger than the cache line size :(
        // less than cache line size
        // 4096 on each side so 8192

        // only accessed by daemon but O(1)
        // string view to char[4]
        std::unordered_map<std::string_view, long> firmHoldings; // hold shares in each company

        long long profit{};

        // O(1) cancellation and replacement access
        // could use std:ref here, but unorderded maps dont delete on erasure so this is fine as long as we remove from here when ever we pop()
        std::unordered_map<std::string_view, OuchEnterOrder*> tokenMap; // just keep a map of token to order so that the daemon can deal with them when executing trades
        // lower hot-path overhead for the consumer threads!

        long counter{};


};