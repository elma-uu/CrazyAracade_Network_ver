#pragma once
// =============================================================
//  Crazy Arcade Online - 서버/클라이언트 공용 프로토콜
//  - 서버(Server/)와 클라이언트(Client/)가 이 파일 하나를 같이 쓴다.
//  - 패킷을 고치면 두 프로젝트를 모두 다시 빌드할 것.
//  - 모든 패킷은 size(2바이트) + type(1바이트) 헤더로 시작한다.
//  - 이름 규칙: 클라 → 서버는 C2S_, 서버 → 클라는 S2C_
//  - 문자열(닉네임 등)은 UTF-8, 끝의 '\0' 포함 고정 길이 배열
// =============================================================

constexpr short SERVER_PORT     = 9000;
constexpr int   MAX_USER        = 32;    // 방 8개 × 4명
constexpr int   MAX_ROOM        = 8;     // 방 1~8번 고정
constexpr int   MAX_ROOM_PLAYER = 4;     // 한 방 최대 4명
constexpr int   MAX_NAME_LEN    = 16;    // '\0' 포함 (UTF-8 15바이트까지)
constexpr int   MAP_TILES       = 20;
constexpr int   MAX_PACKET_SIZE = 1024;
constexpr int   RECV_BUF_SIZE   = 4096;
constexpr int   MAX_SEND_QUEUE  = 65536;

enum PACKET_TYPE : unsigned char {
    // 에코 · 로그인 · 로비 · 방
    C2S_ECHO = 1, S2C_ECHO,
    C2S_LOGIN, S2C_LOGIN_RESULT,
    C2S_ROOM_LIST, S2C_ROOM_LIST,
    C2S_JOIN_ROOM, S2C_ROOM_ENTER_RESULT, S2C_ROOM_INFO,
    C2S_SELECT_MAP, C2S_SELECT_COLOR, C2S_READY, C2S_START_GAME,
    C2S_LEAVE_ROOM, S2C_PLAYER_LEAVE,
    // 게임 시작 · 인게임
    S2C_GAME_START, S2C_MAP_INFO,
    C2S_INPUT, S2C_MOVE,
    C2S_ATTACK, S2C_ATTACK, S2C_EXPLODE, S2C_BLOCK_BREAK,
    S2C_ITEM_SPAWN, S2C_ITEM_GET, S2C_HIT,
    S2C_STAGE_STATE, S2C_TRAIN,
    // 게임 종료
    S2C_GAME_OVER,
};

enum MAP_ID : unsigned char { MAP_FOREST = 1, MAP_CITY, MAP_ICE };
enum ROOM_STATE_NET : unsigned char {
    ROOM_EMPTY, ROOM_WAITING, ROOM_PLAYING
};
enum ENTER_FAIL : unsigned char {
    ENTER_OK, ENTER_FULL, ENTER_PLAYING
};
enum INPUT_DIR : unsigned char {
    IN_NONE, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT
};
enum ITEM_TYPE : unsigned char {
    ITEM_NONE, ITEM_POWER, ITEM_BOMB, ITEM_SPEED, ITEM_CLOCK
};
enum HIT_CAUSE : unsigned char { HIT_BOMB, HIT_TRAIN };
enum OVER_REASON : unsigned char { OVER_NORMAL, OVER_OTHERS_LEFT };

#pragma pack(push, 1)
struct PACKET_HEADER { unsigned short size; PACKET_TYPE type; };

// ---------- 1단계: 에코 ----------
struct C2S_Echo {
    unsigned short size; PACKET_TYPE type;
    char message[64];
};
struct S2C_Echo {
    unsigned short size; PACKET_TYPE type;
    char message[64];
};

// ---------- 로그인 / 로비 / 방 ----------
struct C2S_Login {
    unsigned short size; PACKET_TYPE type;
    char name[MAX_NAME_LEN];
};
struct S2C_LoginResult {
    unsigned short size; PACKET_TYPE type;
    bool success;
    int  id;
    unsigned short win, lose;
};
struct C2S_RoomList { unsigned short size; PACKET_TYPE type; };

