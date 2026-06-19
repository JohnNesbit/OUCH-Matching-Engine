#include <algorithm>
#include <numeric>
#include <cstddef>
#include <chrono>
#include <string>
#include <cstring>
#include <string_view>
#include <algorithm>
#include <exception>
#include "OrderSimulator/OUCH.hpp"
#include "config.hpp"
#include "OrderBook.hpp"


OrderBook::OrderBook(){} // default construction of std::map and std::vector is good!

long long OrderBook::checkSharesValid(){
    return std::accumulate(firmShares.begin(), firmShares.end(), 0, [](long long sum, auto& a){
        return sum + a.second;

    });
}

long long OrderBook::checkFlowValid(){
    return std::accumulate(firmFlows.begin(), firmFlows.end(), 0, [](long long sum, auto& a){
        return sum + a.second;

    });
}

void OrderBook::updateMaxMin(int price, int side){
    if(side == 'S'){
        if (std::min(price, currentMinSellPrice) == price) currentMinSellPrice = price;
    } else if(side == 'B'){
        if (std::max(price, currentMaxBuyPrice) == price) currentMaxBuyPrice = price;
    }
    
}


inline const token& charsToToken(const char* t){
    return *reinterpret_cast<const token*>(t);
}

void OrderBook::doTrade(OuchOrderWrapper& buyOrder, OuchOrderWrapper& sellOrder){
    long price{buyOrder.e.price}; // this has a LOT of implications
    if (buyOrder.e.shares >= sellOrder.e.shares){
        buyOrder.e.shares -= sellOrder.e.shares;
        firmFlows[sellOrder.e.firm] += sellOrder.e.shares * price; // should definitely make these std::uint32 but we take almost no perf hit from this right now.
        firmFlows[buyOrder.e.firm] -= sellOrder.e.shares * price; 
        firmShares[sellOrder.e.firm] += sellOrder.e.shares;
        firmShares[buyOrder.e.firm] -= sellOrder.e.shares; 
        sellOrder.e.shares = 0;
    } else{
        sellOrder.e.shares -= buyOrder.e.shares;
        firmFlows[sellOrder.e.firm] += buyOrder.e.shares * price;
        firmFlows[buyOrder.e.firm] -= buyOrder.e.shares * price;
        firmShares[sellOrder.e.firm] += buyOrder.e.shares;
        firmShares[buyOrder.e.firm] -= buyOrder.e.shares;
        buyOrder.e.shares = 0;
    }
}

// NOTE: all of these are limit orders as specified by the OUCH standard
__attribute__((noinline))
void OrderBook::consume(const OuchEnterOrder& orderUncast, std::chrono::time_point<std::chrono::steady_clock> time) {
    

    maxSize = std::max(tokenMap.size(), maxSize);
    
    if (time > nextClearingTime){ // implicitly assumes more than 1 order per 100ms, this is fine
        while(!buyHeap.empty() && !sellHeap.empty() && (buyHeap.top().price >= sellHeap.top().price)){

            // get to max buy side 
            while (!buyHeap.empty() && (tokenMap[buyHeap.top().t].e.type == 'X' || tokenMap[buyHeap.top().t].e.shares == 0)){
                tokenMap.erase(buyHeap.top().t);
                buyHeap.pop();
            }

            // minimum sell side 
            while (!sellHeap.empty() && (tokenMap[sellHeap.top().t].e.type == 'X' || tokenMap[sellHeap.top().t].e.shares == 0)){
                tokenMap.erase(sellHeap.top().t);
                sellHeap.pop();
                // this is scary because if this doesn't happen before BuyList we cooked
            }

            // do the trades and continue
            if(!buyHeap.empty() && !sellHeap.empty()) {
                doTrade(tokenMap[buyHeap.top().t], tokenMap[sellHeap.top().t]);
            }
        }
        nextClearingTime += globalConfigs::interval;
    }

    const OuchEnterOrderO& order = reinterpret_cast<const OuchEnterOrderO&>(orderUncast);
    ++OrderBook::counter;
    switch (order.type){
        case 'O':
        {
            if(order.side == 'S'){
                sellHeap.emplace(order.price, charsToToken(order.token));
            } else{
                buyHeap.emplace(order.price, charsToToken(order.token));
            }

            tokenMap.emplace(std::piecewise_construct,
                            std::forward_as_tuple(charsToToken(order.token)),
                            std::forward_as_tuple(order, time));
            break;
        }
        case 'U':
        {
            const OuchReplaceOrderO& replaceOrder = reinterpret_cast<const OuchReplaceOrderO&>(order);
            
            // check that this order to update actually exists
            auto it = tokenMap.find(charsToToken(replaceOrder.existing_token));
            if (it != tokenMap.end()){ 
                OuchEnterOrderO& originalOrder{it->second.e};
                if(originalOrder.type == 'X') {break;} // if cancelled, we don't need to replace!

                // copy over replacement fields for easy copy into new element
                originalOrder.shares = replaceOrder.shares;
                originalOrder.price = replaceOrder.price;

                // cancel the original since we cannot remove from a heap
                it->second.e.type = 'X';

                // create a new order for the replacement token and add it to the heap and hashmap
                tokenMap.emplace(std::piecewise_construct,
                                std::forward_as_tuple(charsToToken(replaceOrder.replacement_token)),
                                std::forward_as_tuple(originalOrder, time));


                if(originalOrder.side == 'S'){
                    sellHeap.emplace(replaceOrder.price, charsToToken(replaceOrder.replacement_token));
                } else{
                    buyHeap.emplace(replaceOrder.price, charsToToken(replaceOrder.replacement_token));
                }

            }
            // if order didn't exist, we do nothing
            break;
        }
         case 'X': 
         {
            const OuchCancelOrderO& cancelOrder = reinterpret_cast<const OuchCancelOrderO&>(order);
            auto it = tokenMap.find(charsToToken(cancelOrder.token));
            if (it != tokenMap.end()){ // if real
                it->second.e.type = 'X'; // cancel
            }
            // Technically we should update continusouly before we cancel, but that is up to details, and.... its easier to write the boring code for this version
            break;
         }
    };
}