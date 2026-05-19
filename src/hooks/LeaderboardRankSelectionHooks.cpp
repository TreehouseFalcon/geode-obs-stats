#include "../format/StatPlaceholders.hpp"
#include "../obs/ObsTextSourceUpdates.hpp"

#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/LeaderboardManagerDelegate.hpp>
#include <Geode/binding/LeaderboardsLayer.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/LeaderboardsLayer.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr int MIN_REFRESH_INTERVAL_SECONDS = 5 * 60;
    constexpr int DEFAULT_REFRESH_INTERVAL_SECONDS = 10 * 60;

    std::atomic_bool g_refreshThreadStarted = false;
    std::atomic<int> g_refreshIntervalSeconds = DEFAULT_REFRESH_INTERVAL_SECONDS;
    std::mutex g_rankMutex;
    std::array<std::optional<int>, 4> g_leaderboardRanks = {};
    std::vector<LeaderboardStat> g_backgroundRefreshQueue;
    std::optional<LeaderboardStat> g_activeBackgroundRefresh;

    std::optional<std::size_t> leaderboardStatIndex(LeaderboardStat stat) {
        int index = static_cast<int>(stat);
        if (index < 0 || index >= static_cast<int>(g_leaderboardRanks.size())) {
            return std::nullopt;
        }

        return static_cast<std::size_t>(index);
    }

    int normalizeRefreshIntervalSeconds(int64_t settingValue) {
        return std::max(MIN_REFRESH_INTERVAL_SECONDS, static_cast<int>(settingValue));
    }

    void setRefreshIntervalSeconds(int64_t settingValue) {
        g_refreshIntervalSeconds.store(normalizeRefreshIntervalSeconds(settingValue));
    }

    void loadRefreshIntervalSetting() {
        if (Mod::get() != nullptr) {
            setRefreshIntervalSeconds(Mod::get()->getSettingValue<int64_t>("leaderboard-rank-refresh-interval") * 60);
        }
    }

    std::set<LeaderboardStat> leaderboardStatsWithPlaceholders() {
        std::set<LeaderboardStat> stats;

        for (obs_stats::StatPlaceholder const& placeholder : obs_stats::statPlaceholders()) {
            if (obs_stats::selectStrategy(placeholder) != obs_stats::SelectStrategy::LeaderboardRank) {
                continue;
            }

            obs_stats::LeaderboardRankSelection const* selection =
                std::get_if<obs_stats::LeaderboardRankSelection>(&placeholder.selection);
            if (selection != nullptr) {
                stats.insert(selection->leaderboardStat);
            }
        }

        return stats;
    }

    class BackgroundLeaderboardRankDelegate : public LeaderboardManagerDelegate {
    public:
        void loadLeaderboardFinished(CCArray* scores, char const*) override;
        void loadLeaderboardFailed(char const*) override;
    };

    BackgroundLeaderboardRankDelegate g_backgroundDelegate;

    // Background refreshes borrow GD's single leaderboard delegate slot, so they are queued and skipped entirely while the real leaderboard UI owns it.
    void finishBackgroundRefreshQueue() {
        g_backgroundRefreshQueue.clear();
        g_activeBackgroundRefresh = std::nullopt;

        GameLevelManager* levelManager = GameLevelManager::sharedState();
        if (levelManager != nullptr && levelManager->m_leaderboardManagerDelegate == &g_backgroundDelegate) {
            levelManager->m_leaderboardManagerDelegate = nullptr;
        }
    }

    void requestNextBackgroundRefresh() {
        GameLevelManager* levelManager = GameLevelManager::sharedState();
        if (levelManager == nullptr || levelManager->m_leaderboardManagerDelegate != &g_backgroundDelegate) {
            finishBackgroundRefreshQueue();
            return;
        }

        if (g_backgroundRefreshQueue.empty()) {
            finishBackgroundRefreshQueue();
            return;
        }

        LeaderboardStat stat = g_backgroundRefreshQueue.front();
        g_backgroundRefreshQueue.erase(g_backgroundRefreshQueue.begin());
        g_activeBackgroundRefresh = stat;

        log::info(
            "requesting global leaderboard rank for stat {}",
            static_cast<int>(stat)
        );

        levelManager->getLeaderboardScores(LeaderboardType::Global, stat);
    }

    void requestCurrentGlobalRanks() {
        GameLevelManager* levelManager = GameLevelManager::sharedState();
        if (levelManager == nullptr || levelManager->m_leaderboardManagerDelegate != nullptr) {
            return;
        }

        std::set<LeaderboardStat> stats = leaderboardStatsWithPlaceholders();
        if (stats.empty()) {
            return;
        }

        levelManager->m_leaderboardManagerDelegate = &g_backgroundDelegate;
        g_backgroundRefreshQueue.assign(stats.begin(), stats.end());
        requestNextBackgroundRefresh();
    }

    void startLeaderboardRankRefreshes() {
        if (g_refreshThreadStarted.exchange(true)) {
            return;
        }

        std::thread([] {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(g_refreshIntervalSeconds.load()));
                queueInMainThread([] {
                    requestCurrentGlobalRanks();
                });
            }
        }).detach();
    }

    // Placeholder updates only need the local user's rank. GD has already parsed leaderboard responses into GJUserScore objects by the time these helpers run.
    std::vector<obs_stats::StatPlaceholder> leaderboardRankPlaceholders(LeaderboardStat stat) {
        std::vector<obs_stats::StatPlaceholder> placeholders;

        for (obs_stats::StatPlaceholder const& placeholder : obs_stats::statPlaceholders()) {
            if (obs_stats::selectStrategy(placeholder) != obs_stats::SelectStrategy::LeaderboardRank) {
                continue;
            }

            obs_stats::LeaderboardRankSelection const* selection =
                std::get_if<obs_stats::LeaderboardRankSelection>(&placeholder.selection);
            if (selection != nullptr && selection->leaderboardStat == stat) {
                placeholders.push_back(placeholder);
            }
        }

        return placeholders;
    }

    void setLeaderboardRank(LeaderboardStat stat, int rank) {
        std::optional<std::size_t> index = leaderboardStatIndex(stat);
        if (!index) {
            return;
        }

        std::lock_guard lock(g_rankMutex);
        g_leaderboardRanks[*index] = rank;
    }

    std::optional<int> currentUserRankFromScores(CCArray* scores) {
        if (scores == nullptr) {
            return std::nullopt;
        }

        for (unsigned int i = 0; i < scores->count(); i++) {
            GJUserScore* score = typeinfo_cast<GJUserScore*>(scores->objectAtIndex(i));
            if (score != nullptr && score->isCurrentUser()) {
                return score->m_playerRank;
            }
        }

        return std::nullopt;
    }

    void updateLeaderboardRankFromScores(LeaderboardStat stat, CCArray* scores) {
        std::optional<int> rank = currentUserRankFromScores(scores);
        if (!rank || *rank <= 0) {
            log::warn(
                "global leaderboard scores for stat {} did not include the local user's rank",
                static_cast<int>(stat)
            );
            return;
        }

        setLeaderboardRank(stat, *rank);
        obs_stats::updateApplicableObsTextSources(leaderboardRankPlaceholders(stat));
    }

    void BackgroundLeaderboardRankDelegate::loadLeaderboardFinished(CCArray* scores, char const*) {
        if (g_activeBackgroundRefresh) {
            updateLeaderboardRankFromScores(*g_activeBackgroundRefresh, scores);
        }

        g_activeBackgroundRefresh = std::nullopt;
        requestNextBackgroundRefresh();
    }

    void BackgroundLeaderboardRankDelegate::loadLeaderboardFailed(char const*) {
        g_activeBackgroundRefresh = std::nullopt;
        requestNextBackgroundRefresh();
    }
}

