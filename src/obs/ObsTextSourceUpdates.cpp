#include "ObsTextSourceUpdates.hpp"

#include "../format/FormatParser.hpp"
#include "../settings/SourceMapping.hpp"
#include "ObsConnection.hpp"

#include <Geode/loader/Log.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace geode::prelude;

namespace obs_stats {
    namespace {
        bool formatContainsPlaceholderName(std::string_view formattedText, std::string_view placeholderName) {
            std::vector<StatPlaceholder> placeholders = parseFormatPlaceholders(formattedText);
            return std::ranges::any_of(placeholders, [placeholderName](StatPlaceholder const& placeholder) {
                return placeholder.name == placeholderName;
            });
        }

        void updateObsTextSource(SourceMapping const& mapping) {
            std::string text = formatStatPlaceholders(mapping.formattedText);
            std::vector<StatPlaceholder> unresolvedPlaceholders = parseFormatPlaceholders(text);
            if (!unresolvedPlaceholders.empty()) {
                log::warn(
                    "OBS websocket: text source '{}' still contains unresolved placeholder(s); format='{}', text='{}'",
                    mapping.sourceName,
                    mapping.formattedText,
                    text
                );
            }

            std::optional<std::string> requestId = obs::getClient().sendRequest(
                "SetInputSettings",
                matjson::makeObject({
                    { "inputName", mapping.sourceName },
                    { "inputSettings", matjson::makeObject({
                        { "text", text },
                    }) },
                    { "overlay", true },
                }),
                [sourceName = mapping.sourceName](bool success, matjson::Value) {
                    if (!success) {
                        log::warn("OBS websocket: failed to update text source '{}'", sourceName);
                    }
                }
            );

            if (requestId) {
                log::info("OBS websocket: updating text source '{}'", mapping.sourceName);
            }
        }
    }

    void updateApplicableObsTextSources(StatPlaceholder const& placeholder) {
        for (SourceMapping const& mapping : getSourceMappings()) {
            if (formatContainsPlaceholderName(mapping.formattedText, placeholder.name)) {
                updateObsTextSource(mapping);
            }
        }
    }

    void updateApplicableObsTextSources(std::vector<StatPlaceholder> const& placeholders) {
        for (SourceMapping const& mapping : getSourceMappings()) {
            if (std::ranges::any_of(placeholders, [&](StatPlaceholder const& placeholder) {
                return formatContainsPlaceholderName(mapping.formattedText, placeholder.name);
            })) {
                updateObsTextSource(mapping);
            }
        }
    }

    void updateAllObsTextSources() {
        for (SourceMapping const& mapping : getSourceMappings()) {
            updateObsTextSource(mapping);
        }
    }
}
