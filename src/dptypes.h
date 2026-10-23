// dptypes.h - Eigene Definitionen der DirectPlay-4-Schnittstelle (DirectX 6/7).
//
// Das aktuelle Windows SDK enthaelt dplay.h/dplobby.h nicht mehr, deshalb werden
// hier nur die Teile nachgebaut, die Anstoss 3 tatsaechlich verwendet (plus die
// komplette Vtable-Reihenfolge, damit jeder Methodenaufruf am richtigen Platz landet).
//
// Ausserhalb von Windows (fuer die Tests unter Linux) werden die Win32-Basistypen
// minimal nachgebildet.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <objbase.h>
#else
typedef uint32_t DWORD;
typedef int32_t  LONG;
typedef int32_t  HRESULT;
typedef int32_t  BOOL;
typedef uint8_t  BYTE;
typedef void*    LPVOID;
typedef const void* LPCVOID;
typedef void*    HANDLE;
typedef DWORD*   LPDWORD;
typedef unsigned long ULONG;
struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
};
typedef GUID IID;
typedef GUID CLSID;
typedef const GUID& REFGUID;
typedef const GUID& REFIID;
typedef const GUID& REFCLSID;
typedef const GUID* LPCGUID;
typedef GUID* LPGUID;
struct IUnknown;
#  define WINAPI
#  define STDMETHODCALLTYPE
#  define TRUE 1
#  define FALSE 0
#  define S_OK ((HRESULT)0)
#  define E_NOTIMPL ((HRESULT)0x80004001L)
#  define E_NOINTERFACE ((HRESULT)0x80004002L)
#  define E_POINTER ((HRESULT)0x80004003L)
#  define E_FAIL ((HRESULT)0x80004005L)
#  define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#  define E_INVALIDARG ((HRESULT)0x80070057L)
#  define E_PENDING ((HRESULT)0x8000000AL)
#  define CLASS_E_NOAGGREGATION ((HRESULT)0x80040110L)
#endif

inline bool guidEq(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }

// ---------------------------------------------------------------------------
// GUIDs (Werte identisch mit dem Original-SDK; im Spiel ab VA 0x838850 abgelegt)
// ---------------------------------------------------------------------------
#define A3_GUID(n, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID n = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

