// directplay.cpp - IDirectPlay4A- und IDirectPlayLobby2A-Ersatz
//
// Wichtig: Die Vtable-Reihenfolge und die Parameteranzahl jeder Methode muessen exakt
// dem Original entsprechen (__stdcall: der Aufgerufene raeumt den Stack ab). Deshalb
// sind auch alle nicht benoetigten Methoden mit korrekter Signatur vorhanden.
#include "directplay.h"
#include "log.h"

#include <atomic>
#include <vector>

namespace a3 {

Platform& platform() {
    static Platform p;
    return p;
}

#define VLOG(...) do { if (platform().verbose) logf(__VA_ARGS__); } while (0)

// ===========================================================================
// IDirectPlay4A
// ===========================================================================
struct DPObj;
struct DPVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(DPObj*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(DPObj*);
    ULONG   (STDMETHODCALLTYPE* Release)(DPObj*);
    // IDirectPlay2
    HRESULT (STDMETHODCALLTYPE* AddPlayerToGroup)(DPObj*, DPID, DPID);
    HRESULT (STDMETHODCALLTYPE* Close)(DPObj*);
    HRESULT (STDMETHODCALLTYPE* CreateGroup)(DPObj*, LPDPID, LPDPNAME, LPVOID, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* CreatePlayer)(DPObj*, LPDPID, LPDPNAME, HANDLE, LPVOID, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* DeletePlayerFromGroup)(DPObj*, DPID, DPID);
    HRESULT (STDMETHODCALLTYPE* DestroyGroup)(DPObj*, DPID);
    HRESULT (STDMETHODCALLTYPE* DestroyPlayer)(DPObj*, DPID);
    HRESULT (STDMETHODCALLTYPE* EnumGroupPlayers)(DPObj*, DPID, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* EnumGroups)(DPObj*, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* EnumPlayers)(DPObj*, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* EnumSessions)(DPObj*, LPDPSESSIONDESC2, DWORD, LPDPENUMSESSIONSCALLBACK2, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetCaps)(DPObj*, LPDPCAPS, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetGroupData)(DPObj*, DPID, LPVOID, LPDWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetGroupName)(DPObj*, DPID, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetMessageCount)(DPObj*, DPID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetPlayerAddress)(DPObj*, DPID, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetPlayerCaps)(DPObj*, DPID, LPDPCAPS, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetPlayerData)(DPObj*, DPID, LPVOID, LPDWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetPlayerName)(DPObj*, DPID, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetSessionDesc)(DPObj*, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* Initialize)(DPObj*, LPGUID);
    HRESULT (STDMETHODCALLTYPE* Open)(DPObj*, LPDPSESSIONDESC2, DWORD);
    HRESULT (STDMETHODCALLTYPE* Receive)(DPObj*, LPDPID, LPDPID, DWORD, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* Send)(DPObj*, DPID, DPID, DWORD, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* SetGroupData)(DPObj*, DPID, LPVOID, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* SetGroupName)(DPObj*, DPID, LPDPNAME, DWORD);
    HRESULT (STDMETHODCALLTYPE* SetPlayerData)(DPObj*, DPID, LPVOID, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* SetPlayerName)(DPObj*, DPID, LPDPNAME, DWORD);
    HRESULT (STDMETHODCALLTYPE* SetSessionDesc)(DPObj*, LPDPSESSIONDESC2, DWORD);
    // IDirectPlay3
    HRESULT (STDMETHODCALLTYPE* AddGroupToGroup)(DPObj*, DPID, DPID);
    HRESULT (STDMETHODCALLTYPE* CreateGroupInGroup)(DPObj*, DPID, LPDPID, LPDPNAME, LPVOID, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* DeleteGroupFromGroup)(DPObj*, DPID, DPID);
    HRESULT (STDMETHODCALLTYPE* EnumConnections)(DPObj*, LPCGUID, LPDPENUMCONNECTIONSCALLBACK, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* EnumGroupsInGroup)(DPObj*, DPID, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetGroupConnectionSettings)(DPObj*, DWORD, DPID, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* InitializeConnection)(DPObj*, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* SecureOpen)(DPObj*, LPCDPSESSIONDESC2, DWORD, LPCVOID, LPCVOID);
    HRESULT (STDMETHODCALLTYPE* SendChatMessage)(DPObj*, DPID, DPID, DWORD, LPVOID);
    HRESULT (STDMETHODCALLTYPE* SetGroupConnectionSettings)(DPObj*, DWORD, DPID, LPVOID);
    HRESULT (STDMETHODCALLTYPE* StartSession)(DPObj*, DWORD, DPID);
    HRESULT (STDMETHODCALLTYPE* GetGroupFlags)(DPObj*, DPID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetGroupParent)(DPObj*, DPID, LPDPID);
    HRESULT (STDMETHODCALLTYPE* GetPlayerAccount)(DPObj*, DPID, DWORD, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetPlayerFlags)(DPObj*, DPID, LPDWORD);
    // IDirectPlay4
    HRESULT (STDMETHODCALLTYPE* GetGroupOwner)(DPObj*, DPID, LPDPID);
    HRESULT (STDMETHODCALLTYPE* SetGroupOwner)(DPObj*, DPID, DPID);
    HRESULT (STDMETHODCALLTYPE* SendEx)(DPObj*, DPID, DPID, DWORD, LPVOID, DWORD, DWORD, DWORD, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* GetMessageQueue)(DPObj*, DPID, DPID, DWORD, LPDWORD, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* CancelMessage)(DPObj*, DWORD, DWORD);
    HRESULT (STDMETHODCALLTYPE* CancelPriority)(DPObj*, DWORD, DWORD, DWORD);
};
static_assert(sizeof(DPVtbl) == 53 * sizeof(void*), "IDirectPlay4A hat 53 Methoden");

struct DPObj {
    const DPVtbl* vtbl;
    std::atomic<long> ref{1};
    Session* s = nullptr;
    uint32_t sendCount = 0, recvCount = 0;
};

// Haelt das Objekt am Leben, waehrend Callbacks des Spiels laufen
// (Anstoss 3 gibt das Objekt z.B. im EnumConnections-Callback frei!)
struct SelfRef {
    DPObj* o;
    explicit SelfRef(DPObj* p);
    ~SelfRef();
};

static ULONG STDMETHODCALLTYPE DP_AddRef(DPObj* o) { return (ULONG)++o->ref; }
static ULONG STDMETHODCALLTYPE DP_Release(DPObj* o) {
    long r = --o->ref;
    if (r == 0) {
        logf("IDirectPlay4A::Release -> Objekt %p freigegeben", (void*)o);
        delete o->s;
        delete o;
    }
    return (ULONG)(r < 0 ? 0 : r);
}
SelfRef::SelfRef(DPObj* p) : o(p) { DP_AddRef(o); }
SelfRef::~SelfRef() { DP_Release(o); }

static bool isDPIID(REFIID riid) {
    return guidEq(riid, A3_IID_IUnknown) || guidEq(riid, A3_IID_IDirectPlay2) || guidEq(riid, A3_IID_IDirectPlay2A) ||
           guidEq(riid, A3_IID_IDirectPlay3) || guidEq(riid, A3_IID_IDirectPlay3A) || guidEq(riid, A3_IID_IDirectPlay4) ||
           guidEq(riid, A3_IID_IDirectPlay4A);
}

static HRESULT STDMETHODCALLTYPE DP_QueryInterface(DPObj* o, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (isDPIID(riid)) {
        DP_AddRef(o);
        *ppv = o;
        return S_OK;
    }
    *ppv = nullptr;
    logf("IDirectPlay::QueryInterface: unbekannte Schnittstelle %s", guidStr(riid).c_str());
    return E_NOINTERFACE;
}

static HRESULT unsupported(const char* name) {
    logf("IDirectPlay4A::%s - nicht unterstuetzt", name);
    return DPERR_UNSUPPORTED;
}

// --- Hilfen fuer Strings in Ausgabepuffern --------------------------------
static size_t strSpace(const std::string& s) { return s.empty() ? 0 : s.size() + 1; }
static char* putStr(char*& cursor, const std::string& s) {
    if (s.empty()) return nullptr;
    std::memcpy(cursor, s.c_str(), s.size() + 1);
    char* r = cursor;
    cursor += s.size() + 1;
    return r;
}
static std::string getStr(const char* p) { return p ? std::string(p) : std::string(); }

static void fillDesc(DPSESSIONDESC2& sd, const SessionInfo& si, char* name, char* pw) {
    std::memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(DPSESSIONDESC2);
    sd.dwFlags = si.flags;
    sd.guidInstance = si.instance;
    sd.guidApplication = si.application;
    sd.dwMaxPlayers = si.maxPlayers;
    sd.dwCurrentPlayers = si.curPlayers;
    sd.lpszSessionNameA = name;
    sd.lpszPasswordA = pw;
    sd.dwUser1 = si.user[0];
    sd.dwUser2 = si.user[1];
    sd.dwUser3 = si.user[2];
    sd.dwUser4 = si.user[3];
}

static uint32_t playerEnumFlags(const Player& p) {
    uint32_t f = p.local ? DPENUMPLAYERS_LOCAL : DPENUMPLAYERS_REMOTE;
    if (p.flags & DPPLAYER_SERVERPLAYER) f |= DPENUMPLAYERS_SERVERPLAYER;
    if (p.flags & DPPLAYER_SPECTATOR) f |= DPENUMPLAYERS_SPECTATOR;
    return f;
}

// --- Implementierte Methoden ----------------------------------------------
static HRESULT STDMETHODCALLTYPE DP_Close(DPObj* o) {
    logf("IDirectPlay4A::Close");
    o->s->close();
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_CreatePlayer(DPObj* o, LPDPID lpid, LPDPNAME name, HANDLE ev, LPVOID data,
                                                DWORD size, DWORD flags) {
    if (!lpid) return DPERR_INVALIDPARAMS;
    std::string sn = name ? getStr(name->lpszShortNameA) : std::string();
    std::string ln = name ? getStr(name->lpszLongNameA) : std::string();
    DPID id = 0;
    HRESULT hr = o->s->createPlayer(sn, ln, flags, (void*)ev, id);
    if (hr == DP_OK) {
        *lpid = id;
        if (data && size) o->s->setPlayerData(id, data, size, false);
    }
    logf("IDirectPlay4A::CreatePlayer('%s', flags=0x%X, event=%p) -> 0x%08X id=0x%X", sn.c_str(), (unsigned)flags, (void*)ev,
         (unsigned)hr, (unsigned)id);
    return hr;
}

static HRESULT STDMETHODCALLTYPE DP_DestroyPlayer(DPObj* o, DPID id) {
    HRESULT hr = o->s->destroyPlayer(id);
    logf("IDirectPlay4A::DestroyPlayer(0x%X) -> 0x%08X", (unsigned)id, (unsigned)hr);
    return hr;
}

static HRESULT STDMETHODCALLTYPE DP_EnumPlayers(DPObj* o, LPGUID inst, LPDPENUMPLAYERSCALLBACK2 cb, LPVOID ctx,
                                               DWORD flags) {
    if (!cb) return DPERR_INVALIDPARAMS;
    if (!o->s->isOpen()) {
        logf("IDirectPlay4A::EnumPlayers ohne offene Session - nicht unterstuetzt");
        return DPERR_NOCONNECTION;
    }
    (void)inst;
    SelfRef keep(o);
    auto pl = o->s->players();
    int n = 0;
    for (const Player& p : pl) {
        if ((flags & DPENUMPLAYERS_LOCAL) && !p.local) continue;
        if ((flags & DPENUMPLAYERS_REMOTE) && p.local) continue;
        // DPENUMPLAYERS_SERVERPLAYER heisst "Server-Spieler mit aufzaehlen" - wir zaehlen ihn immer auf
        if (flags & DPENUMPLAYERS_GROUP) continue;
        std::string sn = p.shortName, ln = p.longName;
        DPNAME nm{sizeof(DPNAME), 0, sn.empty() ? nullptr : &sn[0], ln.empty() ? nullptr : &ln[0]};
        ++n;
        if (!cb(p.id, DPPLAYERTYPE_PLAYER, &nm, playerEnumFlags(p), ctx)) break;
    }
    logf("IDirectPlay4A::EnumPlayers(flags=0x%X) -> %d Spieler", (unsigned)flags, n);
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_EnumSessions(DPObj* o, LPDPSESSIONDESC2 desc, DWORD timeout,
                                                LPDPENUMSESSIONSCALLBACK2 cb, LPVOID ctx, DWORD flags) {
    if (!cb && !(flags & DPENUMSESSIONS_STOPASYNC)) return DPERR_INVALIDPARAMS;
    (void)desc;
    SelfRef keep(o);
    std::vector<SessionInfo> list;
    HRESULT hr = o->s->enumSessions(flags, timeout, list);
    VLOG("IDirectPlay4A::EnumSessions(flags=0x%X, timeout=%u) -> 0x%08X, %u Sessions", (unsigned)flags, (unsigned)timeout, (unsigned)hr,
         (unsigned)list.size());
    if (hr != DP_OK || (flags & DPENUMSESSIONS_STOPASYNC)) return hr;
    DWORD to = timeout;
    for (const SessionInfo& si : list) {
        std::string name = si.name, pw = si.password;
        DPSESSIONDESC2 sd;
        fillDesc(sd, si, name.empty() ? nullptr : &name[0], pw.empty() ? nullptr : &pw[0]);
        if (!cb(&sd, &to, 0, ctx)) return DP_OK;
    }
    cb(nullptr, &to, DPESC_TIMEDOUT, ctx);
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_GetCaps(DPObj* o, LPDPCAPS caps, DWORD flags) {
    (void)o; (void)flags;
    if (!caps || caps->dwSize < sizeof(DPCAPS)) return DPERR_INVALIDPARAMS;
    DWORD sz = caps->dwSize;
    std::memset(caps, 0, sizeof(DPCAPS));
    caps->dwSize = sz;
    caps->dwFlags = 0x00000040 | 0x00000020;   // GUARANTEEDSUPPORTED | GUARANTEEDOPTIMIZED
    caps->dwMaxBufferSize = 0x10000;
    caps->dwMaxQueueSize = 0;
    caps->dwMaxPlayers = 64;
    caps->dwHundredBaud = 0;
    caps->dwLatency = 50;
    caps->dwMaxLocalPlayers = 0xFFFF;
    caps->dwHeaderLength = 0;
    caps->dwTimeout = 5000;
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_GetPlayerCaps(DPObj* o, DPID id, LPDPCAPS caps, DWORD flags) {
    Player p;
    if (!o->s->getPlayer(id, p)) return DPERR_INVALIDPLAYER;
    return DP_GetCaps(o, caps, flags);
}

static HRESULT STDMETHODCALLTYPE DP_GetMessageCount(DPObj* o, DPID id, LPDWORD count) {
    if (!count) return DPERR_INVALIDPARAMS;
    *count = o->s->messageCount(id);
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_GetPlayerData(DPObj* o, DPID id, LPVOID data, LPDWORD size, DWORD flags) {
    if (!size) return DPERR_INVALIDPARAMS;
    Player p;
    if (!o->s->getPlayer(id, p)) return DPERR_INVALIDPLAYER;
    const std::vector<uint8_t>& v = (flags & DPGET_LOCAL) ? p.localData : p.remoteData;
    if (!data || *size < v.size()) {
        *size = (DWORD)v.size();
        return v.empty() ? DP_OK : DPERR_BUFFERTOOSMALL;
    }
    if (!v.empty()) std::memcpy(data, v.data(), v.size());
    *size = (DWORD)v.size();
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_GetPlayerName(DPObj* o, DPID id, LPVOID data, LPDWORD size) {
    if (!size) return DPERR_INVALIDPARAMS;
    Player p;
    if (!o->s->getPlayer(id, p)) return DPERR_INVALIDPLAYER;
    DWORD need = (DWORD)(sizeof(DPNAME) + strSpace(p.shortName) + strSpace(p.longName));
    if (!data || *size < need) {
        *size = need;
        return DPERR_BUFFERTOOSMALL;
    }
    DPNAME* nm = (DPNAME*)data;
    char* cur = (char*)data + sizeof(DPNAME);
    nm->dwSize = sizeof(DPNAME);
    nm->dwFlags = 0;
    nm->lpszShortNameA = putStr(cur, p.shortName);
    nm->lpszLongNameA = putStr(cur, p.longName);
    *size = need;
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_GetSessionDesc(DPObj* o, LPVOID data, LPDWORD size) {
    if (!size) return DPERR_INVALIDPARAMS;
    if (!o->s->isOpen()) return DPERR_NOCONNECTION;
    SessionInfo si = o->s->desc();
    DWORD need = (DWORD)(sizeof(DPSESSIONDESC2) + strSpace(si.name) + strSpace(si.password));
    if (!data || *size < need) {
        *size = need;
        return DPERR_BUFFERTOOSMALL;
    }
    char* cur = (char*)data + sizeof(DPSESSIONDESC2);
    char* n = putStr(cur, si.name);
    char* pw = putStr(cur, si.password);
    fillDesc(*(DPSESSIONDESC2*)data, si, n, pw);
    *size = need;
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_SetSessionDesc(DPObj* o, LPDPSESSIONDESC2 sd, DWORD flags) {
    (void)flags;
    if (!sd) return DPERR_INVALIDPARAMS;
    SessionInfo si = o->s->desc();
    si.flags = sd->dwFlags;
    si.maxPlayers = sd->dwMaxPlayers;
    si.name = getStr(sd->lpszSessionNameA);
    si.password = getStr(sd->lpszPasswordA);
    si.user[0] = sd->dwUser1;
    si.user[1] = sd->dwUser2;
    si.user[2] = sd->dwUser3;
    si.user[3] = sd->dwUser4;
    HRESULT hr = o->s->setDesc(si);
    logf("IDirectPlay4A::SetSessionDesc(user1=0x%X) -> 0x%08X", (unsigned)sd->dwUser1, (unsigned)hr);
    return hr;
}

static HRESULT STDMETHODCALLTYPE DP_Initialize(DPObj*, LPGUID) { return DPERR_ALREADYINITIALIZED; }

static HRESULT STDMETHODCALLTYPE DP_Open(DPObj* o, LPDPSESSIONDESC2 sd, DWORD flags) {
    if (!sd) return DPERR_INVALIDPARAMS;
    HRESULT hr;
    if (flags & DPOPEN_CREATE) {
        SessionInfo si;
        si.application = sd->guidApplication;
        si.flags = sd->dwFlags;
        si.maxPlayers = sd->dwMaxPlayers;
        si.name = getStr(sd->lpszSessionNameA);
        si.password = getStr(sd->lpszPasswordA);
        si.user[0] = sd->dwUser1;
        si.user[1] = sd->dwUser2;
        si.user[2] = sd->dwUser3;
        si.user[3] = sd->dwUser4;
        hr = o->s->hostOpen(si);
        logf("IDirectPlay4A::Open(CREATE '%s', max=%u, flags=0x%X) -> 0x%08X", si.name.c_str(), si.maxPlayers,
             si.flags, (unsigned)hr);
    } else if (flags & DPOPEN_JOIN) {
        hr = o->s->joinOpen(sd->guidInstance, getStr(sd->lpszPasswordA));
        logf("IDirectPlay4A::Open(JOIN %s) -> 0x%08X", guidStr(sd->guidInstance).c_str(), (unsigned)hr);
    } else {
        hr = DPERR_INVALIDFLAGS;
    }
    return hr;
}

static HRESULT STDMETHODCALLTYPE DP_Receive(DPObj* o, LPDPID from, LPDPID to, DWORD flags, LPVOID data,
                                           LPDWORD size) {
    if (!size || !from || !to) return DPERR_INVALIDPARAMS;
    QMsg m;
    if (!o->s->peek(flags, *from, *to, m)) return DPERR_NOMESSAGES;

    DWORD need;
    if (!m.sys) {
        need = (DWORD)m.data.size();
    } else if (m.sm.type == DPSYS_CREATEPLAYERORGROUP) {
        need = (DWORD)(sizeof(DPMSG_CREATEPLAYERORGROUP) + strSpace(m.sm.shortName) + strSpace(m.sm.longName));
    } else if (m.sm.type == DPSYS_DESTROYPLAYERORGROUP) {
        need = (DWORD)(sizeof(DPMSG_DESTROYPLAYERORGROUP) + strSpace(m.sm.shortName) + strSpace(m.sm.longName));
    } else {
        need = sizeof(DPMSG_GENERIC);
    }
    if (!data || *size < need) {
        *size = need;
        return DPERR_BUFFERTOOSMALL;
    }

    if (!m.sys) {
        if (need) std::memcpy(data, m.data.data(), need);
    } else if (m.sm.type == DPSYS_CREATEPLAYERORGROUP) {
        DPMSG_CREATEPLAYERORGROUP* c = (DPMSG_CREATEPLAYERORGROUP*)data;
        std::memset(c, 0, sizeof(*c));
        char* cur = (char*)data + sizeof(*c);
        c->dwType = DPSYS_CREATEPLAYERORGROUP;
        c->dwPlayerType = m.sm.playerType;
        c->dpId = m.sm.id;
        c->dwCurrentPlayers = m.sm.curPlayers;
        c->dpnName.dwSize = sizeof(DPNAME);
        c->dpnName.lpszShortNameA = putStr(cur, m.sm.shortName);
        c->dpnName.lpszLongNameA = putStr(cur, m.sm.longName);
    } else if (m.sm.type == DPSYS_DESTROYPLAYERORGROUP) {
        DPMSG_DESTROYPLAYERORGROUP* d = (DPMSG_DESTROYPLAYERORGROUP*)data;
        std::memset(d, 0, sizeof(*d));
        char* cur = (char*)data + sizeof(*d);
        d->dwType = DPSYS_DESTROYPLAYERORGROUP;
        d->dwPlayerType = m.sm.playerType;
        d->dpId = m.sm.id;
        d->dpnName.dwSize = sizeof(DPNAME);
        d->dpnName.lpszShortNameA = putStr(cur, m.sm.shortName);
        d->dpnName.lpszLongNameA = putStr(cur, m.sm.longName);
    } else {
        ((DPMSG_GENERIC*)data)->dwType = m.sm.type;
    }
    *from = m.from;
    *to = m.to;
    *size = need;
    if (!(flags & DPRECEIVE_PEEK)) o->s->pop(m.seq);

    if (m.sys) logf("IDirectPlay4A::Receive: Systemnachricht 0x%X (Spieler 0x%X) an 0x%X", (unsigned)m.sm.type, (unsigned)m.sm.id, (unsigned)m.to);
    else if (platform().verbose || (++o->recvCount % 500) == 1)
        logf("IDirectPlay4A::Receive: %u Bytes von 0x%X an 0x%X [%s] (#%u)", (unsigned)need, (unsigned)m.from, (unsigned)m.to,
             hexdump(m.data.data(), m.data.size(), 16).c_str(), o->recvCount);
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE DP_Send(DPObj* o, DPID from, DPID to, DWORD flags, LPVOID data, DWORD size) {
    if (!data && size) return DPERR_INVALIDPARAMS;
    HRESULT hr = o->s->send(from, to, flags, data, size);
    if (hr != DP_OK || platform().verbose || (++o->sendCount % 500) == 1)
        logf("IDirectPlay4A::Send(0x%X -> 0x%X, flags=0x%X, %u Bytes [%s]) -> 0x%08X (#%u)", (unsigned)from, (unsigned)to, (unsigned)flags, (unsigned)size,
             hexdump(data, size, 16).c_str(), (unsigned)hr, o->sendCount);
    return hr;
}

static HRESULT STDMETHODCALLTYPE DP_SendEx(DPObj* o, DPID from, DPID to, DWORD flags, LPVOID data, DWORD size,
                                          DWORD prio, DWORD timeout, LPVOID ctx, LPDWORD msgid) {
    (void)prio; (void)timeout; (void)ctx;
    if (msgid) *msgid = 0;
    return DP_Send(o, from, to, flags, data, size);
}

static HRESULT STDMETHODCALLTYPE DP_SetPlayerData(DPObj* o, DPID id, LPVOID data, DWORD size, DWORD flags) {
    return o->s->setPlayerData(id, data, size, (flags & 0x8) != 0);
}

static HRESULT STDMETHODCALLTYPE DP_SetPlayerName(DPObj* o, DPID id, LPDPNAME name, DWORD) {
    if (!name) return DPERR_INVALIDPARAMS;
    return o->s->setPlayerName(id, getStr(name->lpszShortNameA), getStr(name->lpszLongNameA));
}

static HRESULT STDMETHODCALLTYPE DP_GetPlayerFlags(DPObj* o, DPID id, LPDWORD flags) {
    if (!flags) return DPERR_INVALIDPARAMS;
    Player p;
    if (!o->s->getPlayer(id, p)) return DPERR_INVALIDPLAYER;
    *flags = (p.local ? DPPLAYER_LOCAL : 0) | p.flags;
    return DP_OK;
}

// Die einzige "Verbindung", die wir anbieten: TCP/IP
static std::vector<uint8_t> makeAddress(const GUID& sp, const std::string* inet) {
    std::vector<uint8_t> a;
    auto chunk = [&](const GUID& g, const void* d, uint32_t n) {
        const uint8_t* gp = (const uint8_t*)&g;
        a.insert(a.end(), gp, gp + sizeof(GUID));
        for (int i = 0; i < 4; ++i) a.push_back((uint8_t)(n >> (8 * i)));
        a.insert(a.end(), (const uint8_t*)d, (const uint8_t*)d + n);
    };
    uint32_t total = 0;
    chunk(A3_DPAID_TotalSize, &total, 4);
    chunk(A3_DPAID_ServiceProvider, &sp, sizeof(GUID));
    if (inet) chunk(A3_DPAID_INet, inet->c_str(), (uint32_t)inet->size() + 1);
    total = (uint32_t)a.size();
    std::memcpy(&a[20], &total, 4);
    return a;
}

static HRESULT STDMETHODCALLTYPE DP_EnumConnections(DPObj* o, LPCGUID app, LPDPENUMCONNECTIONSCALLBACK cb, LPVOID ctx,
                                                   DWORD flags) {
    (void)app;
    if (!cb) return DPERR_INVALIDPARAMS;
    if (flags && !(flags & DPCONNECTION_DIRECTPLAY)) return DP_OK;   // nur DirectPlay-Verbindungen
    SelfRef keep(o);
    std::vector<uint8_t> addr = makeAddress(A3_DPSPGUID_TCPIP, nullptr);
    char shortName[] = "Internet TCP/IP (A3Net)";
    DPNAME nm{sizeof(DPNAME), 0, shortName, nullptr};
    logf("IDirectPlay4A::EnumConnections -> TCP/IP (A3Net)");
    cb(&A3_DPSPGUID_TCPIP, addr.data(), (DWORD)addr.size(), &nm, DPCONNECTION_DIRECTPLAY, ctx);
    return DP_OK;
}

// Zerlegt eine DirectPlay-Adresse in ihre Teile
static bool parseAddress(const void* p, DWORD size, GUID& sp, std::string& inet, uint16_t& port) {
    const uint8_t* b = (const uint8_t*)p;
    size_t i = 0;
    bool haveSp = false;
    port = 0;
    while (i + 20 <= size) {
        GUID g;
        std::memcpy(&g, b + i, sizeof(GUID));
        uint32_t n;
        std::memcpy(&n, b + i + 16, 4);
        i += 20;
        if (i + n > size) break;
        if (guidEq(g, A3_DPAID_ServiceProvider) && n >= sizeof(GUID)) {
            std::memcpy(&sp, b + i, sizeof(GUID));
            haveSp = true;
        } else if (guidEq(g, A3_DPAID_INet) && n > 0) {
            inet.assign((const char*)b + i, strnlen((const char*)b + i, n));
        } else if (guidEq(g, A3_DPAID_INetPort) && n >= 2) {
            std::memcpy(&port, b + i, 2);
        }
        i += n;
    }
    return haveSp;
}

static HRESULT STDMETHODCALLTYPE DP_InitializeConnection(DPObj* o, LPVOID conn, DWORD flags) {
    (void)flags;
    if (!conn) return DPERR_INVALIDPARAMS;
    GUID sp{};
    std::string inet;
    uint16_t port = 0;
    // Die Groesse steht im ersten Element (DPAID_TotalSize)
    uint32_t total = 0;
    std::memcpy(&total, (const uint8_t*)conn + 20, 4);
    if (total < 24 || total > 65536) total = 1024;
    if (!parseAddress(conn, total, sp, inet, port)) {
        logf("IDirectPlay4A::InitializeConnection: Adresse ohne Service Provider");
        return DPERR_INVALIDPARAMS;
    }
    if (!guidEq(sp, A3_DPSPGUID_TCPIP) && !guidEq(sp, A3_DPSPGUID_IPX)) {
        logf("IDirectPlay4A::InitializeConnection: Service Provider %s nicht unterstuetzt", guidStr(sp).c_str());
        return DPERR_UNSUPPORTED;
    }
    if (port && !inet.empty()) inet += ":" + std::to_string(port);
    HRESULT hr = o->s->initializeConnection(inet);
    logf("IDirectPlay4A::InitializeConnection(SP=%s) -> 0x%08X", guidEq(sp, A3_DPSPGUID_TCPIP) ? "TCP/IP" : "IPX",
         (unsigned)hr);
    return hr;
}

// --- Nicht unterstuetzte Methoden (korrekte Signaturen!) -------------------
static HRESULT STDMETHODCALLTYPE DP_AddPlayerToGroup(DPObj*, DPID, DPID) { return unsupported("AddPlayerToGroup"); }
static HRESULT STDMETHODCALLTYPE DP_CreateGroup(DPObj*, LPDPID, LPDPNAME, LPVOID, DWORD, DWORD) { return unsupported("CreateGroup"); }
static HRESULT STDMETHODCALLTYPE DP_DeletePlayerFromGroup(DPObj*, DPID, DPID) { return unsupported("DeletePlayerFromGroup"); }
static HRESULT STDMETHODCALLTYPE DP_DestroyGroup(DPObj*, DPID) { return unsupported("DestroyGroup"); }
static HRESULT STDMETHODCALLTYPE DP_EnumGroupPlayers(DPObj*, DPID, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD) { return unsupported("EnumGroupPlayers"); }
static HRESULT STDMETHODCALLTYPE DP_EnumGroups(DPObj*, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD) { return DP_OK; /* keine Gruppen */ }
static HRESULT STDMETHODCALLTYPE DP_GetGroupData(DPObj*, DPID, LPVOID, LPDWORD, DWORD) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_GetGroupName(DPObj*, DPID, LPVOID, LPDWORD) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_GetPlayerAddress(DPObj*, DPID, LPVOID, LPDWORD) { return unsupported("GetPlayerAddress"); }
static HRESULT STDMETHODCALLTYPE DP_SetGroupData(DPObj*, DPID, LPVOID, DWORD, DWORD) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_SetGroupName(DPObj*, DPID, LPDPNAME, DWORD) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_AddGroupToGroup(DPObj*, DPID, DPID) { return unsupported("AddGroupToGroup"); }
static HRESULT STDMETHODCALLTYPE DP_CreateGroupInGroup(DPObj*, DPID, LPDPID, LPDPNAME, LPVOID, DWORD, DWORD) { return unsupported("CreateGroupInGroup"); }
static HRESULT STDMETHODCALLTYPE DP_DeleteGroupFromGroup(DPObj*, DPID, DPID) { return unsupported("DeleteGroupFromGroup"); }
static HRESULT STDMETHODCALLTYPE DP_EnumGroupsInGroup(DPObj*, DPID, LPGUID, LPDPENUMPLAYERSCALLBACK2, LPVOID, DWORD) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_GetGroupConnectionSettings(DPObj*, DWORD, DPID, LPVOID, LPDWORD) { return unsupported("GetGroupConnectionSettings"); }
static HRESULT STDMETHODCALLTYPE DP_SecureOpen(DPObj* o, LPCDPSESSIONDESC2 sd, DWORD flags, LPCVOID, LPCVOID) {
    logf("IDirectPlay4A::SecureOpen -> wie Open behandelt");
    return DP_Open(o, (LPDPSESSIONDESC2)sd, flags);
}
static HRESULT STDMETHODCALLTYPE DP_SendChatMessage(DPObj*, DPID, DPID, DWORD, LPVOID) { return unsupported("SendChatMessage"); }
static HRESULT STDMETHODCALLTYPE DP_SetGroupConnectionSettings(DPObj*, DWORD, DPID, LPVOID) { return unsupported("SetGroupConnectionSettings"); }
static HRESULT STDMETHODCALLTYPE DP_StartSession(DPObj*, DWORD, DPID) { return unsupported("StartSession"); }
static HRESULT STDMETHODCALLTYPE DP_GetGroupFlags(DPObj*, DPID, LPDWORD) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_GetGroupParent(DPObj*, DPID, LPDPID) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_GetPlayerAccount(DPObj*, DPID, DWORD, LPVOID, LPDWORD) { return unsupported("GetPlayerAccount"); }
static HRESULT STDMETHODCALLTYPE DP_GetGroupOwner(DPObj*, DPID, LPDPID) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_SetGroupOwner(DPObj*, DPID, DPID) { return DPERR_INVALIDGROUP; }
static HRESULT STDMETHODCALLTYPE DP_GetMessageQueue(DPObj* o, DPID from, DPID to, DWORD, LPDWORD n, LPDWORD bytes) {
    (void)from;
    if (n) *n = o->s->messageCount(to);
    if (bytes) *bytes = 0;
    return DP_OK;
}
static HRESULT STDMETHODCALLTYPE DP_CancelMessage(DPObj*, DWORD, DWORD) { return unsupported("CancelMessage"); }
static HRESULT STDMETHODCALLTYPE DP_CancelPriority(DPObj*, DWORD, DWORD, DWORD) { return unsupported("CancelPriority"); }

static const DPVtbl g_dpVtbl = {
    DP_QueryInterface, DP_AddRef, DP_Release,
    DP_AddPlayerToGroup, DP_Close, DP_CreateGroup, DP_CreatePlayer, DP_DeletePlayerFromGroup, DP_DestroyGroup,
    DP_DestroyPlayer, DP_EnumGroupPlayers, DP_EnumGroups, DP_EnumPlayers, DP_EnumSessions, DP_GetCaps,
    DP_GetGroupData, DP_GetGroupName, DP_GetMessageCount, DP_GetPlayerAddress, DP_GetPlayerCaps, DP_GetPlayerData,
    DP_GetPlayerName, DP_GetSessionDesc, DP_Initialize, DP_Open, DP_Receive, DP_Send, DP_SetGroupData,
    DP_SetGroupName, DP_SetPlayerData, DP_SetPlayerName, DP_SetSessionDesc,
    DP_AddGroupToGroup, DP_CreateGroupInGroup, DP_DeleteGroupFromGroup, DP_EnumConnections, DP_EnumGroupsInGroup,
    DP_GetGroupConnectionSettings, DP_InitializeConnection, DP_SecureOpen, DP_SendChatMessage,
    DP_SetGroupConnectionSettings, DP_StartSession, DP_GetGroupFlags, DP_GetGroupParent, DP_GetPlayerAccount,
    DP_GetPlayerFlags,
    DP_GetGroupOwner, DP_SetGroupOwner, DP_SendEx, DP_GetMessageQueue, DP_CancelMessage, DP_CancelPriority,
};

// ===========================================================================
// IDirectPlayLobby2A / 3A (nur Adress-Funktionen)
// ===========================================================================
struct LobbyObj;
struct LobbyVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(LobbyObj*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE* AddRef)(LobbyObj*);
    ULONG   (STDMETHODCALLTYPE* Release)(LobbyObj*);
    HRESULT (STDMETHODCALLTYPE* Connect)(LobbyObj*, DWORD, void**, IUnknown*);
    HRESULT (STDMETHODCALLTYPE* CreateAddress)(LobbyObj*, REFGUID, REFGUID, LPCVOID, DWORD, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* EnumAddress)(LobbyObj*, LPDPENUMADDRESSCALLBACK, LPCVOID, DWORD, LPVOID);
    HRESULT (STDMETHODCALLTYPE* EnumAddressTypes)(LobbyObj*, LPVOID, REFGUID, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* EnumLocalApplications)(LobbyObj*, LPVOID, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* GetConnectionSettings)(LobbyObj*, DWORD, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* ReceiveLobbyMessage)(LobbyObj*, DWORD, DWORD, LPDWORD, LPVOID, LPDWORD);
    HRESULT (STDMETHODCALLTYPE* RunApplication)(LobbyObj*, DWORD, LPDWORD, LPVOID, HANDLE);
    HRESULT (STDMETHODCALLTYPE* SendLobbyMessage)(LobbyObj*, DWORD, DWORD, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE* SetConnectionSettings)(LobbyObj*, DWORD, DWORD, LPVOID);
    HRESULT (STDMETHODCALLTYPE* SetLobbyMessageEvent)(LobbyObj*, DWORD, DWORD, HANDLE);
    // IDirectPlayLobby2
    HRESULT (STDMETHODCALLTYPE* CreateCompoundAddress)(LobbyObj*, LPCDPCOMPOUNDADDRESSELEMENT, DWORD, LPVOID, LPDWORD);
    // IDirectPlayLobby3
    HRESULT (STDMETHODCALLTYPE* ConnectEx)(LobbyObj*, DWORD, REFIID, void**, IUnknown*);
    HRESULT (STDMETHODCALLTYPE* RegisterApplication)(LobbyObj*, DWORD, LPVOID);
    HRESULT (STDMETHODCALLTYPE* UnregisterApplication)(LobbyObj*, DWORD, REFGUID);
    HRESULT (STDMETHODCALLTYPE* WaitForConnectionSettings)(LobbyObj*, DWORD);
};

struct LobbyObj {
    const LobbyVtbl* vtbl;
    std::atomic<long> ref{1};
};

static ULONG STDMETHODCALLTYPE LB_AddRef(LobbyObj* o) { return (ULONG)++o->ref; }
static ULONG STDMETHODCALLTYPE LB_Release(LobbyObj* o) {
    long r = --o->ref;
    if (r == 0) delete o;
    return (ULONG)(r < 0 ? 0 : r);
}
static bool isLobbyIID(REFIID riid) {
    return guidEq(riid, A3_IID_IUnknown) || guidEq(riid, A3_IID_IDirectPlayLobby) ||
           guidEq(riid, A3_IID_IDirectPlayLobbyA) || guidEq(riid, A3_IID_IDirectPlayLobby2) ||
           guidEq(riid, A3_IID_IDirectPlayLobby2A) || guidEq(riid, A3_IID_IDirectPlayLobby3) ||
           guidEq(riid, A3_IID_IDirectPlayLobby3A);
}
static HRESULT STDMETHODCALLTYPE LB_QueryInterface(LobbyObj* o, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (isLobbyIID(riid)) {
        LB_AddRef(o);
        *ppv = o;
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE LB_CreateCompoundAddress(LobbyObj*, LPCDPCOMPOUNDADDRESSELEMENT el, DWORD count,
                                                         LPVOID addr, LPDWORD size) {
    if (!el || !count || !size) return DPERR_INVALIDPARAMS;
    DWORD need = 24;
    for (DWORD i = 0; i < count; ++i) need += 20 + el[i].dwDataSize;
    if (!addr || *size < need) {
        *size = need;
        return DPERR_BUFFERTOOSMALL;
    }
    uint8_t* p = (uint8_t*)addr;
    auto chunk = [&](const GUID& g, const void* d, uint32_t n) {
        std::memcpy(p, &g, sizeof(GUID));
        std::memcpy(p + 16, &n, 4);
        if (n) std::memcpy(p + 20, d, n);
        p += 20 + n;
    };
    chunk(A3_DPAID_TotalSize, &need, 4);
    for (DWORD i = 0; i < count; ++i) chunk(el[i].guidDataType, el[i].lpData, el[i].dwDataSize);
    *size = need;
    return DP_OK;
}

static HRESULT STDMETHODCALLTYPE LB_CreateAddress(LobbyObj* o, REFGUID sp, REFGUID type, LPCVOID data, DWORD n,
                                                 LPVOID addr, LPDWORD size) {
    DPCOMPOUNDADDRESSELEMENT el[2];
    el[0].guidDataType = A3_DPAID_ServiceProvider;
    el[0].dwDataSize = sizeof(GUID);
    el[0].lpData = (LPVOID)&sp;
    el[1].guidDataType = type;
    el[1].dwDataSize = n;
    el[1].lpData = (LPVOID)data;
    return LB_CreateCompoundAddress(o, el, 2, addr, size);
}

static HRESULT STDMETHODCALLTYPE LB_EnumAddress(LobbyObj*, LPDPENUMADDRESSCALLBACK cb, LPCVOID addr, DWORD size,
                                               LPVOID ctx) {
    if (!cb || !addr) return DPERR_INVALIDPARAMS;
    const uint8_t* b = (const uint8_t*)addr;
    size_t i = 0;
    while (i + 20 <= size) {
        GUID g;
        std::memcpy(&g, b + i, sizeof(GUID));
        uint32_t n;
        std::memcpy(&n, b + i + 16, 4);
        if (i + 20 + n > size) break;
        if (!cb(g, n, b + i + 20, ctx)) break;
        i += 20 + n;
    }
    return DP_OK;
}

static HRESULT lbUnsupported(const char* n) {
    logf("IDirectPlayLobby::%s - nicht unterstuetzt", n);
    return DPERR_UNSUPPORTED;
}
static HRESULT STDMETHODCALLTYPE LB_Connect(LobbyObj*, DWORD, void** pp, IUnknown*) { if (pp) *pp = nullptr; return DPERR_NOTLOBBIED; }
static HRESULT STDMETHODCALLTYPE LB_EnumAddressTypes(LobbyObj*, LPVOID, REFGUID, LPVOID, DWORD) { return lbUnsupported("EnumAddressTypes"); }
static HRESULT STDMETHODCALLTYPE LB_EnumLocalApplications(LobbyObj*, LPVOID, LPVOID, DWORD) { return DP_OK; }
static HRESULT STDMETHODCALLTYPE LB_GetConnectionSettings(LobbyObj*, DWORD, LPVOID, LPDWORD) { return DPERR_NOTLOBBIED; }
static HRESULT STDMETHODCALLTYPE LB_ReceiveLobbyMessage(LobbyObj*, DWORD, DWORD, LPDWORD, LPVOID, LPDWORD) { return DPERR_NOMESSAGES; }
static HRESULT STDMETHODCALLTYPE LB_RunApplication(LobbyObj*, DWORD, LPDWORD, LPVOID, HANDLE) { return lbUnsupported("RunApplication"); }
static HRESULT STDMETHODCALLTYPE LB_SendLobbyMessage(LobbyObj*, DWORD, DWORD, LPVOID, DWORD) { return DPERR_NOTLOBBIED; }
static HRESULT STDMETHODCALLTYPE LB_SetConnectionSettings(LobbyObj*, DWORD, DWORD, LPVOID) { return lbUnsupported("SetConnectionSettings"); }
static HRESULT STDMETHODCALLTYPE LB_SetLobbyMessageEvent(LobbyObj*, DWORD, DWORD, HANDLE) { return DPERR_NOTLOBBIED; }
static HRESULT STDMETHODCALLTYPE LB_ConnectEx(LobbyObj*, DWORD, REFIID, void** pp, IUnknown*) { if (pp) *pp = nullptr; return DPERR_NOTLOBBIED; }
static HRESULT STDMETHODCALLTYPE LB_RegisterApplication(LobbyObj*, DWORD, LPVOID) { return lbUnsupported("RegisterApplication"); }
static HRESULT STDMETHODCALLTYPE LB_UnregisterApplication(LobbyObj*, DWORD, REFGUID) { return lbUnsupported("UnregisterApplication"); }
static HRESULT STDMETHODCALLTYPE LB_WaitForConnectionSettings(LobbyObj*, DWORD) { return DPERR_NOTLOBBIED; }

static const LobbyVtbl g_lobbyVtbl = {
    LB_QueryInterface, LB_AddRef, LB_Release, LB_Connect, LB_CreateAddress, LB_EnumAddress, LB_EnumAddressTypes,
    LB_EnumLocalApplications, LB_GetConnectionSettings, LB_ReceiveLobbyMessage, LB_RunApplication,
    LB_SendLobbyMessage, LB_SetConnectionSettings, LB_SetLobbyMessageEvent, LB_CreateCompoundAddress,
    LB_ConnectEx, LB_RegisterApplication, LB_UnregisterApplication, LB_WaitForConnectionSettings,
};

// ===========================================================================
// Fabrik
// ===========================================================================
bool isOurClsid(REFCLSID rclsid) {
    return guidEq(rclsid, A3_CLSID_DirectPlay) || guidEq(rclsid, A3_CLSID_DirectPlayLobby);
}

HRESULT createObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (guidEq(rclsid, A3_CLSID_DirectPlay)) {
        if (!isDPIID(riid)) return E_NOINTERFACE;
        DPObj* o = new DPObj();
        o->vtbl = &g_dpVtbl;
        o->s = new Session(platform().config);
        o->s->signalEvent = platform().signalEvent;
        o->s->askAddress = platform().askAddress;
        o->s->portMapping = platform().portMapping;
        *ppv = o;
        logf("CoCreateInstance(CLSID_DirectPlay, %s) -> A3Net-Objekt %p", guidStr(riid).c_str(), (void*)o);
        return S_OK;
    }
    if (guidEq(rclsid, A3_CLSID_DirectPlayLobby)) {
        if (!isLobbyIID(riid)) return E_NOINTERFACE;
        LobbyObj* o = new LobbyObj();
        o->vtbl = &g_lobbyVtbl;
        *ppv = o;
        logf("CoCreateInstance(CLSID_DirectPlayLobby) -> A3Net-Lobby %p", (void*)o);
        return S_OK;
    }
    return E_NOINTERFACE;
}

} // namespace a3
