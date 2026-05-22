
class simpleGenerator {
    long count{};
    
    public:
        simpleGenerator() : count{} {}

        void generate(long* a) {
            *a = ++count;
        }

        long getCount(){
            return count;
        }
};