A3_GUID(A3_IID_IUnknown,        0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
A3_GUID(A3_CLSID_DirectPlay,    0xD1EB6D20, 0x8923, 0x11D0, 0x9D, 0x97, 0x00, 0xA0, 0xC9, 0x0A, 0x43, 0xCB);
A3_GUID(A3_CLSID_DirectPlayLobby,0x2FE8F810,0xB2A5, 0x11D0, 0xA7, 0x87, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC);

A3_GUID(A3_IID_IDirectPlay2,    0x2B74F7C0, 0x9154, 0x11CF, 0xA9, 0xCD, 0x00, 0xAA, 0x00, 0x68, 0x86, 0xE3);
A3_GUID(A3_IID_IDirectPlay2A,   0x9D460580, 0xA822, 0x11CF, 0x96, 0x0C, 0x00, 0x80, 0xC7, 0x53, 0x4E, 0x82);
A3_GUID(A3_IID_IDirectPlay3,    0x133EFE40, 0x32DC, 0x11D0, 0x9C, 0xFB, 0x00, 0xA0, 0xC9, 0x0A, 0x43, 0xCB);
A3_GUID(A3_IID_IDirectPlay3A,   0x133EFE41, 0x32DC, 0x11D0, 0x9C, 0xFB, 0x00, 0xA0, 0xC9, 0x0A, 0x43, 0xCB);
A3_GUID(A3_IID_IDirectPlay4,    0x0AB1C530, 0x4745, 0x11D1, 0xA7, 0xA1, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC);
A3_GUID(A3_IID_IDirectPlay4A,   0x0AB1C531, 0x4745, 0x11D1, 0xA7, 0xA1, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC);

A3_GUID(A3_IID_IDirectPlayLobby,  0xAF465C71, 0x9588, 0x11CF, 0xA0, 0x20, 0x00, 0xAA, 0x00, 0x61, 0x57, 0xAC);
A3_GUID(A3_IID_IDirectPlayLobbyA, 0x26C66A70, 0xB367, 0x11CF, 0xA0, 0x24, 0x00, 0xAA, 0x00, 0x61, 0x57, 0xAC);
A3_GUID(A3_IID_IDirectPlayLobby2, 0x0194C220, 0xA303, 0x11D0, 0x9C, 0x4F, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E);
A3_GUID(A3_IID_IDirectPlayLobby2A,0x1BB4AF80, 0xA303, 0x11D0, 0x9C, 0x4F, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E);
A3_GUID(A3_IID_IDirectPlayLobby3, 0x2DB72490, 0x652C, 0x11D1, 0xA7, 0xA8, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC);
A3_GUID(A3_IID_IDirectPlayLobby3A,0x2DB72491, 0x652C, 0x11D1, 0xA7, 0xA8, 0x00, 0x00, 0xF8, 0x03, 0xAB, 0xFC);

// Service Provider
A3_GUID(A3_DPSPGUID_IPX,    0x685BC400, 0x9D2C, 0x11CF, 0xA9, 0xCD, 0x00, 0xAA, 0x00, 0x68, 0x86, 0xE3);
A3_GUID(A3_DPSPGUID_TCPIP,  0x36E95EE0, 0x8577, 0x11CF, 0x96, 0x0C, 0x00, 0x80, 0xC7, 0x53, 0x4E, 0x82);
A3_GUID(A3_DPSPGUID_SERIAL, 0x0F1D6860, 0x88D9, 0x11CF, 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E);
A3_GUID(A3_DPSPGUID_MODEM,  0x44EAA760, 0xCB68, 0x11CF, 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E);

// Adress-Datentypen
A3_GUID(A3_DPAID_TotalSize,       0x1318F560, 0x912C, 0x11D0, 0x9D, 0xAA, 0x00, 0xA0, 0xC9, 0x0A, 0x43, 0xCB);
A3_GUID(A3_DPAID_ServiceProvider, 0x07D916C0, 0xE0AF, 0x11CF, 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E);
A3_GUID(A3_DPAID_INet,            0xC4A54DA0, 0xE0AF, 0x11CF, 0x9C, 0x4E, 0x00, 0xA0, 0xC9, 0x05, 0x42, 0x5E);
A3_GUID(A3_DPAID_INetPort,        0xE4524541, 0x8EA5, 0x11D1, 0x8A, 0x96, 0x00, 0x60, 0x97, 0xB0, 0x14, 0x11);

// ---------------------------------------------------------------------------
// Fehlercodes
// ---------------------------------------------------------------------------
#define A3_DPHR(code) ((HRESULT)(0x88770000u | (code)))
#define DP_OK                   S_OK
#define DPERR_ALREADYINITIALIZED A3_DPHR(5)
#define DPERR_ACCESSDENIED       A3_DPHR(10)
#define DPERR_BUFFERTOOSMALL     A3_DPHR(30)
#define DPERR_CANTADDPLAYER      A3_DPHR(40)
#define DPERR_CANTCREATEPLAYER   A3_DPHR(60)
#define DPERR_CANTCREATESESSION  A3_DPHR(70)
#define DPERR_GENERIC            E_FAIL
#define DPERR_INVALIDFLAGS       A3_DPHR(120)
#define DPERR_INVALIDOBJECT      A3_DPHR(130)
#define DPERR_INVALIDPARAMS      E_INVALIDARG
#define DPERR_INVALIDPLAYER      A3_DPHR(150)
#define DPERR_INVALIDGROUP       A3_DPHR(155)
#define DPERR_NOCONNECTION       A3_DPHR(170)
#define DPERR_NOMESSAGES         A3_DPHR(190)
#define DPERR_NOSESSIONS         A3_DPHR(220)
#define DPERR_SENDTOOBIG         A3_DPHR(230)
#define DPERR_TIMEOUT            A3_DPHR(240)
#define DPERR_UNSUPPORTED        E_NOTIMPL
#define DPERR_USERCANCEL         A3_DPHR(280)
#define DPERR_CANNOTCREATESERVER A3_DPHR(290)
#define DPERR_SESSIONLOST        A3_DPHR(310)
#define DPERR_UNINITIALIZED      A3_DPHR(320)
#define DPERR_NONEWPLAYERS       A3_DPHR(330)
#define DPERR_INVALIDPASSWORD    A3_DPHR(340)
#define DPERR_CONNECTING         A3_DPHR(350)
#define DPERR_CONNECTIONLOST     A3_DPHR(360)
#define DPERR_NOTLOBBIED         A3_DPHR(1070)

// ---------------------------------------------------------------------------
// Konstanten
// ---------------------------------------------------------------------------
typedef DWORD DPID;
typedef DPID* LPDPID;
#define DPID_SYSMSG        0
#define DPID_ALLPLAYERS    0
#define DPID_SERVERPLAYER  1
#define DPID_UNKNOWN       0xFFFFFFFF

#define DPOPEN_JOIN          0x00000001
#define DPOPEN_CREATE        0x00000002
#define DPOPEN_RETURNSTATUS  0x00000080

#define DPENUMSESSIONS_AVAILABLE     0x00000001
#define DPENUMSESSIONS_ALL           0x00000002
#define DPENUMSESSIONS_ASYNC         0x00000010
#define DPENUMSESSIONS_STOPASYNC     0x00000020
#define DPENUMSESSIONS_PASSWORDREQUIRED 0x00000040
#define DPENUMSESSIONS_RETURNSTATUS  0x00000080

#define DPESC_TIMEDOUT 0x00000001

#define DPSESSION_NEWPLAYERSDISABLED 0x00000001
#define DPSESSION_MIGRATEHOST        0x00000004
#define DPSESSION_JOINDISABLED       0x00000020
#define DPSESSION_KEEPALIVE          0x00000040
#define DPSESSION_PASSWORDREQUIRED   0x00000400
#define DPSESSION_DIRECTPLAYPROTOCOL 0x00002000

#define DPPLAYER_SERVERPLAYER 0x00000100
#define DPPLAYER_SPECTATOR    0x00000200
#define DPPLAYER_LOCAL        0x00000008

#define DPENUMPLAYERS_LOCAL       0x00000008
#define DPENUMPLAYERS_REMOTE      0x00000010
#define DPENUMPLAYERS_GROUP       0x00000020
#define DPENUMPLAYERS_SERVERPLAYER 0x00000100
#define DPENUMPLAYERS_SPECTATOR   0x00000200

#define DPPLAYERTYPE_GROUP  0
#define DPPLAYERTYPE_PLAYER 1

#define DPRECEIVE_ALL        0x00000001
#define DPRECEIVE_TOPLAYER   0x00000002
#define DPRECEIVE_FROMPLAYER 0x00000004
#define DPRECEIVE_PEEK       0x00000008

#define DPSEND_GUARANTEED 0x00000001

#define DPCONNECTION_DIRECTPLAY 0x00000001

#define DPSYS_CREATEPLAYERORGROUP  0x0003
#define DPSYS_DESTROYPLAYERORGROUP 0x0005
#define DPSYS_SESSIONLOST          0x0031
#define DPSYS_HOST                 0x0101
#define DPSYS_SETSESSIONDESC       0x0104

#define DPGET_REMOTE 0
#define DPGET_LOCAL  1

// ---------------------------------------------------------------------------
// Strukturen (nur ANSI-Varianten, Anstoss 3 nutzt die A-Schnittstellen)
// ---------------------------------------------------------------------------
#pragma pack(push, 4)
struct DPNAME {
    DWORD dwSize;
    DWORD dwFlags;
    char* lpszShortNameA;
    char* lpszLongNameA;
};
typedef DPNAME* LPDPNAME;
typedef const DPNAME* LPCDPNAME;

struct DPSESSIONDESC2 {
    DWORD dwSize;
    DWORD dwFlags;
    GUID  guidInstance;
    GUID  guidApplication;
    DWORD dwMaxPlayers;
    DWORD dwCurrentPlayers;
    char* lpszSessionNameA;
    char* lpszPasswordA;
    DWORD dwReserved1;
    DWORD dwReserved2;
    DWORD dwUser1;
    DWORD dwUser2;
    DWORD dwUser3;
    DWORD dwUser4;
};
typedef DPSESSIONDESC2* LPDPSESSIONDESC2;
typedef const DPSESSIONDESC2* LPCDPSESSIONDESC2;

struct DPCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwMaxBufferSize;
    DWORD dwMaxQueueSize;
    DWORD dwMaxPlayers;
    DWORD dwHundredBaud;
    DWORD dwLatency;
    DWORD dwMaxLocalPlayers;
    DWORD dwHeaderLength;
    DWORD dwTimeout;
};
typedef DPCAPS* LPDPCAPS;

