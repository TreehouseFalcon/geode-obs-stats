#pragma once

#include <Geode/Result.hpp>
#include <matjson.hpp>
#include <matjson/std.hpp>

#include <string>
#include <vector>

namespace obs_stats {
    struct SourceMapping {
        std::string sourceName;
        std::string formattedText;

        bool operator==(SourceMapping const&) const = default;
    };

    std::vector<SourceMapping> getSourceMappings();
}

template <>
struct matjson::Serialize<obs_stats::SourceMapping> {
    static geode::Result<obs_stats::SourceMapping> fromJson(matjson::Value const& value) {
        if (!value.isObject()) {
            return geode::Err("mapping entry is not an object");
        }

        return geode::Ok(obs_stats::SourceMapping {
            .sourceName = GEODE_UNWRAP(value["sourceName"].asString()),
            .formattedText = GEODE_UNWRAP(value["formattedText"].asString()),
        });
    }

    static matjson::Value toJson(obs_stats::SourceMapping const& value) {
        return matjson::makeObject({
            { "sourceName", value.sourceName },
            { "formattedText", value.formattedText },
        });
    }
};
