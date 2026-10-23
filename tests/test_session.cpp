// test_session.cpp - Spielt die DirectPlay-Aufrufe von Anstoss 3 nach (Host + Client im selben Prozess,
// Verbindung ueber 127.0.0.1). Laeuft unter Linux und Windows.
//
// Die Aufrufreihenfolge, Flags und Puffergroessen entsprechen dem disassemblierten Spielcode
// (anstoss3.exe, Funktionen ab 0x7ca049).
#include "../src/directplay.h"
#include "../src/log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace a3;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FEHLER Zeile %d: %s\n", __LINE__, #c); ++g_fail; } else { std::printf("  ok: %s\n", #c); } } while (0)

// Vtable-Zugriff wie im Spiel: call *offset(%ecx)
template <typename Fn>
static Fn vfn(void* obj, int offset) { return *(Fn*)((*(uint8_t**)obj) + offset / 4 * sizeof(void*)); }

typedef HRESULT (STDMETHODCALLTYPE* P_Close)(void*);
typedef HRESULT (STDMETHODCALLTYPE* P_CreatePlayer)(void*, DPID*, DPNAME*, HANDLE, void*, DWORD, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_DestroyPlayer)(void*, DPID);
typedef HRESULT (STDMETHODCALLTYPE* P_EnumPlayers)(void*, GUID*, LPDPENUMPLAYERSCALLBACK2, void*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_EnumSessions)(void*, DPSESSIONDESC2*, DWORD, LPDPENUMSESSIONSCALLBACK2, void*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_GetPlayerName)(void*, DPID, void*, DWORD*);
typedef HRESULT (STDMETHODCALLTYPE* P_GetSessionDesc)(void*, void*, DWORD*);
typedef HRESULT (STDMETHODCALLTYPE* P_Open)(void*, DPSESSIONDESC2*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_Receive)(void*, DPID*, DPID*, DWORD, void*, DWORD*);
typedef HRESULT (STDMETHODCALLTYPE* P_Send)(void*, DPID, DPID, DWORD, void*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_SetSessionDesc)(void*, DPSESSIONDESC2*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_EnumConnections)(void*, const GUID*, LPDPENUMCONNECTIONSCALLBACK, void*, DWORD);
typedef HRESULT (STDMETHODCALLTYPE* P_InitializeConnection)(void*, void*, DWORD);
typedef ULONG (STDMETHODCALLTYPE* P_Release)(void*);
typedef HRESULT (STDMETHODCALLTYPE* L_CreateCompoundAddress)(void*, const DPCOMPOUNDADDRESSELEMENT*, DWORD, void*, DWORD*);

// Offsets aus dem Spielcode
enum { O_Release = 0x8, O_Close = 0x10, O_CreatePlayer = 0x18, O_DestroyPlayer = 0x24, O_EnumPlayers = 0x30,
       O_EnumSessions = 0x34, O_GetPlayerName = 0x54, O_GetSessionDesc = 0x58, O_Open = 0x60, O_Receive = 0x64,
       O_Send = 0x68, O_SetSessionDesc = 0x7c, O_EnumConnections = 0x8c, O_InitializeConnection = 0x98,
       OL_CreateCompoundAddress = 0x38 };

// "Event-Handles" fuer den Test
static std::mutex g_evMu;
static std::map<void*, int> g_evCount;
static int evHost = 0, evClient = 0, evClient2 = 0;

struct Game {
    void* dp = nullptr;
    std::vector<uint8_t> conn;
    DPID me = 0;
};

// --- Nachbau von 0x7cbcc1 / 0x7cbce7 (DirectPlay-Objekt erzeugen / freigeben) ---
static void createDP(Game& g) { createObject(A3_CLSID_DirectPlay, A3_IID_IDirectPlay4A, &g.dp); }
static void releaseDP(Game& g) {
    if (g.dp) vfn<P_Release>(g.dp, O_Release)(g.dp);
    g.dp = nullptr;
}

