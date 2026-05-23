#include <vector>
#include <algorithm>

class debugAccumulator{

    private:
        std::vector<long> receivedList;

    public:
        void consume(long a){
            receivedList.push_back(a);
        }

        int getCounter(){
            return getLength();
        }

        int getMax() {
            return *std::max_element(receivedList.begin(), receivedList.end());
        }
        int getMin() {
            return *std::min_element(receivedList.begin(), receivedList.end());
        }
        int getLength() {
            return receivedList.size();
        }
        int getMisses(){
            return (getMax() - getMin()) - getLength();
        }
};