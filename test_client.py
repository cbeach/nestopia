#import curses
import copy
import json
import socket
import struct
import inspect

#from matplotlib import pyplot as plt
#from termcolor import cprint
#import cv2
import numpy as np


port = 9090                # Reserve a port for your service.
#stdscr = curses.initscr()
#curses.cbreak()
#stdscr.keypad(1)
#
#stdscr.addstr(0, 10, "Hit 'q' to quit")
#stdscr.refresh()
#
#key = ''

button_mask = {
    'up': 1 << 1,
    'down': 1 << 2,
    'left': 1 << 3,
    'right': 1 << 4,
    'select': 1 << 5,
    'start': 1 << 6,
    'a': 1 << 7,
    'b': 1 << 8,
    'turbo_a': 1 << 9,
    'turbo_b': 1 << 10,
    'altspeed': 1 << 11,
    'insertcoin1': 1 << 12,
    'insertcoin2': 1 << 13,
    'fdsflip': 1 << 14,
    'fdsswitch': 1 << 15,
    'qsave1': 1 << 16,
    'qsave2': 1 << 17,
    'qload1': 1 << 18,
    'qload2': 1 << 19,
    'screenshot': 1 << 20,
    'reset': 1 << 21,
    'rwstart': 1 << 22,
    'rwstop': 1 << 23,
    'fullscreen': 1 << 24,
    'video_filter': 1 << 25,
    'scalefactor': 1 << 26,
    'quit': 1 << 27,
}


def pack_input(player, encoded_input):
    encoded_bytes = bytearray(5)

    mask = 255
    for i in range(4):
        encoded_bytes[i] = encoded_input & (mask << (8 * i))
    encoded_bytes[4] = player

    return struct.pack('<BBBBB', encoded_bytes[4], encoded_bytes[3], encoded_bytes[2],
                       encoded_bytes[1], encoded_bytes[0])


def encode_input(player=1, up=False, down=False, left=False, right=False, select=False,
                 start=False, a=False, b=False, turbo_a=False, turbo_b=False, altspeed=False,
                 insertcoin1=False, insertcoin2=False, fdsflip=False, fdsswitch=False,
                 qsave1=False, qsave2=False, qload1=False, qload2=False, screenshot=False,
                 reset=False, rwstart=False, rwstop=False, fullscreen=False, video_filter=False,
                 scalefactor=False, quit=False):
    frame = inspect.currentframe()
    args, _, _, values = inspect.getargvalues(frame)

    encoded_input = 0
    for arg in args:
        if values[arg] is True:
            encoded_input |= button_mask[arg]

    return pack_input(player, encoded_input)


def decode_input(encoded_input):
    decoded_input = copy.copy(button_mask)
    for k in button_mask.keys():
        if encoded_input & button_mask[k] == 0:
            decoded_input[k] = False
        else:
            decoded_input[k] = True


def decode_input_as_json(encoded_input):
    return json.dumps(decode_input(encoded_input))


class EmulatorClient:
    def __init__(self):
        self.sock = socket.socket()         # Create a socket object
        self.host = socket.gethostname()  # Get local machine name
        self.sock.connect((self.host, port))
        self.pixels = []

    def next_frame(self, packed_input, width=512, height=448, depth=4):
        self.sock.send(str(packed_input))
        while(len(self.pixels) < width * height * depth):
            chunk = self.sock.recv(width * height * depth)
            self.pixels.extend([ord(i) for i in chunk])

        ret_val = np.array(self.pixels[0:width * height * depth])\
            .reshape((width, height, depth)).astype('int8')
        self.pixels = self.pixels[width * height * depth:]
        return ret_val


if __name__ == '__main__':
    client = EmulatorClient()
    frame_count = 0
    while True:
        frame = client.next_frame(encode_input(player=2, down=True))
        frame_count += 1
        if frame_count % 100 == 0:
            print(frame_count)



#while key != ord('q'):
#    key = stdscr.getch()
#    stdscr.addch(20, 25, key)
#    stdscr.refresh()
#    if key == curses.KEY_UP:
#        stdscr.addstr(2, 20, "Up")
#    elif key == curses.KEY_DOWN:
#        stdscr.addstr(3, 20, "Down")
#
#curses.endwin()
