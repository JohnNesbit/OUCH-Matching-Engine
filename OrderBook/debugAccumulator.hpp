#include <vector>
#include <algorithm>
#include <numeric>
#include "OrderBook.hpp"

class debugAccumulator{

    private:
        long max{};
        long min{std::numeric_limits<int>::max()};
        long counter;

    public:
        void consume([[maybe_unused]] OuchEnterOrder& a){
            //max = std::max(max, a.id);
            //min = std::min(min, a.id);
            ++counter;
        }
        void consume(long& a){
            max = std::max(max, a);
            min = std::min(min, a);
            ++counter;
        }
        int getCounter(){
            return counter;
        }

        long getMax() {
            return max;
        }
        long getMin() {
            return min;
        }
        int getLength() {
            return counter;
        }
        int getMisses(){
            return (getMax() - getMin()) - getLength();
        }
};