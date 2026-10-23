// session.h - Plattformunabhaengiger Kern: Sessions, Spieler, Nachrichten, Netzwerk.
//
// Die COM-Schicht (directplay.cpp) uebersetzt nur zwischen den DirectPlay-Strukturen
// und dieser Klasse. Dadurch laesst sich der Kern auch ausserhalb von Windows testen.
#pragma once

#include "dptypes.h"
#include "netio.h"
#include "proto.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace a3 {

struct Config {
    uint16_t port = 47624;          // TCP (Spiel) und UDP (LAN-Suche)
    std::string joinAddress;        // fest eingestellte Host-Adresse (leer = fragen)
    bool askForAddress = true;      // beim Suchen Dialog fuer IP/Hostname zeigen
    bool lanBroadcast = true;       // zusaetzlich im LAN per Broadcast suchen
    bool upnp = true;               // als Host Port per UPnP im Router oeffnen
    int  timeoutSec = 30;           // Verbindung gilt nach so vielen Sekunden Stille als verloren
};

struct Player {
    DPID id = 0;
    std::string shortName;
    std::string longName;
    uint32_t flags = 0;             // DPPLAYER_SERVERPLAYER / DPPLAYER_SPECTATOR
    bool local = false;
    void* event = nullptr;          // Event-Handle des Spiels (nur lokale Spieler)
    std::vector<uint8_t> localData, remoteData;
    uint32_t ownerConn = 0;         // nur Host: Verbindung, der der Spieler gehoert
};

struct SysMsg {
    uint32_t type = 0;              // DPSYS_*
    uint32_t playerType = DPPLAYERTYPE_PLAYER;
    DPID id = 0;
    std::string shortName, longName;
    uint32_t curPlayers = 0;
    uint32_t flags = 0;
};

struct QMsg {
    uint64_t seq = 0;
    DPID from = 0;                  // 0 = Systemnachricht
    DPID to = 0;
    bool sys = false;
    SysMsg sm;
    std::vector<uint8_t> data;
};

class Session {
public:
    explicit Session(const Config& cfg);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Vom Plattform-Layer gesetzt:
    std::function<void(void* event)> signalEvent;                  // SetEvent
    std::function<bool(std::string& address)> askAddress;          // Dialog, false = Abbruch
    std::function<void(uint16_t port, bool add)> portMapping;      // UPnP

    // --- Verbindung (InitializeConnection) ---
    HRESULT initializeConnection(const std::string& inetAddress);
    bool connectionInitialized() const { return connInit_; }

    // --- Suche ---
    HRESULT enumSessions(uint32_t flags, uint32_t timeoutMs, std::vector<SessionInfo>& out);
    void stopEnum();

    // --- Session ---
    HRESULT hostOpen(const SessionInfo& desc);
    HRESULT joinOpen(const GUID& instance, const std::string& password);
    void close();
    bool isOpen() const { return role_ != Role::None; }
    bool isHost() const { return role_ == Role::Host; }
    SessionInfo desc();
    HRESULT setDesc(const SessionInfo& d);

    // --- Spieler ---
    HRESULT createPlayer(const std::string& shortName, const std::string& longName,
                         uint32_t flags, void* event, DPID& outId);
    HRESULT destroyPlayer(DPID id);
    std::vector<Player> players();
    bool getPlayer(DPID id, Player& out);
    HRESULT setPlayerName(DPID id, const std::string& s, const std::string& l);
    HRESULT setPlayerData(DPID id, const void* data, uint32_t size, bool local);

    // --- Nachrichten ---
    HRESULT send(DPID from, DPID to, uint32_t flags, const void* data, uint32_t size);
    // Liefert die naechste passende Nachricht (ohne zu entfernen). false = keine.
    bool peek(uint32_t flags, DPID fromFilter, DPID toFilter, QMsg& out);
    void pop(uint64_t seq);
    uint32_t messageCount(DPID player);

private:
    enum class Role { None, Host, Client };
    enum class ConnState { Pending, Joined };

