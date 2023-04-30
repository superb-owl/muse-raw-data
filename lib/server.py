import asyncio
import websockets
import json
import time

import lib.eeg as eeg
import lib.joystick as joystick
import lib.params as params

# WebSocket server handler function
async def websocket_handler(websocket, path):
    while True:
        data = eeg.get_data()
        data.update(joystick.get_data())
        try:
            await websocket.send(json.dumps(data))
        except Exception as e:
            break
        time.sleep(params.OVERLAP_LENGTH)

# Function to start the WebSocket server
async def start_server():
    server = await websockets.serve(websocket_handler, 'localhost', 8080)
    await server.wait_closed()

def start_server_in_thread(loop):
    asyncio.set_event_loop(loop)
    loop.run_until_complete(start_server())

