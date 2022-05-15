import socket
import struct
import time
from enum import IntEnum

if __name__ == '__main__':
    class Cmd(IntEnum):
        ON = 0x12
        OFF = 0x13
        COLOR = 0x20
        BRIGHTNESS = 0x21

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(('localhost', 9999))
        sock.listen()
        print('server started')
        while True:
            try:
                conn, addr = sock.accept()
                with conn:
                    print('send data')
                    conn.sendall(bytes([Cmd.ON]))
                    conn.sendall(bytes([Cmd.OFF]))
                    conn.sendall(bytes([Cmd.ON]))
                    payload = [0x1, 0x0, 0xff]
                    conn.sendall(bytes(
                        [Cmd.COLOR, *struct.pack(f'>H{len(payload)}s', len(payload), bytes(payload))]))
                    payload = [0xff, 0x0, 0x2]
                    conn.sendall(bytes(
                        [Cmd.COLOR, *struct.pack(f'>H{len(payload)}s', len(payload), bytes(payload))]))
                    conn.sendall(bytes([Cmd.OFF]))
                    conn.sendall(bytes([Cmd.BRIGHTNESS + 1]))
                    time.sleep(5)
                    payload = [0x81]
                    conn.sendall(bytes(
                        [Cmd.BRIGHTNESS, *struct.pack(f'>H{len(payload)}s', len(payload), bytes(payload))]))
            except Exception as e:
                print(f'error: {e}')
