#pragma once
#include <atomic>
#include <memory>

inline int min(int a, int b) {return (a < b) ? a : b;}

template<class T, class A>
concept isConsumerOf = requires(T consumed, A consumer) {
    {consumer.consume(consumed)} -> std::same_as<void>;
};

namespace MSG_GLOBALS {
    constexpr int MSG_BATCH_SIZE = 10;
    constexpr int MSG_MAX_SIZE = 128;//193; // max size of an OUCH message with all flags is 193 bytes
    constexpr double TIMEOUT = 10; // 1ms timeout
}

// we do std::allocator and break RAII so that T does not need to be default constructable!
template <class T>
struct queue{
    int bufferSize;
    std::atomic<int> bufferTailIndex{}; // we always start producing before consuming so its fine... usually
    std::atomic<int> bufferHeadIndex{};
    std::allocator<T> allocator;
    T* buffer;

    queue(int bufferSize) : bufferSize{bufferSize}, allocator{}, buffer{allocator.allocate(bufferSize)} {}

    ~queue(){
        allocator.deallocate(buffer, bufferSize);
    }

    void clear(){
        bufferTailIndex.store(0);
        bufferHeadIndex.store(0);
    }

    queue& operator=(queue&) = delete;
    queue& operator=(queue&&) = delete;
    queue(queue&) = delete;
    queue(queue&&) = delete;
};