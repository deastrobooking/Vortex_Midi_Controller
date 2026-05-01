#pragma once
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>

// WebSocket server wrapping libwebsockets (or uWebSockets).
// Messages are UTF-8 JSON strings.

using WsMessageCallback = std::function<void(const std::string& message)>;
using WsBroadcastFn     = std::function<void(const std::string& message)>;

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    // Start listening on the given port. Non-blocking: runs on own thread.
    bool listen(uint16_t port);
    void stop();

    // Called for every message received from any client.
    void onMessage(WsMessageCallback cb);

    // Broadcast a JSON string to all connected clients.
    void broadcast(const std::string& json);

    int connectedClients() const;

private:
    void runLoop();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::thread           m_thread;
    std::atomic<bool>     m_running{false};
    WsMessageCallback     m_callback;
    uint16_t              m_port{8080};
};
