import numpy as np
import util

def compute_fft(data, sample_rate):
    winSampleLength, nbCh = data.shape

    # Apply Hamming window
    w = np.hamming(winSampleLength)
    dataWinCentered = data - np.mean(data, axis=0)  # Remove offset
    dataWinCenteredHam = (dataWinCentered.T * w).T

    NFFT = util.nextpow2(winSampleLength)
    Y = np.fft.fft(dataWinCenteredHam, n=NFFT, axis=0) / winSampleLength
    PSD = 2 * np.abs(Y[0:int(NFFT / 2), :])
    freq_buckets = sample_rate / 2 * np.linspace(0, 1, int(NFFT / 2))
    f = freq_buckets

    bands = {
            'delta': get_band(1, 4, f, PSD).tolist(),
            'theta': get_band(4, 8, f, PSD).tolist(),
            'alpha': get_band(8, 12, f, PSD).tolist(),
            'beta':  get_band(12, 30, f, PSD).tolist(),
            'gamma': get_band(30, 80, f, PSD).tolist(),
    }

    return PSD, freq_buckets, bands

def get_band(start, end, f, PSD):
    # Band power is set to max of all relevant frequencies
    # Other people take the sum or average
    # Max seems like the best measure IMO--no penalty for a tight peak
    return np.max(PSD[np.where((f >= start) & (f < end)), :], axis=1)[0]
