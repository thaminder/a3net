#include "netio.h"
#include "log.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <cerrno>
#  include <fcntl.h>
#  include <poll.h>
#  include <unistd.h>
#endif

namespace a3 {

#ifdef _WIN32
#  define A3_SEND_FLAGS 0
#else
#  define A3_SEND_FLAGS MSG_NOSIGNAL
#endif

bool netInit() {
#ifdef _WIN32
    static bool done = false;
    if (done) return true;
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) return false;
    done = true;
#endif
    return true;
}

void sockClose(sock_t s) {
    if (s == A3_BADSOCK) return;
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

void sockShutdown(sock_t s) {
    if (s == A3_BADSOCK) return;
#ifdef _WIN32
    shutdown(s, SD_BOTH);
#else
    shutdown(s, SHUT_RDWR);
#endif
}

int sockLastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static void setNonBlocking(sock_t s, bool nb) {
#ifdef _WIN32
    u_long v = nb ? 1 : 0;
    ioctlsocket(s, FIONBIO, &v);
#else
    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, nb ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK));
#endif
}

std::string NetAddr::str() const {
    char host[INET6_ADDRSTRLEN] = "?";
    uint16_t port = 0;
    if (ss.ss_family == AF_INET) {
        const sockaddr_in* a = (const sockaddr_in*)&ss;
        inet_ntop(AF_INET, (void*)&a->sin_addr, host, sizeof(host));
        port = ntohs(a->sin_port);
        return std::string(host) + ":" + std::to_string(port);
    }
    if (ss.ss_family == AF_INET6) {
        const sockaddr_in6* a = (const sockaddr_in6*)&ss;
        inet_ntop(AF_INET6, (void*)&a->sin6_addr, host, sizeof(host));
        port = ntohs(a->sin6_port);
        return "[" + std::string(host) + "]:" + std::to_string(port);
    }
    return "?";
}

bool NetAddr::sameHost(const NetAddr& o) const {
    if (ss.ss_family != o.ss.ss_family) return false;
    if (ss.ss_family == AF_INET)
        return std::memcmp(&((const sockaddr_in*)&ss)->sin_addr, &((const sockaddr_in*)&o.ss)->sin_addr, 4) == 0;
    if (ss.ss_family == AF_INET6)
        return std::memcmp(&((const sockaddr_in6*)&ss)->sin6_addr, &((const sockaddr_in6*)&o.ss)->sin6_addr, 16) == 0;
    return false;
}

void splitHostPort(const std::string& inRaw, uint16_t defPort, std::string& host, uint16_t& port) {
    std::string in = inRaw;
    // Leerzeichen entfernen
    while (!in.empty() && (in.back() == ' ' || in.back() == '\t' || in.back() == '\r' || in.back() == '\n')) in.pop_back();
    size_t b = 0;
    while (b < in.size() && (in[b] == ' ' || in[b] == '\t')) ++b;
    in = in.substr(b);

    port = defPort;
    host = in;
    if (in.empty()) return;
    if (in[0] == '[') {                              // [v6]:port
        size_t e = in.find(']');
        if (e == std::string::npos) return;
        host = in.substr(1, e - 1);
        if (e + 1 < in.size() && in[e + 1] == ':') port = (uint16_t)std::atoi(in.c_str() + e + 2);
        return;
    }
    size_t c = in.find(':');
    if (c != std::string::npos && in.find(':', c + 1) == std::string::npos) { // genau ein ':'
        host = in.substr(0, c);
        int p = std::atoi(in.c_str() + c + 1);
        if (p > 0 && p < 65536) port = (uint16_t)p;
    }
    if (port == 0) port = defPort;
}

