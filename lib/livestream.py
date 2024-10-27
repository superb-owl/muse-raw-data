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

class BioSignalStreamer:
    def __init__(self, host='0.0.0.0', port=8765):
        self.host = host
        self.port = port
        self.clients = set()
        self.eeg_inlet = None
        self.ppg_inlet = None
        self.eeg_buffer = np.zeros((int(params.BUFFER_LENGTH * EEG_SAMPLE_RATE), params.NUM_EEG_SENSORS))
        self.ppg_buffer = np.zeros((int(params.BUFFER_LENGTH * PPG_SAMPLE_RATE), params.NUM_PPG_SENSORS))
        self.eeg_filter_state = None
        self.ppg_filter_state = None

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
        eeg_streams = []
        ppg_streams = []
        while len(eeg_streams) == 0 or len(ppg_streams) == 0:
            print("Waiting for streams...")
            await asyncio.sleep(1)
            eeg_streams = resolve_byprop('type', 'EEG', timeout=2)
            ppg_streams = resolve_byprop('type', 'PPG', timeout=2)
        print("got streams", len(eeg_streams), len(ppg_streams))

        self.eeg_inlet = StreamInlet(eeg_streams[0], max_chunklen=EEG_SAMPLES_PER_CHUNK)
        self.ppg_inlet = StreamInlet(ppg_streams[0], max_chunklen=PPG_SAMPLES_PER_CHUNK)

        print("sample rates", self.eeg_inlet.info().nominal_srate(), self.ppg_inlet.info().nominal_srate())

    async def stream_data(self):
        await self.setup_streams()
        while True:
            await self.pull_samples()

    async def pull_samples(self):
        timestamp = time.time()
        eeg_data, eeg_timestamps = self.eeg_inlet.pull_chunk(max_samples=EEG_SAMPLES_PER_CHUNK)
        ppg_data, ppg_timestamps = self.ppg_inlet.pull_chunk(max_samples=PPG_SAMPLES_PER_CHUNK)

        if eeg_data:
            eeg_data = np.array(eeg_data)
            self.eeg_buffer, self.eeg_filter_state = util.update_buffer(
                self.eeg_buffer, eeg_data, notch=True,
                filter_state=self.eeg_filter_state)

        if ppg_data:
            ppg_data = np.array(ppg_data)
            self.ppg_buffer, self.ppg_filter_state = util.update_buffer(
                self.ppg_buffer, ppg_data, notch=True,
                filter_state=self.ppg_filter_state)

        data = {
            'timestamp': timestamp,
            'eeg_channels': self.eeg_buffer[-1].tolist(),
            'ppg_channels': self.ppg_buffer[-1].tolist()
        }
        message = json.dumps(data)

        if self.clients:
            await asyncio.gather(
                *[client.send(message) for client in self.clients],
                return_exceptions=True
            )
        else:
            pass
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

