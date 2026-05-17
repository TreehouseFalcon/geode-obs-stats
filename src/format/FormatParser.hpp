#pragma once

#include "StatPlaceholders.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace obs_stats {
    using StatPlaceholderCallback = std::function<void(StatPlaceholder const&)>;

    std::vector<StatPlaceholder> parseFormatPlaceholders(std::string_view formattedText);
    std::string formatStatPlaceholders(
        std::string_view formattedText,
        StatPlaceholderCallback const& callback = {}
    );
}