std::vector<NetAddr> resolve(const std::string& hostport, uint16_t defPort, bool udp) {
    std::vector<NetAddr> out;
    std::string host;
    uint16_t port;
    splitHostPort(hostport, defPort, host, port);
    if (host.empty()) return out;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = udp ? SOCK_DGRAM : SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string ps = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), ps.c_str(), &hints, &res);
    if (rc != 0 || !res) {
        logf("resolve('%s') fehlgeschlagen (%d)", host.c_str(), rc);
        return out;
    }
    for (addrinfo* r = res; r; r = r->ai_next) {
        if (r->ai_family != AF_INET && r->ai_family != AF_INET6) continue;
        NetAddr a;
        std::memcpy(&a.ss, r->ai_addr, r->ai_addrlen);
        a.len = (int)r->ai_addrlen;
        out.push_back(a);
    }
    freeaddrinfo(res);
    return out;
}

sock_t tcpConnect(const NetAddr& a, int timeoutMs) {
    sock_t s = socket(a.ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (s == A3_BADSOCK) return s;
    setNonBlocking(s, true);
    int rc = connect(s, (const sockaddr*)&a.ss, a.len);
    if (rc != 0) {
        int e = sockLastError();
#ifdef _WIN32
        bool inProgress = (e == WSAEWOULDBLOCK || e == WSAEINPROGRESS);
#else
        bool inProgress = (e == EINPROGRESS || e == EWOULDBLOCK);
#endif
        if (!inProgress) { sockClose(s); return A3_BADSOCK; }
        fd_set wf, ef;
        FD_ZERO(&wf); FD_ZERO(&ef);
        FD_SET(s, &wf); FD_SET(s, &ef);
        timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        rc = select((int)s + 1, nullptr, &wf, &ef, &tv);
        if (rc <= 0 || FD_ISSET(s, &ef)) { sockClose(s); return A3_BADSOCK; }
        int err = 0;
        socklen_t el = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &el);
        if (err != 0) { sockClose(s); return A3_BADSOCK; }
    }
    setNonBlocking(s, false);
    sockSetNoDelay(s);
    return s;
}

sock_t tcpListen(uint16_t port, bool& dualStack) {
    dualStack = false;
#ifndef _WIN32
    int one = 1;
#endif
    // Zuerst IPv6 mit IPv4-Unterstuetzung versuchen
    sock_t s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (s != A3_BADSOCK) {
        int zero = 0;
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&zero, sizeof(zero));
#ifndef _WIN32
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
#endif
        sockaddr_in6 a{};
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(port);
        a.sin6_addr = in6addr_any;
        if (bind(s, (sockaddr*)&a, sizeof(a)) == 0 && listen(s, 8) == 0) {
            dualStack = true;
            return s;
        }
        sockClose(s);
    }
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == A3_BADSOCK) return s;
#ifndef _WIN32
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
#endif
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (sockaddr*)&a, sizeof(a)) != 0 || listen(s, 8) != 0) {
        logf("tcpListen: Port %u belegt oder gesperrt (Fehler %d)", port, sockLastError());
        sockClose(s);
        return A3_BADSOCK;
    }
    return s;
}

sock_t udpOpen(uint16_t port) {
    sock_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == A3_BADSOCK) return s;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&one, sizeof(one));
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (sockaddr*)&a, sizeof(a)) != 0) {
        logf("udpOpen: bind auf Port %u fehlgeschlagen (%d)", port, sockLastError());
        sockClose(s);
        return A3_BADSOCK;
    }
    return s;
}

void sockSetNoDelay(sock_t s) {
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
}

bool sendAll(sock_t s, const void* data, size_t len) {
    const char* p = (const char*)data;
    while (len > 0) {
        int n = (int)::send(s, p, (int)len, A3_SEND_FLAGS);
        if (n <= 0) {
#ifndef _WIN32
            if (n < 0 && errno == EINTR) continue;
#endif
            return false;
        }
        p += n;
        len -= (size_t)n;
    }
    return true;
}

int waitReadable(sock_t s, int timeoutMs) {
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(s, &rf);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    int rc = select((int)s + 1, &rf, nullptr, nullptr, &tv);
    if (rc < 0) return -1;
    return rc > 0 ? 1 : 0;
}

} // namespace a3
