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
    constexpr int experimentLoops{10};
    bool experimentFlag{false};
    int port{8080}, bufferSize{128}, sendSize{128};
    switch (argc) { // intentionally fallthrough here
        case 5:
            experimentFlag = static_cast<bool>(std::atoi(argv[4]));
            [[fallthrough]];
        case 4:
            sendSize = std::atoi(argv[3]);        
            [[fallthrough]];
        case 3:
            bufferSize = std::atoi(argv[2]);
            [[fallthrough]];
        case 2:
            port = std::atoi(argv[1]);
    }

    if (experimentFlag){

        // create incoming orders simulator(make terminate flag to exit after experiments!)
        std::atomic<bool> terminateFlag{false};
        exchangeSimulator<orderType> simulator(port+1, sendSize);
        SPSC<orderType> exchangeQueue(bufferSize, port);

        // start generator
        generatorType generator{};

        int accumulatedCount[experimentLoops];

        for (int i{1}; i < experimentLoops; ++i){
            // initalize orderbook for this run
            accumulatorType accumulator{};

            // start simulation
            std::thread exchangeThread([&simulator, &terminateFlag, &exchangeQueue, &generator](){
                simulator.run<generatorType>(terminateFlag, exchangeQueue.getAddr(), generator);
            });


            // run producer and consumer
            exchangeQueue.run<accumulatorType>(sendSize, accumulator, time*i);

            // end simulation
            terminateFlag.store(true, std::memory_order_release);
            exchangeThread.join();
            terminateFlag.store(false, std::memory_order_release);

            // print data
            accumulatedCount[i] = accumulator.getCounter();
            std::cout << "With " << i << " seconds: " << accumulatedCount[i] << " trades" << std::endl;
            std::cout << "Trades sent so far: " << simulator.getCounter() << std::endl;
            
            simulator.clearCounter();
            exchangeQueue.resetObject(); // just clears queue via changing pointers :)
            
        }

        return 0;
    } 

    // create exchange
    accumulatorType accumulator{};
    SPSC<orderType> exchangeQueue(bufferSize, port);

    // create incoming orders simulator(make terminate flag to exit after experiments!)
    std::atomic<bool> terminateFlag{false};
    exchangeSimulator<orderType> simulator(port+1, sendSize);
    
    // start generator
    generatorType generator{};
    std::thread exchangeThread([&simulator, &terminateFlag, &exchangeQueue, &generator](){
        simulator.run<generatorType>(terminateFlag, exchangeQueue.getAddr(), generator);
    });

    // run producer and consumer
    exchangeQueue.run<accumulatorType>(sendSize, accumulator, time);

    // end simulation
    terminateFlag.store(true, std::memory_order_release);
    exchangeThread.join();

    std::cout << "Total trades sent: " << simulator.getCounter() << std::endl;

#ifndef DEBUGSIM
    std::cout << "Checksum on orderbook(0 is correct): " << accumulator.checkFlowValid() << " and " << accumulator.checkSharesValid() << std::endl;
    std::cout << "Total trades processed: " << accumulator.getCounter() << std::endl;
    //std::cout << "Total Profit: " << accumulator.getProfit()/10000 << std::endl;

    auto book = accumulator.getBook();
    int numOrders{};
    for(int i{}; i < OrderBookConstants::PriceRange; ++i){
        numOrders += book[i].size();
        for (auto p : book[i]){
            std::cout << "Type: " << p.e.type << " Side: " << p.e.side << " Price: " << p.e.price << " Id: " << p.id << std::endl;
        }
    }
    
    std::unordered_map<std::string, long long>& flows = accumulator.getFirmFlows();
    std::unordered_map<std::string, long long>& shares = accumulator.getFirmShares();
    // get volume, largest, and price of share paid by largest
    long long volume{};
    long long mostShares{};
    long long priceOfMostShares{};
    
    for(auto it{shares.begin()}; it != shares.end(); ++it){
        volume += std::abs(it->second);
        if (it->second > mostShares){
            mostShares = it->second;
            std::cout << it->first << " should equal ";
            std::string_view a{it->first.substr(0, 4)};
            std::cout << a  << std::endl;
            priceOfMostShares = std::abs(flows[it->first]);
        }
    }

    std::cout << "Total Firms: " << shares.size() << " Total share volume: " << volume << " Firm with most shares paid on average: " << priceOfMostShares/mostShares << std::endl;
    std::cout << "Total Orders Active: " << numOrders << std::endl;
#endif
#ifdef DEBUGSIM
    
    //int misses = accumulator.getMisses() - 128;

    std::cout << "Ended Exchange\n";
    //std::cout << "Total misses: " << misses;
    //std::cout << "\nMiss rate: " << misses/static_cast<float>(accumulator.getLength());
    std::cout << "\nTotal tranferred: " << accumulator.getLength() << std::endl;
    //std::cout << "We sent: " << generator.getCount() << std::endl;
#endif

    // we can sleep this thread until console input by sleeping it via "poll" on the console input fd
    // can have an exit within producer and consumer loops, the producer/consumers exit when that value changes
    // just a reference we pass?

    //std::cout << "Enter any key to stop: "
    //while (!std::cin.peek()){} // while user hasnt hit anything

    return 0;
}