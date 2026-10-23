#include "session.h"
#include "log.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <set>

namespace a3 {

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------
uint64_t nowMs() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

GUID randomGuid() {
    static std::mutex m;
    static std::mt19937_64 rng{std::random_device{}() ^ (uint64_t)nowMs()};
    std::lock_guard<std::mutex> lk(m);
    GUID g;
    uint64_t a = rng(), b = rng();
    std::memcpy(&g, &a, 8);
    std::memcpy(((uint8_t*)&g) + 8, &b, 8);
    g.Data3 = (uint16_t)((g.Data3 & 0x0FFF) | 0x4000);   // Version 4
    g.Data4[0] = (uint8_t)((g.Data4[0] & 0x3F) | 0x80);
    return g;
}

std::string guidKey(const GUID& g) { return std::string((const char*)&g, sizeof(GUID)); }

std::string guidStr(const GUID& g) {
    char b[64];
    std::snprintf(b, sizeof(b), "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", (unsigned)g.Data1, g.Data2, g.Data3,
                  g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return b;
}

// Liest genau einen Frame (blockierend mit Gesamt-Timeout). Fuer Suche und Beitritt.
static bool readFrameBlocking(sock_t s, int timeoutMs, uint8_t& type, std::vector<uint8_t>& payload,
                              std::vector<uint8_t>* rest = nullptr) {
    std::vector<uint8_t> buf;
    uint64_t end = nowMs() + (uint64_t)timeoutMs;
    for (;;) {
        if (buf.size() >= 4) {
            uint32_t len = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
            if (len == 0 || len > kMaxFrame) return false;
            if (buf.size() >= 4 + (size_t)len) {
                type = buf[4];
                payload.assign(buf.begin() + 5, buf.begin() + 4 + len);
                if (rest) rest->assign(buf.begin() + 4 + len, buf.end());   // bereits mitgelesene Folgedaten
                return true;
            }
        }
        uint64_t n = nowMs();
        if (n >= end) return false;
        int w = waitReadable(s, (int)(end - n));
        if (w <= 0) return false;
        char tmp[16384];
        int r = (int)recv(s, tmp, sizeof(tmp), 0);
        if (r <= 0) return false;
        buf.insert(buf.end(), tmp, tmp + r);
    }
}

void Session::Outbox::flush() {
    for (auto& it : items) {
        const ConnPtr& c = it.first;
        if (!c || c->dead) continue;
        std::lock_guard<std::mutex> lk(c->wmu);
        if (c->s == A3_BADSOCK) continue;
        if (!sendAll(c->s, it.second.data(), it.second.size())) {
            c->dead = true;
            sockShutdown(c->s);   // Netz-Thread bemerkt das beim naechsten Lesen
        }
    }
    items.clear();
}

void Session::closeConn(const ConnPtr& c) {
    if (!c) return;
    c->dead = true;
    sockShutdown(c->s);
    std::lock_guard<std::mutex> lk(c->wmu);
    sockClose(c->s);
    c->s = A3_BADSOCK;
}

// ---------------------------------------------------------------------------
// Konstruktion
// ---------------------------------------------------------------------------
Session::Session(const Config& cfg) : cfg_(cfg) { netInit(); }

Session::~Session() {
    close();
    stopEnum();
}

HRESULT Session::initializeConnection(const std::string& inetAddress) {
    std::lock_guard<std::mutex> lk(mu_);
    if (connInit_) return DPERR_ALREADYINITIALIZED;
    connInit_ = true;
    initAddress_ = inetAddress;
    logf("InitializeConnection: TCP/IP, Adresse aus dem Spiel: '%s'", inetAddress.c_str());
    return DP_OK;
}

// ---------------------------------------------------------------------------
// Suche
// ---------------------------------------------------------------------------
HRESULT Session::enumSessions(uint32_t flags, uint32_t timeoutMs, std::vector<SessionInfo>& out) {
    out.clear();
    if (!connInit_) return DPERR_UNINITIALIZED;
    if (flags & DPENUMSESSIONS_STOPASYNC) {
        stopEnum();
        return DP_OK;
    }
    lastEnumCall_ = nowMs();

    bool decide = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        decide = !targetDecided_;
    }
    bool firstPass = false;
    if (decide) {
        std::string t;
        if (!cfg_.joinAddress.empty()) {
            t = cfg_.joinAddress;
        } else if (!initAddress_.empty()) {
            t = initAddress_;
        } else if (cfg_.askForAddress && askAddress) {
            std::string a;
            if (askAddress(a)) t = a;          // Dialog laeuft ohne gehaltene Sperre
            else logf("Adress-Dialog abgebrochen - nur LAN-Suche");
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            targetDecided_ = true;
            target_ = t;
        }
        logf("Suche: Ziel='%s', LAN-Broadcast=%d", t.c_str(), cfg_.lanBroadcast ? 1 : 0);
        firstPass = true;
    }

    if (firstPass) {
        discoveryPass(2500);
    } else if (!(flags & DPENUMSESSIONS_ASYNC)) {
        int budget = (int)std::min<uint32_t>(std::max<uint32_t>(timeoutMs, 1000), 5000);
        discoveryPass(budget);
    }
    if (flags & DPENUMSESSIONS_ASYNC) ensureDiscoveryThread();

    std::lock_guard<std::mutex> lk(mu_);
    uint64_t now = nowMs();
    for (auto& kv : found_) {
        const Found& f = kv.second;
        if (now - f.lastSeen > 6000) continue;
        if (flags & DPENUMSESSIONS_AVAILABLE) {
            if (f.info.maxPlayers && f.info.curPlayers >= f.info.maxPlayers) continue;
            if (f.info.flags & (DPSESSION_NEWPLAYERSDISABLED | DPSESSION_JOINDISABLED)) continue;
        }
        out.push_back(f.info);
    }
    return DP_OK;
}

void Session::ensureDiscoveryThread() {
    if (discRunning_) return;
    if (disc_.joinable()) disc_.join();
    discStop_ = false;
    discRunning_ = true;
    disc_ = std::thread(&Session::discoveryLoop, this);
}

void Session::stopEnum() {
    discStop_ = true;
    if (disc_.joinable()) {
        if (disc_.get_id() == std::this_thread::get_id()) disc_.detach();
        else disc_.join();
    }
    discRunning_ = false;
}

void Session::discoveryLoop() {
    while (!discStop_) {
        if (nowMs() - lastEnumCall_ > 5000) break;   // Spiel sucht nicht mehr
        discoveryPass(1500);
        for (int i = 0; i < 10 && !discStop_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    discRunning_ = false;
}

bool Session::queryTcp(const NetAddr& a, SessionInfo& out) {
    sock_t s = tcpConnect(a, 2000);
    if (s == A3_BADSOCK) return false;
    Writer w;
    w.u32(kProtoMagic);
    auto f = Writer::frame(MT_QUERY, w.b);
    bool ok = false;
    uint8_t type = 0;
    std::vector<uint8_t> pl;
    if (sendAll(s, f.data(), f.size()) && readFrameBlocking(s, 2500, type, pl) && type == MT_INFO) {
        Reader r(pl.data(), pl.size());
        if (r.u32() == kProtoMagic && out.read(r)) ok = true;
    }
    sockClose(s);
    return ok;
}

void Session::discoveryPass(int budgetMs) {
    std::lock_guard<std::mutex> dlk(discMu_);
    std::string t;
    {
        std::lock_guard<std::mutex> lk(mu_);
        t = target_;
    }
    uint64_t start = nowMs();

    // 1) Direkte Adresse (Internet)
    if (!t.empty()) {
        auto addrs = resolve(t, cfg_.port);
        for (auto& a : addrs) {
            SessionInfo si;
            if (queryTcp(a, si)) {
                std::lock_guard<std::mutex> lk(mu_);
                Found& f = found_[guidKey(si.instance)];
                if (f.lastSeen == 0) logf("Session gefunden: '%s' bei %s (%u/%u Spieler)", si.name.c_str(), a.str().c_str(), si.curPlayers, si.maxPlayers);
                f.info = si;
                f.addr = a;
                f.lastSeen = nowMs();
                break;
            }
        }
        if (addrs.empty()) logf("Suche: '%s' konnte nicht aufgeloest werden", t.c_str());
    }

    // 2) LAN-Broadcast
    if (cfg_.lanBroadcast) {
        sock_t u = udpOpen(0);
        if (u != A3_BADSOCK) {
            uint8_t q[8];
            uint32_t m = kUdpQueryMagic;
            std::memcpy(q, &m, 4);
            std::memcpy(q + 4, &kProtoMagic, 4);
            sockaddr_in b{};
            b.sin_family = AF_INET;
            b.sin_port = htons(cfg_.port);
            b.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            sendto(u, (const char*)q, sizeof(q), 0, (sockaddr*)&b, sizeof(b));
            int remain = std::max(150, std::min(500, budgetMs - (int)(nowMs() - start)));
            uint64_t end = nowMs() + (uint64_t)remain;
            while (nowMs() < end) {
                if (waitReadable(u, (int)(end - nowMs())) <= 0) break;
                uint8_t buf[2048];
                sockaddr_storage from{};
                socklen_t fl = sizeof(from);
                int n = (int)recvfrom(u, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fl);
                if (n < 8) continue;
                Reader r(buf, (size_t)n);
                if (r.u32() != kUdpInfoMagic) continue;
                uint16_t tcpPort = (uint16_t)r.u32();
                SessionInfo si;
                if (!si.read(r)) continue;
                NetAddr a;
                std::memcpy(&a.ss, &from, sizeof(from));
                a.len = (int)fl;
                if (a.ss.ss_family == AF_INET) ((sockaddr_in*)&a.ss)->sin_port = htons(tcpPort);
                std::lock_guard<std::mutex> lk(mu_);
                Found& f = found_[guidKey(si.instance)];
                if (f.lastSeen == 0) logf("LAN-Session gefunden: '%s' bei %s", si.name.c_str(), a.str().c_str());
                // Eine per Direkt-Adresse gefundene Session behaelt ihre Adresse
                if (f.lastSeen == 0 || nowMs() - f.lastSeen > 6000 || f.addr.sameHost(a)) f.addr = a;
                f.info = si;
                f.lastSeen = nowMs();
            }
            sockClose(u);
        }
    }
}

// ---------------------------------------------------------------------------
// Session oeffnen / schliessen
// ---------------------------------------------------------------------------
HRESULT Session::hostOpen(const SessionInfo& d) {
    if (role_ != Role::None) return DPERR_ALREADYINITIALIZED;
    if (!connInit_) return DPERR_UNINITIALIZED;
    stopEnum();

    bool dual = false;
    sock_t l = tcpListen(cfg_.port, dual);
    if (l == A3_BADSOCK) {
        logf("FEHLER: Kann nicht auf TCP-Port %u lauschen. Laeuft das Spiel schon oder blockiert die Firewall?", cfg_.port);
        return DPERR_CANNOTCREATESERVER;
    }
    sock_t u = A3_BADSOCK;
    if (cfg_.lanBroadcast) u = udpOpen(cfg_.port);
    {
        std::lock_guard<std::mutex> lk(mu_);
        listen_ = l;
        udp_ = u;
        role_ = Role::Host;
        closing_ = false;
        desc_ = d;
        desc_.instance = randomGuid();
        players_.clear();
        queue_.clear();
        conns_.clear();
    }
    logf("Host: Session '%s' erstellt (max. %u Spieler), TCP-Port %u (%s), LAN-Suche %s",
         d.name.c_str(), d.maxPlayers, cfg_.port, dual ? "IPv4+IPv6" : "IPv4",
         u != A3_BADSOCK ? "aktiv" : "aus");
    startNetThread();
    if (cfg_.upnp && portMapping) {
        portMapped_ = true;
        auto pm = portMapping;
        uint16_t port = cfg_.port;
        std::thread([pm, port] { pm(port, true); }).detach();
    }
    return DP_OK;
}

HRESULT Session::joinOpen(const GUID& instance, const std::string& password) {
    if (role_ != Role::None) return DPERR_ALREADYINITIALIZED;
    if (!connInit_) return DPERR_UNINITIALIZED;
    Found f;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = found_.find(guidKey(instance));
        if (it == found_.end()) {
            logf("Join: Session %s unbekannt", guidStr(instance).c_str());
            return DPERR_NOSESSIONS;
        }
        f = it->second;
    }
    stopEnum();
    logf("Join: verbinde zu %s ...", f.addr.str().c_str());
    sock_t s = tcpConnect(f.addr, 5000);
    if (s == A3_BADSOCK) {
        logf("Join: Verbindung zu %s fehlgeschlagen (%d)", f.addr.str().c_str(), sockLastError());
        return DPERR_NOCONNECTION;
    }
    Writer w;
    w.u32(kProtoMagic);
    w.guid(instance);
    w.str(password);
    auto fr = Writer::frame(MT_JOIN, w.b);
    uint8_t type = 0;
    std::vector<uint8_t> pl;
    std::vector<uint8_t> rest;
    if (!sendAll(s, fr.data(), fr.size()) || !readFrameBlocking(s, 10000, type, pl, &rest)) {
        logf("Join: keine Antwort vom Host");
        sockClose(s);
        return DPERR_TIMEOUT;
    }
    Reader r(pl.data(), pl.size());
    if (type == MT_JOIN_FAIL) {
        HRESULT hr = (HRESULT)r.u32();
        logf("Join: Host lehnt ab (0x%08X)", (unsigned)hr);
        sockClose(s);
        return hr ? hr : DPERR_GENERIC;
    }
    SessionInfo si;
    if (type != MT_JOIN_OK || !si.read(r)) {
        sockClose(s);
        return DPERR_GENERIC;
    }
    uint32_t n = r.u32();
    std::map<DPID, Player> pls;
    for (uint32_t i = 0; i < n && r.ok(); ++i) {
        Player p = readPlayer(r);
        pls[p.id] = p;
    }
    if (!r.ok()) { sockClose(s); return DPERR_GENERIC; }

    auto c = std::make_shared<Conn>();
    c->s = s;
    c->peer = f.addr;
    c->state = ConnState::Joined;
    c->rbuf = rest;
    c->lastRecv = c->lastPing = c->created = nowMs();
    {
        std::lock_guard<std::mutex> lk(mu_);
        role_ = Role::Client;
        closing_ = false;
        desc_ = si;
        players_ = pls;
        queue_.clear();
        replies_.clear();
        pendingCreate_.clear();
        host_ = c;
    }
    logf("Join: Session '%s' beigetreten, %u Spieler vorhanden", si.name.c_str(), n);
    startNetThread();
    return DP_OK;
}

void Session::close() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (role_ == Role::None) return;
        closing_ = true;
        // Sockets schon jetzt abschalten, damit kein Thread in send() haengen bleibt
        for (auto& kv : conns_) sockShutdown(kv.second->s);
        if (host_) sockShutdown(host_->s);
        cv_.notify_all();
    }
    stopNetThread();
    bool unmap = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& kv : conns_) closeConn(kv.second);
        conns_.clear();
        if (host_) closeConn(host_);
        host_.reset();
        sockClose(listen_);
        listen_ = A3_BADSOCK;
        sockClose(udp_);
        udp_ = A3_BADSOCK;
        players_.clear();
        queue_.clear();
        replies_.clear();
        pendingCreate_.clear();
        role_ = Role::None;
        unmap = portMapped_;
        portMapped_ = false;
        cv_.notify_all();
    }
    if (unmap && portMapping) {
        auto pm = portMapping;
        uint16_t port = cfg_.port;
        std::thread([pm, port] { pm(port, false); }).detach();
    }
    logf("Session geschlossen");
}

