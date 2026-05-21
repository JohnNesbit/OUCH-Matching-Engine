
#include "OrderSimulator/OUCH.hpp"

class OrderBook {
    public:
        void consume(const Ouch5EnterOrder&);
        int counter;
};