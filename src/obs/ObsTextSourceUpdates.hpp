#pragma once

#include "../format/StatPlaceholders.hpp"

#include <vector>

namespace obs_stats {
    void updateApplicableObsTextSources(StatPlaceholder const& placeholder);
    void updateApplicableObsTextSources(std::vector<StatPlaceholder> const& placeholders);
    void updateAllObsTextSources();
}
