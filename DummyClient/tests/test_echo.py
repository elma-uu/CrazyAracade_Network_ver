"""Local protocol fixture, NOT the team's production server."""
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'bin' / 'Release' / 'DummyClient.exe'
results = []

def run_case(name, mode, response, expect_pass):
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(10)
    port = listener.getsockname()[1]
    observed = []
    errors = []
    def serve():
        try:
            with listener:
                conn, _ = listener.accept()
                with conn:
                    conn.settimeout(6)
                    def read_exact(size):
                        data = bytearray()
                        while len(data) < size:
                            part = conn.recv(size - len(data))
                            if not part: raise AssertionError("incomplete request")
                            data.extend(part)
                        return data

                    if mode.startswith('bad-'):
                        header = read_exact(3)
                        size, kind = struct.unpack('<HB', header)
                        assert kind == 1 and size == (2 if mode == 'bad-small' else 1025)
                        if response == 'unexpected_reply': conn.sendall(b'X')
                        if response == 'ignore': time.sleep(5.5)
                        return  # close without processing an invalid packet
                    if mode == 'bundle':
                        data = read_exact(201)
                        replies = []
                        while data:
                            size, kind = struct.unpack_from('<HB', data)
                            assert size == 67 and kind == 1
                            body = bytes(data[3:size])
                            number = len(observed) + 1
                            assert body == f'{number}:fragment-test'.encode().ljust(64, b'\0')
                            observed.append(body)
                            replies.append(struct.pack('<HB', 67, 2) + body)
                            del data[:size]
                        assert len(observed) == 3
                        if response == 'reordered': replies.reverse()
                        if response == 'missing': replies.pop()
                        wire = b''.join(replies)
                        if response == 'cross-boundary':
                            conn.sendall(wire[:68]); time.sleep(.03); conn.sendall(wire[68:])
                        else: conn.sendall(wire)
                        return
                    data = bytearray()
                    # Deliberately read ONE byte to force incremental server-side assembly.
                    while len(data) < 67:
                        part = conn.recv(1)
                        if not part: raise AssertionError('premature EOF')
                        data.extend(part)
                        if len(data) < 3: continue
                        size, packet_type = struct.unpack('<HB', data[:3])
                        assert size == 67 and packet_type == 1
                        if len(data) == size: observed.append(bytes(data))
                    assert len(observed) == 1
                    assert data[3:] == b'fragment-test' + bytes(51)
                    reply = struct.pack('<HB', 67, 2) + data[3:]
                    if response == 'whole': conn.sendall(reply)
                    elif response == 'split':
                        for b in reply:
                            conn.sendall(bytes([b])); time.sleep(.003)
                    elif response == 'truncated': conn.sendall(reply[:9])
                    elif response == 'wrong_type': conn.sendall(struct.pack('<HB',67,1)+reply[3:])
                    elif response == 'wrong_size': conn.sendall(struct.pack('<HB',2,2))
                    elif response == 'wrong_content': conn.sendall(reply[:3]+b'X'+reply[4:])
                    elif response == 'timeout': time.sleep(5.5)
        except Exception as e: errors.append(repr(e))
    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    proc = subprocess.run([str(EXE),'127.0.0.1',mode,'fragment-test',str(port)],capture_output=True,text=True,timeout=12)
    thread.join(8)
    assert not thread.is_alive() and not errors, errors
    assert (proc.returncode == 0) == expect_pass, proc.stdout + proc.stderr
    if mode == 'byte': assert '67 successful send calls' in proc.stdout
    results.append(f'## {name}\nExpected success: {expect_pass}; exit code: {proc.returncode}\nFixture reconstructed packets: {len(observed)}\n```text\n{proc.stdout.strip()}\n```\n')
    print('PASS',name,flush=True)

for case in [
    ('normal send / whole response','normal','whole',True),
    ('one-byte send / whole response','byte','whole',True),
    ('one-byte send / split response including header','byte','split',True),
    ('incomplete response / disconnect','normal','truncated',False),
    ('wrong response type','normal','wrong_type',False),
    ('wrong response size','normal','wrong_size',False),
    ('wrong echoed content','normal','wrong_content',False),
    ('no response / timeout','normal','timeout',False),
    ('three requests / combined echoes','bundle','whole',True),
    ('three requests / split inside next header','bundle','cross-boundary',True),
    ('three requests / reversed responses','bundle','reordered',False),
    ('three requests / missing response','bundle','missing',False),
    ('size 2 / peer closes','bad-small','close',True),
    ('size 1025 / peer closes','bad-large','close',True),
    ('invalid size / peer replies','bad-small','unexpected_reply',False),
    ('invalid size / peer stays open','bad-large','ignore',False),
]: run_case(*case)
for args in [[], ['not-an-ip','normal'], ['127.0.0.1','bad-mode'], ['127.0.0.1','normal','x','0']]:
    result = subprocess.run([str(EXE),*args],capture_output=True,text=True,timeout=8)
    assert result.returncode != 0
results.append('## Invalid arguments\nPASS: missing arguments, invalid IPv4, invalid mode, invalid port.\n')
(ROOT/'TEST_RESULTS.md').write_text('# Echo verification through Nov 6 (run 2026-11-05)\n\nThis uses a local Python protocol fixture, not the team server.\nThe fixture calls recv(1) to force assembly; TCP does not preserve send boundaries.\n\n'+'\n'.join(results),encoding='utf-8')
print('All 20 cases passed.',flush=True)
