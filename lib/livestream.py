import asyncio
import websockets
import json
import time
import numpy as np
from pylsl import StreamInlet, resolve_byprop
import lib.params as params
import lib.util as util

from scipy.signal import lfilter, lfilter_zi, firwin

EEG_SAMPLE_RATE = 256
PPG_SAMPLE_RATE = 64
EEG_SAMPLES_PER_CHUNK = 12
PPG_SAMPLES_PER_CHUNK = EEG_SAMPLES_PER_CHUNK * (PPG_SAMPLE_RATE // EEG_SAMPLE_RATE)
EEG_FIRWIN_SIZE = 40
PPG_FIRWIN_SIZE = 10

class GenericSignalStreamer:
    def __init__(self, signal_type, sample_rate, num_sensors, samples_per_chunk):
        self.signal_type = signal_type
        self.sample_rate = sample_rate
        self.num_sensors = num_sensors
        self.samples_per_chunk = samples_per_chunk
        self.inlet = None
        self.window = 5 # what is this?
        self.firwin_size = EEG_FIRWIN_SIZE if self.signal_type == 'EEG' else PPG_FIRWIN_SIZE

    async def setup_stream(self):
        streams = []
        while len(streams) == 0:
            print(f"Waiting for {self.signal_type} stream...")
            await asyncio.sleep(1)
            streams = resolve_byprop('type', self.signal_type, timeout=2)
        print(f"Got {self.signal_type} stream")
        self.inlet = StreamInlet(streams[0], max_chunklen=self.samples_per_chunk)
        print(f"{self.signal_type} sample rate:", self.inlet.info().nominal_srate())
        info = self.inlet.info()
        self.sfreq = info.nominal_srate()
        self.n_samples = int(self.sfreq * self.window)
        self.n_chan = info.channel_count()
        self.data = np.zeros((self.n_samples, self.n_chan))
        self.times = np.arange(-self.window, 0, 1. / self.sfreq)
        firwin_size = EEG_FIRWIN_SIZE if self.signal_type == 'EEG' else PPG_FIRWIN_SIZE
        self.bf = firwin(32, np.array([1, self.firwin_size]) / (self.sfreq / 2.), width=0.05,
                 pass_zero=False)
        self.af = [1.0]

        zi = lfilter_zi(self.bf, self.af)
        self.filt_state = np.tile(zi, (self.n_chan, 1)).transpose()
        self.data_f = np.zeros((self.n_samples, self.n_chan))


    async def pull_samples(self):
        samples, timestamps = self.inlet.pull_chunk(max_samples=self.samples_per_chunk)
        if not timestamps or len(timestamps) == 1:
            return [], []

        timestamps = np.float64(np.arange(len(timestamps)))
        timestamps /= self.sfreq
        timestamps += self.times[-1] + 1. / self.sfreq
        self.times = np.concatenate([self.times, timestamps])
        self.n_samples = int(self.sfreq * self.window)
        self.times = self.times[-self.n_samples:]
        self.data = np.vstack([self.data, samples])
        self.data = self.data[-self.n_samples:]
        filt_samples, self.filt_state = lfilter(
            self.bf, self.af,
            samples,
            axis=0, zi=self.filt_state)
        self.data_f = np.vstack([self.data_f, filt_samples])
        self.data_f = self.data_f[-self.n_samples:]

        new_data = self.data_f[-self.samples_per_chunk:].tolist()
        new_times = self.times[-self.samples_per_chunk:].tolist()
        return new_data, new_times

class BioSignalStreamer:
    def __init__(self, host='0.0.0.0', port=8765):
        self.host = host
        self.port = port
        self.clients = set()
        self.eeg_streamer = GenericSignalStreamer('EEG', EEG_SAMPLE_RATE, params.NUM_EEG_SENSORS, EEG_SAMPLES_PER_CHUNK)
        self.ppg_streamer = GenericSignalStreamer('PPG', PPG_SAMPLE_RATE, params.NUM_PPG_SENSORS, PPG_SAMPLES_PER_CHUNK)
        self.data_buffer = []
        self.buffer_lock = asyncio.Lock()

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
            await asyncio.sleep(0.01)  # Small delay to allow for collation

    async def pull_samples(self):
        eeg_data, eeg_times = await self.eeg_streamer.pull_samples()
        ppg_data, ppg_times = await self.ppg_streamer.pull_samples()

        async with self.buffer_lock:
            for i, eeg_time in enumerate(eeg_times):

                datapoint = {
                    'timestamp': eeg_time,
                    'eeg_channels': eeg_data[i],
                }
                self.data_buffer.append(datapoint)

    async def send_data_to_clients(self):
        while True:
            async with self.buffer_lock:
                datapoints = self.data_buffer
                self.data_buffer = []
            if self.clients:
                for datapoint in datapoints:
                    message = json.dumps(datapoint)
                    await asyncio.gather(
                        *[client.send(message) for client in self.clients],
                        return_exceptions=True
                    )
            await asyncio.sleep(0.1)  # Adjust this delay as needed

    async def run(self):
        server = await websockets.serve(self.handle_client, self.host, self.port)
        print(f"WebSocket server started on ws://{self.host}:{self.port}")
        print("Waiting for clients to connect...")
        
        await asyncio.gather(
            server.wait_closed(),
            self.stream_data(),
            self.send_data_to_clients()
        )

async def main():
    streamer = BioSignalStreamer()
    await streamer.run()

if __name__ == "__main__":
    asyncio.run(main())

