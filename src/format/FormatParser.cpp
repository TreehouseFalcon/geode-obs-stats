#include "FormatParser.hpp"

#include <Geode/loader/Log.hpp>

using namespace geode::prelude;

namespace obs_stats {
    std::vector<StatPlaceholder> parseFormatPlaceholders(std::string_view formattedText) {
        std::vector<StatPlaceholder> placeholders;
        size_t cursor = 0;

        while (cursor < formattedText.size()) {
            size_t open = formattedText.find('{', cursor);
            if (open == std::string_view::npos) {
                break;
            }

            size_t close = formattedText.find('}', open + 1);
            if (close == std::string_view::npos) {
                break;
            }

            std::string_view placeholderText = formattedText.substr(open, close - open + 1);
            if (std::optional<StatPlaceholder> placeholder = findStatPlaceholder(placeholderText)) {
                placeholders.push_back(*placeholder);
            }

            cursor = close + 1;
        }

        return placeholders;
    }

    std::string formatStatPlaceholders(
        std::string_view formattedText,
        StatPlaceholderCallback const& callback
    ) {
        std::string result;
        result.reserve(formattedText.size());

        size_t cursor = 0;

        while (cursor < formattedText.size()) {
            size_t open = formattedText.find('{', cursor);
            if (open == std::string_view::npos) {
                result.append(formattedText.substr(cursor));
                break;
            }

            size_t close = formattedText.find('}', open + 1);
            if (close == std::string_view::npos) {
                result.append(formattedText.substr(cursor));
                break;
            }

            result.append(formattedText.substr(cursor, open - cursor));

            std::string_view placeholderText = formattedText.substr(open, close - open + 1);
            std::optional<StatPlaceholder> placeholder = findStatPlaceholder(placeholderText);
            if (!placeholder) {
                result.append(placeholderText);
                cursor = close + 1;
                continue;
            }

            if (callback) {
                callback(*placeholder);
            }

            std::optional<std::string> selectedValue = selectStatPlaceholderValue(*placeholder);
            if (selectedValue) {
                result.append(*selectedValue);
            }
            else {
                log::warn(
                    "format placeholder '{}' had no selected value; leaving literal in text",
                    placeholderText
                );
                result.append(placeholderText);
            }

            cursor = close + 1;
        }

        return result;
    }
}