struct DPCOMPOUNDADDRESSELEMENT {
    GUID  guidDataType;
    DWORD dwDataSize;
    LPVOID lpData;
};
typedef const DPCOMPOUNDADDRESSELEMENT* LPCDPCOMPOUNDADDRESSELEMENT;

struct DPMSG_GENERIC { DWORD dwType; };

struct DPMSG_CREATEPLAYERORGROUP {
    DWORD  dwType;
    DWORD  dwPlayerType;
    DPID   dpId;
    DWORD  dwCurrentPlayers;
    LPVOID lpData;
    DWORD  dwDataSize;
    DPNAME dpnName;
    DPID   dpIdParent;
    DWORD  dwFlags;
};

struct DPMSG_DESTROYPLAYERORGROUP {
    DWORD  dwType;
    DWORD  dwPlayerType;
    DPID   dpId;
    LPVOID lpLocalData;
    DWORD  dwLocalDataSize;
    LPVOID lpRemoteData;
    DWORD  dwRemoteDataSize;
    DPNAME dpnName;
    DPID   dpIdParent;
    DWORD  dwFlags;
};
#pragma pack(pop)

// Callbacks (alle __stdcall, BOOL-Rueckgabe)
typedef BOOL (WINAPI* LPDPENUMCONNECTIONSCALLBACK)(LPCGUID lpguidSP, LPVOID lpConnection,
                                                   DWORD dwConnectionSize, LPCDPNAME lpName,
                                                   DWORD dwFlags, LPVOID lpContext);
