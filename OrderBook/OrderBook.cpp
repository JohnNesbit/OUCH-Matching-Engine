#include "OrderSimulator/OUCH.hpp"
#include "OrderBook.hpp"

void OrderBook::consume(const Ouch5EnterOrder& order) {
    ++counter;
}