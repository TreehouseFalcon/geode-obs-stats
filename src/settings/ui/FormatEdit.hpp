#pragma once

#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class FormatEdit : public CCNode {
protected:
    TextInput* m_input = nullptr;

    bool init(std::string const& formattedText, float width);

public:
    static FormatEdit* create(std::string const& formattedText, float width = 260.f);

    std::string getFormattedText() const;
};
