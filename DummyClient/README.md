# 더미 클라이언트 — 민규동, 11월 6일까지

계획서의 10/31 뼈대, 11/1 connect/send, 11/2 recv 루프, 11/3 1바이트 분할 송신, 11/4 묶음 송신, 11/5 비정상 size 검증, 11/6 결과 정리까지 구현·검증했다.
게임 화면 없이 서버의 에코 응답을 확인하는 Windows C++ 콘솔 프로그램이다.
기존 게임 소스나 팀원 담당 서버를 수정하지 않았다.

## 열기와 실행

`DummyClient.sln`을 Visual Studio 2022에서 열고 x64 Debug 또는 Release로 빌드한다.
실행 파일은 `bin\Release\DummyClient.exe` 또는 `bin\Debug\DummyClient.exe`이다.

서버를 먼저 실행하고, 명령 프롬프트에서 exe 폴더로 이동한다.
아래 IP는 예시이므로 실제 서버 PC의 IPv4로 바꾼다.

```bat
DummyClient.exe 192.168.0.10 normal "hello"
DummyClient.exe 192.168.0.10 byte "fragment test"
DummyClient.exe 192.168.0.10 bundle
DummyClient.exe 192.168.0.10 bad-small
DummyClient.exe 192.168.0.10 bad-large
```

- normal: 패킷 전체를 전송한다. 부분 송신이면 남은 바이트를 이어 보낸다.
- byte: 헤더까지 포함하여 한 번에 1바이트씩 총 67번 보낸다. 호출 사이 간격은 10ms이다.
- 기본 포트는 9000이며 마지막 인수로 변경 가능하다: `DummyClient.exe 127.0.0.1 byte "test" 19000`
- 메시지는 최대 63바이트이다. 테스트에는 영문 사용을 권장한다.
- 성공하면 `PASS: S2C_Echo...`와 종료 코드 0, 실패하면 오류와 종료 코드 1이 나온다.
- connect, 송신, 응답 수신 각각 5초 제한. 소켓과 Winsock은 마지막에 해제한다.

## 코드 읽는 순서

1. `main`: 인수 확인 → Winsock/socket → 접속 → 송신 → 응답 검사 → 자원 해제.
2. `ConnectServer`: 논블로킹 접속, select와 SO_ERROR로 완료 확인.
3. `SendBytes`: normal/byte 모드에 따라 send 크기를 정하고 실제 전송량만큼 offset 증가.
4. `ReceiveEcho`: 조각을 버퍼에 누적하고 헤더3바이트 → 패킷67바이트 순서로 완성 여부 확인.
5. `WaitSocket`: 준비 상태를 select로 기다리되 종료 시각을 넘기지 않음.

## 서버와 합의할 규격

현재 로컬에서 팀 공용 헤더/서버를 찾지 못해 PDF의 에코 부분만 `protocol.h`에 옮겼다.
이는 최종 공용 헤더를 대체하지 않는다. 팀 헤더가 도착하면 크기와 타입을 비교하여 공용 파일로 통합해야 한다.

- C2S_ECHO=1, S2C_ECHO=2.
- 헤더: size 2바이트 + type 1바이트, 문자열64바이트. 총67바이트, 패딩 없음.
- 계획서의 Windows struct 전송 방식 그대로 사용한다(size는 현재 Windows의 리틀엔디안).
- 서버는 응답 type을 S2C_ECHO로 바꾸고 message[64]를 그대로 돌려줘야 한다.
- bundle은 번호가 다른 에코 3개(201바이트)를 연속된 배열로 한 번에 송신 요청한다. 부분 송신이면 나머지를 이어 보내며 응답 3개의 내용과 순서를 확인한다.

## 테스트와 한계

`tests/test_echo.py`는 팀 서버가 아닌 임시 로컬 검증 서버를 만들고 C++ exe를 실행한다.
서버 fixture가 recv(1)을 사용하여 헤더와 본문이 나눠 도착한 상황을 확실하게 재현한다.
응답도 1바이트씩 나누어 보내 클라이언트의 누적 수신을 확인한다.

TCP는 send 호출 경계를 보존하지 않는다. byte 모드와 TCP_NODELAY를 사용해도 실제 서버의 recv에서
항상 1바이트씩 반환되는 것은 아니다. 따라서 팀 서버 재조립의 최종 검증은 그 서버에서도 별도로 해야 한다.

20개 테스트 결과는 `TEST_RESULTS.md`에 기록했다. 정상/분할 응답, 불완전 종료,
잘못된 응답 크기·종류·내용, 타임아웃, 잘못된 명령행 인수를 검사했다.
bad-small은 size=2, bad-large는 size=1025인 헤더 3바이트만 보낸다. 서버가 응답 없이 연결을 닫으면 성공, 에코를 보내거나 5초 동안 연결을 유지하면 실패다.

**완료:** 11/6 일정까지 클라이언트 구현, Debug/Release 빌드, 로컬 fixture 검증.
**미검증:** 팀원이 만든 실제 서버 및 다른 PC와의 LAN 연동. 해당 서버가 제공되면 위 명령으로 확인한다.

참고한 수업 예제는 test/Windows/Chapter04/TCPClient/TCPClient.cpp의 connect, send, recv 및 종료 흐름이다. 묶음 송신과 잘못된 size에 대한 연결 종료 조건은 추진계획서에서 가져왔다. 묶음 개수 3개, 순서 구분 번호, 경계 바로 바깥 값 2와 1025는 테스트를 위해 정한 값이다.