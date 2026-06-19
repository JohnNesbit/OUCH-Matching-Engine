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


ISOLATED_CPUS=$(sed -n '4p' MachineConfigurations.txt)
IRQ_CPUS=$(sed -n '3p' MachineConfigurations.txt)
MASK=0
for cpu in $(echo "$IRQ_CPUS" | tr ',' ' '); do
    MASK=$((MASK | (1 << cpu)))
done
RPS_MASK=$(printf '%x' $MASK)

sudo ip netns exec receiver_ns sh -c 'echo $RPS_MASK > /sys/class/net/veth-rx/queues/rx-0/rps_cpus'

sudo sed -i "s/^GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT=\"isolcpus=${ISOLATED_CPUS} rcu_nocbs=${ISOLATED_CPUS} nohz_full=${ISOLATED_CPUS} irqaffinity=${IRQ_CPUS}\"/" /etc/default/grub
sudo update-grub
echo "Reboot needed for CPU isolation to take place."