struct ROOM_SUMMARY {
    unsigned char  roomId;
    MAP_ID         mapId;
    unsigned char  playerCount;
    ROOM_STATE_NET state;
    char           hostName[MAX_NAME_LEN];
};
struct S2C_RoomList {
    unsigned short size; PACKET_TYPE type;
    ROOM_SUMMARY rooms[MAX_ROOM];
};
struct C2S_JoinRoom {
    unsigned short size; PACKET_TYPE type;
    unsigned char roomId;
};
struct S2C_RoomEnterResult {
    unsigned short size; PACKET_TYPE type;
    bool          success;
    unsigned char roomId, slot;
    ENTER_FAIL    failReason;
};
struct ROOM_PLAYER {
    int           id;
    char          name[MAX_NAME_LEN];
    unsigned char color;
    bool          ready;
    bool          exist;   // false = 빈 자리
};
struct S2C_RoomInfo {
    unsigned short size; PACKET_TYPE type;
    unsigned char roomId;
    MAP_ID        mapId;
    unsigned char hostSlot;
    ROOM_PLAYER   players[MAX_ROOM_PLAYER];
};
struct C2S_SelectMap {   // 방장만
    unsigned short size; PACKET_TYPE type;
    MAP_ID mapId;
};
struct C2S_SelectColor {
    unsigned short size; PACKET_TYPE type;
    unsigned char color;   // 0~6
};
struct C2S_Ready {
    unsigned short size; PACKET_TYPE type;
    bool ready;
};
// 방장만
struct C2S_StartGame { unsigned short size; PACKET_TYPE type; };
struct C2S_LeaveRoom { unsigned short size; PACKET_TYPE type; };
struct S2C_PlayerLeave {
    unsigned short size; PACKET_TYPE type;
    int           id;
    unsigned char newHostSlot;
};

// ---------- 게임 시작 ----------
struct START_PLAYER { int id; unsigned char x, y, color; };
struct S2C_GameStart {
    unsigned short size; PACKET_TYPE type;
    MAP_ID        mapId;
    unsigned char mySlot;   // 받는 사람마다 다름
    START_PLAYER  players[MAX_ROOM_PLAYER];
};
struct S2C_MapInfo {
    unsigned short size; PACKET_TYPE type;
    MAP_ID mapId;
    // 0 빈칸, 1 블록, 2 사과나무 ...
    unsigned char obstacle[MAP_TILES][MAP_TILES];
    unsigned char obstacleType[MAP_TILES][MAP_TILES];
};

// ---------- 인게임 ----------
struct C2S_Input {
    unsigned short size; PACKET_TYPE type;
    INPUT_DIR dir;
};
struct S2C_Move {
    unsigned short size; PACKET_TYPE type;
    int           id;
    unsigned char x, y, destX, destY, dir;
    float         speed;
};
struct C2S_Attack { unsigned short size; PACKET_TYPE type; };
struct S2C_Attack {
    unsigned short size; PACKET_TYPE type;
    int           bombId, ownerId;
    unsigned char x, y, reach;
};
struct S2C_Explode {
    unsigned short size; PACKET_TYPE type;
    int           bombId;
    unsigned char reachLeft, reachRight, reachUp, reachDown;
};
struct S2C_BlockBreak {
    unsigned short size; PACKET_TYPE type;
    unsigned char x, y;
    ITEM_TYPE     item;   // ITEM_NONE = 아이템 없음
};
struct ITEM_DROP { unsigned char x, y; ITEM_TYPE item; };
struct S2C_ItemSpawn {
    unsigned short size; PACKET_TYPE type;
    unsigned char count;
    ITEM_DROP     drops[6];
};
struct S2C_ItemGet {
    unsigned short size; PACKET_TYPE type;
    int           id;
    unsigned char x, y;
    ITEM_TYPE     item;
    unsigned char bombReach, maxBomb;
    float         speed;
};
struct S2C_Hit {
    unsigned short size; PACKET_TYPE type;
    int       victimId, attackerId;
    HIT_CAUSE cause;
};
struct STAGE_PLAYER {
    int           id;
    unsigned char apple, appleBox, heart;
    short         timeLeft;
    bool          alive;
};
struct S2C_StageState {
    unsigned short size; PACKET_TYPE type;
    STAGE_PLAYER players[MAX_ROOM_PLAYER];
};
struct S2C_Train {
    unsigned short size; PACKET_TYPE type;
    bool spawned;   // true 출발, false 통과 끝
};

