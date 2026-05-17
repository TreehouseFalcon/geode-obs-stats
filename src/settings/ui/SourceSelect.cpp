#include "SourceSelect.hpp"

#include "../../obs/ObsConnection.hpp"

#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/cocos/misc_nodes/CCClippingNode.h>
#include <Geode/ui/NineSlice.hpp>

#include <algorithm>

bool SourceSelect::init(
    std::string const& selectedSource,
    SelectCallback onSelect,
    float width,
    float itemHeight
) {
    if (!CCNode::init()) {
        return false;
    }

    m_selectedSource = selectedSource;
    m_onSelect = std::move(onSelect);
    m_width = width;
    m_itemHeight = itemHeight;

    float contentHeight = m_itemHeight;
    float visibleOptions = 4.5f;
    float visibleHeight = m_itemHeight * visibleOptions;

    this->setContentSize({ m_width, visibleHeight });
    this->setAnchorPoint({ .5f, .5f });

    NineSlice* frame = NineSlice::create("square02b_001.png", { 0.f, 0.f, 80.f, 80.f });
    frame->setContentSize(this->getContentSize());
    frame->setColor({ 0, 0, 0 });
    frame->setOpacity(75);
    frame->setAnchorPoint({ .5f, .5f });
    frame->setPosition({ m_width / 2.f, visibleHeight / 2.f });
    this->addChild(frame);

    NineSlice* stencil = NineSlice::create("square02b_001.png", { 0.f, 0.f, 80.f, 80.f });
    stencil->setContentSize(this->getContentSize());
    stencil->setPosition({ m_width / 2.f, visibleHeight / 2.f });

    CCClippingNode* clipper = CCClippingNode::create(stencil);
    clipper->setContentSize(this->getContentSize());
    clipper->setAnchorPoint({ .5f, .5f });
    clipper->setPosition({ m_width / 2.f, visibleHeight / 2.f });
    clipper->setAlphaThreshold(.05f);
    this->addChild(clipper);

    m_scrollLayer = ScrollLayer::create({ m_width, visibleHeight }, true, true);
    m_scrollLayer->m_contentLayer->setContentSize({ m_width, contentHeight });
    m_scrollLayer->m_cutContent = false;
    m_scrollLayer->setTouchEnabled(false);
    m_scrollLayer->setAnchorPoint({ 0.f, 0.f });
    m_scrollLayer->setPosition({ 0.f, 0.f });
    clipper->addChild(m_scrollLayer);

    m_content = CCNode::create();
    m_content->setContentSize({ m_width, contentHeight });
    m_content->setAnchorPoint({ 0.f, 0.f });
    m_content->setPosition({ 0.f, 0.f });
    m_scrollLayer->m_contentLayer->addChild(m_content);

    this->renderOptions();
    m_scrollLayer->scrollToTop();

    obs::fetchCurrentSceneTextSources([self = Ref(this)](std::vector<std::string> sources) {
        self->setOptions(std::move(sources));
    });

    return true;
}

void SourceSelect::setOptions(std::vector<std::string> options) {
    m_loading = false;
    m_options = std::move(options);

    float contentHeight = m_itemHeight * static_cast<float>(std::max<size_t>(m_options.size(), 1));
    m_content->setContentSize({ m_width, contentHeight });
    m_scrollLayer->m_contentLayer->setContentSize({ m_width, contentHeight });
    m_scrollLayer->setTouchEnabled(m_options.size() > 4);

    this->renderOptions();
    m_scrollLayer->scrollToTop();
}

