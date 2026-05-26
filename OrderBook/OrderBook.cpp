#include <algorithm>
#include <numeric>
#include <cstddef>
#include <string>
#include <cstring>
#include <string_view>
#include <algorithm>
#include <exception>
#include "OrderSimulator/OUCH.hpp"
#include "OrderBook.hpp"


// if we wanted to make this truly zero copy, I could imagine continually shifting the ring buffer pointer the SPSC uses
// the allocated ring buffer is then just pointed to in the order book, meaning we do zero copies of the actual information within user space!
// if kernel-bypassed and DMAed in, we would get true zero-copy from the network
// makes sense only if copying the order into the orderbook's allocated memory is a bottleneck
// probably won't be since we can have multiple consumers actually + I/O is so much slower than this

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

inline std::ptrdiff_t OrderBook::convertPriceToIndex(int price){
    return price - OrderBookConstants::openingCrossPrice;
}

void OrderBook::updateMaxMin(int price, int side){
    if(side == 'S'){
        if (std::min(price, currentMinSellPrice) == price) currentMinSellPrice = price;
    } else if(side == 'B'){
        if (std::max(price, currentMaxBuyPrice) == price) currentMaxBuyPrice = price;
    }
    
}

inline std::uint32_t firmNametoInt(char* firm){
    return *reinterpret_cast<std::uint32_t*>(firm); // this function should get fully optimized out!
}

inline const token& charsToToken(const char* t){
    return *reinterpret_cast<const token*>(t);
}

void OrderBook::doTrade(OuchOrderWrapper& buyOrder, OuchOrderWrapper& sellOrder){
    long price{buyOrder.id >= sellOrder.id ? buyOrder.e.price : sellOrder.e.price};
    if (buyOrder.e.shares >= sellOrder.e.shares){
        buyOrder.e.shares -= sellOrder.e.shares;
        firmFlows[firmNametoInt(sellOrder.e.firm)] += sellOrder.e.shares * price; // should definitely make these std::uint32 but we take almost no perf hit from this right now.
        firmFlows[firmNametoInt(buyOrder.e.firm)] -= sellOrder.e.shares * price; 
        firmShares[firmNametoInt(sellOrder.e.firm)] += sellOrder.e.shares;
        firmShares[firmNametoInt(buyOrder.e.firm)] -= sellOrder.e.shares; 
        sellOrder.e.shares = 0;
    } else{
        sellOrder.e.shares -= buyOrder.e.shares;
        firmFlows[firmNametoInt(sellOrder.e.firm)] += buyOrder.e.shares * price;
        firmFlows[firmNametoInt(buyOrder.e.firm)] -= buyOrder.e.shares * price;
        firmShares[firmNametoInt(sellOrder.e.firm)] += buyOrder.e.shares;
        firmShares[firmNametoInt(buyOrder.e.firm)] -= buyOrder.e.shares;
        buyOrder.e.shares = 0;
    }
}