// ---------- 게임 종료 ----------
struct S2C_GameOver {
    unsigned short size; PACKET_TYPE type;
    int            winnerId;   // 무승부면 -1
    OVER_REASON    reason;
    unsigned short win, lose;  // 받는 사람의 바뀐 전적
};
#pragma pack(pop)

// 패킷 종류별 정해진 크기 (모르는 종류면 0).
// 받는 쪽은 header.size가 이 값과 다르면 처리하지 말 것:
// 크기가 짧은 패킷을 struct로 읽으면 패킷 뒤의 버퍼(이전 데이터)까지 읽게 된다.
inline int GetPacketSize(unsigned char type)
{
    switch (type) {
    case C2S_ECHO:              return sizeof(C2S_Echo);
    case S2C_ECHO:              return sizeof(S2C_Echo);
    case C2S_LOGIN:             return sizeof(C2S_Login);
    case S2C_LOGIN_RESULT:      return sizeof(S2C_LoginResult);
    case C2S_ROOM_LIST:         return sizeof(C2S_RoomList);
    case S2C_ROOM_LIST:         return sizeof(S2C_RoomList);
    case C2S_JOIN_ROOM:         return sizeof(C2S_JoinRoom);
    case S2C_ROOM_ENTER_RESULT: return sizeof(S2C_RoomEnterResult);
    case S2C_ROOM_INFO:         return sizeof(S2C_RoomInfo);
    case C2S_SELECT_MAP:        return sizeof(C2S_SelectMap);
    case C2S_SELECT_COLOR:      return sizeof(C2S_SelectColor);
    case C2S_READY:             return sizeof(C2S_Ready);
    case C2S_START_GAME:        return sizeof(C2S_StartGame);
    case C2S_LEAVE_ROOM:        return sizeof(C2S_LeaveRoom);
    case S2C_PLAYER_LEAVE:      return sizeof(S2C_PlayerLeave);
    case S2C_GAME_START:        return sizeof(S2C_GameStart);
    case S2C_MAP_INFO:          return sizeof(S2C_MapInfo);
    case C2S_INPUT:             return sizeof(C2S_Input);
    case S2C_MOVE:              return sizeof(S2C_Move);
    case C2S_ATTACK:            return sizeof(C2S_Attack);
    case S2C_ATTACK:            return sizeof(S2C_Attack);
    case S2C_EXPLODE:           return sizeof(S2C_Explode);
    case S2C_BLOCK_BREAK:       return sizeof(S2C_BlockBreak);
    case S2C_ITEM_SPAWN:        return sizeof(S2C_ItemSpawn);
    case S2C_ITEM_GET:          return sizeof(S2C_ItemGet);
    case S2C_HIT:               return sizeof(S2C_Hit);
    case S2C_STAGE_STATE:       return sizeof(S2C_StageState);
    case S2C_TRAIN:             return sizeof(S2C_Train);
    case S2C_GAME_OVER:         return sizeof(S2C_GameOver);
    default:                    return 0;
    }
}

// 서버/클라 구조체 크기가 어긋나면 컴파일 단계에서 바로 알 수 있게 한다
static_assert(sizeof(PACKET_HEADER) == 3, "header must be 3 bytes");
static_assert(sizeof(S2C_MapInfo) == 804, "S2C_MapInfo must be 804 bytes");
static_assert(sizeof(S2C_MapInfo) <= MAX_PACKET_SIZE, "packet too big");
