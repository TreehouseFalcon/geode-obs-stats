#include "StatPlaceholders.hpp"

#include "../stats/CompletedDifficultyCounts.hpp"
#include "../utils/NumberFormat.hpp"

#include <Geode/Enums.hpp>
#include <Geode/binding/GameStatsManager.hpp>
#include <Geode/loader/Log.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <type_traits>

using namespace geode::prelude;

namespace obs_stats {
    namespace {
        constexpr std::array<StatPlaceholder, 22> kStatPlaceholders = {{
            { .name = "STARS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Stars, }, },
            { .name = "MOONS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Moons, }, },
            { .name = "INSANES", .selection = GameStatManagerKeySelection { .statKey = StatKey::Insanes, }, },
            { .name = "DEMONS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Demons, }, },
            { .name = "SECRET_COINS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Coins, }, },
            { .name = "USER_COINS", .selection = GameStatManagerKeySelection { .statKey = StatKey::UserCoins, }, },
            { .name = "ORBS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Orbs, }, },
            { .name = "TOTAL_ORBS", .selection = GameStatManagerKeySelection { .statKey = StatKey::OrbsCollected, }, },
            { .name = "DIAMONDS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Diamonds, }, },
            { .name = "KEYS", .selection = GameStatManagerKeySelection { .statKey = StatKey::Keys, }, },
            { .name = "LIST_REWARDS", .selection = GameStatManagerKeySelection { .statKey = StatKey::ListsRewards, }, },

            { .name = "COMPLETED_AUTOS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Auto, }, },
            { .name = "COMPLETED_EASYS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Easy, }, },
            { .name = "COMPLETED_NORMALS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Normal, }, },
            { .name = "COMPLETED_HARDS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Hard, }, },
            { .name = "COMPLETED_HARDERS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Harder, }, },
            { .name = "COMPLETED_INSANES", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Insane, }, },
            { .name = "COMPLETED_EASY_DEMONS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::DemonEasy, }, },
            { .name = "COMPLETED_MEDIUM_DEMONS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::DemonMedium, }, },
            { .name = "COMPLETED_HARD_DEMONS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::Demon, }, },
            { .name = "COMPLETED_INSANE_DEMONS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::DemonInsane, }, },
            { .name = "COMPLETED_EXTREME_DEMONS", .selection = CompletedDifficultySelection { .difficulty = GJDifficulty::DemonExtreme, }, },
        }};

        std::string selectGameStatManagerKeyStat(PlaceholderSelection const& selection) {
            GameStatManagerKeySelection const* gameStatSelection = std::get_if<GameStatManagerKeySelection>(&selection);
            if (gameStatSelection == nullptr) {
                return "...";
            }

            GameStatsManager* manager = GameStatsManager::get();
            if (manager == nullptr) {
                return "...";
            }

            return formatCommaNumber(manager->getStatFromKey(gameStatSelection->statKey));
        }

        std::string selectCompletedDifficulty(PlaceholderSelection const& selection) {
            CompletedDifficultySelection const* completedDifficultySelection =
                std::get_if<CompletedDifficultySelection>(&selection);
            if (completedDifficultySelection == nullptr) {
                return "...";
            }

            std::optional<int> count =
                CompletedDifficultyCounts::countForDifficulty(completedDifficultySelection->difficulty);
            if (!count) {
                log::warn(
                    "completed difficulty count unavailable for difficulty {}",
                    static_cast<int>(completedDifficultySelection->difficulty)
                );
                return "...";
            }

            return formatCommaNumber(*count);
        }

        std::string normalizePlaceholder(std::string_view placeholder) {
            if (placeholder.size() >= 2 && placeholder.front() == '{' && placeholder.back() == '}') {
                placeholder.remove_prefix(1);
                placeholder.remove_suffix(1);
            }

            std::string normalized;
            normalized.reserve(placeholder.size());

            for (char ch : placeholder) {
                unsigned char unsignedCh = static_cast<unsigned char>(ch);
                if (std::isalnum(unsignedCh)) {
                    normalized.push_back(static_cast<char>(std::toupper(unsignedCh)));
                }
            }

            return normalized;
        }

        SelectStrategy selectStrategyForSelection(PlaceholderSelection const& selection) {
            return std::visit([](auto const& value) {
                using Selection = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Selection, GameStatManagerKeySelection>) {
                    return SelectStrategy::GameStatManagerKey;
                }
                else if constexpr (std::is_same_v<Selection, CompletedDifficultySelection>) {
                    return SelectStrategy::CompletedDifficulty;
                }
            }, selection);
        }
    }

    SelectStrategy selectStrategy(StatPlaceholder const& placeholder) {
        return selectStrategyForSelection(placeholder.selection);
    }

    std::optional<std::string> selectStatPlaceholderValue(StatPlaceholder const& placeholder) {
        switch (selectStrategy(placeholder)) {
            case SelectStrategy::GameStatManagerKey:
                return selectGameStatManagerKeyStat(placeholder.selection);
            case SelectStrategy::CompletedDifficulty:
                return selectCompletedDifficulty(placeholder.selection);
        }

        return std::nullopt;
    }

    std::span<StatPlaceholder const> statPlaceholders() {
        return kStatPlaceholders;
    }

    std::optional<StatPlaceholder> findStatPlaceholder(std::string_view placeholder) {
        std::string normalizedPlaceholder = normalizePlaceholder(placeholder);
        auto item = std::ranges::find_if(kStatPlaceholders, [&](StatPlaceholder const& candidate) {
            return normalizePlaceholder(candidate.name) == normalizedPlaceholder;
        });

        if (item == kStatPlaceholders.end()) {
            return std::nullopt;
        }

        return *item;
    }

    std::optional<StatPlaceholder> findStatPlaceholder(
        SelectStrategy strategy,
        StatPlaceholderPredicate const& predicate
    ) {
        auto item = std::ranges::find_if(kStatPlaceholders, [&](StatPlaceholder const& candidate) {
            return selectStrategy(candidate) == strategy && predicate && predicate(candidate);
        });

        if (item == kStatPlaceholders.end()) {
            return std::nullopt;
        }

        return *item;
    }
}