// NOTE: all of these are limit orders as specified by the OUCH standard
__attribute__((noinline))
void OrderBook::consume(const OuchEnterOrder& order) {
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
                            std::forward_as_tuple(order, counter));
            break;
        }
        case 'U':
        {
            const OuchReplaceOrder& replaceOrder = reinterpret_cast<const OuchReplaceOrder&>(order);
            
            // check that this order to update actually exists
            auto it = tokenMap.find(charsToToken(replaceOrder.existing_token));
            if (it != tokenMap.end()){ 
                OuchEnterOrder& originalOrder{it->second.e};
                if(originalOrder.type == 'X') {break;} // if cancelled, we don't need to replace!

                // copy over replacement fields for easy copy into new element
                originalOrder.shares = replaceOrder.shares;
                originalOrder.price = replaceOrder.price;

                // cancel the original since we cannot remove from a heap
                it->second.e.type = 'X';

                // create a new order for the replacement token and add it to the heap and hashmap
                tokenMap.emplace(std::piecewise_construct,
                                std::forward_as_tuple(charsToToken(replaceOrder.replacement_token)),
                                std::forward_as_tuple(originalOrder, counter));


                if(order.side == 'S'){
                    sellHeap.emplace(replaceOrder.price, charsToToken(replaceOrder.replacement_token));
                } else{
                    buyHeap.emplace(replaceOrder.price, charsToToken(replaceOrder.replacement_token));
                }

                // then copy the token in-place to preserve the original's token for cancellation
                //strncpy(priceListForReplace.back().e.token, replaceOrder.replacement_token, OrderBookConstants::tokenLength);

                //update the tokenMap
                //tokenMap[charsToToken(priceListForReplace.back().e.token)] = &priceListForReplace.back();
            }
            // if order didn't exist, we do nothing
            break;
        }
         case 'X': 
         {
            const OuchCancelOrder& cancelOrder = reinterpret_cast<const OuchCancelOrder&>(order);
            auto it = tokenMap.find(charsToToken(cancelOrder.token));
            if (it != tokenMap.end()){ // if real
                it->second.e.type = 'X'; // cancel
            }
            // Technically we should update continusouly before we cancel, but that is up to details, and.... its easier to write the boring code for this version
            break;
         }
    };

    maxSize = std::max(tokenMap.size(), maxSize);

    // don't cross the book!
    // this is amortize O(1) because we necessarily don't do more than one execution per order(more like .3 per order)
    // just two-pointer up from min sell price and down from max buy price until the book is no longer crossed!

    while(!buyHeap.empty() && !sellHeap.empty() && (buyHeap.top().price >= sellHeap.top().price)){
        //std::lower_bound
        //std::lower_bound(book.begin(), book.end(), i);
        //currentMaxBuyPrice = 
        //currentMinSellPrice = 
        // actually, we need to renew the order whenever it is cancelled or has zero quantity.or isn't on the side we are getting!
        // what this means is that 

        //std::list<OuchOrderWrapper>& buyList =  book[convertPriceToIndex(currentMaxBuyPrice)];// this is the index of the order at that price, not the index of the price in the book
        //std::list<OuchOrderWrapper>& sellList = book[convertPriceToIndex(currentMinSellPrice)];


        // do lower bound with search between currentMaxBuyPrice and 0, do currentMinSellPrice and 4096
        // we can do way better than this
        // instead of iterating here, we can just have an O(log n) time cost for creating a new book price list?
        // or to be honest... we could just maintain a sorted list of the things
        // this would be O(1) popping guarenteed and we would do O(log n) insertions... This would prevent a lot of stuff

        // also actually lets just use std::lower_bound with .empty()

        // because our order list is never more than like 25, 
        /*
        if (buyList.empty()){
            --currentMaxBuyPrice;
            continue;
        }
        if (sellList.empty()){
            ++currentMinSellPrice;
            continue;
        }*/

        // get to max buy side 
        while (!buyHeap.empty() && (tokenMap[buyHeap.top().t].e.type == 'X' || tokenMap[buyHeap.top().t].e.shares == 0)){
            tokenMap.erase(buyHeap.top().t);
            buyHeap.pop();
             // this is scary because if this doesn't happen before BuyList we cooked
        }

        // minimum sell side 
        while (!sellHeap.empty() && (tokenMap[sellHeap.top().t].e.type == 'X' || tokenMap[sellHeap.top().t].e.shares == 0)){
            tokenMap.erase(sellHeap.top().t);
            sellHeap.pop();
             // this is scary because if this doesn't happen before BuyList we cooked
        }

        // do the trade and continue
        if(!buyHeap.empty() && !sellHeap.empty()) {
            doTrade(tokenMap[buyHeap.top().t], tokenMap[sellHeap.top().t]);
        }
    }
}
        
        


// oringinal idea was to have consumer update the orderbook and the daemon do the trading, but I realized the critical period was like the entirety of the daemon and consumer
// so really doesn't make sense sharing this load across threads?

// basically we would just be getting rid of the hashmap accesses....
/*
void OrderBook::updateMaxMin(int price){
    if (std::max(price, currentMaxBuyPrice.load()) == price) currentMaxBuyPrice.store(price); // should we let this speculatively execute?
    if (std::min(price, currentMinSellPrice.load()) == price) currentMinSellPrice.store(price);
}



void OrderBook::daemonProcess(std::atomic<bool>& terminateFlag){

    while(!terminateFlag.load(std::memory_order_relaxed)){
        // we want to make sure we always execute correctly so we have to load the atomic for every trade unfortunately...
        // basically each loop has to only be one trade and we re-sync with the consuming thread to make sure we dont trade out of order..
        // this introduces massive overhead on both threads since our daemon and consumer are basically lock-stepped
        // 
        currentMaxBuyPrice.load();
        currentMinSellPrice.load();
    }


    
}
    */