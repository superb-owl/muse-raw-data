![Live Demo](live-monitor.gif)

# Setup
(This took some work, I may not have captured everything)

On OS X: Be sure to give your terminal application Bluetooth access in OS X settings!

```bash
python -m pip install muselsl
muselsl stream --ppg --acc --gyro &
python main.py
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
