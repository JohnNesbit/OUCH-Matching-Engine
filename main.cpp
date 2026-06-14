#include <string>
#include <iostream>
#include <thread>
#include <string_view>
#include "SPSC/queue.hpp"
#include "SPSC/Producer.hpp"
#include "SPSC/Consumer.hpp"
#include "SPSC/SPSC.hpp"
#include "OrderSimulator/exchangeSimulator.hpp"
#include "OrderSimulator/simpleGenerator.hpp"
#include "OrderSimulator/OUCH.hpp"
#include "OrderBook/OrderBook.hpp"
#include "OrderBook/debugAccumulator.hpp"
//#define DEBUGSIM 1

int main(int argc, char* argv[]){

#ifndef DEBUGSIM
    using orderType = OuchEnterOrder;
    using accumulatorType = OrderBook;
    using generatorType = OuchMockGenerator;
#endif
#ifdef DEBUGSIM
    using orderType = long; //long; OuchEnterOrder
    using accumulatorType = debugAccumulator;
    using generatorType = simpleGenerator; //OuchMockGenerator; simpleGenerator
#endif

    int time{1000}; // one second experiment
    int port{8080}, bufferSize{128}, sendSize{128}; // changing the producer batch size requires changing queue.hpp constants
    switch (argc) { // intentionally fallthrough here
        case 4:
            sendSize = std::atoi(argv[3]);        
            [[fallthrough]];
        case 3:
            bufferSize = std::atoi(argv[2]);
            [[fallthrough]];
        case 2:
            port = std::atoi(argv[1]);
    }

    accumulatorType accumulator{};
    SPSC<orderType> exchangeQueue(bufferSize, port);

    // create incoming orders simulator(make terminate flag to exit after experiments!)
    std::atomic<bool> terminateFlag{false};
    exchangeSimulator<orderType> simulator(port+1, sendSize);
    exchangeSimulator<orderType> simulator2(port+2, sendSize);
    
    generatorType generator{};
    std::thread exchangeThread([&simulator, &terminateFlag, &exchangeQueue, &generator](){
        simulator.run<generatorType>(terminateFlag, exchangeQueue.getAddr(), generator, 0);
    });

    //std::this_thread::sleep_for(std::chrono::milliseconds(100)); // get rid of file ownership issue, won't take more than 1ms to read sender namespace, easier than doing some sharing thing
    generatorType generator2{};
    //std::thread exchangeThread2([&simulator2, &terminateFlag, &exchangeQueue, &generator2](){
    //    simulator2.run<generatorType>(terminateFlag, exchangeQueue.getAddr(), generator2, 1);
    //});

    exchangeQueue.run<accumulatorType>(sendSize, accumulator, time);

    // end simulation
    terminateFlag.store(true, std::memory_order_release);
    exchangeThread.join();
    //exchangeThread2.join();

    std::cout << "Total trades sent: " << simulator.getCounter() + simulator2.getCounter() << std::endl;

#ifndef DEBUGSIM
    std::cout << "Checksum on orderbook(0 is correct): " << accumulator.checkFlowValid() << " and " << accumulator.checkSharesValid() << std::endl;
    std::cout << "Total trades processed: " << accumulator.getCounter() << std::endl;

    auto buyBook = accumulator.getBuyHeap();
    auto sellBook = accumulator.getSellHeap();
    std::size_t numOrders{buyBook.size() + sellBook.size()};
    //pqObject p = buyBook.top();
    
    /* prints got to be too much when we hit millions
    while(!buyBook.empty()){
        p = buyBook.top();
        std::cout << "Side: B " << " Price: " << p.price << std::endl; // " Id: " << std::string(p.t.token, 14) << 
        buyBook.pop();
    }

    while(!sellBook.empty()){
        p = sellBook.top();
        std::cout << "Side: S " << " Price: " << p.price  << std::endl; // << " Id: " << std::string(p.t.token, 14)
        sellBook.pop();
    }*/
    
    std::cout << "max book size: " << accumulator.getMaxSize() << std::endl;
    std::unordered_map<std::uint32_t, long long>& flows = accumulator.getFirmFlows();
    std::unordered_map<std::uint32_t, long long>& shares = accumulator.getFirmShares();

    // get volume, largest, and price of share paid by largest
    long long volume{};
    long long mostShares{};
    long long priceOfMostShares{};
    
    for(auto it{shares.begin()}; it != shares.end(); ++it){
        volume += std::abs(it->second);
        if (it->second > mostShares){
            mostShares = it->second;
            std::cout << it->first << " should equal ";
            std::string a{reinterpret_cast<const char*>(&it->first), 3}; 
            std::cout << a  << std::endl;
            priceOfMostShares = std::abs(flows[it->first]);
        }
    }

    std::cout << "Total Firms: " << shares.size() << " Total share volume: " << volume << " Firm with most shares paid on average: " << priceOfMostShares/mostShares << std::endl;
    std::cout << "Total Orders Active: " << numOrders << std::endl;
#endif
#ifdef DEBUGSIM
    
    int misses = accumulator.getMisses() - 128;

    std::cout << "Ended Exchange\n";
    std::cout << "Total misses: " << misses;
    std::cout << "\nMiss rate: " << misses/static_cast<float>(accumulator.getLength());
    std::cout << "\nTotal tranferred: " << accumulator.getLength() << std::endl;
    //std::cout << "We sent: " << generator.getCount() << std::endl;
#endif
    return 0;
}