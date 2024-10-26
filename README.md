![Live Demo](live-monitor.gif)

# Setup
(This took some work, I may not have captured everything)

On OS X: Be sure to give your terminal application Bluetooth access in OS X settings!

```bash
python -m pip install muselsl
muselsl stream --ppg --acc --gyro &
FAST=true python main.py
```

If you don't have your Muse headband handy, you can also run with the pre-recorded data in `fake_data.csv`:
```bash
FAKE=true python main.py
```

All sensor data will be saved to `recording.csv`

To view the web UI, just serve this directory. Example:
```
npm i -g http-server
http-server -p 3000 -c-1 .
```

## Data
The server in main.py serves a websocket at localhost:8080. It sends periodic
messages that look like this:
```json
{
    "eeg_sample_rate": 256,
    "ppg_sample_rate": 64,
    "eeg_buffer": [
        [
            -490.4181751397289,
            -489.13230368008897,
            373.5735741905769,
            205.76140926048873,
            1.33e-322
        ],
        [
            -11.487527872175292,
            -151.39057010439757,
            90.26580003568375,
            123.99942165052266,
            9.04e-322
        ]
    ],
    "ppg_buffer": [
        [
            37347.791021466386,
            32629.174669250584,
            45.893370131341975
        ],
        [
            37318.79773465421,
            32629.238191741075,
            29.57228107959109
        ]
    ],
    "joystick_buffer": [
        [
            0.0,
            0.0
        ],
        [
            0.0,
            0.0
        ]
    ],
    "eeg_fft": [
        [
            3.747081021941294,
            4.296221316276325,
            3.3027627156835675,
            9.291624268461398,
            0.0
        ],
        [
            24.495291881449763,
            18.902058964880545,
            23.326688550180645,
            30.786032904487556,
            0.0
        ],
    ],
    "ppg_fft": [
        [
            2.6107681430051235,
            0.212275377522958,
            1.0345045784915796
        ],
        [
            7.138529888528041,
            6.719424881742815,
            4.24350013820273
        ]
    ],
    "joystick_fft": [
        [
            0.0,
            0.0
        ],
        [
            0.0,
            0.0
        ]
    ],
    "eeg_frequency_buckets": [
        0.0,
        0.5019607843137255,
        1.003921568627451,
        1.5058823529411764,
        2.007843137254902,
    ]
    "ppg_frequency_buckets": [
        0.0,
        0.5079365079365079,
        1.0158730158730158,
        1.5238095238095237,
        2.0317460317460316,
    ],
    "joystick_frequency_buckets": [
        0.0,
        0.39370078740157477,
        0.7874015748031495,
        1.1811023622047243,
    ]
    "eeg_bands": {
        "delta": [
            67.21705548784225,
            61.50382185986569,
            81.35876113831638,
            70.1032592366832,
            0.0
        ],
        "theta": [
            113.5268580585179,
            115.41964142762019,
            41.72016526649826,
            119.86190952493935,
            0.0
        ],
        "alpha": [
            178.5851406118272,
            181.04826337912993,
            67.46725773998459,
            168.22189180360928,
            0.0
        ],
        "beta": [
            170.26359288902364,
            176.31768354873032,
            41.00552486574535,
            171.3152899241239,
            0.0
        ],
        "gamma": [
            95.22927251508904,
            100.66750983667511,
            24.810665083138275,
            82.08265662900305,
            1.7e-322
        ]
    }
}
```
