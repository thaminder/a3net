// netio.h - duenne, plattformunabhaengige Socket-Schicht (Winsock / BSD-Sockets)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
typedef SOCKET sock_t;
#  define A3_BADSOCK INVALID_SOCKET
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
typedef int sock_t;
#  define A3_BADSOCK (-1)
#endif

namespace a3 {

struct NetAddr {
    sockaddr_storage ss{};
    int len = 0;
    std::string str() const;           // "1.2.3.4:5" bzw. "[::1]:5"
    bool sameHost(const NetAddr& o) const;
};

bool netInit();
void sockClose(sock_t s);
void sockShutdown(sock_t s);
int  sockLastError();

// Zerlegt "host", "host:port", "[v6]:port" oder "v6" in Host und Port.
void splitHostPort(const std::string& in, uint16_t defPort, std::string& host, uint16_t& port);
// Namensaufloesung (IPv4 und IPv6). Liefert alle Adressen.
std::vector<NetAddr> resolve(const std::string& hostport, uint16_t defPort, bool udp = false);

// TCP-Verbindung mit Timeout. Liefert A3_BADSOCK bei Fehler.
sock_t tcpConnect(const NetAddr& a, int timeoutMs);
// TCP-Listener auf Port (IPv6 dual-stack, sonst IPv4).
sock_t tcpListen(uint16_t port, bool& dualStack);
// UDP-Socket fuer LAN-Suche (IPv4, Broadcast erlaubt). port=0 -> beliebig.
sock_t udpOpen(uint16_t port);

void sockSetNoDelay(sock_t s);
// Sendet alle Bytes (blockierend). false bei Fehler.
bool sendAll(sock_t s, const void* data, size_t len);
// Wartet bis lesbar. 1 = lesbar, 0 = Timeout, -1 = Fehler
int  waitReadable(sock_t s, int timeoutMs);

} // namespace a3
