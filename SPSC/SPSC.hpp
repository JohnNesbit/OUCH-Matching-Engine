#pragma once
#include <arpa/inet.h> 
#include <csignal>
#include <iostream>
#include <thread>
#include "Producer.hpp"
#include "Consumer.hpp"
#include "queue.hpp"

template<class T>
class SPSC {
    public:
        SPSC(int bufferSize, int port) : q(bufferSize), prod(port, bufferSize) {}

        template<class A>
        requires isConsumerOf<T, A>
        void run(int, A&, int);
        const struct sockaddr_in* getAddr(){return prod.getAddr();}
        void resetObject(){
            q.clear();
        }

    private:
        queue<T> q;
        Producer prod; // prod''s only state is socket
        std::atomic<bool> terminateFlag{false};
};

template<class T>
template<class A>
requires isConsumerOf<T, A>
void SPSC<T>::run(int consumeBatchSize, A& accumulator, int time){

    std::cout << "Starting...\n";

    //terminateFlag.store(false, std::memory_order_relaxed); // this necesarily syncs on std::thread due to fencing

    std::thread producerThread(&Producer::PollSocket<T>, &prod, std::ref(terminateFlag), std::ref(q));
    std::thread consumerThread(&ConsumeQueue<T, A>, std::ref(terminateFlag), consumeBatchSize, std::ref(q), std::ref(accumulator));

    //while (!terminateFlag.load(td::memory_order_relaxed)){} can uset this for std::signal(SIGINT, ) thing

    std::this_thread::sleep_for(std::chrono::milliseconds(time));
    terminateFlag.store(true, std::memory_order_release);

    producerThread.join();
    consumerThread.join();
}

