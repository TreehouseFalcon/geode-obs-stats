#include "ObsConnection.hpp"

#include "ObsTextSourceUpdates.hpp"

#include <Geode/loader/Log.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/ui/Notification.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

using namespace geode::prelude;

namespace {
    constexpr std::chrono::seconds RETRY_DELAY = std::chrono::seconds(5);

    std::atomic_bool g_retryEnabled = false;
    std::atomic_bool g_retryScheduled = false;
    std::atomic_bool g_startingConnection = false;
    std::atomic_bool g_wasConnected = false;
    std::atomic<std::uint64_t> g_connectionGeneration = 0;
    std::mutex g_passwordMutex;
    std::string g_currentPassword;

    std::string getObsPassword() {
        return Mod::get()
            ->getSavedSettingsData()["obs-ws-password"]
            .asString()
            .unwrapOr("");
    }

    std::optional<std::string> getPasswordFromSetting(std::shared_ptr<SettingV3> const& setting) {
        if (!setting) {
            return std::nullopt;
        }

        matjson::Value value;
        if (!setting->save(value)) {
            return std::nullopt;
        }

        auto password = value.asString();
        if (!password) {
            return std::nullopt;
        }

        return password.unwrap();
    }

    void setCurrentPassword(std::string password) {
        std::lock_guard lock(g_passwordMutex);
        g_currentPassword = std::move(password);
    }

    std::string getCurrentPassword() {
        std::lock_guard lock(g_passwordMutex);
        return g_currentPassword;
    }

    void tryConnectToObs() {
        obs::ConnectionOptions options = {
            .password = getCurrentPassword(),
        };

        log::info("Connecting to OBS websocket");
        g_startingConnection.store(true);
        obs::getClient().connect(std::move(options));
        g_startingConnection.store(false);
    }

    void stopConnectionRetries() {
        g_retryEnabled.store(false);
        g_retryScheduled.store(false);
        g_connectionGeneration.fetch_add(1);
    }

    void cancelScheduledConnectionRetry() {
        g_retryScheduled.store(false);
        g_connectionGeneration.fetch_add(1);
    }

    void scheduleConnectionRetry();

    void startConnectionRetries(std::string password) {
        setCurrentPassword(std::move(password));
        g_retryEnabled.store(true);
        g_retryScheduled.store(false);
        g_connectionGeneration.fetch_add(1);
        tryConnectToObs();
    }

    void scheduleConnectionRetry() {
        if (!g_retryEnabled.load() || g_startingConnection.load()) {
            return;
        }
        if (g_retryScheduled.exchange(true)) {
            return;
        }

        std::uint64_t generation = g_connectionGeneration.load();
        log::info("OBS websocket did not respond, retrying in {} seconds", RETRY_DELAY.count());

        std::thread([generation] {
            std::this_thread::sleep_for(RETRY_DELAY);

            queueInMainThread([generation] {
                if (generation != g_connectionGeneration.load()) {
                    return;
                }

                g_retryScheduled.store(false);
                if (!g_retryEnabled.load()) {
                    return;
                }

                if (obs::getClient().getState() != obs::ConnectionState::Disconnected) {
                    return;
                }

                tryConnectToObs();
            });
        }).detach();
    }

    void showObsNotification(std::string text, NotificationIcon icon, float time = NOTIFICATION_DEFAULT_TIME) {
        queueInMainThread([text = std::move(text), icon, time] {
            Notification::create(fmt::format("obs stats - {}", text), icon, time)->show();
        });
    }

    bool isTextInputKind(std::string const& inputKind) {
        return inputKind == "text_gdiplus_v3" || inputKind == "text_ft2_source_v2";
    }

    struct TextSourceFetchState {
        obs::TextSourcesCallback callback;
        std::set<std::string> sceneSourceNames;
        std::set<std::string> groupNames;
        std::set<std::string> visitedGroupNames;
        std::size_t pendingGroupRequests = 0;
    };

    void emitTextSources(obs::TextSourcesCallback callback, std::vector<std::string> sources) {
        queueInMainThread([callback = std::move(callback), sources = std::move(sources)]() mutable {
            if (callback) {
                callback(std::move(sources));
            }
        });
    }

