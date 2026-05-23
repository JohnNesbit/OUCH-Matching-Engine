#include <algorithm>
#include <numeric>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <exception>
#include "OrderSimulator/OUCH.hpp"
#include "OrderBook.hpp"


// what happens if we need to update price?
// genuinely cant oof

// could restructure tbh
// genuniely just insert replaces as if they were actual orders
// we then just cancel the old order?

// implemetation detail

// THE FOLLOWING IS A DISCUSSION OF PERFORMANCE, HOWEVER, IT WAS DISCARDED DUE TO EASE OF IMPLEMENTATION AND THE RELATIVE SPEED OF I/O

// if we wanted to make this truly zero copy, I could imagine continually shifting the ring buffer pointer the SPSC uses
// the allocated ring buffer is then just pointed to in the order book, meaning we do zero copies of the actual information within user space!
// if kernel-bypassed and DMAed in, we would get true zero-copy from the network
// makes sense only if copying the order into the orderbook's allocated memory is a bottleneck
// probably won't be since we can have multiple consumers actually + I/O is so much slower than this

OrderBook::OrderBook(){} // default construction of std::map and std::vector is good!

long long OrderBook::checkValid(){
    return std::accumulate(firmHoldings.begin(), firmHoldings.end(), 0, [](long long sum, auto& a){
        return sum + a.second;

    });
}

inline std::ptrdiff_t OrderBook::convertPriceToIndex(int price){
    int p = price - OrderBookConstants::openingCrossPrice + OrderBookConstants::PriceRange/2;
    if (p > 0 && p < OrderBookConstants::PriceRange) return p;
    else{
        throw std::exception();
    }
}

void OrderBook::updateMaxMin(int price, int side){
    if(side == 'S'){
        if (std::min(price, currentMinSellPrice) == price) currentMinSellPrice = price;
    } else if(side == 'B'){
        if (std::max(price, currentMaxBuyPrice) == price) currentMaxBuyPrice = price; // should we let this speculatively execute on this control dependency of daemon?
    }
    
}

void OrderBook::doTrade(OuchEnterOrder& buyOrder, OuchEnterOrder& sellOrder){
    if (buyOrder.shares >= sellOrder.shares){
        buyOrder.shares -= sellOrder.shares;
        //profit += sellOrder.shares * (buyOrder.price - static_cast<long>(sellOrder.price));
        firmHoldings[std::string_view(sellOrder.firm, 4)] += sellOrder.shares * static_cast<long>(buyOrder.price); // need to change to resting price!
        firmHoldings[std::string_view(buyOrder.firm, 4)] -= sellOrder.shares * static_cast<long>(buyOrder.price); 
        sellOrder.shares = 0;
    } else{
        sellOrder.shares -= buyOrder.shares;
        //profit += buyOrder.shares * (buyOrder.price - static_cast<long>(sellOrder.price));
        firmHoldings[std::string_view(sellOrder.firm, 4)] += buyOrder.shares * static_cast<long>(buyOrder.price);
        firmHoldings[std::string_view(buyOrder.firm, 4)] -= buyOrder.shares * static_cast<long>(buyOrder.price);
        buyOrder.shares = 0;
    }
}

