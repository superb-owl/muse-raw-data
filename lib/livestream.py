import asyncio
import websockets
import json
import random
import time
import numpy as np

class EEGSimulator:
    def __init__(self, num_channels=4, sample_rate=256):
        self.num_channels = num_channels
        self.sample_rate = sample_rate

    def get_sample(self):
        return [random.uniform(-100, 100) for _ in range(self.num_channels)]

class PPGSimulator:
    def __init__(self, num_channels=3, sample_rate=64):
        self.num_channels = num_channels
        self.sample_rate = sample_rate

    def get_sample(self):
        return [random.uniform(0, 1000) for _ in range(self.num_channels)]

class BioSignalStreamer:
    def __init__(self, host='0.0.0.0', port=8765):
        self.host = host
        self.port = port
        self.clients = set()
        self.eeg_simulator = EEGSimulator()
        self.ppg_simulator = PPGSimulator()
        self.ppg_buffer = []
        self.ppg_counter = 0

    async def handle_client(self, websocket, path):
        print(f"New client connected from {websocket.remote_address}")
        self.clients.add(websocket)
        try:
            print(f"Waiting for client {websocket.remote_address} to close")
            await websocket.wait_closed()
        finally:
            self.clients.remove(websocket)
            print(f"Client disconnected: {websocket.remote_address}")

    async def stream_data(self):
        while True:
            eeg_sample = self.eeg_simulator.get_sample()
            timestamp = time.time()

            # Generate PPG samples and add to buffer
            self.ppg_counter += 1
            if self.ppg_counter >= self.eeg_simulator.sample_rate // self.ppg_simulator.sample_rate:
                self.ppg_counter = 0
                ppg_sample = self.ppg_simulator.get_sample()
                self.ppg_buffer.append(ppg_sample)

            # Average PPG samples if available
            if self.ppg_buffer:
                avg_ppg_sample = np.mean(self.ppg_buffer, axis=0).tolist()
                self.ppg_buffer = []
            else:
                avg_ppg_sample = [0] * self.ppg_simulator.num_channels

            data = {
                'timestamp': timestamp,
                'eeg_channels': eeg_sample,
                'ppg_channels': avg_ppg_sample
            }
            message = json.dumps(data)

            if self.clients:
                await asyncio.gather(
                    *[client.send(message) for client in self.clients],
                    return_exceptions=True
                )
            else:
                pass
            await asyncio.sleep(1 / self.eeg_simulator.sample_rate)

    async def run(self):
        server = await websockets.serve(self.handle_client, self.host, self.port)
        print(f"WebSocket server started on ws://{self.host}:{self.port}")
        print("Waiting for clients to connect...")
        
        await asyncio.gather(
            server.wait_closed(),
            self.stream_data()
        )

async def main():
    streamer = BioSignalStreamer()
    await streamer.run()

if __name__ == "__main__":
    asyncio.run(main())

