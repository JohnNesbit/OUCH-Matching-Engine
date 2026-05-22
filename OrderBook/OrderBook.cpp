#include <algorithm>
#include <cstddef>
#include <cstring>
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

inline std::ptrdiff_t OrderBook::convertPriceToIndex(int price){
    return price - OrderBookConstants::openingCrossPrice + OrderBookConstants::PriceRange/2;
}

void OrderBook::updateMaxMin(int price){
    if (std::max(price, currentMaxBuyPrice) == price) currentMaxBuyPrice = price; // should we let this speculatively execute on this control dependency of daemon?
    if (std::min(price, currentMinSellPrice) == price) currentMinSellPrice = price;
}



void doTrade(){}

int OrderBook::getOrder(std::list<OuchEnterOrder>& orderList){

    // flag for if already checked

    // this is literally just prepping the orderbook to be used easily. This is going to get called every trade so we should grab a flag for having been getOrdered
    // check cancellation and update for buyer
    // need to have a  loop on the token existing in the update map because we might have multiple chained reassignments!
    while(!orderList.empty()){

        auto it = cancellationUpdateMap.find(orderList.front().token); // to be in while's scope
        if (it != cancellationUpdateMap.end()){ // if there is a change to the order

            if (it->second.type == 'X') {// fisrt is type, skip this if cancelled
                orderList.pop_front();
            } 
            

            if (it->second.type == 'U') { // updated, do the replacement and re-run to catch if there have been multiple chained replacements
                
                // loop here to find the true last update. Some overhead to ensure correctness for multiple updates.
                auto temp = cancellationUpdateMap.find(it->second.replacement_token);
                while(temp != cancellationUpdateMap.end() && temp->second.type == 'U'){
                    it = temp;
                    temp = cancellationUpdateMap.find(it->second.replacement_token);
                }

                // it is the end of the replacements chain, check if it is a cancel
                if (temp->second.type == 'X'){
                    orderList.pop_front();
                }

                // copy over replacement token for future use
                strncpy(orderList.front().token, it->second.replacement_token, OrderBookConstants::tokenLength); // horrible magic but this is the standard so its fine.
                orderList.front().shares = it->second.shares;
                    
                 // this is our true order!
                // problem: what happens if we get replaced again? We need to write the replacement value!
                // O(1) so not the worst I guess...
            }
        }
    }
}
/*
    char type;             
    char existing_token[14];
    char replacement_token[14];
    uint32_t shares;
    uint32_t price;
*/

// NOTE: all of these are limit orders as specified by the OUCH standard
void OrderBook::consume(const OuchEnterOrder& order) {

    switch (order.type){
        case 'O':
            updateMaxMin(order.price);
            book[convertPriceToIndex(order.price)].push_back(order);
            tokenMap[order.token] = book[convertPriceToIndex(order.price)].back();
            return;
        case 'U':
            const OuchReplaceOrder& replaceOrder = reinterpret_cast<const OuchReplaceOrder&>(order);
            
            // check that this order to update actually exists
            auto it = tokenMap.find(replaceOrder.existing_token);
            if (it != tokenMap.end()){ 

                if(it->second.type == 'X') {return;} // if cancelled, we don't need to replace!

                // create new order *updated*
                updateMaxMin(replaceOrder.price);

                // we are copying over the original order and then changing the fields which is dumb. Original is modified but it is cancelled anyway
                it->second.shares = replaceOrder.shares;
                it->second.price = replaceOrder.price;
                strncpy(it->second.token, replaceOrder.replacement_token, OrderBookConstants::tokenLength);
                book[convertPriceToIndex(replaceOrder.price)].push_back(it->second); 
                tokenMap[replaceOrder.replacement_token] = book[convertPriceToIndex(replaceOrder.price)].back();
                
                // cancel the original
                it->second.type = 'X';
            }
            // if order didn't exist, we do nothing
            return;
         case 'X': 
            const OuchCancelOrder& cancelOrder = reinterpret_cast<const OuchCancelOrder&>(order);
            auto it = tokenMap.find(cancelOrder.token);
            if (it != tokenMap.end()){ // if real
                it->second.type = 'X'; // cancel
            }
            // Technically we should update continusouly before we cancel, but that is up to details, and.... its easier to write the boring code for this version
            return;
    };
    ++counter;

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
        // okay so make this just updateOrCancelOrder. If it is cancelled, we should genuinely just return a std::optional<> with error flag?\
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
        while (buyList.front().type == 'X' || buyList.front().side != 'B' || buyList.front().shares == 0){
            buyList.pop_front();
        }

        // minimum sell side 
        while (sellList.front().type == 'X' || sellList.front().side != 'B' || sellList.front().shares == 0){
            sellList.pop_front();
        }

        // do the trade and continue
        doTrade(buyList, sellList);
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