SessionInfo Session::descLocked() {
    SessionInfo d = desc_;
    d.curPlayers = (uint32_t)players_.size();
    return d;
}

SessionInfo Session::desc() {
    std::lock_guard<std::mutex> lk(mu_);
    return descLocked();
}

HRESULT Session::setDesc(const SessionInfo& d) {
    Outbox ob;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (role_ != Role::Host) return role_ == Role::None ? DPERR_NOCONNECTION : DPERR_ACCESSDENIED;
        GUID inst = desc_.instance, app = desc_.application;
        desc_ = d;
        desc_.instance = inst;
        if (d.application.Data1 == 0 && d.application.Data2 == 0) desc_.application = app;
        Writer w;
        descLocked().write(w, false);
        broadcastLocked(MT_DESC, w, 0, ob);
        logf("SetSessionDesc: flags=0x%X user1=0x%X max=%u", d.flags, d.user[0], d.maxPlayers);
    }
    ob.flush();
    return DP_OK;
}

// ---------------------------------------------------------------------------
// Spieler
// ---------------------------------------------------------------------------
void Session::writePlayer(Writer& w, const Player& p) {
    w.u32(p.id);
    w.u32(p.flags);
    w.str(p.shortName);
    w.str(p.longName);
    w.blob(p.remoteData.data(), p.remoteData.size());
}

