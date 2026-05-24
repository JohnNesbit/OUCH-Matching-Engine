# OUCH OrderBook

This project works to implement an orderbook which operates on the NASDAQ OUCH order format which can process as many orders as possible. I am giving myself a time limit of a week to see how far I can push this implementation.

Right now, it is implemented with a SPSC reading from a single UDP port with batched I/O syscalls, and I am working on an io_uring implementation. 

## Stages

0. Setup

The setup simulation for this work is simple: one core on my machine will continually run the batched linux syscall sendmmsg() to read from pointers to a static generated OUCH message buffer. We populate the buffer with a cheap random OUCH generator function. On my linux box it generates 152k packets/sec on a non-blocked syscall core-bound loop. This will change as my OrderBook can support higher throughput.

1. Single Producer Single Consumer(Batched IO Syscalls)

The Producer and Consumer both run bound to seperate cores. Because both the Producer and Consumer use batched syscalls, we have both a tail and head pointer for the ring buffer which makes batched read/writes easy. Producer gets UDP messages using recvmmgs(). We use std::atomic with release memory orderings for the tail and head pointer updates, however, for reading the tail in the Consumer thread, we don't use the traditional acquire pairing. Because the tail monotonically increases, getting an outdated value is not an issue, and since the buffer write is "happens before" the tail index update, we can use a relaxed memory order to allow for the speculative execution of the consumer on the buffer. On intel machines, however, they compile into the same instructions.

![Consumer is IO-bound](Images/SPSC_CONSUMER_PERF.png)

I optimized the SPSC as much as I could by profiling with perf and got to <99% of the time in the producer spent on recvmmsg() and <<10% of the consumer time within the actual orderbook consumption function.

![](Images/SPSC_PRODUCER_PERF.png)

 Non-blocking I/O doesn't make sense here really since we aren't multiplexing over a bunch of ports, and balancing across many for a multiplexed thread pool would introduce and enormous amount of overhead(epoll has a massive red-black tree, etc) to have the same bottleneck of copying from kernel space. Since we are very I/O bound here on recvmmsg(), I think this is a natural point to look into how to make the syscalls faster by parallelizing the kernel socket buffer writes and eliminating the  syscall overhead.

2. Multiplexed Ports and Async IO

Currently working on this. From what I've read, linux spinlocks on each port are a bottleneck since I/O is treated as a normal process(and can thus be happening on multiple cores at the same time). Also, the syscall overhead for context switching can be eliminated with memory mapped ring buffers in io_uring.

3. Kernel Bypass

4. ## AI Usage

I will not be using any AI-generated code for this project other than where explicitly stated. Currently the only use was generating some of the OUCH generation code which I found tedious.
