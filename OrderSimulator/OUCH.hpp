#pragma once

// this file was generated partially or wholly by an LLM helper

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <arpa/inet.h> // For htonl

// OUCH 5.0 specifications require tight packing
#pragma pack(push, 1)
struct Ouch5EnterOrder {
    char     messageType;       // 'O'
    char     orderToken[14];    // Alphanumeric
    char     buySell;           // 'B', 'S', 'T', 'E'
    uint32_t shares;            // Binary Integer
    char     stock[8];          // Alphanumeric
    uint32_t price;             // Binary Integer (Price * 10000)
    uint32_t timeInForce;       // Binary Integer (Seconds)
    char     firm[4];           // Alphanumeric
    char     display;           // 'Y', 'N', 'A', etc.
    char     capacity;          // 'O', 'A', 'P', 'R'
    char     isoEligible;       // 'Y', 'N'
    char     crossType;         // 'N', 'O', 'C', etc.
    char     customerType;      // 'R', 'B', 'I'
};
#pragma pack(pop)

// Ultra-fast pseudo-random number generator (Xorshift)
// Far faster than std::mt19937 for tight loops
class FastRNG {
    uint64_t state;
public:
    FastRNG(uint64_t seed = 123456789) : state(seed) {}
    
    inline uint64_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

class OuchGenerator {
private:
    std::vector<uint64_t> m_tickers;
    size_t m_numTickers;
    uint64_t m_tokenCounter;
    FastRNG m_rng;

    // Helper to convert a string ticker to a padded 8-byte uint64_t
    static uint64_t packTicker(const std::string& ticker) {
        char buf[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
        for (size_t i = 0; i < ticker.length() && i < 8; ++i) {
            buf[i] = ticker[i];
        }
        uint64_t val;
        std::memcpy(&val, buf, 8);
        return val;
    }

    // Fast conversion of an integer to 14 hex characters for the Order Token
    inline void generateToken(char* dest, uint64_t id) {
        static const char hexChars[] = "0123456789ABCDEF";
        for(int i = 0; i < 14; ++i) {
            // Take 4 bits at a time, shift right, apply mask
            dest[13 - i] = hexChars[(id >> (i * 4)) & 0xF];
        }
    }

public:
    int counter{};

    OuchGenerator(const std::vector<std::string>& tickers, uint64_t initialSeed = 1337)
        : m_tokenCounter(1), m_rng(initialSeed) {
        
        m_numTickers = tickers.size();
        m_tickers.reserve(m_numTickers);
        for (const auto& t : tickers) {
            m_tickers.push_back(packTicker(t));
        }
    }

    // Pass the struct by reference to populate it in place. No heap allocations.
    inline void generate(Ouch5EnterOrder& msg) {
        ++counter;
        uint64_t randVal = m_rng.next();

        // 1. Static/Hardcoded defaults
        msg.messageType  = 'O';
        msg.firm[0]      = 'M'; msg.firm[1] = 'A'; msg.firm[2] = 'K'; msg.firm[3] = 'E'; // e.g. "MAKE"
        msg.display      = 'Y';
        msg.capacity     = 'O'; // Principal
        msg.isoEligible  = 'N';
        msg.crossType    = 'N';
        msg.customerType = 'R'; // Retail

        // 2. Fast Token Generation
        generateToken(msg.orderToken, m_tokenCounter++);

        // 3. Fast Random B/S indicator (use bottom bit of random val)
        msg.buySell = (randVal & 1) ? 'B' : 'S';

        // 4. Random Ticker: Cast back to char* via memcpy.
        // The compiler optimizes this into a single 64-bit register assignment.
        uint64_t selectedTicker = m_tickers[(randVal >> 1) % m_numTickers];
        std::memcpy(msg.stock, &selectedTicker, 8);

        // 5. Randomize numeric fields. 
        // OUCH requires binary fields to be in Network Byte Order (Big Endian).
        uint32_t shares = 100 * ((randVal >> 16) % 10 + 1); // Random 100 to 1000 shares
        msg.shares = htonl(shares);

        uint32_t price  = 1000000 + ((randVal >> 24) % 500000); // Random price between 100.00 to 150.00
        msg.price  = htonl(price);

        msg.timeInForce = htonl(99998); // Standard OUCH representation for Market Hours
    }
};