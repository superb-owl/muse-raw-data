import pygame
import time
import numpy as np
import lib.params as params

NUM_AXES = 2
joystick_buffer = np.zeros((int(params.JOYSTICK_SAMPLE_RATE_HZ * params.BUFFER_LENGTH), NUM_AXES))

def maybe_listen_to_joystick():
    global joystick_buffer
    # Initialize pygame and the joystick module
    pygame.init()
    pygame.joystick.init()

    # Check if there are any joysticks available
    joystick_count = pygame.joystick.get_count()
    if joystick_count == 0:
        print("No joysticks available")
    else:
        # Set up the joystick object
        joystick = pygame.joystick.Joystick(0)
        joystick.init()

        # Print the name of the joystick
        print("Joystick name:", joystick.get_name())

        # Print the number of axes and buttons on the joystick
        num_axes = joystick.get_numaxes()
        num_buttons = joystick.get_numbuttons()
        print("Number of axes:", num_axes)
        print("Number of buttons:", num_buttons)

        # Loop indefinitely and read the joystick inputs
        while True:
            # Wait a short amount of time to avoid using too much CPU
            sleep_amt = 1.0 / float(params.JOYSTICK_SAMPLE_RATE_HZ)
            time.sleep(sleep_amt)

            # Handle events
            for event in pygame.event.get():
                if event.type == pygame.JOYAXISMOTION:
                    # Read the joystick axis values
                    x_axis = joystick.get_axis(0)
                    y_axis = joystick.get_axis(1)
                    joystick_buffer = np.roll(joystick_buffer, -1, axis=0)
                    joystick_buffer[-1] = np.array([x_axis, y_axis])
                elif event.type == pygame.JOYBUTTONDOWN:
                    # Read the button that was pressed
                    button = event.button
                    print("Button pressed:", button)

def get_data():
    return {
        'joystick_buffer': joystick_buffer.tolist(),
    }