namespace obs_stats {
    std::optional<int> leaderboardRankForStat(LeaderboardStat stat) {
        std::optional<std::size_t> index = leaderboardStatIndex(stat);
        if (!index) {
            return std::nullopt;
        }

        std::lock_guard lock(g_rankMutex);
        return g_leaderboardRanks[*index];
    }
}

$on_mod(Loaded) {
    loadRefreshIntervalSetting();
    (void)listenForSettingChanges<int64_t>(
        "leaderboard-rank-refresh-interval",
        [](int64_t intervalSeconds) {
            setRefreshIntervalSeconds(intervalSeconds * 60);
        },
        Mod::get()
    );
}

$on_game(Loaded) {
    loadRefreshIntervalSetting();
    requestCurrentGlobalRanks();
    startLeaderboardRankRefreshes();
}

// Foreground leaderboard views are observed after GD finishes loading them; this avoids touching the raw onGetLeaderboardScoresCompleted path which causes crashes.
class $modify(LeaderboardRankLeaderboardsLayerHook, LeaderboardsLayer) {
    void loadLeaderboardFinished(CCArray* scores, char const* key) {
        LeaderboardsLayer::loadLeaderboardFinished(scores, key);

        if (this->m_type == LeaderboardType::Global) {
            updateLeaderboardRankFromScores(this->m_stat, scores);
        }
    }
};
