# Echo verification through Nov 6 (run 2026-11-05)

This uses a local Python protocol fixture, not the team server.
The fixture calls recv(1) to force assembly; TCP does not preserve send boundaries.

## normal send / whole response
Expected success: True; exit code: 0
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62962
TX: 67 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="fragment-test"
```

## one-byte send / whole response
Expected success: True; exit code: 0
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62964
TX: 67 bytes, 67 successful send calls (1 byte per call).
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="fragment-test"
```

## one-byte send / split response including header
Expected success: True; exit code: 0
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62969
TX: 67 bytes, 67 successful send calls (1 byte per call).
RX: 1 bytes, accumulated 1.
RX: 1 bytes, accumulated 2.
RX: 1 bytes, accumulated 3.
RX: 1 bytes, accumulated 4.
RX: 1 bytes, accumulated 5.
RX: 1 bytes, accumulated 6.
RX: 1 bytes, accumulated 7.
RX: 1 bytes, accumulated 8.
RX: 1 bytes, accumulated 9.
RX: 1 bytes, accumulated 10.
RX: 1 bytes, accumulated 11.
RX: 1 bytes, accumulated 12.
RX: 1 bytes, accumulated 13.
RX: 1 bytes, accumulated 14.
RX: 1 bytes, accumulated 15.
RX: 1 bytes, accumulated 16.
RX: 1 bytes, accumulated 17.
RX: 1 bytes, accumulated 18.
RX: 1 bytes, accumulated 19.
RX: 1 bytes, accumulated 20.
RX: 1 bytes, accumulated 21.
RX: 1 bytes, accumulated 22.
RX: 1 bytes, accumulated 23.
RX: 1 bytes, accumulated 24.
RX: 1 bytes, accumulated 25.
RX: 1 bytes, accumulated 26.
RX: 1 bytes, accumulated 27.
RX: 1 bytes, accumulated 28.
RX: 1 bytes, accumulated 29.
RX: 1 bytes, accumulated 30.
RX: 1 bytes, accumulated 31.
RX: 1 bytes, accumulated 32.
RX: 1 bytes, accumulated 33.
RX: 1 bytes, accumulated 34.
RX: 1 bytes, accumulated 35.
RX: 1 bytes, accumulated 36.
RX: 1 bytes, accumulated 37.
RX: 1 bytes, accumulated 38.
RX: 1 bytes, accumulated 39.
RX: 1 bytes, accumulated 40.
RX: 1 bytes, accumulated 41.
RX: 1 bytes, accumulated 42.
RX: 1 bytes, accumulated 43.
RX: 1 bytes, accumulated 44.
RX: 1 bytes, accumulated 45.
RX: 1 bytes, accumulated 46.
RX: 1 bytes, accumulated 47.
RX: 1 bytes, accumulated 48.
RX: 1 bytes, accumulated 49.
RX: 1 bytes, accumulated 50.
RX: 1 bytes, accumulated 51.
RX: 1 bytes, accumulated 52.
RX: 1 bytes, accumulated 53.
RX: 1 bytes, accumulated 54.
RX: 1 bytes, accumulated 55.
RX: 1 bytes, accumulated 56.
RX: 1 bytes, accumulated 57.
RX: 1 bytes, accumulated 58.
RX: 1 bytes, accumulated 59.
RX: 1 bytes, accumulated 60.
RX: 1 bytes, accumulated 61.
RX: 1 bytes, accumulated 62.
RX: 1 bytes, accumulated 63.
RX: 1 bytes, accumulated 64.
RX: 1 bytes, accumulated 65.
RX: 1 bytes, accumulated 66.
RX: 1 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="fragment-test"
```

## incomplete response / disconnect
Expected success: False; exit code: 1
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62971
TX: 67 bytes, 1 successful send calls (normal).
RX: 9 bytes, accumulated 9.
Disconnected with 9 incomplete bytes.
```

## wrong response type
Expected success: False; exit code: 1
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62973
TX: 67 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
Unexpected header: size=67 type=1.
```

## wrong response size
Expected success: False; exit code: 1
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62975
TX: 67 bytes, 1 successful send calls (normal).
RX: 3 bytes, accumulated 3.
Unexpected header: size=2 type=2.
```

## wrong echoed content
Expected success: False; exit code: 1
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62977
TX: 67 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
Echo content mismatch.
```

## no response / timeout
Expected success: False; exit code: 1
Fixture reconstructed packets: 1
```text
Connected: 127.0.0.1:62979
TX: 67 bytes, 1 successful send calls (normal).
Receive timeout/error.
```

## three requests / combined echoes
Expected success: True; exit code: 0
Fixture reconstructed packets: 3
```text
Connected: 127.0.0.1:62982
TX: 201 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="1:fragment-test"
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="2:fragment-test"
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="3:fragment-test"
PASS: 3 ordered echoes for 201 bundled bytes.
```

## three requests / split inside next header
Expected success: True; exit code: 0
Fixture reconstructed packets: 3
```text
Connected: 127.0.0.1:62984
TX: 201 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="1:fragment-test"
RX: 1 bytes, accumulated 1.
RX: 66 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="2:fragment-test"
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="3:fragment-test"
PASS: 3 ordered echoes for 201 bundled bytes.
```

## three requests / reversed responses
Expected success: False; exit code: 1
Fixture reconstructed packets: 3
```text
Connected: 127.0.0.1:62986
TX: 201 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
Echo content mismatch.
```

## three requests / missing response
Expected success: False; exit code: 1
Fixture reconstructed packets: 3
```text
Connected: 127.0.0.1:62989
TX: 201 bytes, 1 successful send calls (normal).
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="1:fragment-test"
RX: 67 bytes, accumulated 67.
PASS: S2C_Echo, size=67, message="2:fragment-test"
Disconnected with 0 incomplete bytes.
```

## size 2 / peer closes
Expected success: True; exit code: 0
Fixture reconstructed packets: 0
```text
Connected: 127.0.0.1:62991
TX: 3 bytes, 1 successful send calls (normal).
PASS: peer disconnected after size=2.
```

## size 1025 / peer closes
Expected success: True; exit code: 0
Fixture reconstructed packets: 0
```text
Connected: 127.0.0.1:62993
TX: 3 bytes, 1 successful send calls (normal).
PASS: peer disconnected after size=1025.
```

## invalid size / peer replies
Expected success: False; exit code: 1
Fixture reconstructed packets: 0
```text
Connected: 127.0.0.1:62995
TX: 3 bytes, 1 successful send calls (normal).
FAIL: expected peer disconnect without reply.
```

## invalid size / peer stays open
Expected success: False; exit code: 1
Fixture reconstructed packets: 0
```text
Connected: 127.0.0.1:62997
TX: 3 bytes, 1 successful send calls (normal).
FAIL: no disconnect observed before deadline.
```

## Invalid arguments
PASS: missing arguments, invalid IPv4, invalid mode, invalid port.
