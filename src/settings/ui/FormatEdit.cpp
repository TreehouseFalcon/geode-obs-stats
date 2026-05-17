#include "FormatEdit.hpp"

bool FormatEdit::init(std::string const& formattedText, float width) {
    if (!CCNode::init()) {
        return false;
    }

    this->setContentSize({ width, 40.f });
    this->setAnchorPoint({ .5f, .5f });

    m_input = TextInput::create(width, "Formatted Text");
    m_input->setCommonFilter(CommonFilter::Any);
    m_input->setMaxCharCount(0);
    m_input->setTextAlign(TextInputAlign::Left);
    m_input->setString(formattedText);
    this->addChildAtPosition(m_input, Anchor::Center);

    return true;
}

FormatEdit* FormatEdit::create(std::string const& formattedText, float width) {
    FormatEdit* ret = new FormatEdit();
    if (ret->init(formattedText, width)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

std::string FormatEdit::getFormattedText() const {
    return m_input->getString();
}
