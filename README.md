# OUCH OrderBook

This project works to implement an orderbook which operates on the NASDAQ OUCH order format which can process as many orders as possible. I am giving myself a time limit of a week to see how far I can push this implementation.

Right now, it is implemented with a SPSC reading from a single UDP port with batched I/O syscalls. 

## Stages

0. Setup

The setup simulation for this work is simple: one core on my machine will continually run the batched linux syscall sendmmsg() to read from pointers to a static generated OUCH message buffer. We populate the buffer with a cheap random OUCH generator function.

2. Single Producer Single Consumer

Producer and Consumer both run bound to seperate cores. Because both the Producer and Consumer use batched syscalls, we have both a tail and head pointer for the ring buffer which makes batched read/writes easy. Producer gets UDP messages using recvmmgs(). We use std::atomic with release memory orderings for the tail and head pointer updates, however, for reading the tail in the Consumer thread, we don't use the traditional acquire pairing. Because the tail monotonically increases, getting an outdated value is not an issue, and since the buffer is "happens before" the tail updates, we can actually use a relaxed memory order to allow for the speculative execution of the consumer on the buffer. I'll look into if this is an advantage or not eventully.

2. Thread Pool

3. Async IO

4. Kernel Bypass

5. ## AI Usage

6. I will not be using any AI-generated code or reccomendations for this project other than where explicitly stated. Currently the only use was generating some of the OUCH decoding code which I found to be unhelpful for learning.
