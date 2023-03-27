import asyncio
import threading
import time
import json
import websockets
import numpy as np
from muselsl import stream, list_muses
from pylsl import StreamInlet, resolve_byprop

from scipy.signal import butter, lfilter, lfilter_zi

NOTCH_B, NOTCH_A = butter(4, np.array([55, 65]) / (256 / 2), btype='bandstop')

eeg_buffer = np.array([])
sample_rate = 0

class Band:
    Delta = 0
    Theta = 1
    Alpha = 2
    Beta = 3

NUM_BANDS = 5

""" EXPERIMENTAL PARAMETERS """
# Modify these to change aspects of the signal processing

# Length of the EEG data buffer (in seconds)
# This buffer will hold last n seconds of data and be used for calculations
BUFFER_LENGTH = 2

# Length of the epochs used to compute the FFT (in seconds)
EPOCH_LENGTH = 1

# Amount of overlap between two consecutive epochs (in seconds)
OVERLAP_LENGTH = 0.8

# Amount to 'shift' the start of each next consecutive epoch
SHIFT_LENGTH = EPOCH_LENGTH - OVERLAP_LENGTH

# Index of the channel(s) (electrodes) to be used
# 0 = left ear, 1 = left forehead, 2 = right forehead, 3 = right ear
INDEX_CHANNEL = [0]


def start_stream():
    muses = list_muses()
    print("streaming from", muses[0]['address'])
    stream(muses[0]['address'], ppg_enabled=True, acc_enabled=True, gyro_enabled=True)
    print("done streaming")

def pull_eeg_data():
    global eeg_buffer
    global sample_rate
    streams = []
    while len(streams) == 0:
        print("Waiting for streams...")
        time.sleep(1)
        streams = resolve_byprop('type', 'EEG', timeout=2)
    print("got streams", len(streams))

    inlet = StreamInlet(streams[0], max_chunklen=12)
    eeg_time_correction = inlet.time_correction()

    # Get the stream info and description
    info = inlet.info()
    description = info.desc()

    # Get the sampling frequency
    # This is an important value that represents how many EEG data points are
    # collected in a second. This influences our frequency band calculation.
    # for the Muse 2016, this should always be 256
    sample_rate = int(info.nominal_srate())

    """ 2. INITIALIZE BUFFERS """

    # Initialize raw EEG data buffer
    eeg_buffer = np.zeros((int(sample_rate * BUFFER_LENGTH), NUM_BANDS))

    # Compute the number of epochs in "buffer_length"
    n_win_test = int(np.floor((BUFFER_LENGTH - EPOCH_LENGTH) /
                              SHIFT_LENGTH + 1))

    # Initialize the band power buffer (for plotting)
    # bands will be ordered: [delta, theta, alpha, beta]
    band_buffer = np.zeros((n_win_test, NUM_BANDS))

    """ 3. GET DATA """

    # The try/except structure allows to quit the while loop by aborting the
    # script with <Ctrl-C>
    print('Press Ctrl-C in the console to break the while loop.')

    filter_state = None
    try:
        while True:
            eeg_data, timestamp = inlet.pull_chunk(
                timeout=1, max_samples=int(SHIFT_LENGTH * fs))

            ch_data = np.array(eeg_data)

            eeg_buffer, filter_state = update_buffer(
                eeg_buffer, ch_data, notch=True,
                filter_state=filter_state)
            print("data")
    except KeyboardInterrupt:
        print('Closing!')


def update_buffer(data_buffer, new_data, notch=False, filter_state=None):
    """
    Concatenates "new_data" into "data_buffer", and returns an array with
    the same size as "data_buffer"
    """
    if new_data.ndim == 1:
        new_data = new_data.reshape(-1, data_buffer.shape[1])

    if notch:
        if filter_state is None:
            filter_state = np.tile(lfilter_zi(NOTCH_B, NOTCH_A),
                                   (data_buffer.shape[1], 1)).T
        new_data, filter_state = lfilter(NOTCH_B, NOTCH_A, new_data, axis=0,
                                         zi=filter_state)

    new_buffer = np.concatenate((data_buffer, new_data), axis=0)
    new_buffer = new_buffer[new_data.shape[0]:, :]

    return new_buffer, filter_state

def nextpow2(i):
    """
    Find the next power of 2 for number i
    """
    n = 1
    while n < i:
        n *= 2
    return n

def get_band(where, PSD):
    return np.mean(PSD[where, :], axis=1)[0]

def compute_fft(data):
    winSampleLength, nbCh = data.shape

    # Apply Hamming window
    w = np.hamming(winSampleLength)
    dataWinCentered = data - np.mean(data, axis=0)  # Remove offset
    dataWinCenteredHam = (dataWinCentered.T * w).T

    NFFT = nextpow2(winSampleLength)
    Y = np.fft.fft(dataWinCenteredHam, n=NFFT, axis=0) / winSampleLength
    PSD = 2 * np.abs(Y[0:int(NFFT / 2), :])
    f = sample_rate / 2 * np.linspace(0, 1, int(NFFT / 2))

    bands = {
            'delta': get_band(np.where(f < 4), PSD).tolist(),
            'theta': get_band(np.where((f >= 4) & (f <= 8)), PSD).tolist(),
            'alpha': get_band(np.where((f >= 8) & (f <= 12)), PSD).tolist(),
            'beta': get_band(np.where((f >= 12) & (f < 30)), PSD).tolist(),
            'gamma': get_band(np.where((f >= 30) & (f < 80)), PSD).tolist(),
    }

    return PSD, bands

# WebSocket server handler function
async def websocket_handler(websocket, path):
    global eeg_buffer
    while True:
        fft, bands = compute_fft(eeg_buffer)
        data = json.dumps({
            'sample_rate': sample_rate,
            'fft': fft.tolist(),
            'bands': bands,
            'eeg_buffer': eeg_buffer.tolist(),
        })
        try:
            await websocket.send(data)
        except (ConnectionClosed):
            break

# Function to start the WebSocket server
async def start_server():
    server = await websockets.serve(websocket_handler, 'localhost', 8080)
    await server.wait_closed()

if __name__ == "__main__":
    update_thread = threading.Thread(target=pull_eeg_data)
    update_thread.start()
    asyncio.run(start_server())