    std::set<std::string> getGroupNames(matjson::Value const& responseData) {
        std::set<std::string> names;
        auto groups = responseData["groups"].asArray();
        if (!groups) {
            return names;
        }

        for (auto const& group : groups.unwrap()) {
            if (auto groupName = group.asString()) {
                names.insert(groupName.unwrap());
            }
        }

        return names;
    }

    std::vector<std::string> getTextInputNames(
        matjson::Value const& responseData,
        std::set<std::string> const& sceneSourceNames
    ) {
        std::vector<std::string> names;
        auto inputs = responseData["inputs"].asArray();
        if (!inputs) {
            return names;
        }

        for (auto const& input : inputs.unwrap()) {
            auto inputName = input["inputName"].asString();
            auto inputKind = input["inputKind"].asString();
            if (!inputName || !inputKind) {
                continue;
            }

            std::string name = inputName.unwrap();
            if (sceneSourceNames.contains(name) && isTextInputKind(inputKind.unwrap())) {
                names.push_back(std::move(name));
            }
        }

        std::ranges::sort(names);
        names.erase(std::ranges::unique(names).begin(), names.end());
        return names;
    }

    void fetchTextInputNames(std::shared_ptr<TextSourceFetchState> state) {
        std::shared_ptr<TextSourceFetchState> callbackState = state;
        std::optional<std::string> requestId = obs::getClient().sendRequest(
            "GetInputList",
            nullptr,
            [state = std::move(callbackState)](bool success, matjson::Value responseData) mutable {
                if (!success) {
                    emitTextSources(std::move(state->callback), {});
                    return;
                }

                emitTextSources(
                    std::move(state->callback),
                    getTextInputNames(responseData, state->sceneSourceNames)
                );
            }
        );
        if (!requestId) {
            emitTextSources(std::move(state->callback), {});
        }
    }

    void fetchNestedGroupSources(std::shared_ptr<TextSourceFetchState> state, std::string groupName);

    void queueNestedGroupFetches(
        std::shared_ptr<TextSourceFetchState> state,
        matjson::Value const& responseData
    ) {
        auto sceneItems = responseData["sceneItems"].asArray();
        if (!sceneItems) {
            return;
        }

        for (auto const& item : sceneItems.unwrap()) {
            auto sourceName = item["sourceName"].asString();
            if (!sourceName) {
                continue;
            }

            std::string name = sourceName.unwrap();
            state->sceneSourceNames.insert(name);
            if (
                state->groupNames.contains(name) &&
                !state->visitedGroupNames.contains(name)
            ) {
                fetchNestedGroupSources(state, std::move(name));
            }
        }
    }

    void completeNestedGroupFetch(std::shared_ptr<TextSourceFetchState> state) {
        if (state->pendingGroupRequests == 0) {
            fetchTextInputNames(std::move(state));
            return;
        }

        state->pendingGroupRequests--;
        if (state->pendingGroupRequests == 0) {
            fetchTextInputNames(std::move(state));
        }
    }

    void fetchNestedGroupSources(std::shared_ptr<TextSourceFetchState> state, std::string groupName) {
        if (!state->visitedGroupNames.insert(groupName).second) {
            return;
        }

        state->pendingGroupRequests++;
        std::shared_ptr<TextSourceFetchState> callbackState = state;
        std::optional<std::string> requestId = obs::getClient().sendRequest(
            "GetGroupSceneItemList",
            matjson::makeObject({
                { "sceneName", groupName },
            }),
            [state = std::move(callbackState)](bool success, matjson::Value responseData) mutable {
                if (success) {
                    queueNestedGroupFetches(state, responseData);
                }

                completeNestedGroupFetch(std::move(state));
            }
        );
        if (!requestId) {
            completeNestedGroupFetch(std::move(state));
        }
    }
}

namespace obs {
    WebSocketClient& getClient() {
        static WebSocketClient client;
        return client;
    }

