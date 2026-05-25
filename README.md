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

Because we are bound on recvmmsg, I perf-ed it with a dwarf call stack and get:

![](Images/recvmmsgPerf.png)

It looks like about 30% of this recvmmsg is in the context switching and syscall overhead, about 30% is in waiting for the next packet, and the remaining 30% is internal to the linux UDP network stack. What is really suprising to me about this is that online, its easy to hear about the millions of packets/sec that io_uring provides over syscalls, however, here we see really only a potential doubling improvement over using the normal blocked syscall because the context switching overhead is about equal to the amount of time it takes to process a UDP packet and give it to userspace on my machine. I'll ensure that the simulator is faster and once that bottleneck is cleared, it will be interesting to read if io_uring makes the actualy UDP packet processing and handoff from the socket into userspace more efficient rather than just eliminating the syscall overhead. At the end of the day, however, the processing it needs to do which takes ~30% seems like it may be similar. I'll read more and we will see!

As for the lack of packets coming in, I was suprised since when I ran an analysis of sent/recieved packets to see if that was the bottleneck, I got this chart:

![](Images/sentTrafficVsRecieved.png)

What we see here is that as we increase the time of the simulation, the total packets obviously rise linearly, but the amount of packets recieved divided by the amount sent, the recieve rate, is not near 1 and not constant! We would expect a near-1 rate if our SPSC was actually bound on the number of orders coming in, however, this is not what we see. One possible explaination is that the SPSC has a higher startup overhead and the simulator sends a bunch of packets before it can start recieving, artifically lowering the recieve rate, however, if we increase the time, we would expect the recieve rate to then converge to one, and instead it stays constant. we see an 80% accept rate across all time horizons, which points to loss on the socket queue acceptance end(i.e tons of deliveries at once, overloading the queue leading to packet rejection).

My first idea of fixing this is to ensure our recvmmsg thread never gets slept by the scheduler if it times out, which will hopefully cause it to poll the whole time and not let bursts come in without taking messages from the socket at a higher rate.

Wow... I ran lscpu and realized I only have 4 physical cores... setting the affinity correctly and disabling irqs on the three I use for SCPSC and the simulator, I literally double the output to 200k/s. Lets re-perf! Looks like the rate of accept to receive is now ~99%.

2. Multiplexed Ports and Async IO

Currently working on this. From what I've read, linux spinlocks on each port are a bottleneck since I/O is treated as a normal process(and can thus be happening on multiple cores at the same time). Also, the syscall overhead for context switching can be eliminated with memory mapped ring buffers in io_uring.

3. Kernel Bypass

4. ## AI Usage

I will not be using any AI-generated code for this project other than where explicitly stated. Currently the only use was generating some of the OUCH generation code which I found tedious.
