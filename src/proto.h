// proto.h - Das eigene Netzwerkprotokoll von A3Net
//
// Jede TCP-Nachricht ("Frame"):  u32 laenge | u8 typ | nutzdaten   (little endian)
// laenge = 1 + Nutzdatenlaenge.
//
// Ablauf:
//   Suche:     Client --QUERY--> Host,  Host --INFO--> Client  (TCP, oder UDP-Broadcast im LAN)
//   Beitritt:  Client --JOIN-->  Host,  Host --JOIN_OK/JOIN_FAIL--> Client
//   Spieler:   Client --CREATE_PLAYER--> Host --CREATE_PLAYER_OK--> Client,
//              Host --PLAYER_ADD/PLAYER_DEL--> alle anderen Clients
//   Daten:     DATA in beide Richtungen; der Host leitet zwischen Clients weiter (Stern).
//   Sonst:     DESC (Session-Beschreibung geaendert), PING (Keepalive)
#pragma once

#include "dptypes.h"

#include <string>
#include <vector>

namespace a3 {

static const uint32_t kProtoMagic   = 0x314E3341; // "A3N1"
static const uint32_t kMaxFrame     = 4u * 1024 * 1024;
static const uint32_t kUdpQueryMagic = 0x514E3341; // "A3NQ"
static const uint32_t kUdpInfoMagic  = 0x494E3341; // "A3NI"

enum MsgType : uint8_t {
    MT_QUERY = 1,
    MT_INFO = 2,
    MT_JOIN = 3,
    MT_JOIN_OK = 4,
    MT_JOIN_FAIL = 5,
    MT_CREATE_PLAYER = 6,
    MT_CREATE_PLAYER_OK = 7,
    MT_PLAYER_ADD = 8,
    MT_PLAYER_DEL = 9,
    MT_DATA = 10,
    MT_DESC = 11,
    MT_PING = 12,
};

class Writer {
public:
    std::vector<uint8_t> b;
    void u8(uint8_t v) { b.push_back(v); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((uint8_t)(v >> (8 * i))); }
    void bytes(const void* p, size_t n) { const uint8_t* q = (const uint8_t*)p; b.insert(b.end(), q, q + n); }
    void blob(const void* p, size_t n) { u32((uint32_t)n); bytes(p, n); }
    void str(const std::string& s) { blob(s.data(), s.size()); }
    void guid(const GUID& g) { bytes(&g, sizeof(GUID)); }
    // Frame-Kopf voranstellen: liefert fertigen Frame
    static std::vector<uint8_t> frame(uint8_t type, const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> f;
        uint32_t len = (uint32_t)payload.size() + 1;
        for (int i = 0; i < 4; ++i) f.push_back((uint8_t)(len >> (8 * i)));
        f.push_back(type);
        f.insert(f.end(), payload.begin(), payload.end());
        return f;
    }
};

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    bool ok() const { return ok_; }
    size_t remaining() const { return n_ - i_; }
    uint8_t u8() { if (!need(1)) return 0; return p_[i_++]; }
    uint32_t u32() {
        if (!need(4)) return 0;
        uint32_t v = (uint32_t)p_[i_] | ((uint32_t)p_[i_ + 1] << 8) | ((uint32_t)p_[i_ + 2] << 16) | ((uint32_t)p_[i_ + 3] << 24);
        i_ += 4;
        return v;
    }
    std::vector<uint8_t> blob() {
        uint32_t n = u32();
        std::vector<uint8_t> v;
        if (!need(n)) return v;
        v.assign(p_ + i_, p_ + i_ + n);
        i_ += n;
        return v;
    }
    std::string str() { auto v = blob(); return std::string(v.begin(), v.end()); }
    GUID guid() { GUID g{}; if (need(sizeof(GUID))) { std::memcpy(&g, p_ + i_, sizeof(GUID)); i_ += sizeof(GUID); } return g; }
private:
    bool need(size_t n) { if (!ok_ || n_ - i_ < n) { ok_ = false; return false; } return true; }
    const uint8_t* p_;
    size_t n_;
    size_t i_ = 0;
    bool ok_ = true;
};

// Session-Beschreibung im internen Format
struct SessionInfo {
    GUID instance{};
    GUID application{};
    uint32_t flags = 0;
    uint32_t maxPlayers = 0;
    uint32_t curPlayers = 0;
    uint32_t user[4] = {0, 0, 0, 0};
    std::string name;
    std::string password;

    void write(Writer& w, bool withPassword) const {
        w.guid(instance); w.guid(application);
        w.u32(flags); w.u32(maxPlayers); w.u32(curPlayers);
        for (uint32_t u : user) w.u32(u);
        w.str(name);
        w.u8(password.empty() ? 0 : 1);
        w.str(withPassword ? password : std::string());
    }
    bool read(Reader& r) {
        instance = r.guid(); application = r.guid();
        flags = r.u32(); maxPlayers = r.u32(); curPlayers = r.u32();
        for (uint32_t& u : user) u = r.u32();
        name = r.str();
        bool hasPw = r.u8() != 0;
        password = r.str();
        if (hasPw && password.empty()) password = "*";   // nur Kennzeichen "Passwort noetig"
        return r.ok();
    }
};

} // namespace a3
