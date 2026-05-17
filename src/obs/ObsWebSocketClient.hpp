#pragma once

#include <Geode/utils/function.hpp>
#include <matjson.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace obs {
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Identifying,
        Identified,
        Error,
    };

    struct ConnectionOptions {
        std::string host = "127.0.0.1";
        std::uint16_t port = 4455;
        std::string password;
        int handshakeTimeoutSecs = 5;
        std::uint32_t eventSubscriptions = 0;
    };

    class WebSocketClient {
    public:
        using StateCallback = std::function<void(ConnectionState)>;
        using RequestCallback = geode::Function<void(bool, matjson::Value)>;

        WebSocketClient();
        ~WebSocketClient();

        void setStateCallback(StateCallback callback);
        void connect(ConnectionOptions options = {});

        [[nodiscard]] ConnectionState getState() const;
        [[nodiscard]] bool isIdentified() const;

        std::optional<std::string> sendRequest(
            std::string const& requestType,
            matjson::Value requestData = nullptr
        );
        std::optional<std::string> sendRequest(
            std::string const& requestType,
            matjson::Value requestData,
            RequestCallback callback
        );

    private:
        class Impl;
        Impl* m_impl = nullptr;
    };

    char const* stateToString(ConnectionState state);
}
