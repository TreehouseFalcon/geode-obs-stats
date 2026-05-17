#include "CompletedDifficultyCounts.hpp"

#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/loader/Log.hpp>

#include <array>
#include <charconv>
#include <string_view>

using namespace geode::prelude;

namespace obs_stats::CompletedDifficultyCounts {
    namespace {
        enum class DifficultyResponseField {
            ClassicLevels,
            Demons,
        };

        struct DifficultyResponseIndex {
            DifficultyResponseField field;
            size_t index;
        };

        constexpr size_t kDifficultyCacheSize = 11;
        constexpr std::array<std::pair<GJDifficulty, DifficultyResponseIndex>, kDifficultyCacheSize>
            kDifficultyResponseIndices = {{
                { GJDifficulty::Auto, { DifficultyResponseField::ClassicLevels, 0 } },
                { GJDifficulty::Easy, { DifficultyResponseField::ClassicLevels, 1 } },
                { GJDifficulty::Normal, { DifficultyResponseField::ClassicLevels, 2 } },
                { GJDifficulty::Hard, { DifficultyResponseField::ClassicLevels, 3 } },
                { GJDifficulty::Harder, { DifficultyResponseField::ClassicLevels, 4 } },
                { GJDifficulty::Insane, { DifficultyResponseField::ClassicLevels, 5 } },
                { GJDifficulty::DemonEasy, { DifficultyResponseField::Demons, 0 } },
                { GJDifficulty::DemonMedium, { DifficultyResponseField::Demons, 1 } },
                { GJDifficulty::Demon, { DifficultyResponseField::Demons, 2 } },
                { GJDifficulty::DemonInsane, { DifficultyResponseField::Demons, 3 } },
                { GJDifficulty::DemonExtreme, { DifficultyResponseField::Demons, 4 } },
            }};

        std::array<std::optional<int>, kDifficultyCacheSize> cachedCounts;

        constexpr std::optional<DifficultyResponseIndex> responseIndexForDifficulty(GJDifficulty difficulty) {
            for (auto const& [candidate, responseIndex] : kDifficultyResponseIndices) {
                if (candidate == difficulty) {
                    return responseIndex;
                }
            }

            return std::nullopt;
        }

        constexpr std::optional<size_t> cacheIndexForDifficulty(GJDifficulty difficulty) {
            int rawDifficulty = static_cast<int>(difficulty);
            if (rawDifficulty < static_cast<int>(GJDifficulty::Auto)
                || rawDifficulty > static_cast<int>(GJDifficulty::DemonExtreme)) {
                return std::nullopt;
            }

            return static_cast<size_t>(rawDifficulty);
        }

        bool isDemonDifficulty(GJDifficulty difficulty) {
            return difficulty == GJDifficulty::Demon
                || difficulty == GJDifficulty::DemonEasy
                || difficulty == GJDifficulty::DemonMedium
                || difficulty == GJDifficulty::DemonInsane
                || difficulty == GJDifficulty::DemonExtreme;
        }

        GJDifficulty demonDifficultyForLevel(GJGameLevel* level) {
            switch (static_cast<DemonDifficultyType>(level->m_demonDifficulty)) {
                case DemonDifficultyType::EasyDemon:
                    return GJDifficulty::DemonEasy;
                case DemonDifficultyType::MediumDemon:
                    return GJDifficulty::DemonMedium;
                case DemonDifficultyType::InsaneDemon:
                    return GJDifficulty::DemonInsane;
                case DemonDifficultyType::ExtremeDemon:
                    return GJDifficulty::DemonExtreme;
                case DemonDifficultyType::HardDemon:
                default:
                    return GJDifficulty::Demon;
            }
        }

        std::optional<GJDifficulty> classicDifficultyForLevel(GJGameLevel* level) {
            if (level->m_autoLevel) {
                return GJDifficulty::Auto;
            }

            switch (level->m_stars.value()) {
                case 1:
                    return GJDifficulty::Auto;
                case 2:
                    return GJDifficulty::Easy;
                case 3:
                    return GJDifficulty::Normal;
                case 4:
                case 5:
                    return GJDifficulty::Hard;
                case 6:
                case 7:
                    return GJDifficulty::Harder;
                case 8:
                case 9:
                    return GJDifficulty::Insane;
                default:
                    break;
            }

            if (!isDemonDifficulty(level->m_difficulty) && responseIndexForDifficulty(level->m_difficulty)) {
                return level->m_difficulty;
            }

            return std::nullopt;
        }

