import asyncio
import logging
import time
import sys
import os
import matplotlib.pyplot as plt
from collections import defaultdict

# ==================== 配置区 ====================
TARGET_HOST = '127.0.0.1'         # 目标服务器 IP
TARGET_PORT = 5050                # 目标服务器端口
NUM_CLIENTS = 1100              # 并发客户端数量
MESSAGE_INTERVAL = 0.1            # 每个客户端发送消息间隔（秒）
MESSAGE = "PING"                  # 发送的消息内容
TEST_DURATION = 10                # 测试持续时间（秒）
LOG_LEVEL = logging.INFO
PLOT_RESULTS = True               # 是否生成图表
OUTPUT_DIR = "./output"           # 统一输出目录
# ===============================================

# 确保输出目录存在
os.makedirs(OUTPUT_DIR, exist_ok=True)

# 日志文件和输出路径
LOG_FILE = os.path.join(OUTPUT_DIR, "stress_test.log")
PLOT_FILE = os.path.join(OUTPUT_DIR, "stress_test_report.png")
CSV_FILE = os.path.join(OUTPUT_DIR, "stress_test_detail.csv")

# 日志配置
logging.basicConfig(
    level=LOG_LEVEL,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE, encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)

class Client:
    def __init__(self, client_id):
        self.client_id = client_id
        self.reader = None
        self.writer = None
        self.connected = False
        self.sent_count = 0
        self.recv_count = 0
        self.rtt_list = []  # 往返时间记录
        self.running = True

    async def connect(self):
        try:
            self.reader, self.writer = await asyncio.wait_for(
                asyncio.open_connection(TARGET_HOST, TARGET_PORT),
                timeout=10.0
            )
            self.connected = True
            logging.info(f"Client-{self.client_id:04d} connected to {TARGET_HOST}:{TARGET_PORT}")
        except Exception as e:
            logging.error(f"Client-{self.client_id:04d} failed to connect: {e}")
            self.connected = False
        return self.connected

    async def send_message(self):
        if not self.connected:
            return
        try:
            timestamp = time.time()
            msg = f"{MESSAGE} {self.client_id} {int(timestamp * 1000)}\n"  # 毫秒时间戳
            self.writer.write(msg.encode())
            await self.writer.drain()
            self.sent_count += 1

            # 读取响应
            resp = await asyncio.wait_for(self.reader.readline(), timeout=5.0)
            recv_time = time.time()
            response = resp.decode().strip()

            # 解析时间戳计算 RTT
            try:
                sent_ms = int(msg.strip().split()[-1])
                rtt = (recv_time - sent_ms / 1000.0) * 1000  # 毫秒
                self.rtt_list.append(rtt)
            except:
                pass

            self.recv_count += 1
        except Exception as e:
            logging.error(f"Client-{self.client_id:04d} send error: {e}")
            self.connected = False
            self.close()

    def close(self):
        if self.writer:
            try:
                self.writer.close()
                if hasattr(self.writer, 'wait_closed'):
                    asyncio.create_task(self.writer.wait_closed())
            except Exception as e:
                logging.debug(f"Close error: {e}")
        self.connected = False

    async def run(self):
        if not await self.connect():
            return

        start_time = time.time()
        while self.running and self.connected:
            if time.time() - start_time > TEST_DURATION:
                break
            try:
                await self.send_message()
                await asyncio.sleep(MESSAGE_INTERVAL)
            except asyncio.CancelledError:
                break
            except Exception as e:
                logging.error(f"Client-{self.client_id} runtime error: {e}")
                break

        self.close()
        logging.info(f"Client-{self.client_id:04d} stopped. Sent: {self.sent_count}, Received: {self.recv_count}")


