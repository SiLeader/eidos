import argparse
import socket


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', help='port number', default=6379, type=int)
    args = parser.parse_args()

    port = args.port

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', port))

    try:
        while True:
            cmd = input(f'localhost:{port} > ')
            if cmd.startswith('exit') or cmd.startswith('quit'):
                break
            cmds = [s.encode(encoding='utf-8') for s in cmd.split(' ')]
            sock.send(b'*' + str(len(cmds)).encode() + b'\r\n')
            for c in cmds:
                sock.send(b'$' + str(len(c)).encode() + b'\r\n')
                sock.send(c + b'\r\n')
            buffer = sock.recv(4096)
            print(f'< localhost:{port}')
            print(buffer.decode())
    finally:
        sock.close()


if __name__ == '__main__':
    main()
