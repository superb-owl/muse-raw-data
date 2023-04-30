import os
import asyncio
import threading
import time
import json
import websockets
import pygame
import numpy as np

from lib.joystick import maybe_listen_to_joystick
import lib.eeg as eeg

# WebSocket server handler function
async def websocket_handler(websocket, path):
    while True:
        fft, buckets, bands = compute_fft(eeg_buffer, eeg_sample_rate)
        ppg_fft, ppg_buckets, _ = compute_fft(ppg_buffer, ppg_sample_rate)
        data = json.dumps({
            'eeg_sample_rate': eeg_sample_rate,
            'ppg_sample_rate': ppg_sample_rate,
            'eeg_buffer': eeg_buffer.tolist(),
            'ppg_buffer': ppg_buffer.tolist(),
            'ppg_fft': ppg_fft.tolist(),
            'ppg_frequency_buckets': ppg_buckets.tolist(),
            'eeg_fft': fft.tolist(),
            'eeg_frequency_buckets': buckets.tolist(),
            'eeg_bands': bands,
        })
        try:
            await websocket.send(data)
        except Exception as e:
            break
        time.sleep(OVERLAP_LENGTH)



# Function to start the WebSocket server
async def start_server():
    server = await websockets.serve(websocket_handler, 'localhost', 8080)
    await server.wait_closed()

def start_server_in_thread(loop):
    asyncio.set_event_loop(loop)
    loop.run_until_complete(start_server())

if __name__ == "__main__":
    print('Press Ctrl-C in the console to break the while loop.')
    if os.getenv("FAKE") == "true":
        update_thread = threading.Thread(target=eeg.start_fake_eeg_loop)
    else:
        update_thread = threading.Thread(target=eeg.pull_eeg_data)
    update_thread.start()
    loop = asyncio.get_event_loop()
    server_thread = threading.Thread(target=start_server_in_thread, args=(loop,))
    server_thread.start()
    maybe_listen_to_joystick()
    print("run")
    while True:
        print("wait")
        time.sleep(1)