    void fetchCurrentSceneTextSources(TextSourcesCallback callback) {
        if (!getClient().isIdentified()) {
            log::warn("OBS websocket: cannot fetch text sources before identify completed");
            emitTextSources(std::move(callback), {});
            return;
        }

        std::shared_ptr<TextSourceFetchState> state = std::make_shared<TextSourceFetchState>();
        state->callback = std::move(callback);

        std::shared_ptr<TextSourceFetchState> requestState = state;
        std::optional<std::string> requestId = getClient().sendRequest(
            "GetCurrentProgramScene",
            nullptr,
            [state = std::move(requestState)](bool success, matjson::Value responseData) mutable {
                if (!success) {
                    emitTextSources(std::move(state->callback), {});
                    return;
                }

                auto sceneName = responseData["currentProgramSceneName"].asString()
                    .orElse([&responseData]() {
                        return responseData["sceneName"].asString();
                    });
                if (!sceneName) {
                    log::warn("OBS websocket: GetCurrentProgramScene response did not include a scene name");
                    emitTextSources(std::move(state->callback), {});
                    return;
                }

                std::string sceneNameValue = sceneName.unwrap();
                std::shared_ptr<TextSourceFetchState> requestState = state;
                std::optional<std::string> requestId = getClient().sendRequest(
                    "GetGroupList",
                    nullptr,
                    [
                        state = std::move(requestState),
                        sceneName = std::move(sceneNameValue)
                    ](bool success, matjson::Value responseData) mutable {
                        if (success) {
                            state->groupNames = getGroupNames(responseData);
                        }

                        std::shared_ptr<TextSourceFetchState> requestState = state;
                        std::optional<std::string> requestId = getClient().sendRequest(
                            "GetSceneItemList",
                            matjson::makeObject({
                                { "sceneName", sceneName },
                            }),
                            [state = std::move(requestState)](bool success, matjson::Value responseData) mutable {
                                if (!success) {
                                    emitTextSources(std::move(state->callback), {});
                                    return;
                                }

                                queueNestedGroupFetches(state, responseData);
                                if (state->pendingGroupRequests == 0) {
                                    fetchTextInputNames(std::move(state));
                                }
                            }
                        );
                        if (!requestId) {
                            emitTextSources(std::move(state->callback), {});
                        }
                    }
                );
                if (!requestId) {
                    emitTextSources(std::move(state->callback), {});
                }
            }
        );
        if (!requestId) {
            emitTextSources(std::move(state->callback), {});
        }
    }
}

$on_mod(Loaded) {
    obs::getClient().setStateCallback([](obs::ConnectionState state) {
        log::info("OBS websocket state: {}", obs::stateToString(state));

        switch (state) {
            case obs::ConnectionState::Identified:
                g_wasConnected.store(true);
                cancelScheduledConnectionRetry();
                obs_stats::updateAllObsTextSources();
                showObsNotification("obs connected", NotificationIcon::Success);
                break;
            case obs::ConnectionState::Error:
                g_wasConnected.store(false);
                stopConnectionRetries();
                showObsNotification(
                    "obs connect failed",
                    NotificationIcon::Error,
                    NOTIFICATION_LONG_TIME
                );
                break;
            case obs::ConnectionState::Disconnected:
                if (!g_startingConnection.load() && g_wasConnected.exchange(false)) {
                    showObsNotification(
                        "obs disconnected",
                        NotificationIcon::Error,
                        NOTIFICATION_LONG_TIME
                    );
                }
                scheduleConnectionRetry();
                break;
            default:
                break;
        }
    });

    (void)listenForAllSettingChanges(
        [](std::string_view key, std::shared_ptr<SettingV3> setting) {
            if (key == "source-mappings") {
                obs_stats::updateAllObsTextSources();
                return;
            }

            if (key != "obs-ws-password") {
                return;
            }

            std::optional<std::string> password = getPasswordFromSetting(setting);
            if (!password) {
                log::warn("OBS websocket: could not read updated password setting");
                return;
            }

            log::info("OBS websocket password updated, reconnecting");
            startConnectionRetries(std::move(*password));
        },
        Mod::get()
    );
}

$on_game(Loaded) {
    startConnectionRetries(getObsPassword());
}
