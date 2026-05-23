#include <string>
#include <iostream>
#include <thread>
#include "SPSC/queue.hpp"
#include "SPSC/Producer.hpp"
#include "SPSC/Consumer.hpp"
#include "SPSC/SPSC.hpp"
#include "OrderSimulator/exchangeSimulator.hpp"
#include "OrderSimulator/simpleGenerator.hpp"
#include "OrderSimulator/OUCH.hpp"
#include "OrderBook/OrderBook.hpp"
#include "OrderBook/debugAccumulator.hpp"

int main(int argc, char* argv[]){

    using orderType = OuchEnterOrder;
    using accumulatorType = OrderBook;
    using generatorType = OuchMockGenerator;

    //using orderType = long;
    //using accumulatorType = debugAccumulator;
    //using generatorType = simpleGenerator;


    int port{8080}, bufferSize{128}, sendSize{128};
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

    // create exchange
    accumulatorType accumulator{};
    SPSC<orderType> exchangeQueue(bufferSize, port);

    // create incoming orders simulator(make terminate flag to exit after experiments!)
    std::atomic<bool> terminateFlag{false};
    exchangeSimulator<orderType> simulator(port+1, sendSize);
        
    generatorType generator{};
    std::thread exchangeThread([&simulator, &terminateFlag, &exchangeQueue, &generator](){
        simulator.run<generatorType>(terminateFlag, exchangeQueue.getAddr(), generator);
    });
    
    
    // run producer and consumer
    exchangeQueue.run<accumulatorType>(10, accumulator);

    //std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    // end simulation
    terminateFlag.store(true, std::memory_order_release);
    exchangeThread.join();


    std::cout << "Checksum on orderbook(0 is correct): " << accumulator.checkValid() + accumulator.getProfit() << std::endl;
    std::cout << "Total trades processed: " << accumulator.getCounter() << std::endl;
    std::cout << "Total Profit: " << accumulator.getProfit()/10000 << std::endl;

    auto book = accumulator.getBook();
    int numOrders;
    for(int i{}; i < OrderBookConstants::PriceRange; ++i){
        numOrders += book[i].size();
        for (auto p : book[i]){
            std::cout << "Type: " << p.type << " Side: " << p.side << " Price: " << p.price << std::endl;
        }
        
    }

    std::cout << "Total Orders Active: " << numOrders << std::endl;

    /*
    int misses = accumulator.getMisses();

    std::cout << "Ended Exchange\n";
    std::cout << "Total misses: " << misses;
    std::cout << "\nMiss rate: " << misses/static_cast<float>(accumulator.getLength());
    std::cout << "\nTotal tranferred: " << accumulator.getLength() << std::endl;
    std::cout << "We sent: " << generator.getCount() << std::endl;
    */
    // we can sleep this thread until console input by sleeping it via "poll" on the console input fd
    // can have an exit within producer and consumer loops, the producer/consumers exit when that value changes
    // just a reference we pass?

    //std::cout << "Enter any key to stop: "
    //while (!std::cin.peek()){} // while user hasnt hit anything

    return 0;
}