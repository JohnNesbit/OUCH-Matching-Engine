#pragma once
#include <list>
#include <limits>
#include <optional>
#include <string_view>
#include <atomic>
#include <map>
#include <unordered_map>
#include <queue>
#include "config.hpp"


struct OuchOrderWrapper {
    OuchEnterOrderO  e;
    long id;
};

struct token {
    char token[14]; //unit 16 [7]?
};

struct pqObject{
    pqObject(int price, const token& t): t{t}, price{price} {}
    token t;
    int price;
};

template<>
struct std::hash<token> {
    size_t operator()(const token& t) const {
        return std::hash<std::string_view>{}({t.token, 14});
    }
};

inline bool operator==(const token& a, const token& b) {
    return std::memcmp(a.token, b.token, 14) == 0;
}

inline auto maxHeapFunctor = [](const pqObject& a, const pqObject& b){ return a.price < b.price; };
inline auto minHeapFunctor = [](const pqObject& a, const pqObject& b){ return a.price > b.price; };

class OrderBook {
    public:
        OrderBook();

        void consume(const OuchEnterOrder&); 
        void updateMaxMin(int, int);
        void doTrade(OuchOrderWrapper& buyOrder, OuchOrderWrapper& sellOrder);
        long long checkFlowValid();
        long long checkSharesValid();
        std::unordered_map<std::uint32_t, long long>& getFirmShares(){return firmShares;}
        std::unordered_map<std::uint32_t, long long>& getFirmFlows(){return firmFlows;}
        long getCounter(){return OrderBook::counter;}
        auto& getBuyHeap(){return buyHeap;}
        auto& getSellHeap(){return sellHeap;}
        std::size_t getMaxSize(){return maxSize;}
        ~OrderBook(){
        }


    private:

        int currentMinSellPrice{std::numeric_limits<int>::max()};
        int currentMaxBuyPrice{};
        std::size_t maxSize{};

        // test if it is worth it to map an int or a pointer to the token to accelerate the prefetching/ops on this
        
        std::priority_queue<pqObject, std::vector<pqObject>, 
                    decltype(maxHeapFunctor)> buyHeap; // largest first
        std::priority_queue<pqObject, std::vector<pqObject>,  
                    decltype(minHeapFunctor)> sellHeap;

        std::unordered_map<std::uint32_t, long long> firmShares; // hold shares in each company
        std::unordered_map<std::uint32_t, long long> firmFlows; // hold how much owed/owe
        // hold company inflow and outflow

        // O(1) cancellation and replacement access
        // could use std:ref here, but unorderded maps dont delete on erasure so this is fine as long as we remove from here when ever we pop()
        std::unordered_map<token, OuchOrderWrapper> tokenMap; // just keep a map of token to order so that the daemon can deal with them when executing trades
        // lower hot-path overhead for the consumer threads!

        long counter{0};


};