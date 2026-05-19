#include "ObsWebSocketClient.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/utils/base64.hpp>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr std::array<std::uint32_t, 64> SHA256_CONSTANTS = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    };

    std::uint32_t rotateRight(std::uint32_t value, std::uint32_t bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    std::array<std::uint8_t, 32> sha256(std::string_view input) {
        std::array<std::uint32_t, 8> hash = {
            0x6a09e667,
            0xbb67ae85,
            0x3c6ef372,
            0xa54ff53a,
            0x510e527f,
            0x9b05688c,
            0x1f83d9ab,
            0x5be0cd19,
        };

        std::vector<std::uint8_t> message(input.begin(), input.end());
        std::uint64_t const bitLength = static_cast<std::uint64_t>(message.size()) * 8;
        message.push_back(0x80);

        while ((message.size() + 8) % 64 != 0) {
            message.push_back(0);
        }

        for (int shift = 56; shift >= 0; shift -= 8) {
            message.push_back(static_cast<std::uint8_t>((bitLength >> shift) & 0xff));
        }

        for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
            std::array<std::uint32_t, 64> words = {};

            for (std::size_t i = 0; i < 16; i++) {
                size_t offset = chunk + i * 4;
                words[i] =
                    (static_cast<std::uint32_t>(message[offset]) << 24) |
                    (static_cast<std::uint32_t>(message[offset + 1]) << 16) |
                    (static_cast<std::uint32_t>(message[offset + 2]) << 8) |
                    static_cast<std::uint32_t>(message[offset + 3]);
            }

            for (std::size_t i = 16; i < 64; i++) {
                std::uint32_t s0 =
                    rotateRight(words[i - 15], 7) ^ rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
                std::uint32_t s1 =
                    rotateRight(words[i - 2], 17) ^ rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
                words[i] = words[i - 16] + s0 + words[i - 7] + s1;
            }

            std::uint32_t a = hash[0];
            std::uint32_t b = hash[1];
            std::uint32_t c = hash[2];
            std::uint32_t d = hash[3];
            std::uint32_t e = hash[4];
            std::uint32_t f = hash[5];
            std::uint32_t g = hash[6];
            std::uint32_t h = hash[7];

            for (std::size_t i = 0; i < 64; i++) {
                std::uint32_t s1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
                std::uint32_t choice = (e & f) ^ (~e & g);
                std::uint32_t temp1 = h + s1 + choice + SHA256_CONSTANTS[i] + words[i];
                std::uint32_t s0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
                std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                std::uint32_t temp2 = s0 + majority;

                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            hash[0] += a;
            hash[1] += b;
            hash[2] += c;
            hash[3] += d;
            hash[4] += e;
            hash[5] += f;
            hash[6] += g;
            hash[7] += h;
        }

        std::array<std::uint8_t, 32> digest = {};
        for (std::size_t i = 0; i < hash.size(); i++) {
            digest[i * 4] = static_cast<std::uint8_t>((hash[i] >> 24) & 0xff);
            digest[i * 4 + 1] = static_cast<std::uint8_t>((hash[i] >> 16) & 0xff);
            digest[i * 4 + 2] = static_cast<std::uint8_t>((hash[i] >> 8) & 0xff);
            digest[i * 4 + 3] = static_cast<std::uint8_t>(hash[i] & 0xff);
        }

        return digest;
    }

    std::string base64Sha256(std::string_view value) {
        std::array<std::uint8_t, 32> digest = sha256(value);
        return geode::utils::base64::encode(
            std::span<std::uint8_t const>(digest.data(), digest.size()),
            geode::utils::base64::Base64Variant::Normal
        );
    }

    std::string makeObsUrl(obs::ConnectionOptions const& options) {
        return fmt::format("ws://{}:{}", options.host, options.port);
    }

    std::string createAuthenticationString(
        std::string const& password,
        std::string const& salt,
        std::string const& challenge
    ) {
        std::string secretInput = password + salt;
        std::string secret = base64Sha256(secretInput);
        std::string authInput = secret + challenge;
        return base64Sha256(authInput);
    }
}

namespace obs {
    class WebSocketClient::Impl {
    public:
        ix::WebSocket webSocket;
        ConnectionOptions options;
        StateCallback stateCallback;
        std::atomic<ConnectionState> state = ConnectionState::Disconnected;
        std::atomic<std::uint64_t> requestCounter = 1;
        std::mutex callbackMutex;
        std::mutex requestCallbackMutex;
        std::map<std::string, RequestCallback> requestCallbacks;
        std::atomic_bool shuttingDown = false;
        bool networkInitialized = false;