// NOTE: all of these are limit orders as specified by the OUCH standard
void OrderBook::consume(const OuchEnterOrder& order) {
    ++OrderBook::counter;
    switch (order.type){
        case 'O':
        {
            updateMaxMin(order.price, order.side);
            book[convertPriceToIndex(order.price)].push_back(order);
            std::string_view token_view(book[convertPriceToIndex(order.price)].back().token, OrderBookConstants::tokenLength);
            tokenMap[token_view] = &book[convertPriceToIndex(order.price)].back();
            break;
        }
        case 'U':
        {
            const OuchReplaceOrder& replaceOrder = reinterpret_cast<const OuchReplaceOrder&>(order);
            
            // check that this order to update actually exists
            auto it = tokenMap.find(replaceOrder.existing_token);
            if (it != tokenMap.end()){ 

                if(it->second->type == 'X') {return;} // if cancelled, we don't need to replace!

                // create new order *updated*
                updateMaxMin(replaceOrder.price, it->second->side);

                // we are copying over the original order and then changing the fields which is dumb. Original is modified but it is cancelled anyway
                it->second->shares = replaceOrder.shares;
                it->second->price = replaceOrder.price;
                // preserve the token for when cancelled.
                book[convertPriceToIndex(replaceOrder.price)].push_back(*it->second); 
                strncpy(book[convertPriceToIndex(replaceOrder.price)].back().token, replaceOrder.replacement_token, OrderBookConstants::tokenLength);
                std::string_view token_view(book[convertPriceToIndex(replaceOrder.price)].back().token, OrderBookConstants::tokenLength);
                tokenMap[token_view] = &book[convertPriceToIndex(replaceOrder.price)].back();
                
                // cancel the original
                it->second->type = 'X';
            }
            // if order didn't exist, we do nothing
            break;
        }
         case 'X': 
         {
            const OuchCancelOrder& cancelOrder = reinterpret_cast<const OuchCancelOrder&>(order);
            auto it = tokenMap.find(cancelOrder.token);
            if (it != tokenMap.end()){ // if real

                it->second->type = 'X'; // cancel
            }
            // Technically we should update continusouly before we cancel, but that is up to details, and.... its easier to write the boring code for this version
            break;
         }
    };

    // don't cross the book!
    // this is amortize O(1) because we necessarily don't do more than one execution per order(more like .3 per order)
    // just two-pointer up from min sell price and down from max buy price until the book is no longer crossed!

    while(currentMaxBuyPrice >= currentMinSellPrice){
        
        // OLD COMMENTS
        // we need to zero out the cancelationUpdateMap entries as we find them
        // we need to find the last update/cancel entry, then we need to hold this as our current trading object
        // we loop on sell side until this is fullfilled and check again
        // sell side also should have its object persist until it needs to regenerate

        // lets split this massive horrible thing into some functions

        //updateOrCancelOrder() - handels the updating logic and returns void, it will put the correct values into the order object in-place
        // because we are single threaded, we don't need to worry about shifting references withing std::vector so we can act like its actual memory that we own
        // overhead... is what it is... this allows us to pickup where we left off and be correct
        // both buy and sell side can easily use this function

        // getOrder() - this function gets the next order up for sell/buy side once the last one was exhausted?
        // maybe this is literally just the updateOrCancelOrder() function since we are updating the index and the price here manually!
        // okay so make this just updateOrCancelOrder. If it is cancelled, we should genuinely just return a std::optional<> with error flag?
        // actually... we are updating this in-place so we don't need ot have indecies at all! Just check list emptiness and call getOrder()
        // can just do -1 if fails!

        // so, our structure is basically loop on book being crossed, within that, loop on the order of orders within the buy/sell queue

        // we get our buy order - if we have an unfilled order from before, keep it. Do this via just updating the quantity inplace in the vector!
        // we get our sell order
        // do as many trades as possible on each - doTrade() with references
        // failure mode is that one is empty, let loop continue to reset
        // do Trade needs to adjust the buyList and sellList quantity fields, on the zeroed one, it should pop the value

        // getOrder only fails if the current list is empty, if so, we should iterate to the next one!
        // END OLD COMMENTS
        
        // actually, we need to renew the order whenever it is cancelled or has zero quantity.or isn't on the side we are getting!
        // what this means is that 

        std::list<OuchEnterOrder>& buyList =  book[convertPriceToIndex(currentMaxBuyPrice)];// this is the index of the order at that price, not the index of the price in the book
        std::list<OuchEnterOrder>& sellList = book[convertPriceToIndex(currentMinSellPrice)];

        if (buyList.empty()){
            --currentMaxBuyPrice;
            continue;
        }
        if (sellList.empty()){
            ++currentMinSellPrice;
            continue;
        }

        // get to max buy side 
        while (!buyList.empty() && (buyList.front().type == 'X' || buyList.front().side != 'B' || buyList.front().shares == 0)){
            tokenMap.erase(buyList.front().token);
            buyList.pop_front();
        }

        // minimum sell side 
        while (!sellList.empty() && (sellList.front().type == 'X' || sellList.front().side != 'S' || sellList.front().shares == 0)){
            tokenMap.erase(sellList.front().token);
            sellList.pop_front();
        }

        // do the trade and continue
        if(!buyList.empty() && !sellList.empty()) {
            doTrade(buyList.front(), sellList.front());
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