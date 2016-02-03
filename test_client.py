#import curses
import socket

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


class TestClient:
    def __init__(self):
        self.sock = socket.socket()         # Create a socket object
        self.host = socket.gethostname()  # Get local machine name
        self.sock.connect((self.host, port))

    def getFrame(self, width=512, height=448, depth=4):
        pixels = []
        while(len(pixels) < width * height * depth):
            print(pixels)
            print(len(pixels))
            chunk = self.sock.recv(width * height * depth)
            pixels.extend(chunk)
        pixels = np.array(pixels).reshape((width, height, depth))
        return pixels



if __name__ == '__main__':
   client = TestClient()
   print(client.getFrame())

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