void SourceSelect::renderOptions() {
    m_content->removeAllChildren();

    auto makeRow = [this](std::string const& text, ccColor4B const& color, size_t index, bool includeButton = true) {
        constexpr float buttonSize = 22.f;
        constexpr float buttonInset = 4.f;
        constexpr float labelPadding = 8.f;

        CCNode* row = CCNode::create();
        row->setContentSize({ m_width, m_itemHeight });
        row->setAnchorPoint({ 0.f, 0.f });

        CCLayerColor* background = CCLayerColor::create(color, m_width, m_itemHeight);
        row->addChildAtPosition(background, Anchor::BottomLeft, ccp(0, 0), ccp(0, 0));

        CCLabelBMFont* label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        label->setAnchorPoint({ 0.f, .5f });
        float reservedButtonWidth = includeButton ? buttonSize + buttonInset : 0.f;
        label->limitLabelWidth(m_width - labelPadding * 2.f - reservedButtonWidth, .3f, .1f);
        row->addChildAtPosition(label, Anchor::Left, ccp(labelPadding, 0), ccp(0.f, .5f));

        if (!includeButton) {
            return row;
        }

        CCNode* buttonContainer = CCNode::create();
        buttonContainer->setContentSize({ buttonSize, m_itemHeight });
        buttonContainer->setAnchorPoint({ 1.f, .5f });
        buttonContainer->setLayout(
            RowLayout::create()
                ->setAxisAlignment(AxisAlignment::Center)
                ->setCrossAxisAlignment(AxisAlignment::Center)
        );
        row->addChildAtPosition(buttonContainer, Anchor::Right, ccp(-buttonInset, 0), ccp(1.f, .5f));

        CCMenu* selectButtonMenu = CCMenu::create();
        selectButtonMenu->setContentSize({ buttonSize, buttonSize });
        selectButtonMenu->setLayout(
            RowLayout::create()
                ->setAxisAlignment(AxisAlignment::Center)
                ->setCrossAxisAlignment(AxisAlignment::Center)
        );
        buttonContainer->addChild(selectButtonMenu);

        char const* selectSpriteName =
            text == m_selectedSource ? "GJ_selectSongOnBtn_001.png" : "GJ_selectSongBtn_001.png";
        CCSprite* selectSprite = CCSprite::createWithSpriteFrameName(selectSpriteName);
        selectSprite->setScale(.55f);
        CCMenuItemSpriteExtra* selectButton = CCMenuItemSpriteExtra::create(
            selectSprite,
            this,
            menu_selector(SourceSelect::onSelectSource)
        );
        selectButton->setTag(static_cast<int>(index));
        selectButtonMenu->addChild(selectButton);
        selectButtonMenu->updateLayout();
        buttonContainer->updateLayout();

        return row;
    };

    if (m_loading || m_options.empty()) {
        CCNode* row = makeRow(
            m_loading ? "Loading sources..." : "No text sources found",
            { 255, 255, 255, 25 },
            0,
            false
        );
        CCLabelBMFont* label = static_cast<CCLabelBMFont*>(row->getChildren()->objectAtIndex(1));
        label->setColor(ccGRAY);

        row->setPosition({ 0.f, 0.f });
        m_content->addChild(row);
        return;
    }

    float height = m_content->getContentHeight();
    for (size_t i = 0; i < m_options.size(); i++) {
        std::string const& option = m_options[i];
        ccColor4B color = i % 2 == 0 ? ccColor4B { 255, 255, 255, 25 } : ccColor4B { 0, 0, 0, 0 };
        float rowY = height - m_itemHeight * (static_cast<float>(i) + 1.f);

        CCNode* row = makeRow(option, color, i);
        row->setPosition({ 0.f, rowY });
        m_content->addChild(row);
    }
}

void SourceSelect::onSelectSource(CCObject* sender) {
    size_t index = static_cast<size_t>(sender->getTag());
    if (index >= m_options.size()) {
        return;
    }

    m_selectedSource = m_options[index];
    this->renderOptions();
}

SourceSelect* SourceSelect::create(
    std::string const& selectedSource,
    SelectCallback onSelect,
    float width,
    float itemHeight
) {
    SourceSelect* ret = new SourceSelect();
    if (ret->init(selectedSource, std::move(onSelect), width, itemHeight)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

std::string SourceSelect::getSelectedSource() const {
    return m_selectedSource;
}

void SourceSelect::emitSelectedSource() {
    if (m_onSelect) {
        m_onSelect(m_selectedSource);
    }
}
