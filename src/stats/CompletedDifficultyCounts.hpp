#pragma once

#include <Geode/Enums.hpp>

#include <optional>
#include <string_view>

class GJGameLevel;

namespace obs_stats::CompletedDifficultyCounts {
    std::optional<int> countForDifficulty(GJDifficulty difficulty);
    std::optional<GJDifficulty> incrementCountForLevel(GJGameLevel* level, int amount = 1);
    bool updateCountsFromProfileResponse(std::string_view response);
}