Player Session::readPlayer(Reader& r) {
    Player p;
    p.id = r.u32();
    p.flags = r.u32();
    p.shortName = r.str();
    p.longName = r.str();
    p.remoteData = r.blob();
    p.local = false;
    return p;
}

DPID Session::allocIdLocked(uint32_t flags) {
    if ((flags & DPPLAYER_SERVERPLAYER) && !players_.count(DPID_SERVERPLAYER)) return DPID_SERVERPLAYER;
    for (;;) {
        DPID id = nextId_;
        nextId_ += 0x11;
        if (id > 1 && !players_.count(id)) return id;
    }
}

void Session::broadcastLocked(uint8_t type, const Writer& w, uint32_t exceptConn, Outbox& ob) {
    for (auto& kv : conns_) {
        if (kv.first == exceptConn) continue;
        if (kv.second->state != ConnState::Joined || kv.second->dead) continue;
        ob.add(kv.second, type, w);
    }
}

HRESULT Session::createPlayer(const std::string& sname, const std::string& lname, uint32_t flags,
                              void* event, DPID& outId) {
    Outbox ob;
    if (role_ == Role::Host) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (desc_.maxPlayers && players_.size() >= desc_.maxPlayers) return DPERR_CANTADDPLAYER;
            Player p;
            p.id = allocIdLocked(flags);
            p.shortName = sname;
            p.longName = lname;
            p.flags = flags & (DPPLAYER_SERVERPLAYER | DPPLAYER_SPECTATOR);
            p.local = true;
            p.event = event;
            players_[p.id] = p;
            outId = p.id;
            Writer w;
            writePlayer(w, p);
            w.u32((uint32_t)players_.size());
            broadcastLocked(MT_PLAYER_ADD, w, 0, ob);
            logf("CreatePlayer (Host): id=0x%X '%s' flags=0x%X", (unsigned)p.id, sname.c_str(), (unsigned)flags);
        }
        ob.flush();
        return DP_OK;
    }
    if (role_ != Role::Client) return DPERR_NOCONNECTION;

    uint32_t req;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (!host_ || host_->dead) return DPERR_CONNECTIONLOST;
        req = nextReq_++;
        PendingCreate pc;
        pc.s = sname;
        pc.l = lname;
        pc.flags = flags & DPPLAYER_SPECTATOR;
        pc.event = event;
        pendingCreate_[req] = pc;
        Writer w;
        w.u32(req);
        w.u32(pc.flags);
        w.str(sname);
        w.str(lname);
        ob.add(host_, MT_CREATE_PLAYER, w);
    }
    ob.flush();

    std::unique_lock<std::mutex> lk(mu_);
    bool got = cv_.wait_for(lk, std::chrono::seconds(10), [&] {
        return replies_.count(req) || closing_ || role_ != Role::Client || !host_ || host_->dead;
    });
    auto it = replies_.find(req);
    if (!got || it == replies_.end()) {
        pendingCreate_.erase(req);
        logf("CreatePlayer (Client): keine Antwort vom Host");
        return got ? DPERR_CONNECTIONLOST : DPERR_TIMEOUT;
    }
    HRESULT hr = it->second.first;
    outId = it->second.second;
    replies_.erase(it);
    logf("CreatePlayer (Client): hr=0x%08X id=0x%X '%s'", (unsigned)hr, (unsigned)outId, sname.c_str());
    return hr;
}