async def main():
    logging.info(f"Starting stress test with {NUM_CLIENTS} clients for {TEST_DURATION} seconds...")
    logging.info(f"Target: {TARGET_HOST}:{TARGET_PORT}, Interval: {MESSAGE_INTERVAL}s")

    start_time = time.time()
    clients = [Client(i) for i in range(NUM_CLIENTS)]
    tasks = [asyncio.create_task(client.run()) for client in clients]

    logging.info("All clients launched.")

    # 运行指定时长
    await asyncio.sleep(TEST_DURATION)

    # 停止所有客户端
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)

    end_time = time.time()
    logging.info(f"Stress test ended after {end_time - start_time:.2f} seconds.")

    # === 数据统计 ===
    total_sent = sum(c.sent_count for c in clients)
    total_recv = sum(c.recv_count for c in clients)
    success_rate = (total_recv / total_sent * 100) if total_sent > 0 else 0

    logging.info(f"Summary: Total Sent = {total_sent}, Total Received = {total_recv}, "
                 f"Success Rate = {success_rate:.2f}%")

    # RTT 统计
    all_rtts = []
    for c in clients:
        all_rtts.extend(c.rtt_list)

    avg_rtt = sum(all_rtts) / len(all_rtts) if all_rtts else 0
    max_rtt = max(all_rtts) if all_rtts else 0
    min_rtt = min(all_rtts) if all_rtts else 0

    logging.info(f"RTT - Min: {min_rtt:.2f}ms, Max: {max_rtt:.2f}ms, Avg: {avg_rtt:.2f}ms")

    # === 生成图表 ===
    if PLOT_RESULTS:
        try:
            # 尝试支持中文
            try:
                plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS', 'DejaVu Sans']
                plt.rcParams['axes.unicode_minus'] = False
            except:
                pass

            fig, ax = plt.subplots(2, 2, figsize=(12, 8))

            # 图1：总发送 vs 接收
            ax[0,0].bar(['Sent', 'Received'], [total_sent, total_recv], color=['skyblue', 'lightgreen'])
            ax[0,0].set_title('Total Messages: Sent vs Received')
            ax[0,0].set_ylabel('Count')
            for i, v in enumerate([total_sent, total_recv]):
                ax[0,0].text(i, v + max(total_sent, total_recv)*0.01, str(v), ha='center')

            # 图2：成功率饼图
            failed = total_sent - total_recv
            ax[0,1].pie([success_rate, 100-success_rate], labels=['Success', 'Lost'], autopct='%1.1f%%',
                        colors=['lightgreen', 'lightcoral'])
            ax[0,1].set_title('Echo Success Rate')

            # 图3：RTT 分布
            if all_rtts:
                ax[1,0].hist(all_rtts, bins=30, color='purple', alpha=0.7)
                ax[1,0].set_title('RTT Distribution (ms)')
                ax[1,0].set_xlabel('RTT (ms)')
                ax[1,0].set_ylabel('Frequency')

            # 图4：前20个客户端对比
            sample_clients = clients[:20]
            client_ids = [f"{c.client_id}" for c in sample_clients]
            sent_data = [c.sent_count for c in sample_clients]
            recv_data = [c.recv_count for c in sample_clients]

            x = range(len(client_ids))
            ax[1,1].bar(x, sent_data, width=0.4, label='Sent', color='skyblue', align='center')
            ax[1,1].bar([i + 0.4 for i in x], recv_data, width=0.4, label='Received', color='lightgreen', align='center')
            ax[1,1].set_title('Sample Clients: Sent vs Received (First 20)')
            ax[1,1].set_xticks([i + 0.2 for i in x])
            ax[1,1].set_xticklabels(client_ids, rotation=45)
            ax[1,1].legend()
            ax[1,1].set_ylabel('Message Count')

            plt.tight_layout()
            plt.savefig(PLOT_FILE, dpi=150)
            logging.info(f"Chart saved to {PLOT_FILE}")
            plt.show()

        except ImportError:
            logging.warning(f"matplotlib not installed. Skipping chart generation.")
            print(f"Install matplotlib to enable chart: pip install matplotlib")

    # === 保存详细数据到 CSV ===
    with open(CSV_FILE, "w", encoding="utf-8") as f:
        f.write("client_id,sent_count,recv_count,success_rate,avg_rtt_ms\n")
        for c in clients:
            avg_rtt_client = sum(c.rtt_list)/len(c.rtt_list) if c.rtt_list else 0
            rate = c.recv_count / c.sent_count * 100 if c.sent_count > 0 else 0
            f.write(f"{c.client_id},{c.sent_count},{c.recv_count},{rate:.2f},{avg_rtt_client:.2f}\n")
    logging.info(f"Detailed results saved to {CSV_FILE}")

    # 最终汇总打印
    print("\n" + "="*50)
    print("STRESS TEST COMPLETED")
    print(f"Duration: {TEST_DURATION}s | Clients: {NUM_CLIENTS}")
    print(f"Total Sent: {total_sent}")
    print(f"Total Received: {total_recv}")
    print(f"Success Rate: {success_rate:.2f}%")
    print(f"RTT Avg: {avg_rtt:.2f}ms | Min: {min_rtt:.2f}ms | Max: {max_rtt:.2f}ms")
    print(f"Report saved in: {os.path.abspath(OUTPUT_DIR)}")
    print("="*50)


if __name__ == "__main__":
    if sys.platform == 'win32':
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nTest stopped by user.")