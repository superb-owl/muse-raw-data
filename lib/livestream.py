import asyncio
import websockets
import json
import time
import numpy as np
from pylsl import StreamInlet, resolve_byprop
import lib.params as params
import lib.util as util

EEG_SAMPLE_RATE = 256
PPG_SAMPLE_RATE = 64
EEG_SAMPLES_PER_CHUNK = 12
PPG_SAMPLES_PER_CHUNK = EEG_SAMPLES_PER_CHUNK * (PPG_SAMPLE_RATE // EEG_SAMPLE_RATE)

class GenericSignalStreamer:
    def __init__(self, signal_type, sample_rate, num_sensors, samples_per_chunk):
        self.signal_type = signal_type
        self.sample_rate = sample_rate
        self.num_sensors = num_sensors
        self.samples_per_chunk = samples_per_chunk
        self.inlet = None
        self.buffer = np.zeros((int(params.BUFFER_LENGTH * sample_rate), num_sensors))
        self.filter_state = None

    async def setup_stream(self):
        streams = []
        while len(streams) == 0:
            print(f"Waiting for {self.signal_type} stream...")
            await asyncio.sleep(1)
            streams = resolve_byprop('type', self.signal_type, timeout=2)
        print(f"Got {self.signal_type} stream")
        self.inlet = StreamInlet(streams[0], max_chunklen=self.samples_per_chunk)
        print(f"{self.signal_type} sample rate:", self.inlet.info().nominal_srate())

    async def pull_samples(self):
        data, timestamps = self.inlet.pull_chunk(max_samples=self.samples_per_chunk)
        if data:
            data = np.array(data)
            self.buffer, self.filter_state = util.update_buffer(
                self.buffer, data, notch=True,
                filter_state=self.filter_state)
        return self.buffer[-1].tolist()

class BioSignalStreamer:
    def __init__(self, host='0.0.0.0', port=8765):
        self.host = host
        self.port = port
        self.clients = set()
        self.eeg_streamer = GenericSignalStreamer('EEG', EEG_SAMPLE_RATE, params.NUM_EEG_SENSORS, EEG_SAMPLES_PER_CHUNK)
        self.ppg_streamer = GenericSignalStreamer('PPG', PPG_SAMPLE_RATE, params.NUM_PPG_SENSORS, PPG_SAMPLES_PER_CHUNK)

    async def handle_client(self, websocket, path):
        print(f"New client connected from {websocket.remote_address}")
        self.clients.add(websocket)
        try:
            print(f"Waiting for client {websocket.remote_address} to close")
            await websocket.wait_closed()
        finally:
            self.clients.remove(websocket)
            print(f"Client disconnected: {websocket.remote_address}")

    async def setup_streams(self):
        await asyncio.gather(
            self.eeg_streamer.setup_stream(),
            self.ppg_streamer.setup_stream()
        )

    async def stream_data(self):
        await self.setup_streams()
        while True:
            await self.pull_samples()

    async def pull_samples(self):
        timestamp = time.time()
        eeg_data = await self.eeg_streamer.pull_samples()
        ppg_data = await self.ppg_streamer.pull_samples()

        data = {
            'timestamp': timestamp,
            'eeg_channels': eeg_data,
            'ppg_channels': ppg_data
        }
        message = json.dumps(data)

        if self.clients:
            await asyncio.gather(
                *[client.send(message) for client in self.clients],
                return_exceptions=True
            )
        await asyncio.sleep(EEG_SAMPLES_PER_CHUNK / EEG_SAMPLE_RATE)

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

