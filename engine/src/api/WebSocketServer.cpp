#include "WebSocketServer.h"
#include "MessageHandler.h"
#include <libwebsockets.h>
#include <mutex>
#include <queue>
#include <vector>
#include <cstring>

// ─── Per-session data ─────────────────────────────────────────────────────────
struct SessionData {
    uint8_t sendBuf[LWS_PRE + 4096];
};

// ─── Global state shared with lws callbacks ───────────────────────────────────
struct WebSocketServer::Impl {
    lws_context*                m_ctx{nullptr};
    std::vector<lws*>           m_clients;
    std::mutex                  m_clientMtx;
    std::queue<std::string>     m_outbox;
    std::mutex                  m_outboxMtx;
    WsMessageCallback*          m_callback{nullptr};
    std::atomic<int>            m_clientCount{0};
};

// Forward declarations
static int lwsCallback(lws* wsi, lws_callback_reasons reason,
                       void* user, void* in, size_t len);

WebSocketServer::WebSocketServer() : m_impl(std::make_unique<Impl>()) {}

WebSocketServer::~WebSocketServer() { stop(); }

bool WebSocketServer::listen(uint16_t port) {
    m_port = port;
    m_impl->m_callback = &m_callback;

    static lws_protocols protocols[] = {
        {"vortex-ws", lwsCallback, sizeof(SessionData), 4096, 0, nullptr, 0},
        {nullptr, nullptr, 0, 0, 0, nullptr, 0}
    };

    lws_context_creation_info info{};
    info.port      = port;
    info.protocols = protocols;
    info.user      = m_impl.get();
    info.options   = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    m_impl->m_ctx = lws_create_context(&info);
    if (!m_impl->m_ctx) return false;

    m_running = true;
    m_thread  = std::thread(&WebSocketServer::runLoop, this);
    return true;
}

void WebSocketServer::stop() {
    m_running = false;
    if (m_impl->m_ctx) {
        lws_context_destroy(m_impl->m_ctx);
        m_impl->m_ctx = nullptr;
    }
    if (m_thread.joinable()) m_thread.join();
}

void WebSocketServer::onMessage(WsMessageCallback cb) {
    m_callback = std::move(cb);
}

void WebSocketServer::broadcast(const std::string& json) {
    std::lock_guard<std::mutex> lock(m_impl->m_outboxMtx);
    m_impl->m_outbox.push(json);
    if (m_impl->m_ctx) lws_cancel_service(m_impl->m_ctx);
}

int WebSocketServer::connectedClients() const {
    return m_impl->m_clientCount.load();
}

void WebSocketServer::runLoop() {
    while (m_running) {
        lws_service(m_impl->m_ctx, 10 /* timeout ms */);
    }
}

// ─── libwebsockets callback ───────────────────────────────────────────────────

static int lwsCallback(lws* wsi, lws_callback_reasons reason,
                       void* /*user*/, void* in, size_t len) {
    auto* impl = static_cast<WebSocketServer::Impl*>(
        lws_context_user(lws_get_context(wsi)));

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            std::lock_guard<std::mutex> lock(impl->m_clientMtx);
            impl->m_clients.push_back(wsi);
            ++impl->m_clientCount;
            break;
        }
        case LWS_CALLBACK_CLOSED: {
            std::lock_guard<std::mutex> lock(impl->m_clientMtx);
            impl->m_clients.erase(
                std::remove(impl->m_clients.begin(), impl->m_clients.end(), wsi),
                impl->m_clients.end());
            --impl->m_clientCount;
            break;
        }
        case LWS_CALLBACK_RECEIVE:
            if (impl->m_callback && *impl->m_callback) {
                std::string msg(static_cast<const char*>(in), len);
                (*impl->m_callback)(msg);
            }
            break;
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            std::lock_guard<std::mutex> lock(impl->m_outboxMtx);
            if (!impl->m_outbox.empty()) {
                const std::string& msg = impl->m_outbox.front();
                std::vector<uint8_t> buf(LWS_PRE + msg.size());
                std::memcpy(buf.data() + LWS_PRE, msg.data(), msg.size());
                lws_write(wsi,
                          buf.data() + LWS_PRE,
                          msg.size(),
                          LWS_WRITE_TEXT);
                impl->m_outbox.pop();
            }
            break;
        }
        case LWS_CALLBACK_CANCEL_SERVICE: {
            // Wake up all writable clients to drain the outbox.
            std::lock_guard<std::mutex> lock(impl->m_clientMtx);
            for (lws* client : impl->m_clients)
                lws_callback_on_writable(client);
            break;
        }
        default:
            break;
    }

    return 0;
}
