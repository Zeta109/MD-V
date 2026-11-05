// ============================================================
// MD-V - GLOBALER RING C36 GEERDET - 35 KNOTEN WELTWEIT
// EIN FILE = ALLES - node.cpp
// ============================================================
// DOCKERFILE - dazu in selben Ordner legen:
// FROM gcc:12-bookworm
// WORKDIR /app
// COPY node.cpp .
// RUN g++ -O2 -pthread -o node node.cpp
// CMD ["./node"]
//
// DEPLOY - 35 Nodes:
// REGIONS="fra ams lhr cdg iad ord dfw mad lis ber muc vie zrh sto osl cph hel waw bud ath ist dxb bom sin hkg nrt syd gru eze yyz yul sea lax sfo"
// i=1; for R in $REGIONS; do
//   LEFT=""; RIGHT=""
//   if [ $i -gt 1 ]; then LEFT="https://mdv-node-$((i-1)).fly.dev"; fi
//   if [ $i -lt 35 ]; then RIGHT="https://mdv-node-$((i+1)).fly.dev"; fi
//   flyctl deploy --app mdv-node-$i --region $R -e MY_ID=$i -e PORT=8080 -e LEFT_URL=$LEFT -e RIGHT_URL=$RIGHT
//   i=$((i+1))
// done
//
// VERSCHACHTELUNG:
// Node 1:  MY_ID=1,  LEFT="", RIGHT=node-2  -> links geerdet 0
// Node i:  MY_ID=i,  LEFT=node-(i-1), RIGHT=node-(i+1)
// Node 35: MY_ID=35, LEFT=node-34, RIGHT="" -> rechts geerdet 0
// Regel: x = x - 0.25 * (2*x - left - right)
// DT=0.5, 5000 Steps, 50ms, Start x in [-1,1] Zufall
// Schlüssel korrigiert: (1+sqrt(2))^2 = 5.828427124746...
// ============================================================

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

int MY_ID = 1;
int PORT = 8080;
std::string LEFT_URL;
std::string RIGHT_URL;
std::string LOGGER_URL;

std::mutex mtx;
double x_val = 0.0;
double left_x = 0.0;
double right_x = 0.0;

std::string get_env(const char* k, const char* d) {
    const char* v = getenv(k);
    return v ? std::string(v) : std::string(d);
}

// Einfacher HTTP GET, gibt x aus JSON {"id":..,"x":..} zurück
bool http_get_x(const std::string& url, double& out) {
    if (url.empty()) return false;
    // URL parsen: https://host[:port]/ oder http://host:port/
    std::string host = url;
    std::string path = "/";
    size_t p = host.find("://");
    if (p != std::string::npos) host = host.substr(p + 3);
    size_t s = host.find("/");
    if (s != std::string::npos) { path = host.substr(s); host = host.substr(0, s); }
    int port = 80;
    size_t c = host.find(":");
    if (c != std::string::npos) { port = atoi(host.substr(c + 1).c_str()); host = host.substr(0, c); }
    // Für fly.dev: HTTPS wird von Fly terminiert, intern HTTP auf 8080
    // Darum: wenn Host .fly.dev ist, Port 80 nutzen, Pfad bleibt /
    if (url.find("fly.dev") != std::string::npos) port = 80;

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) return false;
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return false; }
    struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) { close(sock); freeaddrinfo(res); return false; }
    freeaddrinfo(res);
    std::string req = "GET " + path + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    send(sock, req.c_str(), req.size(), 0);
    std::string resp;
    char buf[4096];
    int n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) { buf[n] = 0; resp += buf; }
    close(sock);
    size_t pos = resp.find("\"x\":");
    if (pos == std::string::npos) return false;
    try { out = std::stod(resp.substr(pos + 4)); } catch (...) { return false; }
    return true;
}

void http_post_log(const std::string& url, const std::string& body) {
    if (url.empty()) return;
    std::string host = url;
    size_t p = host.find("://");
    if (p != std::string::npos) host = host.substr(p + 3);
    size_t s = host.find("/");
    std::string path = "/";
    if (s != std::string::npos) { path = host.substr(s); host = host.substr(0, s); }
    int port = 80;
    size_t c = host.find(":");
    if (c != std::string::npos) { port = atoi(host.substr(c + 1).c_str()); host = host.substr(0, c); }
    if (url.find("fly.dev") != std::string::npos) port = 80;
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) return;
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return; }
    struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) { close(sock); freeaddrinfo(res); return; }
    freeaddrinfo(res);
    std::string req = "POST " + path + " HTTP/1.0\r\nHost: " + host + "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    send(sock, req.c_str(), req.size(), 0);
    close(sock);
}

// HTTP Server - gibt {"id":MY_ID,"x":x} zurück
void server_loop() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, 16);
    while (true) {
        int client = accept(srv, nullptr, nullptr);
        if (client < 0) continue;
        char buf[2048]; recv(client, buf, sizeof(buf) - 1, 0);
        double v;
        { std::lock_guard<std::mutex> lk(mtx); v = x_val; }
        std::string body = "{\"id\":" + std::to_string(MY_ID) + ",\"x\":" + std::to_string(v) + "}";
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        send(client, resp.c_str(), resp.size(), 0);
        close(client);
    }
}

int main() {
    MY_ID = atoi(get_env("MY_ID", "1").c_str());
    PORT = atoi(get_env("PORT", "8080").c_str());
    LEFT_URL = get_env("LEFT_URL", "");
    RIGHT_URL = get_env("RIGHT_URL", "");
    LOGGER_URL = get_env("LOGGER_URL", "");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    { std::lock_guard<std::mutex> lk(mtx); x_val = dist(gen); }

    std::cout << "Node " << MY_ID << " startet, PORT=" << PORT
              << " LEFT=" << LEFT_URL << " RIGHT=" << RIGHT_URL
              << " x=" << x_val << std::endl;

    std::thread(server_loop).detach();

    for (int step = 0; step < 5000; step++) {
        double l = 0.0, r = 0.0;
        if (MY_ID != 1 && !LEFT_URL.empty()) {
            double tmp; if (http_get_x(LEFT_URL, tmp)) l = tmp;
        } else l = 0.0;
        if (MY_ID != 35 && !RIGHT_URL.empty()) {
            double tmp; if (http_get_x(RIGHT_URL, tmp)) r = tmp;
        } else r = 0.0;

        double v;
        {
            std::lock_guard<std::mutex> lk(mtx);
            left_x = l; right_x = r;
            // Kern-Regel: Lx = 2*x - left - right, x = x - 0.25 * Lx
            x_val = x_val - 0.25 * (2 * x_val - left_x - right_x);
            v = x_val;
        }

        if (!LOGGER_URL.empty() && step % 100 == 0) {
            std::string body = "{\"id\":" + std::to_string(MY_ID) +
                               ",\"step\":" + std::to_string(step) +
                               ",\"x\":" + std::to_string(v) + "}";
            http_post_log(LOGGER_URL, body);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << "Node " << MY_ID << " fertig. x=" << x_val << std::endl;
    return 0;
}
