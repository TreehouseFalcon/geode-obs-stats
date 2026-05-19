#pragma once

#include <Geode/Enums.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace obs_stats {
    struct StatPlaceholder;

    using StatPlaceholderPredicate = std::function<bool(StatPlaceholder const&)>;

    enum class SelectStrategy {
        GameStatManagerKey,
        CompletedDifficulty,
        LeaderboardRank,
    };

    struct GameStatManagerKeySelection {
        StatKey statKey;
    };

    struct CompletedDifficultySelection {
        GJDifficulty difficulty;
    };

    struct LeaderboardRankSelection {
        LeaderboardStat leaderboardStat;
    };

    using PlaceholderSelection = std::variant<
        GameStatManagerKeySelection,
        CompletedDifficultySelection,
        LeaderboardRankSelection
    >;

    struct StatPlaceholder {
        std::string_view name;
        PlaceholderSelection selection;
    };

    SelectStrategy selectStrategy(StatPlaceholder const& placeholder);
    std::optional<std::string> selectStatPlaceholderValue(StatPlaceholder const& placeholder);
    std::optional<int> leaderboardRankForStat(LeaderboardStat stat);
    std::span<StatPlaceholder const> statPlaceholders();
    std::optional<StatPlaceholder> findStatPlaceholder(std::string_view placeholder);
    std::optional<StatPlaceholder> findStatPlaceholder(
        SelectStrategy strategy,
        StatPlaceholderPredicate const& predicate
    );
}