        ~Impl() {
            shutdown();
        }

        void connect(ConnectionOptions nextOptions) {
            if (shuttingDown.load()) {
                return;
            }

            disconnect();

            options = std::move(nextOptions);
            if (!ix::initNetSystem()) {
                log::error("OBS websocket: failed to initialize network system");
                setState(ConnectionState::Error);
                return;
            }

            networkInitialized = true;
            setState(ConnectionState::Connecting);

            webSocket.setUrl(makeObsUrl(options));
            webSocket.setHandshakeTimeout(options.handshakeTimeoutSecs);
            webSocket.addSubProtocol("obswebsocket.json");
            webSocket.disableAutomaticReconnection();
            webSocket.disablePerMessageDeflate();
            webSocket.setOnMessageCallback([this](ix::WebSocketMessagePtr const& message) {
                handleMessage(message);
            });

            webSocket.start();
        }

        void disconnect(bool notify = true) {
            webSocket.stop();
            webSocket.setOnMessageCallback(nullptr);

            {
                std::lock_guard lock(requestCallbackMutex);
                requestCallbacks.clear();
            }

            if (networkInitialized) {
                (void)ix::uninitNetSystem();
                networkInitialized = false;
            }

            if (notify && !shuttingDown.load()) {
                setState(ConnectionState::Disconnected);
            }
            else {
                state.store(ConnectionState::Disconnected);
            }
        }

        void shutdown() {
            if (shuttingDown.exchange(true)) {
                return;
            }

            {
                std::lock_guard lock(callbackMutex);
                stateCallback = nullptr;
            }

            disconnect(false);
        }

        std::optional<std::string> sendRequest(
            std::string const& requestType,
            matjson::Value requestData,
            RequestCallback callback = nullptr
        ) {
            if (shuttingDown.load()) {
                return std::nullopt;
            }

            if (state.load() != ConnectionState::Identified) {
                log::warn("OBS websocket: tried to send '{}' before identify completed", requestType);
                return std::nullopt;
            }

            std::string requestId = fmt::format("geode-obs-stats-{}", requestCounter.fetch_add(1));
            if (callback) {
                std::lock_guard lock(requestCallbackMutex);
                requestCallbacks.emplace(requestId, std::move(callback));
            }

            auto data = matjson::makeObject({
                { "requestType", requestType },
                { "requestId", requestId },
            });

            if (!requestData.isNull()) {
                data.set("requestData", std::move(requestData));
            }

            auto payload = matjson::makeObject({
                { "op", 6 },
                { "d", std::move(data) },
            }).dump(matjson::NO_INDENTATION);

            ix::WebSocketSendInfo result = webSocket.sendUtf8Text(payload);
            if (!result.success) {
                if (callback) {
                    std::lock_guard lock(requestCallbackMutex);
                    requestCallbacks.erase(requestId);
                }
                log::error("OBS websocket: failed to send request '{}'", requestType);
                return std::nullopt;
            }

            return requestId;
        }

        void setState(ConnectionState nextState) {
            state.store(nextState);
            if (shuttingDown.load()) {
                return;
            }

            StateCallback callback;
            {
                std::lock_guard lock(callbackMutex);
                callback = stateCallback;
            }

            if (callback) {
                callback(nextState);
            }
        }

        void handleMessage(ix::WebSocketMessagePtr const& message) {
            if (shuttingDown.load()) {
                return;
            }

            switch (message->type) {
                case ix::WebSocketMessageType::Open:
                    log::info("OBS websocket: connected, waiting for Hello");
                    break;

                case ix::WebSocketMessageType::Message:
                    handlePayload(message->str);
                    break;

                case ix::WebSocketMessageType::Close:
                    log::info(
                        "OBS websocket: closed with code {} ({})",
                        message->closeInfo.code,
                        message->closeInfo.reason
                    );
                    if (message->closeInfo.code >= 4000) {
                        setState(ConnectionState::Error);
                    }
                    else {
                        setState(ConnectionState::Disconnected);
                    }
                    break;

                case ix::WebSocketMessageType::Error:
                    log::error("OBS websocket: {}", message->errorInfo.reason);
                    if (
                        state.load() == ConnectionState::Connecting ||
                        state.load() == ConnectionState::Identifying
                    ) {
                        setState(ConnectionState::Disconnected);
                    }
                    else {
                        setState(ConnectionState::Error);
                    }
                    break;

                default:
                    break;
            }
        }

