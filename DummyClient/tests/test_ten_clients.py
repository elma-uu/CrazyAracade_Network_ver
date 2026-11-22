"""Run ten dummy clients against a local fixture or the team echo server."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack
from pathlib import Path
import socket
import struct
import subprocess
import threading

EXE = Path(__file__).resolve().parents[1] / "bin/Release/DummyClient.exe"


def serve_ten(listener):
    # Keep all ten connections open before answering the first request.
    with ExitStack() as connections:
        for _ in range(10):
            conn, _ = listener.accept()
            connections.enter_context(conn)
            conn.settimeout(4)
            data = bytearray()
            while len(data) < 67:
                chunk = conn.recv(67 - len(data))
                if not chunk:
                    raise RuntimeError("Incomplete echo request")
                data.extend(chunk)
            if struct.unpack_from("<HB", data) != (67, 1):
                raise ValueError("Unexpected echo header")
            data[2] = 2
            connections.callback(conn.sendall, bytes(data))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server", nargs="?", help="team echo server IPv4")
    parser.add_argument("--port", type=int, default=9000)
    args = parser.parse_args()
    with ExitStack() as resources:
        host, port = args.server, args.port
        if host is None:
            listener = resources.enter_context(socket.socket())
            listener.bind(("127.0.0.1", 0))
            listener.listen(10)
            listener.settimeout(5)
            host, port = listener.getsockname()
        pool = resources.enter_context(ThreadPoolExecutor(max_workers=11))
        fixture = pool.submit(serve_ten, listener) if args.server is None else None
        start = threading.Barrier(10)

        def run_client(index):
            start.wait(timeout=5)
            subprocess.run([EXE, host, "normal", f"client-{index}", str(port)],
                           check=True, timeout=10)

        list(pool.map(run_client, range(1, 11)))
        if fixture:
            fixture.result()
    print("PASS: 10 clients received their own echoes")


if __name__ == "__main__":
    main()
