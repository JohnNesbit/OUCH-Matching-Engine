PINNED_CORES=$(sed -n '1p' MachineConfigurations.txt)
UNPINNED_CORE=$(sed -n '2p' MachineConfigurations.txt)

sudo ./build/Exchange $PINNED_CORES $UNPINNED_CORE
