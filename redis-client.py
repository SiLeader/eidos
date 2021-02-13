#!/usr/bin/env python3

import argparse
import socket
import typing


def __parse_response(payload: bytes) -> typing.List[bytes]:
    payload_lines = payload.split(b'\r\n')
    if len(payload_lines) == 0:
        pass
    first_line = payload_lines[0]
    if len(first_line) <= 1:
        pass

    if first_line[0] == b'*':
        whole_length = int(first_line[1:].decode())
    elif first_line[0] == b'+':
        return [first_line[1:]]
    elif first_line[0] == b'-':
        return [first_line[1:]]
    else:
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-s', '--server', help='server host', default='localhost')
    parser.add_argument('-p', '--port', help='port number', default=6379, type=int)
    parser.add_argument('-r', '--raw', help='show raw payload from server', action='store_true')
    args = parser.parse_args()

    server = args.server
    port = args.port
    is_raw = args.raw

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((server, port))

    try:
        while True:
            cmd = input(f'{server}:{port} > ')
            if cmd.startswith('exit') or cmd.startswith('quit'):
                break
            cmds = [s.encode(encoding='utf-8') for s in cmd.split(' ')]
            sock.send(b'*' + str(len(cmds)).encode() + b'\r\n')
            for c in cmds:
                sock.send(b'$' + str(len(c)).encode() + b'\r\n')
                sock.send(c + b'\r\n')
            buffer = sock.recv(4096)
            if buffer is None:
                break
            if is_raw:
                print(f'< {server}:{port} raw')
                print(buffer.decode())
    finally:
        sock.close()


if __name__ == '__main__':
    main()