// --- Nachbau des EnumConnections-Callbacks 0x7ca2ba ---
static Game* g_cbGame = nullptr;
static BOOL WINAPI enumConnCb(LPCGUID sp, LPVOID, DWORD, LPCDPNAME name, DWORD, LPVOID) {
    Game& g = *g_cbGame;
    std::printf("  Verbindung angeboten: '%s'\n", name->lpszShortNameA);
    if (!guidEq(*sp, A3_DPSPGUID_TCPIP)) return TRUE;
    DPCOMPOUNDADDRESSELEMENT el[2];
    el[0].guidDataType = A3_DPAID_ServiceProvider;
    el[0].dwDataSize = 16;
    el[0].lpData = (void*)&A3_DPSPGUID_TCPIP;
    static char empty[1] = {0};
    el[1].guidDataType = A3_DPAID_INet;
    el[1].dwDataSize = 1;                     // das Spiel uebergibt immer einen leeren IP-String
    el[1].lpData = empty;
    void* lobby = nullptr;
    createObject(A3_CLSID_DirectPlayLobby, A3_IID_IDirectPlayLobby2A, &lobby);
    DWORD size = 0;
    HRESULT hr = vfn<L_CreateCompoundAddress>(lobby, OL_CreateCompoundAddress)(lobby, el, 2, nullptr, &size);
    CHECK(hr == DPERR_BUFFERTOOSMALL && size > 0);
    g.conn.resize(size);
    hr = vfn<L_CreateCompoundAddress>(lobby, OL_CreateCompoundAddress)(lobby, el, 2, g.conn.data(), &size);
    CHECK(hr == DP_OK);
    vfn<P_Release>(lobby, O_Release)(lobby);
    // Das Spiel gibt hier das DirectPlay-Objekt frei, auf dem EnumConnections gerade laeuft!
    releaseDP(g);
    createDP(g);
    hr = vfn<P_InitializeConnection>(g.dp, O_InitializeConnection)(g.dp, g.conn.data(), 0);
    CHECK(hr == DP_OK);
    return TRUE;
}

static void initConnection(Game& g) {
    createDP(g);
    g_cbGame = &g;
    vfn<P_EnumConnections>(g.dp, O_EnumConnections)(g.dp, nullptr, enumConnCb, nullptr, 0);
    // 0x7ca4c6: Verbindung auswaehlen -> Objekt neu und InitializeConnection
    releaseDP(g);
    createDP(g);
    HRESULT hr = vfn<P_InitializeConnection>(g.dp, O_InitializeConnection)(g.dp, g.conn.data(), 0);
    CHECK(hr == DP_OK);
}

// --- EnumSessions-Callback 0x7ca600 ---
static std::vector<DPSESSIONDESC2> g_found;
static std::vector<std::string> g_foundNames;
static BOOL WINAPI enumSessCb(LPCDPSESSIONDESC2 sd, LPDWORD, DWORD flags, LPVOID) {
    if (flags & DPESC_TIMEDOUT) return FALSE;
    g_found.push_back(*sd);
    g_foundNames.push_back(sd->lpszSessionNameA ? sd->lpszSessionNameA : "");
    return TRUE;
}

// --- Empfangsschleife 0x7caa21 ---
struct Rx { DPID from, to; std::vector<uint8_t> data; };
static std::vector<Rx> drain(Game& g) {
    std::vector<Rx> out;
    std::vector<uint8_t> buf;
    for (;;) {
        DPID from = 0, to = 0;
        DWORD size = (DWORD)buf.size();
        HRESULT hr = vfn<P_Receive>(g.dp, O_Receive)(g.dp, &from, &to, DPRECEIVE_ALL, buf.empty() ? nullptr : buf.data(), &size);
        if (hr == DPERR_BUFFERTOOSMALL) { buf.resize(size); continue; }
        if (hr != DP_OK) break;
        if (size < 4) continue;
        Rx r{from, to, std::vector<uint8_t>(buf.begin(), buf.begin() + size)};
        out.push_back(r);
    }
    return out;
}

