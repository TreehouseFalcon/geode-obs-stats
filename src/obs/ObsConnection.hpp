#pragma once

#include "ObsWebSocketClient.hpp"

#include <Geode/utils/function.hpp>

#include <string>
#include <vector>

namespace obs {
    using TextSourcesCallback = geode::Function<void(std::vector<std::string>)>;

    WebSocketClient& getClient();
    void shutdownClient();
    void fetchCurrentSceneTextSources(TextSourcesCallback callback);
}
