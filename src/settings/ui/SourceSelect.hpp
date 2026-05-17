#pragma once

#include <Geode/utils/function.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

class SourceSelect : public CCNode {
public:
    using SelectCallback = Function<void(std::string const&)>;

protected:
    CCNode* m_content = nullptr;
    ScrollLayer* m_scrollLayer = nullptr;
    std::vector<std::string> m_options;
    std::string m_selectedSource;
    SelectCallback m_onSelect;
    float m_width = 0.f;
    float m_itemHeight = 24.f;
    bool m_loading = true;

    bool init(
        std::string const& selectedSource,
        SelectCallback onSelect,
        float width,
        float itemHeight
    );

    void setOptions(std::vector<std::string> options);
    void renderOptions();
    void onSelectSource(CCObject* sender);

public:
    static SourceSelect* create(
        std::string const& selectedSource = "",
        SelectCallback onSelect = nullptr,
        float width = 260.f,
        float itemHeight = 24.f
    );

    std::string getSelectedSource() const;
    void emitSelectedSource();
};
