#pragma once
#include <arpa/inet.h> 
#include <csignal>
#include <iostream>
#include <thread>
#include "Producer.hpp"
#include "Consumer.hpp"
#include "queue.hpp"


// tail should be on a cell that does not have a value unless it is on the head in which case no adding will be done!
// need to give the type and the accumulator btw

template<class T>
class SPSC {
    public:
        SPSC(int bufferSize, int port) : q(bufferSize), prod(port, bufferSize) {}

        template<class A>
        requires isConsumerOf<T, A>
        void run(int, A&);
        const struct sockaddr_in* getAddr(){return prod.getAddr();}

    private:
        queue<T> q;
        Producer prod;
        std::atomic<bool> terminateFlag{false};
};

template<class T>
template<class A>
requires isConsumerOf<T, A>
void SPSC<T>::run(int consumeBatchSize, A& accumulator){

    std::cout << "Starting...\n";

    std::thread producerThread(&Producer::PollSocket<T>, &prod, std::ref(terminateFlag), std::ref(q));
    std::thread consumerThread(&ConsumeQueue<T, A>, std::ref(terminateFlag), consumeBatchSize, std::ref(q), std::ref(accumulator));

    //while (!terminateFlag.load(td::memory_order_relaxed)){} can uset this for std::signal(SIGINT, ) thing

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    terminateFlag.store(true, std::memory_order_acquire);

    producerThread.join();
    consumerThread.join();

    // can do stuff and look at what happens after everything exits!
}