HRESULT Session::destroyPlayer(DPID id) {
    Outbox ob;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = players_.find(id);
        if (it == players_.end() || !it->second.local) return DPERR_INVALIDPLAYER;
        players_.erase(it);
        Writer w;
        w.u32(id);
        if (role_ == Role::Host) broadcastLocked(MT_PLAYER_DEL, w, 0, ob);
        else if (host_) ob.add(host_, MT_PLAYER_DEL, w);
        // Nachrichten an diesen Spieler verwerfen
        queue_.erase(std::remove_if(queue_.begin(), queue_.end(), [id](const QMsg& m) { return m.to == id; }), queue_.end());
        logf("DestroyPlayer: 0x%X", (unsigned)id);
    }
    ob.flush();
    return DP_OK;
}

std::vector<Player> Session::players() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Player> v;
    for (auto& kv : players_) v.push_back(kv.second);
    return v;
}

bool Session::getPlayer(DPID id, Player& out) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = players_.find(id);
    if (it == players_.end()) return false;
    out = it->second;
    return true;
}

HRESULT Session::setPlayerName(DPID id, const std::string& s, const std::string& l) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = players_.find(id);
    if (it == players_.end()) return DPERR_INVALIDPLAYER;
    it->second.shortName = s;
    it->second.longName = l;
    return DP_OK;   // Hinweis: wird (noch) nicht an andere uebertragen
}

