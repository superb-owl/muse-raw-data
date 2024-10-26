import time
import csv
import numpy as np
from muselsl import stream, list_muses
from pylsl import StreamInlet, resolve_byprop
from scipy.ndimage import interpolation

import lib.params as params
import lib.util as util
from lib.fft import compute_fft

class Band:
    Delta = 0
    Theta = 1
    Alpha = 2
    Beta = 3

eeg_sample_rate = 256 # Will be set explicitly below, in case it's different
ppg_sample_rate = 64  # Will be set explicitly below, in case it's different

eeg_buffer = np.zeros((int(eeg_sample_rate * params.BUFFER_LENGTH), params.NUM_EEG_SENSORS))
ppg_buffer = np.zeros((int(ppg_sample_rate * params.BUFFER_LENGTH), params.NUM_PPG_SENSORS))

def pull_eeg_data():
    global eeg_sample_rate
    global ppg_sample_rate
    eeg_streams = []
    ppg_streams = []
    while len(eeg_streams) == 0 or len(ppg_streams) == 0:
        print("Waiting for streams...")
        time.sleep(1)
        eeg_streams = resolve_byprop('type', 'EEG', timeout=2)
        ppg_streams = resolve_byprop('type', 'PPG', timeout=2)
    print("got streams", len(eeg_streams), len(ppg_streams))

    eeg_inlet = StreamInlet(eeg_streams[0], max_chunklen=12)
    ppg_inlet = StreamInlet(ppg_streams[0], max_chunklen=12)

    # Get the sampling frequency
    # This is an important value that represents how many EEG data points are
    # collected in a second. This influences our frequency band calculation.
    # for the Muse 2016, this should always be 256
    print("sample rates", eeg_inlet.info().nominal_srate(), ppg_inlet.info().nominal_srate())
    eeg_sample_rate = int(eeg_inlet.info().nominal_srate())
    ppg_sample_rate = int(ppg_inlet.info().nominal_srate())

    start_eeg_loop(eeg_inlet, ppg_inlet)

def start_fake_eeg_loop():
    global eeg_buffer
    global ppg_buffer
    eeg_buffer = np.zeros((int(eeg_sample_rate * params.BUFFER_LENGTH), params.NUM_EEG_SENSORS))
    ppg_buffer = np.zeros((int(ppg_sample_rate * params.BUFFER_LENGTH), params.NUM_PPG_SENSORS))

    eeg_filter_state = None
    ppg_filter_state = None
    with open('fake_data.csv', 'r') as f:
        reader = csv.reader(f)
        lines = [row for row in reader][1:]
        timestamps = [row[0] for row in lines]
        eeg_data = [row[1:6] for row in lines]
        eeg_data = [[float(x) for x in row] for row in eeg_data]
        ppg_data = [row[6:9] for row in lines]
        ppg_data = [[float(x) for x in row] for row in ppg_data]

    cur_idx = 0

    try:
        while True:
            start_idx = cur_idx
            end_idx = start_idx + int(params.SHIFT_LENGTH * eeg_sample_rate)
            if end_idx > len(eeg_data):
                cur_idx = 0
                continue
            timestamp = timestamps[start_idx:end_idx]
            eeg_slice = eeg_data[start_idx:end_idx]
            ppg_slice = ppg_data[start_idx:end_idx]
            cur_idx = end_idx

            eeg_slice = np.array(eeg_slice)
            ppg_slice = np.array(ppg_slice)

            eeg_buffer, eeg_filter_state = util.update_buffer(
                eeg_buffer, eeg_slice, notch=True,
                filter_state=eeg_filter_state)
            ppg_buffer, ppg_filter_state = util.update_buffer(
                ppg_buffer, ppg_slice, notch=True,
                filter_state=ppg_filter_state)
            time.sleep(params.SHIFT_LENGTH)
            print("data")
    except KeyboardInterrupt:
        print('Closing!')

def start_eeg_loop(eeg_inlet, ppg_inlet):
    global eeg_buffer
    global ppg_buffer
    eeg_buffer = np.zeros((int(eeg_sample_rate * params.BUFFER_LENGTH), params.NUM_EEG_SENSORS))
    ppg_buffer = np.zeros((int(ppg_sample_rate * params.BUFFER_LENGTH), params.NUM_PPG_SENSORS))

    eeg_filter_state = None
    ppg_filter_state = None
    recording = None
    if os.getenv('RECORDING_FILE', ''):
        f = open(os.getenv('RECORDING_FILE'), 'w')
        recording = csv.writer(f)
    try:
        while True:
            ppg_data, ppg_timestamp = ppg_inlet.pull_chunk(
                timeout=1, max_samples=int(params.SHIFT_LENGTH * ppg_sample_rate))
            eeg_data, eeg_timestamp = eeg_inlet.pull_chunk(
                timeout=1, max_samples=int(params.SHIFT_LENGTH * eeg_sample_rate))
            ppg_data = np.array(ppg_data)
            eeg_data = np.array(eeg_data)
            # Don't try to sync timestamps--just collate the data
            # TODO: can try and sync timestamps, but probably hard to do. Off by ~200ms right now
            # print("PPG", ppg_timestamp[0], ppg_timestamp[-1])
            # print("EEG", eeg_timestamp[0], eeg_timestamp[-1])
            ppg_data_big = interpolation.zoom(ppg_data, (len(eeg_data) / len(ppg_data), 1.0))
            eeg_timestamp = np.array([eeg_timestamp]).T
            if recording:
                recording_data = np.concatenate((eeg_timestamp, eeg_data, ppg_data_big), axis=1)
                recording.writerows(recording_data)

            eeg_buffer, eeg_filter_state = util.update_buffer(
                eeg_buffer, eeg_data,
                notch=True,
                filter_state=eeg_filter_state)
            ppg_buffer, ppg_filter_state = util.update_buffer(
                ppg_buffer, ppg_data,
                notch=False,
                filter_state=ppg_filter_state)
            print("data")
    except KeyboardInterrupt:
        f.close()
        print('Closing!')

def get_data():
    fft, buckets, bands = compute_fft(eeg_buffer, eeg_sample_rate)
    ppg_fft, ppg_buckets, _ = compute_fft(ppg_buffer, ppg_sample_rate)
    return {
        'eeg_sample_rate': eeg_sample_rate,
        'ppg_sample_rate': ppg_sample_rate,
        'eeg_buffer': eeg_buffer.tolist(),
        'ppg_buffer': ppg_buffer.tolist(),
        'ppg_fft': ppg_fft.tolist(),
        'ppg_frequency_buckets': ppg_buckets.tolist(),
        'eeg_fft': fft.tolist(),
        'eeg_frequency_buckets': buckets.tolist(),
        'eeg_bands': bands,
    }
