#pragma once

#include <format>
#include <locale>
#include <string>

namespace obs_stats {
    namespace detail {
        struct CommaNumpunct : std::numpunct<char> {
            char do_thousands_sep() const override { return ','; }
            std::string do_grouping() const override { return "\3"; }
        };

        inline std::locale const& commaLocale() {
            static std::locale locale = std::locale(std::locale::classic(), new CommaNumpunct);
            return locale;
        }
    }

    template <class Number>
    std::string formatCommaNumber(Number value) {
        return std::format(detail::commaLocale(), "{:L}", value);
    }
}