HRESULT Session::setPlayerData(DPID id, const void* data, uint32_t size, bool local) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = players_.find(id);
    if (it == players_.end()) return DPERR_INVALIDPLAYER;
    std::vector<uint8_t> v((const uint8_t*)data, (const uint8_t*)data + size);
    if (local) it->second.localData = v;
    else it->second.remoteData = v;
    return DP_OK;
}

// ---------------------------------------------------------------------------
// Nachrichten
// ---------------------------------------------------------------------------
void Session::queueMsgLocked(QMsg&& m) {
    m.seq = nextSeq_++;
    if (queue_.size() > 100000) {
        logf("WARNUNG: Nachrichten-Warteschlange voll, aelteste wird verworfen");
        queue_.pop_front();
    }
    DPID to = m.to;
    queue_.push_back(std::move(m));
    auto it = players_.find(to);
    if (it != players_.end() && it->second.local && it->second.event && signalEvent) signalEvent(it->second.event);
}

void Session::queueSysLocked(const SysMsg& sm) {
    bool any = false;
    for (auto& kv : players_) {
        if (!kv.second.local) continue;
        QMsg m;
        m.sys = true;
        m.from = DPID_SYSMSG;
        m.to = kv.first;
        m.sm = sm;
        queueMsgLocked(std::move(m));
        any = true;
    }
    if (!any) {
        QMsg m;
        m.sys = true;
        m.sm = sm;
        queueMsgLocked(std::move(m));
    }
}

void Session::deliverLocked(DPID from, DPID to, const std::vector<uint8_t>& data, uint32_t excludeConn, Outbox& ob) {
    if (to == DPID_ALLPLAYERS) {
        std::set<uint32_t> sent;
        for (auto& kv : players_) {
            const Player& p = kv.second;
            if (p.id == from) continue;
            if (p.local) {
                QMsg m;
                m.from = from;
                m.to = p.id;
                m.data = data;
                queueMsgLocked(std::move(m));
            } else if (role_ == Role::Host) {
                if (p.ownerConn == excludeConn || sent.count(p.ownerConn)) continue;
                auto c = conns_.find(p.ownerConn);
                if (c == conns_.end()) continue;
                Writer w;
                w.u32(from);
                w.u32(DPID_ALLPLAYERS);
                w.u32(0);
                w.blob(data.data(), data.size());
                ob.add(c->second, MT_DATA, w);
                sent.insert(p.ownerConn);
            }
        }
        return;
    }
    auto it = players_.find(to);
    if (it == players_.end()) return;
    const Player& p = it->second;
    if (p.local) {
        QMsg m;
        m.from = from;
        m.to = to;
        m.data = data;
        queueMsgLocked(std::move(m));
    } else if (role_ == Role::Host && p.ownerConn != excludeConn) {
        auto c = conns_.find(p.ownerConn);
        if (c == conns_.end()) return;
        Writer w;
        w.u32(from);
        w.u32(to);
        w.u32(0);
        w.blob(data.data(), data.size());
        ob.add(c->second, MT_DATA, w);
    }
}

HRESULT Session::send(DPID from, DPID to, uint32_t flags, const void* data, uint32_t size) {
    if (size > kMaxFrame - 256) return DPERR_SENDTOOBIG;
    Outbox ob;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (role_ == Role::None) return DPERR_NOCONNECTION;
        auto f = players_.find(from);
        if (f == players_.end() || !f->second.local) return DPERR_INVALIDPLAYER;
        if (to != DPID_ALLPLAYERS && !players_.count(to)) return DPERR_INVALIDPLAYER;
        std::vector<uint8_t> v((const uint8_t*)data, (const uint8_t*)data + size);
        if (role_ == Role::Host) {
            deliverLocked(from, to, v, 0, ob);
        } else {
            if (role_ == Role::Client && (!host_ || host_->dead)) return DPERR_CONNECTIONLOST;
            bool toLocal = (to != DPID_ALLPLAYERS && players_[to].local);
            if (to == DPID_ALLPLAYERS || toLocal) deliverLocked(from, to, v, 0, ob);
            if (!toLocal) {
                Writer w;
                w.u32(from);
                w.u32(to);
                w.u32(flags);
                w.blob(v.data(), v.size());
                ob.add(host_, MT_DATA, w);
            }
        }
    }
    ob.flush();
    return DP_OK;
}