        std::optional<std::string_view> valueForProfileResponseKey(
            std::string_view response,
            std::string_view targetKey
        ) {
            std::string_view key;
            bool readingKey = true;
            size_t start = 0;

            while (start <= response.size()) {
                size_t end = response.find(':', start);
                std::string_view token = response.substr(
                    start,
                    end == std::string_view::npos ? std::string_view::npos : end - start
                );

                if (readingKey) {
                    key = token;
                }
                else if (key == targetKey) {
                    return token;
                }

                if (end == std::string_view::npos) {
                    break;
                }

                readingKey = !readingKey;
                start = end + 1;
            }

            return std::nullopt;
        }

        std::optional<int> commaSeparatedIntAt(std::string_view values, size_t targetIndex) {
            size_t start = 0;
            size_t index = 0;

            while (start <= values.size()) {
                size_t end = values.find(',', start);
                std::string_view value = values.substr(
                    start,
                    end == std::string_view::npos ? std::string_view::npos : end - start
                );

                if (index == targetIndex) {
                    int parsed = 0;
                    std::from_chars_result result = std::from_chars(
                        value.data(),
                        value.data() + value.size(),
                        parsed
                    );
                    if (result.ec != std::errc() || result.ptr != value.data() + value.size()) {
                        return std::nullopt;
                    }

                    return parsed;
                }

                if (end == std::string_view::npos) {
                    break;
                }

                ++index;
                start = end + 1;
            }

            return std::nullopt;
        }

        std::optional<std::string_view> responseValueForField(
            std::string_view response,
            DifficultyResponseField field
        ) {
            switch (field) {
                case DifficultyResponseField::ClassicLevels:
                    return valueForProfileResponseKey(response, "56");
                case DifficultyResponseField::Demons:
                    return valueForProfileResponseKey(response, "55");
            }

            return std::nullopt;
        }

        void setCountForDifficulty(GJDifficulty difficulty, int count) {
            std::optional<size_t> cacheIndex = cacheIndexForDifficulty(difficulty);
            if (!cacheIndex) {
                return;
            }

            cachedCounts[*cacheIndex] = count;
        }

        void incrementCountForDifficulty(GJDifficulty difficulty, int amount) {
            std::optional<size_t> cacheIndex = cacheIndexForDifficulty(difficulty);
            if (!cacheIndex) {
                return;
            }

            cachedCounts[*cacheIndex] = cachedCounts[*cacheIndex].value_or(0) + amount;
        }

        std::optional<GJDifficulty> difficultyForCompletedLevel(GJGameLevel* level) {
            if (level == nullptr) {
                return std::nullopt;
            }

            if (level->m_demon.value() != 0 || isDemonDifficulty(level->m_difficulty)) {
                return demonDifficultyForLevel(level);
            }

            std::optional<GJDifficulty> difficulty = classicDifficultyForLevel(level);
            if (!difficulty) {
                return std::nullopt;
            }

            return *difficulty;
        }
    }

    std::optional<int> countForDifficulty(GJDifficulty difficulty) {
        std::optional<size_t> cacheIndex = cacheIndexForDifficulty(difficulty);
        if (!cacheIndex) {
            return std::nullopt;
        }

        return cachedCounts[*cacheIndex];
    }

    std::optional<GJDifficulty> incrementCountForLevel(GJGameLevel* level, int amount) {
        std::optional<GJDifficulty> difficulty = difficultyForCompletedLevel(level);
        if (!difficulty) {
            log::warn("could not resolve completed difficulty for level");
            return std::nullopt;
        }

        incrementCountForDifficulty(*difficulty, amount);
        return difficulty;
    }

    bool updateCountsFromProfileResponse(std::string_view response) {
        bool updatedAnyCount = false;

        for (auto const& [difficulty, responseIndex] : kDifficultyResponseIndices) {
            std::optional<std::string_view> responseValue = responseValueForField(response, responseIndex.field);
            if (!responseValue) {
                log::warn(
                    "profile completed difficulty payload missing field for difficulty {}",
                    static_cast<int>(difficulty)
                );
                continue;
            }

            std::optional<int> count = commaSeparatedIntAt(*responseValue, responseIndex.index);
            if (!count) {
                log::warn(
                    "profile completed difficulty payload could not parse index {} from '{}' for difficulty {}",
                    responseIndex.index,
                    *responseValue,
                    static_cast<int>(difficulty)
                );
                continue;
            }

            setCountForDifficulty(difficulty, *count);
            updatedAnyCount = true;
        }

        return updatedAnyCount;
    }
}