typedef BOOL (WINAPI* LPDPENUMSESSIONSCALLBACK2)(LPCDPSESSIONDESC2 lpThisSD, LPDWORD lpdwTimeOut,
                                                 DWORD dwFlags, LPVOID lpContext);
typedef BOOL (WINAPI* LPDPENUMPLAYERSCALLBACK2)(DPID dpId, DWORD dwPlayerType, LPCDPNAME lpName,
                                                DWORD dwFlags, LPVOID lpContext);
typedef BOOL (WINAPI* LPDPENUMADDRESSCALLBACK)(REFGUID guidDataType, DWORD dwDataSize,
                                               LPCVOID lpData, LPVOID lpContext);

// Die Strukturgroessen muessen auf 32-Bit-Windows exakt dem Original entsprechen
// (das Spiel uebergibt z.B. dwSize = 0x50 fuer DPSESSIONDESC2).
#if defined(_WIN32) && !defined(_WIN64)
static_assert(sizeof(DPNAME) == 16, "DPNAME");
static_assert(sizeof(DPSESSIONDESC2) == 0x50, "DPSESSIONDESC2");
static_assert(sizeof(DPCAPS) == 40, "DPCAPS");
static_assert(sizeof(DPCOMPOUNDADDRESSELEMENT) == 24, "DPCOMPOUNDADDRESSELEMENT");
static_assert(sizeof(DPMSG_CREATEPLAYERORGROUP) == 48, "DPMSG_CREATEPLAYERORGROUP");
static_assert(sizeof(DPMSG_DESTROYPLAYERORGROUP) == 52, "DPMSG_DESTROYPLAYERORGROUP");
static_assert(offsetof(DPSESSIONDESC2, lpszSessionNameA) == 0x30, "lpszSessionNameA");
static_assert(offsetof(DPSESSIONDESC2, dwUser1) == 0x40, "dwUser1");
static_assert(offsetof(DPMSG_CREATEPLAYERORGROUP, dpnName) == 0x18, "dpnName");
#endif