static bool matches(const QMsg& m, uint32_t flags, DPID fromFilter, DPID toFilter) {
    if ((flags & DPRECEIVE_TOPLAYER) && m.to != toFilter) return false;
    if ((flags & DPRECEIVE_FROMPLAYER) && m.from != fromFilter) return false;
    return true;
}

bool Session::peek(uint32_t flags, DPID fromFilter, DPID toFilter, QMsg& out) {
    std::lock_guard<std::mutex> lk(mu_);
    for (const QMsg& m : queue_) {
        if (!matches(m, flags, fromFilter, toFilter)) continue;
        out = m;
        return true;
    }
    return false;
}

void Session::pop(uint64_t seq) {
    std::lock_guard<std::mutex> lk(mu_);
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if (it->seq == seq) { queue_.erase(it); return; }
    }
}

uint32_t Session::messageCount(DPID player) {
    std::lock_guard<std::mutex> lk(mu_);
    uint32_t n = 0;
    for (const QMsg& m : queue_)
        if (player == 0 || m.to == player) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// Netzwerk-Thread
// ---------------------------------------------------------------------------
void Session::startNetThread() {
    netStop_ = false;
    net_ = std::thread(&Session::netLoop, this);
}

void Session::stopNetThread() {
    netStop_ = true;
    if (net_.joinable()) {
        if (net_.get_id() == std::this_thread::get_id()) net_.detach();
        else net_.join();
    }
}

void Session::hostRemoveConnLocked(const ConnPtr& c, Outbox& ob) {
    closeConn(c);
    conns_.erase(c->id);
    std::vector<DPID> gone;
    for (auto& kv : players_)
        if (!kv.second.local && kv.second.ownerConn == c->id) gone.push_back(kv.first);
    for (DPID id : gone) {
        Player p = players_[id];
        players_.erase(id);
        SysMsg sm;
        sm.type = DPSYS_DESTROYPLAYERORGROUP;
        sm.id = id;
        sm.shortName = p.shortName;
        sm.longName = p.longName;
        queueSysLocked(sm);
        Writer w;
        w.u32(id);
        broadcastLocked(MT_PLAYER_DEL, w, c->id, ob);
        logf("Host: Spieler 0x%X ('%s') hat die Session verlassen", (unsigned)id, p.shortName.c_str());
    }
}

void Session::connLost(const ConnPtr& c, const char* why) {
    Outbox ob;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (role_ == Role::Host) {
            if (conns_.count(c->id)) {
                if (c->state == ConnState::Joined) logf("Host: Verbindung zu %s getrennt (%s)", c->peer.str().c_str(), why);
                hostRemoveConnLocked(c, ob);
            }
        } else if (role_ == Role::Client && c == host_) {
            closeConn(c);
            if (!closing_ && !c->lostHandled.exchange(true)) {
                logf("Client: Verbindung zum Host verloren (%s)", why);
                SysMsg sm;
                sm.type = DPSYS_SESSIONLOST;
                queueSysLocked(sm);
            }
            cv_.notify_all();
        } else {
            closeConn(c);
        }
    }
    ob.flush();
}

bool Session::readFrames(const ConnPtr& c) {
    char tmp[65536];
    int n = (int)recv(c->s, tmp, sizeof(tmp), 0);
    if (n <= 0) return false;
    c->lastRecv = nowMs();
    c->rbuf.insert(c->rbuf.end(), tmp, tmp + n);
    size_t off = 0;
    while (c->rbuf.size() - off >= 5) {
        const uint8_t* b = c->rbuf.data() + off;
        uint32_t len = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
        if (len == 0 || len > kMaxFrame) return false;
        if (c->rbuf.size() - off < 4 + (size_t)len) break;
        Reader r(b + 5, len - 1);
        handleFrame(c, b[4], r);
        off += 4 + len;
        if (c->dead) return false;
    }
    if (off) c->rbuf.erase(c->rbuf.begin(), c->rbuf.begin() + (std::ptrdiff_t)off);
    return true;
}

