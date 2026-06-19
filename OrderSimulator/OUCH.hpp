#pragma once

// parts of this file were AI-generated
// replace and account information message types were ommitted.
// I am assuming min_qty is 1 for ease of implementation

#include <cstdint>
#include <cstring>
#include <cstddef>

#pragma pack(push, 1)

struct OuchEnterOrder {
    char type;             
    char token[14];
    char side;             
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint32_t tif;
    char firm[4];
    char display;
    char capacity;
    char iso;
    uint32_t min_qty;
    char cross_type;
};

struct OuchCancelOrder {
    char type;             
    char token[14];
    uint32_t shares;
};

struct OuchReplaceOrder {
    char type;             
    char existing_token[14];
    char replacement_token[14];
    uint32_t shares;
    uint32_t price;
    uint32_t tif;
    char display;
    char iso;
    uint32_t min_qty;
};

#pragma pack(pop)

class OuchMockGenerator {
private:
    uint64_t seed_state;
    uint64_t token_seq;

    // Fast random generation
    inline uint64_t xorshift64() {
        seed_state ^= seed_state << 13;
        seed_state ^= seed_state >> 7;
        seed_state ^= seed_state << 17;
        return seed_state;
    }

    // --- State Tracking ---
    // 1024 is a power of 2, allowing bitwise AND (& 1023) instead of slow modulo (%)
    static constexpr uint32_t RING_MASK = 1023; 
    char active_tokens[1024][14];
    uint32_t token_head;

    // --- Firm Name Pool ---
    // Stored as uint32_t to allow single-instruction 4-byte assignments
    uint32_t firm_pool[64];

    // Templates
    OuchEnterOrder tmpl_enter;
    OuchCancelOrder tmpl_cancel;
    OuchReplaceOrder tmpl_replace;

    // Helper to generate a unique 14-byte token rapidly
    inline void generate_new_token(char* dest, uint64_t r) {
        uint64_t seq = token_seq++;
        // Write 8 bytes of entropy
        *reinterpret_cast<uint64_t*>(dest) = r;
        // Write 6 bytes of strictly increasing sequence to guarantee uniqueness
        std::memcpy(dest + 8, &seq, 6); 
    }

public:
    OuchMockGenerator(uint64_t seed = 88172645463325252ull) 
        : seed_state(seed), token_seq(1), token_head(0) {
        
        // 1. Initialize Firm Pool
        for (int i = 0; i < 64; ++i) {
            char firm_str[4];
            firm_str[0] = 'F';
            firm_str[1] = '0' + (i / 10);
            firm_str[2] = '0' + (i % 10);
            firm_str[3] =  '0' + (i % 100);

            // Pre-cast to uint32_t for faster assignment later
            std::memcpy(&firm_pool[i], firm_str, 4);
        }

        // 2. Prime the ring buffer with dummy tokens so initial Cancels/Replaces are valid
        for (int i = 0; i < 1024; ++i) {
            generate_new_token(active_tokens[i], xorshift64());
        }

        // 3. Initialize Templates
        std::memset(&tmpl_enter, 0, sizeof(tmpl_enter));
        tmpl_enter.type = 'O';
        std::memcpy(tmpl_enter.stock, "AAPL    ", 8);
        tmpl_enter.tif = 99998; 
        tmpl_enter.display = 'Y';
        tmpl_enter.capacity = 'O';
        tmpl_enter.iso = 'N';
        tmpl_enter.min_qty = 0;
        tmpl_enter.cross_type = 'N';

        std::memset(&tmpl_cancel, 0, sizeof(tmpl_cancel));
        tmpl_cancel.type = 'X';

        std::memset(&tmpl_replace, 0, sizeof(tmpl_replace));
        tmpl_replace.type = 'U';
        tmpl_replace.tif = 99998;
        tmpl_replace.display = 'Y';
        tmpl_replace.iso = 'N';
        tmpl_replace.min_qty = 0;
    }

    // In-place generation
    inline size_t generate(void* buffer) {
        uint64_t r = xorshift64();
        uint8_t dist = r & 0xFF;

        if (dist < 102) {
            // --- CANCEL (40%) ---
            auto* msg = reinterpret_cast<OuchCancelOrder*>(buffer);
            *msg = tmpl_cancel;
            
            // Pick a random known token from the ring buffer
            uint32_t ring_idx = (r >> 8) & RING_MASK;
            std::memcpy(msg->token, active_tokens[ring_idx], 14);
            
            msg->shares = (r >> 18) & 0x0000FFFF; 
            return sizeof(OuchCancelOrder);

        } else if (dist < 179) {
            // --- ENTER (30%) ---
            auto* msg = reinterpret_cast<OuchEnterOrder*>(buffer);
            *msg = tmpl_enter;
            
            // Generate new token and add it to the ring buffer
            token_head = (token_head + 1) & RING_MASK;
            generate_new_token(msg->token, r);
            std::memcpy(active_tokens[token_head], msg->token, 14);
            
            // Assign Firm Name (fast 4-byte copy from pool)
            uint32_t firm_idx = (r >> 8) & 63; // 64 firms
            *reinterpret_cast<uint32_t*>(msg->firm) = firm_pool[firm_idx];

            msg->side = (r & 0x4000) ? 'B' : 'S';
            msg->shares = (r >> 15) & 0x0000FFFF;
            msg->price = 1500000 + ((r >> 31) & 0xFFF); 
            return sizeof(OuchEnterOrder);

        } else {
            // --- REPLACE (30%) ---
            auto* msg = reinterpret_cast<OuchReplaceOrder*>(buffer);
            *msg = tmpl_replace;
            
            // 1. Fetch an existing token
            uint32_t ring_idx = (r >> 8) & RING_MASK;
            std::memcpy(msg->existing_token, active_tokens[ring_idx], 14);
            
            // 2. Generate the replacement token
            generate_new_token(msg->replacement_token, r ^ 0xAAAAAAAAAAAAAAAAull);
            
            // 3. Update the ring buffer in-place so the new token can be modified later
            std::memcpy(active_tokens[ring_idx], msg->replacement_token, 14);

            msg->shares = (r >> 18) & 0x0000FFFF;
            msg->price = 1500000 + ((r >> 34) & 0xFFF);
            return sizeof(OuchReplaceOrder);
        }
    }
};