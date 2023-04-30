import os
import asyncio
import threading
import time

from lib.joystick import maybe_listen_to_joystick
import lib.eeg as eeg
import lib.server as server

if __name__ == "__main__":
    print('Press Ctrl-C in the console to break the while loop.')
    if os.getenv("FAKE") == "true":
        update_thread = threading.Thread(target=eeg.start_fake_eeg_loop)
    else:
        update_thread = threading.Thread(target=eeg.pull_eeg_data)
    update_thread.start()
    loop = asyncio.get_event_loop()
    server_thread = threading.Thread(target=server.start_server_in_thread, args=(loop,))
    server_thread.start()
    maybe_listen_to_joystick()
    print("run")
    while True:
        print("wait")
        time.sleep(1)