void Session::handleFrame(const ConnPtr& c, uint8_t type, Reader& r) {
    Outbox ob;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (role_ == Role::Host) {
            switch (type) {
            case MT_QUERY: {
                if (r.u32() != kProtoMagic) { c->dead = true; break; }
                Writer w;
                w.u32(kProtoMagic);
                descLocked().write(w, false);
                ob.add(c, MT_INFO, w);
                break;
            }
            case MT_JOIN: {
                if (c->state == ConnState::Joined) break;
                uint32_t magic = r.u32();
                GUID inst = r.guid();
                std::string pw = r.str();
                HRESULT hr = DP_OK;
                if (magic != kProtoMagic || !r.ok()) hr = DPERR_GENERIC;
                else if (!guidEq(inst, desc_.instance)) hr = DPERR_NOSESSIONS;
                else if (desc_.flags & (DPSESSION_JOINDISABLED | DPSESSION_NEWPLAYERSDISABLED)) hr = DPERR_NONEWPLAYERS;
                else if (desc_.maxPlayers && players_.size() >= desc_.maxPlayers) hr = DPERR_CANTADDPLAYER;
                else if (!desc_.password.empty() && pw != desc_.password) hr = DPERR_INVALIDPASSWORD;
                Writer w;
                if (hr != DP_OK) {
                    w.u32((uint32_t)hr);
                    ob.add(c, MT_JOIN_FAIL, w);
                    logf("Host: Beitritt von %s abgelehnt (0x%08X)", c->peer.str().c_str(), (unsigned)hr);
                    break;
                }
                c->state = ConnState::Joined;
                c->lastPing = nowMs();
                descLocked().write(w, false);
                w.u32((uint32_t)players_.size());
                for (auto& kv : players_) writePlayer(w, kv.second);
                ob.add(c, MT_JOIN_OK, w);
                logf("Host: %s ist der Session beigetreten", c->peer.str().c_str());
                break;
            }
            case MT_CREATE_PLAYER: {
                if (c->state != ConnState::Joined) break;
                uint32_t req = r.u32();
                uint32_t flags = r.u32() & DPPLAYER_SPECTATOR;
                std::string s = r.str(), l = r.str();
                if (!r.ok()) break;
                HRESULT hr = DP_OK;
                DPID id = 0;
                if (desc_.maxPlayers && players_.size() >= desc_.maxPlayers) hr = DPERR_CANTADDPLAYER;
                else if (desc_.flags & DPSESSION_NEWPLAYERSDISABLED) hr = DPERR_CANTADDPLAYER;
                Writer reply;
                if (hr == DP_OK) {
                    Player p;
                    p.id = id = allocIdLocked(0);
                    p.shortName = s;
                    p.longName = l;
                    p.flags = flags;
                    p.ownerConn = c->id;
                    players_[id] = p;
                    reply.u32(req); reply.u32(DP_OK); reply.u32(id);
                    ob.add(c, MT_CREATE_PLAYER_OK, reply);   // zuerst die Antwort
                    Writer w;
                    writePlayer(w, p);
                    w.u32((uint32_t)players_.size());
                    broadcastLocked(MT_PLAYER_ADD, w, c->id, ob);
                    SysMsg sm;
                    sm.type = DPSYS_CREATEPLAYERORGROUP;
                    sm.id = id;
                    sm.shortName = s;
                    sm.longName = l;
                    sm.curPlayers = (uint32_t)players_.size();
                    queueSysLocked(sm);
                    logf("Host: neuer Spieler 0x%X '%s' von %s", (unsigned)id, s.c_str(), c->peer.str().c_str());
                } else {
                    reply.u32(req); reply.u32((uint32_t)hr); reply.u32(0);
                    ob.add(c, MT_CREATE_PLAYER_OK, reply);
                }
                break;
            }
            case MT_PLAYER_DEL: {
                if (c->state != ConnState::Joined) break;
                DPID id = r.u32();
                auto it = players_.find(id);
                if (it == players_.end() || it->second.local || it->second.ownerConn != c->id) break;
                Player p = it->second;
                players_.erase(it);
                SysMsg sm;
                sm.type = DPSYS_DESTROYPLAYERORGROUP;
                sm.id = id;
                sm.shortName = p.shortName;
                sm.longName = p.longName;
                queueSysLocked(sm);
                Writer w;
                w.u32(id);
                broadcastLocked(MT_PLAYER_DEL, w, c->id, ob);
                break;
            }
            case MT_DATA: {
                if (c->state != ConnState::Joined) break;
                DPID from = r.u32(), to = r.u32();
                r.u32();   // flags
                std::vector<uint8_t> data = r.blob();
                if (!r.ok()) break;
                auto it = players_.find(from);
                if (it == players_.end() || it->second.local || it->second.ownerConn != c->id) break;
                deliverLocked(from, to, data, c->id, ob);
                break;
            }
            case MT_PING:
            default:
                break;
            }
        } else if (role_ == Role::Client && c == host_) {
            switch (type) {
            case MT_PLAYER_ADD: {
                Player p = readPlayer(r);
                uint32_t cur = r.u32();
                (void)cur;
                if (!r.ok() || players_.count(p.id)) break;
                players_[p.id] = p;
                SysMsg sm;
                sm.type = DPSYS_CREATEPLAYERORGROUP;
                sm.id = p.id;
                sm.shortName = p.shortName;
                sm.longName = p.longName;
                sm.curPlayers = (uint32_t)players_.size();
                queueSysLocked(sm);
                break;
            }
            case MT_PLAYER_DEL: {
                DPID id = r.u32();
                auto it = players_.find(id);
                if (it == players_.end() || it->second.local) break;
                Player p = it->second;
                players_.erase(it);
                SysMsg sm;
                sm.type = DPSYS_DESTROYPLAYERORGROUP;
                sm.id = id;
                sm.shortName = p.shortName;
                sm.longName = p.longName;
                queueSysLocked(sm);
                break;
            }
            case MT_DATA: {
                DPID from = r.u32(), to = r.u32();
                r.u32();
                std::vector<uint8_t> data = r.blob();
                if (!r.ok()) break;
                deliverLocked(from, to, data, 0, ob);
                break;
            }
            case MT_DESC: {
                SessionInfo d;
                if (!d.read(r)) break;
                GUID inst = desc_.instance;
                desc_ = d;
                desc_.instance = inst;
                break;
            }
            case MT_CREATE_PLAYER_OK: {
                uint32_t req = r.u32();
                HRESULT hr = (HRESULT)r.u32();
                DPID id = r.u32();
                if (!r.ok()) break;
                auto pc = pendingCreate_.find(req);
                if (pc != pendingCreate_.end()) {
                    if (hr == DP_OK) {
                        // Spieler sofort eintragen, damit direkt folgende Daten nicht verloren gehen
                        Player p;
                        p.id = id;
                        p.shortName = pc->second.s;
                        p.longName = pc->second.l;
                        p.flags = pc->second.flags;
                        p.local = true;
                        p.event = pc->second.event;
                        players_[id] = p;
                    }
                    pendingCreate_.erase(pc);
                }
                replies_[req] = std::make_pair(hr, id);
                cv_.notify_all();
                break;
            }
            case MT_PING:
            default:
                break;
            }
        }
    }
    ob.flush();
}

