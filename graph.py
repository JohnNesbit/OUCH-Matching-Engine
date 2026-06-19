import sys
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 graph.py <results_file>")
        print("  Run the exchange with 'experiment' as last arg, pipe stdout to a file")
        print('  e.g. sudo ./build/Exchange "3,4,5" 2 8080 128 128 experiment > results.txt')
        sys.exit(1)

    ramp_durations = []
    ramp_packets = []
    steady_runs = []
    steady_packets = []

    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if line.startswith("RAMP,"):
                parts = line.split(",")
                ramp_durations.append(int(parts[1]) / 1000)
                ramp_packets.append(int(parts[2]))
            elif line.startswith("STEADY,"):
                parts = line.split(",")
                steady_runs.append(int(parts[1]))
                steady_packets.append(int(parts[2]))

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # ramp-up: total packets at each duration
    ax1.plot(ramp_durations, ramp_packets, "bo-", linewidth=2, markersize=8)
    ax1.set_xlabel("Duration (seconds)")
    ax1.set_ylabel("Total Packets Processed")
    ax1.set_title("Throughput Ramp-Up")
    ax1.grid(True, alpha=0.3)

    # secondary axis: packets/sec
    ramp_throughput = [p / d for p, d in zip(ramp_packets, ramp_durations)]
    ax1_twin = ax1.twinx()
    ax1_twin.plot(ramp_durations, ramp_throughput, "r--", linewidth=1.5, alpha=0.7, label="packets/sec")
    ax1_twin.set_ylabel("Packets/sec", color="r")
    ax1_twin.legend(loc="lower right")

    # steady-state: 10 runs at 5s
    ax2.bar(steady_runs, steady_packets, color="steelblue", edgecolor="black")
    ax2.set_xlabel("Run")
    ax2.set_ylabel("Packets Processed (5s)")
    ax2.set_title("Steady-State Consistency (10 runs @ 5s)")
    ax2.set_xticks(steady_runs)
    ax2.grid(True, axis="y", alpha=0.3)

    if steady_packets:
        mean_val = sum(steady_packets) / len(steady_packets)
        ax2.axhline(y=mean_val, color="r", linestyle="--", linewidth=1.5, label=f"Mean: {mean_val:,.0f}")
        ax2.legend()

        std_dev = (sum((x - mean_val) ** 2 for x in steady_packets) / len(steady_packets)) ** 0.5
        print(f"Steady-state stats:")
        print(f"  Mean:    {mean_val:,.0f} packets/5s ({mean_val/5:,.0f} packets/sec)")
        print(f"  Std Dev: {std_dev:,.0f}")
        print(f"  CV:      {std_dev / mean_val * 100:.2f}%")

    plt.tight_layout()
    plt.savefig("experiment_results.png", dpi=150)
    print(f"Saved to experiment_results.png")
    plt.show()

if __name__ == "__main__":
    main()