        void handlePayload(std::string const& payload) {
            auto parsed = matjson::parse(payload);
            if (parsed.isErr()) {
                log::warn("OBS websocket: received invalid JSON: {}", parsed.unwrapErr());
                return;
            }

            auto message = std::move(parsed).unwrap();
            auto opResult = message.get<int>("op");
            if (opResult.isErr()) {
                log::warn("OBS websocket: received message without op code");
                return;
            }

            int op = opResult.unwrap();
            if (op == 0) {
                handleHello(message["d"]);
            }
            else if (op == 2) {
                setState(ConnectionState::Identified);
                log::info("OBS websocket: identified");
            }
            else if (op == 7) {
                handleRequestResponse(message["d"]);
            }
        }

        void handleHello(matjson::Value const& data) {
            int rpcVersion = 1;
            auto rpcVersionResult = data["rpcVersion"].as<int>();
            if (rpcVersionResult.isOk()) {
                rpcVersion = rpcVersionResult.unwrap();
            }

            auto identifyData = matjson::makeObject({
                { "rpcVersion", rpcVersion },
                { "eventSubscriptions", options.eventSubscriptions },
            });

            matjson::Value const& authData = data["authentication"];
            if (authData.isObject()) {
                auto salt = authData["salt"].asString();
                auto challenge = authData["challenge"].asString();

                if (salt.isErr() || challenge.isErr()) {
                    log::error("OBS websocket: authentication challenge was malformed");
                    setState(ConnectionState::Error);
                    return;
                }

                identifyData.set(
                    "authentication",
                    createAuthenticationString(options.password, salt.unwrap(), challenge.unwrap())
                );
            }

            auto identify = matjson::makeObject({
                { "op", 1 },
                { "d", std::move(identifyData) },
            }).dump(matjson::NO_INDENTATION);

            setState(ConnectionState::Identifying);
            ix::WebSocketSendInfo result = webSocket.sendUtf8Text(identify);
            if (!result.success) {
                log::error("OBS websocket: failed to send Identify");
                setState(ConnectionState::Error);
            }
        }

        void handleRequestResponse(matjson::Value const& data) {
            std::string requestType = data["requestType"].asString().unwrapOr("unknown");
            std::string requestId = data["requestId"].asString().unwrapOr("");
            matjson::Value status = data["requestStatus"];
            bool succeeded = status["result"].asBool().unwrapOr(false);

            RequestCallback callback;
            if (!requestId.empty()) {
                std::lock_guard lock(requestCallbackMutex);
                if (auto it = requestCallbacks.find(requestId); it != requestCallbacks.end()) {
                    callback = std::move(it->second);
                    requestCallbacks.erase(it);
                }
            }

            if (!succeeded) {
                int code = status["code"].as<int>().unwrapOr(0);
                std::string comment = status["comment"].asString().unwrapOr("");
                log::warn(
                    "OBS websocket: request '{}' failed with code {} ({})",
                    requestType,
                    code,
                    comment
                );
            }

            if (callback) {
                callback(succeeded, data["responseData"]);
            }
        }
    };

    WebSocketClient::WebSocketClient() : m_impl(new Impl()) {}

    WebSocketClient::~WebSocketClient() {
        delete m_impl;
    }

    void WebSocketClient::setStateCallback(StateCallback callback) {
        std::lock_guard lock(m_impl->callbackMutex);
        m_impl->stateCallback = std::move(callback);
    }

    void WebSocketClient::connect(ConnectionOptions options) {
        m_impl->connect(std::move(options));
    }

    void WebSocketClient::disconnect() {
        m_impl->disconnect();
    }

    void WebSocketClient::shutdown() {
        m_impl->shutdown();
    }

    ConnectionState WebSocketClient::getState() const {
        return m_impl->state.load();
    }

    bool WebSocketClient::isIdentified() const {
        return this->getState() == ConnectionState::Identified;
    }

    std::optional<std::string> WebSocketClient::sendRequest(
        std::string const& requestType,
        matjson::Value requestData
    ) {
        return m_impl->sendRequest(requestType, std::move(requestData));
    }

    std::optional<std::string> WebSocketClient::sendRequest(
        std::string const& requestType,
        matjson::Value requestData,
        RequestCallback callback
    ) {
        return m_impl->sendRequest(requestType, std::move(requestData), std::move(callback));
    }

    char const* stateToString(ConnectionState state) {
        switch (state) {
            case ConnectionState::Disconnected: return "Disconnected";
            case ConnectionState::Connecting: return "Connecting";
            case ConnectionState::Identifying: return "Identifying";
            case ConnectionState::Identified: return "Identified";
            case ConnectionState::Error: return "Error";
        }

        return "Unknown";
    }
}