void Session::handleUdp() {
    uint8_t buf[512];
    sockaddr_storage from{};
    socklen_t fl = sizeof(from);
    int n = (int)recvfrom(udp_, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fl);
    if (n < 8) return;
    uint32_t m, v;
    std::memcpy(&m, buf, 4);
    std::memcpy(&v, buf + 4, 4);
    if (m != kUdpQueryMagic || v != kProtoMagic) return;
    Writer w;
    w.u32(kUdpInfoMagic);
    w.u32(cfg_.port);
    {
        std::lock_guard<std::mutex> lk(mu_);
        descLocked().write(w, false);
    }
    sendto(udp_, (const char*)w.b.data(), (int)w.b.size(), 0, (sockaddr*)&from, fl);
}

void Session::netLoop() {
    uint64_t lastHousekeeping = 0;
    while (!netStop_) {
        // Momentaufnahme der Sockets
        std::vector<ConnPtr> cs;
        sock_t l = A3_BADSOCK, u = A3_BADSOCK;
        Role role;
        {
            std::lock_guard<std::mutex> lk(mu_);
            role = role_;
            l = listen_;
            u = udp_;
            if (role == Role::Host)
                for (auto& kv : conns_) cs.push_back(kv.second);
            else if (role == Role::Client && host_ && !host_->dead)
                cs.push_back(host_);
        }
        fd_set rf;
        FD_ZERO(&rf);
        sock_t maxs = 0;
        auto add = [&](sock_t s) {
            if (s == A3_BADSOCK) return;
            FD_SET(s, &rf);
            if (s > maxs) maxs = s;
        };
        if (role == Role::Host) { add(l); add(u); }
        for (auto& c : cs) if (!c->dead) add(c->s);
        timeval tv{0, 20000};
        int rc = select((int)maxs + 1, &rf, nullptr, nullptr, &tv);
        if (netStop_) break;

        if (rc > 0) {
            if (role == Role::Host && l != A3_BADSOCK && FD_ISSET(l, &rf)) {
                sockaddr_storage ss{};
                socklen_t sl = sizeof(ss);
                sock_t ns = accept(l, (sockaddr*)&ss, &sl);
                if (ns != A3_BADSOCK) {
                    sockSetNoDelay(ns);
                    auto c = std::make_shared<Conn>();
                    c->s = ns;
                    std::memcpy(&c->peer.ss, &ss, sizeof(ss));
                    c->peer.len = (int)sl;
                    c->created = c->lastRecv = c->lastPing = nowMs();
                    std::lock_guard<std::mutex> lk(mu_);
                    size_t pending = 0;
                    for (auto& kv : conns_) if (kv.second->state == ConnState::Pending) ++pending;
                    if (pending > 16) {
                        closeConn(c);
                    } else {
                        c->id = nextConn_++;
                        conns_[c->id] = c;
                    }
                }
            }
            if (role == Role::Host && u != A3_BADSOCK && FD_ISSET(u, &rf)) handleUdp();
            for (auto& c : cs) {
                if (c->dead || c->s == A3_BADSOCK || !FD_ISSET(c->s, &rf)) continue;
                if (!readFrames(c)) connLost(c, "Verbindung beendet");
            }
        }

        // Keepalive, Zeitueberschreitungen
        uint64_t now = nowMs();
        if (now - lastHousekeeping >= 250) {
            lastHousekeeping = now;
            Outbox ob;
            std::vector<ConnPtr> lost, stale;
            {
                std::lock_guard<std::mutex> lk(mu_);
                for (auto& c : cs) {
                    if (c->dead) { lost.push_back(c); continue; }
                    if (c->state == ConnState::Pending) {
                        if (now - c->created > 15000) stale.push_back(c);
                        continue;
                    }
                    if (now - c->lastRecv > (uint64_t)cfg_.timeoutSec * 1000) { lost.push_back(c); continue; }
                    if (now - c->lastPing >= 5000) {
                        c->lastPing = now;
                        Writer w;
                        ob.add(c, MT_PING, w);
                    }
                }
            }
            ob.flush();
            for (auto& c : stale) connLost(c, "kein Beitritt");
            for (auto& c : lost) connLost(c, "Zeitueberschreitung");
        }
    }
}

} // namespace a3
