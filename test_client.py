#import curses
import inspect
import json
import socket
import sys
import time

#from matplotlib import pyplot as plt
#from termcolor import cprint
import cv2
import numpy as np

MB = 2 ** 20
port = 9090


def show_image(image):
    cv2.imshow('image', image)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        sys.exit(0)
    #cv2.destroyAllWindows()


def encode_input(player=0, up=False, down=False, left=False, right=False, select=False,
                 start=False, a=False, b=False, turbo_a=False, turbo_b=False, altspeed=False,
                 insertcoin1=False, insertcoin2=False, fdsflip=False, fdsswitch=False,
                 qsave1=False, qsave2=False, qload1=False, qload2=False, screenshot=False,
                 reset=False, rwstart=False, rwstop=False, fullscreen=False, video_filter=False,
                 scalefactor=False, quit=False):
    buttons = ['up', 'down', 'left', 'right', 'select', 'start', 'a', 'b', 'turbo_a', 'turbo_b',
               'altspeed', 'insertcoin1', 'insertcoin2', 'fdsflip', 'fdsswitch', 'qsave1',
               'qsave2', 'qload1', 'qload2', 'screenshot', 'reset', 'rwstart', 'rwstop',
               'fullscreen', 'video_filter', 'scalefactor', 'quit']

    frame = inspect.currentframe()
    args, _, _, values = inspect.getargvalues(frame)

    pressed_buttons = []
    for arg in args:
        if values[arg] is True and arg in buttons:
            pressed_buttons.append(arg)
    return json.dumps({
        'controls': pressed_buttons,
        'player': player,
    })


class EmulatorClient:
    def __init__(self, rom_file):
        self.sock = socket.socket()         # Create a socket object
        self.host = socket.gethostname()  # Get local machine name
        self.sock.connect((self.host, port))
        self.sock.send(json.dumps({
            'rom_file': rom_file
        }))
        response = json.loads(self.sock.recv(MB))
        width = int(response['width'])
        height = int(response['height'])
        scale = int(response['scale'])
        self.scale = scale
        self.width = width * scale
        self.height = height * scale
        self.depth = 4
        self.frame = np.empty((self.height, self.width, self.depth), dtype=np.uint8)
        self.non_black_frame_received = False
        self.frame_size = self.height * self.width * self.depth

    def next_frame(self, packed_input):
        self.sock.send(packed_input)
        self.sock.recv_into(self.frame, self.frame_size, socket.MSG_WAITALL)
        return self.frame


if __name__ == '__main__':
    start_time = time.time()
    rom = '/home/mcsmash/dev/nestopia/smb.nes'
    client = EmulatorClient(rom)
    frame_count = 0
    control_sequence = []
    control_sequence.append(
        (175, 1, {
            'start': True,
        })
    )
    control_sequence.append(
        (225, 1, {
            'start': True,
        })
    )
    control_sequence.append(
        (400, 1, {
            'a': True,
        })
    )
    countdown = 0
    current_control_sequence = None
    while True:
        if current_control_sequence is None:
            current_control_sequence = control_sequence.pop(0)
        if current_control_sequence[0] == frame_count:
            countdown = current_control_sequence[1]
        if countdown > 0:
            countdown -= 1
            frame = client.next_frame(encode_input(**current_control_sequence[2]))
        else:
            if frame_count > current_control_sequence[0]:
                try:
                    current_control_sequence = control_sequence.pop(0)
                except(IndexError):
                    pass
            frame = client.next_frame(encode_input())
        show_image(frame)
        frame_count += 1
        print('frames/sec: {}'.format(frame_count / (time.time() - start_time)))
