# OUCH OrderBook

This project works to implement an orderbook which operates on the NASDAQ OUCH order format which can process as many orders as possible. I am giving myself a time limit of a week to see how far I can push this implementation.

Right now, it is implemented with a SPSC reading from a single UDP port with batched I/O syscalls, and I am working on an io_uring implementation. 

## 0. Setup

The setup simulation for this work is simple: one core on my machine will continually run the batched linux syscall sendmmsg() to read from pointers to a static generated OUCH message buffer. We populate the buffer with a cheap random OUCH generator function. On my linux box it generates 152k packets/sec on a non-blocked syscall core-bound loop. This will change as my OrderBook can support higher throughput.

## 1. Single Producer Single Consumer(Batched IO Syscalls)

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

Wow... I ran lscpu and realized I only have 4 physical cores... setting the affinity correctly and disabling irqs on the three I use for SCPSC and the simulator, I literally double the output to 200k/s. Insane! It looks like all along the huge bottleneck there was literally the producer just being slept and then packets getting rejected by the kernel for a full socket queue! With a receive rate of about 1, we know that the actual bottleneck here is now in the the simulator rather than the producer. Lets perf the simulator!

![](Images/afterBoundSendmmsgPerfs.png)

Okay, the context switching overhead is actually only 1/39 of the used cpu time. It looks like 7/39 is on nf hooks, which is a clear candidate to get rid of. That is far from a major breakthrough though or the context-switch bound situation that we people online talk about needing io_uring for. Looking further, 14/38 of the time is actually spent... recieveing?

![](Images/ip_recv_perf.png)

So, we receive with 14%/39% of our sender thread, which 7% of that 14% is nf_hooks again, meaning that we spend 14/39 on nf_hooks and we do 7% of the sender thread on actually putting our message within the socket for the recv thread. Of that 7%, we are actually spending half on a spinlock on the socket.

So, the obvious next step here to optimize our sender is turning off our network hooks(looks like our local deliver(NF_INET_LOCAL_IN) and outward send hoooks(NF_INET_LOCAL_OUT) are triggering) and then maybe getting the receive of the packet onto a different thread(ideally the systems core 0 rather than the producer/recv thread). Lets do that!

Well, also here we have to make a decision about how we want to think about this simulation. Because we are just sending packets between two applications, we could use eBPF to directly link them, however, I think that is contrary to waht I want to learn in optimizing this stack, instead, I would rather pretend(or actually steup) a network interface that passes these packets so there is room to optimize down there later as well. Because of that, I'm going to use veth(reccommended by Claude as a way to simulate this). This will do the pass over the data-link layer. All this entailed was creating two network namespaces and binding veth interfaces to each other within the namespaces as described in [this](https://superuser.com/questions/764986/howto-setup-a-veth-virtual-network). Because we have a virtual network interface(only bound between two veth interfaces inside two network namespaces) we load them individually in the sender and recieving threads, bind our sockets, and then we can use SO_BINDTODEVICE which will bypass our network hooks!

![](Images/postVethPerf.png)

As you can see, our overhead from our network filtering hooks is gone now that we are on our private network namespaces! After this change, we get boosted to 360k/s with >99% packet reception rate on our producer. Despite this victory, we still see that 14%/35% of our sender is actually still doing the direct local_ip delivery and doing the ip_recv function itself within the softirq's. Really, we would like the irq's to get picked up by our allocated system cores rather than the ones we have running our sender. Our irq's are presumably being triggered by our tranmission queue since we see that queue_xmit function. Because this is not actually one process part of our send, we can set our irq affinity to 0 for that core to prevent this(and also all the other non-sys cores while we are at it).

Well, looking into irqbalance people seem to think there is a good reason not to shift soft irqs anywhere.

trying out non-blockign for our simulator somewhat unsprisingly changes neither the output nor even the perf graph. Because the sendmmsg stack is immediately putting a softirq onto this core and everything is tied to this core, the waking and sleeping overhead is actually the same since the nonblocking call still sleeps the sender until the message is written due to the softirq being scheduled ahead of it.

Interestingly enough, however, at this point we seem to be bound on our consumer! Our producer is actually never waiting for messages and is instead busy waiting for about 1/3 of its time on the consumer!! Below we have a perf to show this:

![](Images/consumerBoundPerf.png)

Because of this, I figure it is about time to optimize the orderbook logic itself. 

![]()

huh.. weird, 1/3 of the time of the consumer is waiting on the producer and 2/3 of the time of the consumer is waiting on the producer. What this likely means is our consumer has some really heavy peaks which halt the consumer and then lap around quickly, making the producer wait on the consumer 1/3 of the time(it should be 0% ideally). Roughly 20% of the consumer's time is spent in the consume function.

To prevent spiking, I re-structured the actual trade execution. Instead of storing all trades in an array of prices with a DLL of orders in each, I just am making a heap for buy and sell which allows for which comparisons at the top of each, making execution O(log n * k) where n is the size of the order book and k is the number of trades executed. This compares to the previous algortihm which was O(l) on l being the range of the spread of the orderbook.
    - another approach would have been doing binary search across the spread to find the next highest sell/lowest buy as fast as possible.


## 2. Multiplexed Ports and Async IO

Currently working on this. From what I've read, linux spinlocks on each port are a bottleneck since I/O is treated as a normal process(and can thus be happening on multiple cores at the same time). Also, the syscall overhead for context switching can be eliminated with memory mapped ring buffers in io_uring.

## 3. Kernel Bypass

## 4. AI Usage

I will not be using any AI-generated code for this project other than where explicitly stated. Currently the only use was generating some of the OUCH generation code which I found tedious.
