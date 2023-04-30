import pygame
import time

def maybe_listen_to_joystick():
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
            time.sleep(0.01)

            # Handle events
            for event in pygame.event.get():
                if event.type == pygame.JOYAXISMOTION:
                    # Read the joystick axis values
                    x_axis = joystick.get_axis(0)
                    y_axis = joystick.get_axis(1)
                elif event.type == pygame.JOYBUTTONDOWN:
                    # Read the button that was pressed
                    button = event.button
                    print("Button pressed:", button)