    struct Conn {
        uint32_t id = 0;
        sock_t s = A3_BADSOCK;
        NetAddr peer;
        ConnState state = ConnState::Pending;
        std::vector<uint8_t> rbuf;
        std::mutex wmu;
        std::atomic<bool> dead{false};
        std::atomic<bool> lostHandled{false};
        uint64_t lastRecv = 0;
        uint64_t lastPing = 0;
        uint64_t created = 0;
    };
    typedef std::shared_ptr<Conn> ConnPtr;

    // Zu sendende Frames werden unter mu_ gesammelt und erst nach dem Entsperren
    // verschickt (verhindert Verklemmungen bei vollen TCP-Puffern).
    struct Outbox {
        std::vector<std::pair<ConnPtr, std::vector<uint8_t>>> items;
        void add(const ConnPtr& c, uint8_t type, const Writer& w) { items.emplace_back(c, Writer::frame(type, w.b)); }
        void flush();
    };

    struct Found {
        SessionInfo info;
        NetAddr addr;               // TCP-Adresse des Hosts
        uint64_t lastSeen = 0;
    };

    // Hilfsfunktionen (mu_ gehalten)
    void queueMsgLocked(QMsg&& m);
    void queueSysLocked(const SysMsg& sm);
    void deliverLocked(DPID from, DPID to, const std::vector<uint8_t>& data, uint32_t excludeConn, Outbox& ob);
    void hostRemoveConnLocked(const ConnPtr& c, Outbox& ob);
    void writePlayer(Writer& w, const Player& p);
    Player readPlayer(Reader& r);
    SessionInfo descLocked();
    DPID allocIdLocked(uint32_t flags);
    void broadcastLocked(uint8_t type, const Writer& w, uint32_t exceptConn, Outbox& ob);

    // Netzwerk-Thread
    void startNetThread();
    void stopNetThread();
    void netLoop();
    bool readFrames(const ConnPtr& c);  // false = Verbindung zu
    void connLost(const ConnPtr& c, const char* why);
    static void closeConn(const ConnPtr& c);
    void handleFrame(const ConnPtr& c, uint8_t type, Reader& r);
    void handleUdp();

    // Suche
    void discoveryLoop();
    void discoveryPass(int budgetMs);
    bool queryTcp(const NetAddr& a, SessionInfo& out);
    void ensureDiscoveryThread();

    Config cfg_;
    std::mutex mu_;
    std::condition_variable cv_;

    std::atomic<bool> connInit_{false};
    std::string initAddress_;
    std::atomic<Role> role_{Role::None};
    SessionInfo desc_;
    std::map<DPID, Player> players_;
    std::deque<QMsg> queue_;
    uint64_t nextSeq_ = 1;
    DPID nextId_ = 0x00002001;

    // Host
    sock_t listen_ = A3_BADSOCK;
    sock_t udp_ = A3_BADSOCK;
    std::map<uint32_t, ConnPtr> conns_;
    uint32_t nextConn_ = 1;
    bool portMapped_ = false;

    // Client
    ConnPtr host_;
    std::map<uint32_t, std::pair<HRESULT, DPID>> replies_;
    struct PendingCreate { std::string s, l; uint32_t flags = 0; void* event = nullptr; };
    std::map<uint32_t, PendingCreate> pendingCreate_;   // wird beim OK vom Netz-Thread eingetragen
    uint32_t nextReq_ = 1;
    bool closing_ = false;

    // Netzwerk-Thread
    std::thread net_;
    std::atomic<bool> netStop_{false};

    // Suche
    std::thread disc_;
    std::atomic<bool> discStop_{false};
    std::atomic<bool> discRunning_{false};
    std::atomic<uint64_t> lastEnumCall_{0};
    bool targetDecided_ = false;
    std::string target_;
    std::map<std::string, Found> found_;   // Schluessel: GUID als Bytes
    std::mutex discMu_;                    // serialisiert Suchdurchlaeufe
};

uint64_t nowMs();
GUID randomGuid();
std::string guidKey(const GUID& g);
std::string guidStr(const GUID& g);

} // namespace a3
