#!/bin/bash
# Create the network namespaces
ip netns add sender_ns
ip netns add receiver_ns

sudo ip link add veth-rx type veth peer name veth-tx

sudo ip link set veth-tx netns sender_ns
sudo ip link set veth-rx netns receiver_ns

sudo ip -n sender_ns addr add 10.0.0.2/24 dev veth-tx
sudo ip -n sender_ns link set veth-tx up
sudo ip -n sender_ns link set lo up

sudo ip -n receiver_ns addr add 10.0.0.1/24 dev veth-rx
sudo ip -n receiver_ns link set veth-rx up
sudo ip -n receiver_ns link set lo up
