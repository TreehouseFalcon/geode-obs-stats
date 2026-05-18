#pragma once

#include <algorithm>
#include <string>
#include <type_traits>

namespace obs_stats {
    template <class Number>
    std::string formatCommaNumber(Number value) {
        static_assert(std::is_integral_v<Number>, "formatCommaNumber only supports integer values");

        using UnsignedNumber = std::make_unsigned_t<Number>;
        UnsignedNumber remaining;
        bool const isNegative = std::is_signed_v<Number> && value < 0;

        if constexpr (std::is_signed_v<Number>) {
            remaining = isNegative
                ? static_cast<UnsignedNumber>(-(value + 1)) + 1
                : static_cast<UnsignedNumber>(value);
        }
        else {
            remaining = value;
        }

        std::string formatted;
        int groupSize = 0;

        do {
            if (groupSize == 3) {
                formatted.push_back(',');
                groupSize = 0;
            }

            formatted.push_back(static_cast<char>('0' + (remaining % 10)));
            remaining /= 10;
            ++groupSize;
        } while (remaining > 0);

        if (isNegative) {
            formatted.push_back('-');
        }

        std::reverse(formatted.begin(), formatted.end());
        return formatted;
    }
}
