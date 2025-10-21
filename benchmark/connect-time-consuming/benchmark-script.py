import socket
import time
import threading
from concurrent.futures import ThreadPoolExecutor
import statistics
import matplotlib.pyplot as plt
import numpy as np
import os

# ================== Configuration ==================
SERVER_HOST = '127.0.0.1'      # 👉 Change to your server IP
SERVER_PORT = 5050             # 👉 Change to your server port
NUM_CONNECTIONS = 1000          # Number of test connections
USE_THREADS = False            # False: sequential, True: concurrent
TIMEOUT = 10.0                 # Connection timeout in seconds
OUTPUT_DIR = './output'        # Directory to save results
# ===================================================

# Create output directory
os.makedirs(OUTPUT_DIR, exist_ok=True)

latencies = []        # Latency in ms
statuses = []         # 'success' or 'fail'
lock = threading.Lock()

def test_single_connection(client_id):
    start_time = time.time()
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(TIMEOUT)
    try:
        # --- Connect ---
        sock.connect((SERVER_HOST, SERVER_PORT))
        
        # Optional: send/receive data (simulate real usage)
        # sock.send(b"TEST\n")
        # sock.recv(1024)

        # --- Close ---
        sock.close()

        end_time = time.time()
        latency_ms = (end_time - start_time) * 1000

        with lock:
            latencies.append(latency_ms)
            statuses.append('success')
            print(f"[{client_id:3d}] ✅ {latency_ms:6.2f} ms")

    except Exception as e:
        end_time = time.time()
        latency_ms = (end_time - start_time) * 1000
        with lock:
            latencies.append(latency_ms)
            statuses.append('fail')
            print(f"[{client_id:3d}] ❌ {latency_ms:6.2f} ms | {e}")

def run_test():
    print(f"🚀 Starting test: {NUM_CONNECTIONS} connections to {SERVER_HOST}:{SERVER_PORT}")
    print(f"   Mode: {'Concurrent' if USE_THREADS else 'Sequential'} | Timeout: {TIMEOUT}s")
    print("-" * 60)

    start_total = time.time()

    if USE_THREADS:
        with ThreadPoolExecutor(max_workers=50) as executor:
            executor.map(test_single_connection, range(NUM_CONNECTIONS))
    else:
        for i in range(NUM_CONNECTIONS):
            test_single_connection(i)

    total_time = time.time() - start_total

    # ===== Statistics =====
    success_latencies = [t for t, s in zip(latencies, statuses) if s == 'success']
    fail_count = len([s for s in statuses if s == 'fail'])
    success_rate = (len(success_latencies) / len(latencies)) * 100 if latencies else 0

    if success_latencies:
        avg = statistics.mean(success_latencies)
        min_t = min(success_latencies)
        max_t = max(success_latencies)
        std_dev = statistics.stdev(success_latencies) if len(success_latencies) > 1 else 0.0
    else:
        avg = min_t = max_t = std_dev = 0.0

    # ===== Print Results =====
    print("-" * 60)
    print("📊 Test Results:")
    print(f"   Total Attempts: {len(latencies)}")
    print(f"   Success: {len(success_latencies)} | Failed: {fail_count}")
    print(f"   Success Rate: {success_rate:.1f}%")
    print(f"   Average Latency: {avg:.2f} ms")
    print(f"   Min: {min_t:.2f} ms | Max: {max_t:.2f} ms")
    print(f"   Std Dev: {std_dev:.2f} ms")
    print(f"   Total Time: {total_time:.2f} seconds")

    # ===== Save Raw Data to CSV =====
    csv_path = os.path.join(OUTPUT_DIR, 'latency_data.csv')
    with open(csv_path, 'w') as f:
        f.write('id,latency_ms,status\n')
        for i, (t, s) in enumerate(zip(latencies, statuses)):
            f.write(f"{i},{t:.2f},{s}\n")
    print(f"   📄 Latency data saved to: {csv_path}")

    # ===== Generate and Save Chart =====
    plot_results(success_latencies, statuses, avg, total_time)

def plot_results(success_latencies, statuses, avg_latency, total_time):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # Plot 1: Latency Trend
    colors = ['green' if s == 'success' else 'red' for s in statuses]
    x = np.arange(len(latencies))
    ax1.scatter(x, latencies, c=colors, alpha=0.7, s=20, edgecolor='none')
    if success_latencies:
        ax1.axhline(avg_latency, color='blue', linestyle='--', label=f'Average: {avg_latency:.2f}ms')
    ax1.set_title('Connection Latency Trend')
    ax1.set_xlabel('Connection ID')
    ax1.set_ylabel('Latency (ms)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Plot 2: Latency Distribution
    if success_latencies:
        ax2.hist(success_latencies, bins=15, alpha=0.7, color='skyblue', edgecolor='black')
        ax2.axvline(avg_latency, color='red', linestyle='-', label=f'Average: {avg_latency:.2f}ms')
        ax2.set_title('Latency Distribution (Success)')
        ax2.set_xlabel('Latency (ms)')
        ax2.set_ylabel('Frequency')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
    else:
        ax2.text(0.5, 0.5, 'No successful connections', ha='center', va='center',
                 transform=ax2.transAxes, fontsize=14, color='red')
        ax2.set_title('Latency Distribution')

    plt.tight_layout()

    # Save chart
    image_path = os.path.join(OUTPUT_DIR, 'latency_chart.png')
    plt.savefig(image_path, dpi=150, bbox_inches='tight')
    plt.close()  # Prevent display in headless environments
    print(f"   📈 Chart saved to: {image_path}")

if __name__ == "__main__":
    run_test()