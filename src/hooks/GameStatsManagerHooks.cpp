#include "../format/StatPlaceholders.hpp"
#include "../obs/ObsTextSourceUpdates.hpp"
#include "../stats/CompletedDifficultyCounts.hpp"

#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/modify/GameStatsManager.hpp>

#include <charconv>
#include <optional>
#include <set>
#include <string>
#include <string_view>

using namespace geode::prelude;

namespace {
    std::set<std::string> locallyIncrementedCompletedLevelKeys;

    bool isDemonDifficulty(GJDifficulty difficulty) {
        return difficulty == GJDifficulty::Demon
            || difficulty == GJDifficulty::DemonEasy
            || difficulty == GJDifficulty::DemonMedium
            || difficulty == GJDifficulty::DemonInsane
            || difficulty == GJDifficulty::DemonExtreme;
    }

    bool isDemonLevel(GJGameLevel* level) {
        return level != nullptr && (level->m_demon.value() != 0 || isDemonDifficulty(level->m_difficulty));
    }

    std::optional<std::string> completedLevelKey(GameStatsManager* manager, GJGameLevel* level) {
        if (manager == nullptr || level == nullptr) {
            return std::nullopt;
        }

        char const* key = isDemonLevel(level)
            ? manager->getDemonLevelKey(level)
            : manager->getStarLevelKey(level);
        if (key == nullptr || key[0] == '\0') {
            return std::nullopt;
        }

        return std::string(key);
    }

    bool isLevelCompleted(GameStatsManager* manager, GJGameLevel* level) {
        if (manager == nullptr || manager->m_completedLevels == nullptr) {
            return false;
        }

        std::optional<std::string> key = completedLevelKey(manager, level);
        return key && manager->m_completedLevels->objectForKey(key->c_str()) != nullptr;
    }

    void updateCompletedDifficultyObsTextSources(GJDifficulty difficulty) {
        std::optional<obs_stats::StatPlaceholder> placeholder = obs_stats::findStatPlaceholder(
            obs_stats::SelectStrategy::CompletedDifficulty,
            [difficulty](obs_stats::StatPlaceholder const& candidate) {
                obs_stats::CompletedDifficultySelection const* selection =
                    std::get_if<obs_stats::CompletedDifficultySelection>(&candidate.selection);
                return selection != nullptr && selection->difficulty == difficulty;
            }
        );

        if (!placeholder) {
            log::warn("no registered placeholder found for completed difficulty '{}'", static_cast<int>(difficulty));
            return;
        }

        obs_stats::updateApplicableObsTextSources(*placeholder);
    }

    void incrementCompletedDifficultyCacheIfNewlyCompleted(
        GameStatsManager* manager,
        GJGameLevel* level,
        bool wasCompleted
    ) {
        if (wasCompleted || !isLevelCompleted(manager, level)) {
            return;
        }

        std::optional<std::string> key = completedLevelKey(manager, level);
        if (!key || !locallyIncrementedCompletedLevelKeys.insert(*key).second) {
            return;
        }

        std::optional<GJDifficulty> difficulty = obs_stats::CompletedDifficultyCounts::incrementCountForLevel(level);
        if (difficulty) {
            updateCompletedDifficultyObsTextSources(*difficulty);
        }
    }
}

class $modify(GameStatsManagerHooks, GameStatsManager) {
    std::optional<StatKey> statKeyFromGameStatManagerKey(std::string_view key) {
        int rawKey = 0;
        std::from_chars_result result = std::from_chars(key.data(), key.data() + key.size(), rawKey);
        if (result.ec != std::errc() || result.ptr != key.data() + key.size()) {
            return std::nullopt;
        }

        return static_cast<StatKey>(rawKey);
    }

    void statUpdated(std::string_view key) {
        std::optional<StatKey> statKey = statKeyFromGameStatManagerKey(key);
        if (!statKey) {
            return;
        }

        std::optional<obs_stats::StatPlaceholder> placeholder = obs_stats::findStatPlaceholder(
            obs_stats::SelectStrategy::GameStatManagerKey,
            [statKey](obs_stats::StatPlaceholder const& candidate) {
                obs_stats::GameStatManagerKeySelection const* selection =
                    std::get_if<obs_stats::GameStatManagerKeySelection>(&candidate.selection);
                return selection != nullptr && selection->statKey == statKey;
            }
        );

        if (!placeholder) {
            log::warn("no registered placeholder found for GameStatsManager stat key '{}'", static_cast<int>(*statKey));
            return;
        }

        obs_stats::updateApplicableObsTextSources(*placeholder);
    }

    void incrementStat(char const* key, int amount) {
        GameStatsManager::incrementStat(key, amount);
        statUpdated(key != nullptr ? std::string_view(key) : std::string_view());
    }

    void setStat(char const* key, int value) {
        GameStatsManager::setStat(key, value);
        statUpdated(key != nullptr ? std::string_view(key) : std::string_view());
    }

    void completedStarLevel(GJGameLevel* level) {
        bool wasCompleted = isLevelCompleted(this, level);
        GameStatsManager::completedStarLevel(level);
        incrementCompletedDifficultyCacheIfNewlyCompleted(
            this,
            level,
            wasCompleted
        );
    }

    void completedDemonLevel(GJGameLevel* level) {
        bool wasCompleted = isLevelCompleted(this, level);
        GameStatsManager::completedDemonLevel(level);
        incrementCompletedDifficultyCacheIfNewlyCompleted(
            this,
            level,
            wasCompleted
        );
    }

    GJRewardItem* completedDailyLevel(GJGameLevel* level) {
        bool wasCompleted = isLevelCompleted(this, level);
        GJRewardItem* reward = GameStatsManager::completedDailyLevel(level);
        incrementCompletedDifficultyCacheIfNewlyCompleted(
            this,
            level,
            wasCompleted
        );
        return reward;
    }

    void markLevelAsCompletedAndClaimed(GJGameLevel* level) {
        bool wasCompleted = isLevelCompleted(this, level);
        GameStatsManager::markLevelAsCompletedAndClaimed(level);
        incrementCompletedDifficultyCacheIfNewlyCompleted(
            this,
            level,
            wasCompleted
        );
    }
};