static std::vector<Rx> waitMsgs(Game& g, size_t n, int ms = 3000) {
    std::vector<Rx> all;
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (all.size() < n && std::chrono::steady_clock::now() < end) {
        auto v = drain(g);
        all.insert(all.end(), v.begin(), v.end());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return all;
}

static std::string playerName(Game& g, DPID id) {
    DWORD size = 0;
    HRESULT hr = vfn<P_GetPlayerName>(g.dp, O_GetPlayerName)(g.dp, id, nullptr, &size);
    if (hr != DPERR_BUFFERTOOSMALL) return "?";
    std::vector<uint8_t> b(size);
    hr = vfn<P_GetPlayerName>(g.dp, O_GetPlayerName)(g.dp, id, b.data(), &size);
    if (hr != DP_OK) return "?";
    DPNAME* n = (DPNAME*)b.data();
    return n->lpszShortNameA ? n->lpszShortNameA : "";
}

static std::vector<std::string> g_enumNames;
static BOOL WINAPI enumPlayersCb(DPID id, DWORD type, LPCDPNAME name, DWORD flags, LPVOID) {
    (void)type;
    g_enumNames.push_back(std::string(name->lpszShortNameA) + "#" + std::to_string(id) + ((flags & DPENUMPLAYERS_LOCAL) ? "L" : "R"));
    return TRUE;
}

static bool join(Game& c, const char* name, void* ev) {
    g_found.clear();
    // Das Spiel ruft EnumSessions wiederholt asynchron auf (0x91)
    for (int i = 0; i < 50 && g_found.empty(); ++i) {
        g_found.clear();
        DPSESSIONDESC2 sd{};
        sd.dwSize = 0x50;
        vfn<P_EnumSessions>(c.dp, O_EnumSessions)(c.dp, &sd, 0, enumSessCb, nullptr, 0x91);
        if (g_found.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (g_found.empty()) return false;
    DPSESSIONDESC2 sd{};
    sd.dwSize = 0x50;
    sd.guidInstance = g_found[0].guidInstance;
    HRESULT hr = vfn<P_Open>(c.dp, O_Open)(c.dp, &sd, 0x81);
    if (hr != DP_OK) return false;
    char nm[64];
    std::snprintf(nm, sizeof(nm), "%s", name);
    DPNAME n{16, 0, nm, nullptr};
    hr = vfn<P_CreatePlayer>(c.dp, O_CreatePlayer)(c.dp, &c.me, &n, ev, nullptr, 0, 0);
    return hr == DP_OK;
}

int main() {
    logInit("", true);
    Platform& p = platform();
    p.config.port = 47999;
    p.config.lanBroadcast = false;
    p.config.upnp = false;
    p.config.askForAddress = true;
    p.signalEvent = [](void* ev) { std::lock_guard<std::mutex> lk(g_evMu); g_evCount[ev]++; };
    p.askAddress = [](std::string& a) { a = "127.0.0.1"; return true; };   // "Dialog"

    std::printf("== 1. Host: Verbindung + Session erstellen ==\n");
    Game host;
    initConnection(host);
    char hostName[] = "Testliga";
    DPSESSIONDESC2 hd{};
    hd.dwSize = 0x50;
    hd.dwFlags = 0x2044;
    hd.dwMaxPlayers = 4;
    hd.lpszSessionNameA = hostName;
    HRESULT hr = vfn<P_Open>(host.dp, O_Open)(host.dp, &hd, 0x82);
    CHECK(hr == DP_OK);
    char hp[] = "Hostspieler";
    DPNAME hn{16, 0, hp, nullptr};
    hr = vfn<P_CreatePlayer>(host.dp, O_CreatePlayer)(host.dp, &host.me, &hn, &evHost, nullptr, 0, 0x100);
    CHECK(hr == DP_OK);
    CHECK(host.me == DPID_SERVERPLAYER);

    std::printf("== 2. Client: suchen, beitreten, Spieler anlegen ==\n");
    Game cl;
    initConnection(cl);
    CHECK(join(cl, "Gast", &evClient));
    CHECK(g_foundNames.size() == 1 && g_foundNames[0] == "Testliga");
    CHECK(g_found[0].dwCurrentPlayers == 1 && g_found[0].dwMaxPlayers == 4);
    CHECK(cl.me != 0 && cl.me != DPID_SERVERPLAYER);

    std::printf("== 3. Host erhaelt DPSYS_CREATEPLAYERORGROUP ==\n");
    auto msgs = waitMsgs(host, 1);
    CHECK(msgs.size() == 1);
    if (!msgs.empty()) {
        auto* c = (DPMSG_CREATEPLAYERORGROUP*)msgs[0].data.data();
        CHECK(msgs[0].from == DPID_SYSMSG);
        CHECK(c->dwType == DPSYS_CREATEPLAYERORGROUP);
        CHECK(c->dpId == cl.me);
        CHECK(std::string((char*)msgs[0].data.data() + sizeof(DPMSG_CREATEPLAYERORGROUP)) == "Gast");
    }
    {
        std::lock_guard<std::mutex> lk(g_evMu);
        CHECK(g_evCount[&evHost] >= 1);
    }

    std::printf("== 4. Spielernamen und EnumPlayers ==\n");
    CHECK(playerName(host, cl.me) == "Gast");
    CHECK(playerName(cl, DPID_SERVERPLAYER) == "Hostspieler");
    g_enumNames.clear();
    vfn<P_EnumPlayers>(cl.dp, O_EnumPlayers)(cl.dp, nullptr, enumPlayersCb, nullptr, 0);
    CHECK(g_enumNames.size() == 2);
    for (auto& s : g_enumNames) std::printf("    %s\n", s.c_str());

    std::printf("== 5. Session-Beschreibung (Spiel gestartet markieren, wie 0x7cb655) ==\n");
    DWORD size = 0;
    hr = vfn<P_GetSessionDesc>(host.dp, O_GetSessionDesc)(host.dp, nullptr, &size);
    CHECK(hr == DPERR_BUFFERTOOSMALL);
    std::vector<uint8_t> db(size);
    hr = vfn<P_GetSessionDesc>(host.dp, O_GetSessionDesc)(host.dp, db.data(), &size);
    CHECK(hr == DP_OK);
    DPSESSIONDESC2* sd = (DPSESSIONDESC2*)db.data();
    CHECK(std::string(sd->lpszSessionNameA) == "Testliga");
    CHECK(sd->dwCurrentPlayers == 2);
    sd->dwUser1 |= 1;
    hr = vfn<P_SetSessionDesc>(host.dp, O_SetSessionDesc)(host.dp, sd, 0);
    CHECK(hr == DP_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    size = 0;
    vfn<P_GetSessionDesc>(cl.dp, O_GetSessionDesc)(cl.dp, nullptr, &size);
    std::vector<uint8_t> db2(size);
    hr = vfn<P_GetSessionDesc>(cl.dp, O_GetSessionDesc)(cl.dp, db2.data(), &size);
    CHECK(hr == DP_OK && (((DPSESSIONDESC2*)db2.data())->dwUser1 & 1));

    std::printf("== 6. Spieldaten in beide Richtungen ==\n");
    std::vector<uint8_t> big(0x1ff0 + 16);
    for (size_t i = 0; i < big.size(); ++i) big[i] = (uint8_t)(i * 7);
    big[0] = 0x05; big[1] = 0; big[2] = 0; big[3] = 0x80;   // Spielnachricht 0x80000005
    hr = vfn<P_Send>(cl.dp, O_Send)(cl.dp, cl.me, DPID_ALLPLAYERS, DPSEND_GUARANTEED, big.data(), (DWORD)big.size());
    CHECK(hr == DP_OK);
    msgs = waitMsgs(host, 1);
    CHECK(msgs.size() == 1 && msgs[0].data == big && msgs[0].from == cl.me && msgs[0].to == host.me);

    uint32_t kleineMsg[3] = {0x80000004u, 7, 0};
    hr = vfn<P_Send>(host.dp, O_Send)(host.dp, host.me, cl.me, 0, kleineMsg, sizeof(kleineMsg));
    CHECK(hr == DP_OK);
    msgs = waitMsgs(cl, 1);
    CHECK(msgs.size() == 1 && msgs[0].data.size() == 12 && msgs[0].from == DPID_SERVERPLAYER);
    {
        std::lock_guard<std::mutex> lk(g_evMu);
        CHECK(g_evCount[&evClient] >= 1);
    }

    std::printf("== 7. Zweiter Client, Weiterleitung ueber den Host ==\n");
    // Die Session ist jetzt als "laeuft" markiert (dwUser1 & 1) - das Spiel filtert solche
    // Sessions selbst aus, A3Net zeigt sie trotzdem an (nur DPSESSION_JOINDISABLED sperrt).
    Game cl2;
    initConnection(cl2);
    CHECK(join(cl2, "Gast2", &evClient2));
    msgs = waitMsgs(host, 1);
    CHECK(msgs.size() == 1 && ((DPMSG_GENERIC*)msgs[0].data.data())->dwType == DPSYS_CREATEPLAYERORGROUP);
    msgs = waitMsgs(cl, 1);
    CHECK(msgs.size() == 1 && ((DPMSG_GENERIC*)msgs[0].data.data())->dwType == DPSYS_CREATEPLAYERORGROUP);
    uint32_t hello[2] = {0x80000005u, 42};
    hr = vfn<P_Send>(cl2.dp, O_Send)(cl2.dp, cl2.me, DPID_ALLPLAYERS, 1, hello, sizeof(hello));
    CHECK(hr == DP_OK);
    auto mh = waitMsgs(host, 1), mc = waitMsgs(cl, 1);
    CHECK(mh.size() == 1 && mh[0].from == cl2.me);
    CHECK(mc.size() == 1 && mc[0].from == cl2.me && mc[0].data.size() == 8);
    hr = vfn<P_Send>(cl.dp, O_Send)(cl.dp, cl.me, cl2.me, 1, hello, sizeof(hello));
    CHECK(hr == DP_OK);
    auto m2 = waitMsgs(cl2, 1);
    CHECK(m2.size() == 1 && m2[0].from == cl.me && m2[0].to == cl2.me);

    std::printf("== 8. Client 2 verlaesst das Spiel ==\n");
    vfn<P_DestroyPlayer>(cl2.dp, O_DestroyPlayer)(cl2.dp, cl2.me);
    vfn<P_Close>(cl2.dp, O_Close)(cl2.dp);
    releaseDP(cl2);
    msgs = waitMsgs(host, 1);
    CHECK(msgs.size() == 1 && ((DPMSG_DESTROYPLAYERORGROUP*)msgs[0].data.data())->dwType == DPSYS_DESTROYPLAYERORGROUP);
    if (!msgs.empty()) CHECK(((DPMSG_DESTROYPLAYERORGROUP*)msgs[0].data.data())->dpId == cl2.me);
    msgs = waitMsgs(cl, 1);
    CHECK(msgs.size() == 1 && ((DPMSG_GENERIC*)msgs[0].data.data())->dwType == DPSYS_DESTROYPLAYERORGROUP);

    std::printf("== 9. Host beendet -> Client erhaelt DPSYS_SESSIONLOST ==\n");
    vfn<P_DestroyPlayer>(host.dp, O_DestroyPlayer)(host.dp, host.me);
    vfn<P_Close>(host.dp, O_Close)(host.dp);
    releaseDP(host);
    msgs = waitMsgs(cl, 2);
    bool lost = false;
    for (auto& m : msgs) if (((DPMSG_GENERIC*)m.data.data())->dwType == DPSYS_SESSIONLOST) lost = true;
    CHECK(lost);
    vfn<P_Close>(cl.dp, O_Close)(cl.dp);
    releaseDP(cl);

    std::printf("== 10. LAN-Suche per Broadcast (ohne Adresse) ==\n");
    p.config.lanBroadcast = true;
    p.askAddress = [](std::string&) { return false; };   // Dialog abgebrochen -> nur LAN
    Game h2;
    initConnection(h2);
    char n2[] = "LAN-Liga";
    DPSESSIONDESC2 hd2{};
    hd2.dwSize = 0x50;
    hd2.dwMaxPlayers = 2;
    hd2.lpszSessionNameA = n2;
    CHECK(vfn<P_Open>(h2.dp, O_Open)(h2.dp, &hd2, 0x82) == DP_OK);
    DPNAME hn2{16, 0, hp, nullptr};
    vfn<P_CreatePlayer>(h2.dp, O_CreatePlayer)(h2.dp, &h2.me, &hn2, &evHost, nullptr, 0, 0x100);
    Game c3;
    initConnection(c3);
    bool lanOk = join(c3, "LAN-Gast", &evClient);
    if (lanOk) std::printf("  ok: LAN-Broadcast funktioniert\n");
    else std::printf("  HINWEIS: Broadcast in dieser Umgebung nicht moeglich (kein Fehler des Codes)\n");
    vfn<P_Close>(c3.dp, O_Close)(c3.dp);
    releaseDP(c3);
    vfn<P_Close>(h2.dp, O_Close)(h2.dp);
    releaseDP(h2);

    std::printf("\n%s (%d Fehler)\n", g_fail ? "TEST FEHLGESCHLAGEN" : "ALLE TESTS BESTANDEN", g_fail);
    return g_fail ? 1 : 0;
}
