import os

NUM_EEG_SENSORS = 5
NUM_PPG_SENSORS = 3

""" EXPERIMENTAL PARAMETERS """
# Modify these to change aspects of the signal processing

# Length of the EEG data buffer (in seconds)
# This buffer will hold last n seconds of data and be used for calculations
BUFFER_LENGTH = 10

# Length of the epochs used to compute the FFT (in seconds)
EPOCH_LENGTH = 5

# Amount of overlap between two consecutive epochs (in seconds)
OVERLAP_LENGTH = 1

if os.getenv("FAST") == "true":
    BUFFER_LENGTH = 2
    EPOCH_LENGTH = 1
    OVERLAP_LENGTH = .5

    # this gives 2 seconds of data in each frame
    # that's 512 samples per frame
    # .5 seconds of overlap, meaning we get 128 new samples each frame

# Amount to 'shift' the start of each next consecutive epoch
# NOTE: SHIFT_LENGTH * sample_rate should be an integer
SHIFT_LENGTH = EPOCH_LENGTH - OVERLAP_LENGTH

JOYSTICK_SAMPLE_RATE_HZ = 100
