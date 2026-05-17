#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <matjson/std.hpp>

#include "SourceMapping.hpp"
#include "ui/FormatEdit.hpp"
#include "ui/SourceSelect.hpp"

#include <memory>

using namespace geode::prelude;
using obs_stats::SourceMapping;

namespace {
    class MappingComponentPopup : public Popup {
    protected:
        Function<void()> m_callback;

        bool init(
            std::string const& title,
            CCNode* content,
            CCSize const& size,
            Function<void()> callback
        ) {
            if (!Popup::init(size)) {
                return false;
            }

            m_callback = std::move(callback);
            this->setTitle(title);
            m_mainLayer->addChildAtPosition(content, Anchor::Center, ccp(0, 6.f));

            ButtonSprite* saveSprite =
                ButtonSprite::create("Save", 80, true, "goldFont.fnt", "GJ_button_01.png", 20.f, .6f);
            CCMenuItemSpriteExtra* saveButton = CCMenuItemSpriteExtra::create(
                saveSprite,
                this,
                menu_selector(MappingComponentPopup::onSave)
            );
            m_buttonMenu->addChildAtPosition(saveButton, Anchor::Bottom, ccp(0, 25));

            return true;
        }

        void onCancel(CCObject*) {
            this->onClose(nullptr);
        }

        void onSave(CCObject*) {
            if (m_callback) {
                m_callback();
            }

            this->onClose(nullptr);
        }

    public:
        static MappingComponentPopup* create(
            std::string const& title,
            CCNode* content,
            CCSize const& size,
            Function<void()> callback
        ) {
            MappingComponentPopup* ret = new MappingComponentPopup();
            if (ret->init(title, content, size, std::move(callback))) {
                ret->autorelease();
                return ret;
            }

            delete ret;
            return nullptr;
        }
    };

    class SourceMappingsSettingV3 : public SettingV3 {
    protected:
        std::vector<SourceMapping> m_defaultItems;
        std::vector<SourceMapping> m_items;

    public:
        static Result<std::shared_ptr<SettingV3>> parse(
            std::string const& key,
            std::string const& modID,
            matjson::Value const& json
        ) {
            std::shared_ptr<SourceMappingsSettingV3> res = std::make_shared<SourceMappingsSettingV3>();
            auto root = checkJson(json, "SourceMappingsSettingV3");

            res->init(key, modID, root);
            res->parseNameAndDescription(root);
            res->parseEnableIf(root);
            res->parseValueProperties(root);
            root.has("default").into(res->m_defaultItems);
            res->m_items = res->m_defaultItems;
            root.checkUnknownKeys();

            return root.ok(std::static_pointer_cast<SettingV3>(res));
        }

        bool load(matjson::Value const& json) override {
            auto value = json.as<std::vector<SourceMapping>>();
            if (!value) return false;
            m_items = value.unwrap();
            return true;
        }

        bool save(matjson::Value& json) const override {
            json = matjson::Value(m_items);
            return true;
        }

        bool isDefaultValue() const override {
            return m_items == m_defaultItems;
        }

        void reset() override {
            m_items = m_defaultItems;
        }

        SettingNodeV3* createNode(float width) override;

        std::vector<SourceMapping> const& getItems() const {
            return m_items;
        }

        std::vector<SourceMapping> const& getDefaultItems() const {
            return m_defaultItems;
        }

        void setItems(std::vector<SourceMapping> items) {
            m_items = std::move(items);
            this->markChanged();
        }
    };

    class SourceMappingsSettingNodeV3 : public SettingNodeV3 {
    protected:
        CCNode* m_list = nullptr;
        std::vector<SourceMapping> m_currentItems;

        bool init(std::shared_ptr<SourceMappingsSettingV3> setting, float width) {
            if (!SettingNodeV3::init(setting, width)) {
                return false;
            }

            m_currentItems = setting->getItems();

            m_list = CCNode::create();
            m_list->setAnchorPoint({ .5f, .5f });
            this->addChild(m_list);

            ButtonSprite* addSprite = ButtonSprite::create(
                "Add",
                70,
                true,
                "goldFont.fnt",
                "GJ_button_01.png",
                20.f,
                .5f
            );
            CCMenuItemSpriteExtra* addButton = CCMenuItemSpriteExtra::create(
                addSprite,
                this,
                menu_selector(SourceMappingsSettingNodeV3::onAdd)
            );
            this->getButtonMenu()->addChildAtPosition(addButton, Anchor::Center);

            this->renderList();
            this->updateState(nullptr);

            return true;
        }

        void renderList() {
            constexpr float headerHeight = 30.f;
            constexpr float itemHeight = 24.f;
            constexpr float verticalGap = 8.f;
            constexpr float bottomPadding = 6.f;
            constexpr float rowRightPadding = 10.f;
            constexpr float buttonGap = 2.f;
            constexpr float sourceButtonWidth = 24.f;
            constexpr float sourceButtonGap = 4.f;

            m_list->removeAllChildren();

            float listWidth = this->getContentWidth() - 20.f;
            float editButtonWidth = 95.f;
            float deleteButtonWidth = 18.f;
            float rowButtonsWidth = editButtonWidth + deleteButtonWidth + buttonGap;
            float rowWidth = listWidth + rowRightPadding;
            float listHeight = itemHeight * static_cast<float>(std::max<size_t>(m_currentItems.size(), 1));
            float contentHeight = headerHeight + listHeight + verticalGap + bottomPadding;

            m_list->setContentSize({ listWidth, listHeight });

            if (m_currentItems.empty()) {
                CCLabelBMFont* label = CCLabelBMFont::create("No mappings added", "bigFont.fnt");
                label->setColor(ccGRAY);
                label->setAnchorPoint({ 0.f, .5f });
                label->limitLabelWidth(listWidth, .3f, .1f);
                m_list->addChildAtPosition(label, Anchor::Left, ccp(0, 0), ccp(0, .5f));
            }
            else {
                for (size_t i = 0; i < m_currentItems.size(); i++) {
                    CCNode* row = CCNode::create();
                    row->setContentSize({ rowWidth, itemHeight });
                    row->setAnchorPoint({ .5f, .5f });
                    row->setPosition({
                        rowWidth / 2.f,
                        listHeight - itemHeight * (static_cast<float>(i) + .5f)
                    });

                    CCMenu* sourceButtonMenu = CCMenu::create();
                    sourceButtonMenu->setContentSize({ sourceButtonWidth, itemHeight });
                    row->addChildAtPosition(sourceButtonMenu, Anchor::Left, ccp(0, 0), ccp(0.f, .5f));

                    CCSprite* sourceEditSprite = CCSprite::createWithSpriteFrameName("gj_findBtn_001.png");
                    sourceEditSprite->setScale(.55f);
                    CCMenuItemSpriteExtra* sourceEditButton = CCMenuItemSpriteExtra::create(
                        sourceEditSprite,
                        this,
                        menu_selector(SourceMappingsSettingNodeV3::onEditSource)
                    );
                    sourceEditButton->setTag(static_cast<int>(i));
                    sourceButtonMenu->addChildAtPosition(sourceEditButton, Anchor::Center);

                    CCLabelBMFont* label = CCLabelBMFont::create(m_currentItems[i].sourceName.c_str(), "bigFont.fnt");
                    label->setAnchorPoint({ 0.f, .5f });
                    label->limitLabelWidth(
                        listWidth - rowButtonsWidth - sourceButtonWidth - sourceButtonGap - 10.f,
                        .3f,
                        .1f
                    );
                    row->addChildAtPosition(
                        label,
                        Anchor::Left,
                        ccp(sourceButtonWidth + sourceButtonGap, 0),
                        ccp(0, .5f)
                    );

                    CCMenu* rowButtons = CCMenu::create();
                    rowButtons->setContentSize({ rowButtonsWidth, itemHeight });
                    rowButtons->setLayout(
                        RowLayout::create()
                            ->setAxisAlignment(AxisAlignment::End)
                            ->setGap(buttonGap)
                    );
                    row->addChildAtPosition(rowButtons, Anchor::Right, ccp(-rowRightPadding, 0), ccp(1.f, .5f));

                    ButtonSprite* editSprite = ButtonSprite::create(
                        "Edit Format",
                        static_cast<int>(editButtonWidth),
                        true,
                        "goldFont.fnt",
                        "GJ_button_01.png",
                        18.f,
                        .45f
                    );
                    CCMenuItemSpriteExtra* editButton = CCMenuItemSpriteExtra::create(
                        editSprite,
                        this,
                        menu_selector(SourceMappingsSettingNodeV3::onEditFormat)
                    );
                    editButton->setTag(static_cast<int>(i));
                    rowButtons->addChild(editButton);

                    CCSprite* deleteSprite = CCSprite::createWithSpriteFrameName("GJ_resetBtn_001.png");
                    deleteSprite->setScale(0.8f);
                    CCMenuItemSpriteExtra* deleteButton = CCMenuItemSpriteExtra::create(
                        deleteSprite,
                        this,
                        menu_selector(SourceMappingsSettingNodeV3::onDeleteMapping)
                    );
                    deleteButton->setID("delete-button");
                    deleteButton->setTag(static_cast<int>(i));
                    rowButtons->addChild(deleteButton);
                    rowButtons->updateLayout();

                    m_list->addChild(row);
                }
            }

            this->setContentHeight(contentHeight);
            float headerY = contentHeight - headerHeight / 2.f;
            float listY = bottomPadding + listHeight / 2.f;

            this->getNameMenu()->setPosition({ 10.f, headerY });
            this->getButtonMenu()->setPosition({ this->getContentWidth() - 10.f, headerY });
            m_list->setPosition({ this->getContentWidth() / 2.f, listY });
        }

        void onAdd(CCObject*) {
            SourceSelect* sourceSelect = SourceSelect::create();
            FormatEdit* formatEdit = FormatEdit::create("");
            constexpr float gap = 10.f;

            float contentWidth = std::max(sourceSelect->getContentWidth(), formatEdit->getContentWidth());
            float contentHeight = sourceSelect->getContentHeight() + gap + formatEdit->getContentHeight();
            CCNode* content = CCNode::create();
            content->setContentSize({ contentWidth, contentHeight });
            content->setAnchorPoint({ .5f, .5f });

            sourceSelect->setPosition({ contentWidth / 2.f, contentHeight - sourceSelect->getContentHeight() / 2.f });
            content->addChild(sourceSelect);

            formatEdit->setPosition({ contentWidth / 2.f, formatEdit->getContentHeight() / 2.f });
            content->addChild(formatEdit);

            MappingComponentPopup::create(
                "Add Mapping",
                content,
                { 320.f, contentHeight + 95.f },
                [this, sourceSelect, formatEdit]() {
                    std::string sourceName = sourceSelect->getSelectedSource();
                    if (sourceName.empty()) {
                        return;
                    }

                    m_currentItems.push_back({
                        .sourceName = sourceName,
                        .formattedText = formatEdit->getFormattedText(),
                    });
                    this->renderList();
                    this->markChanged(nullptr);
                }
            )->show();
        }

        void onEditSource(CCObject* sender) {
            size_t index = static_cast<size_t>(sender->getTag());
            if (index >= m_currentItems.size()) {
                return;
            }

            SourceSelect* sourceSelect = SourceSelect::create(
                m_currentItems[index].sourceName,
                [this, index](std::string const& sourceName) {
                    if (index >= m_currentItems.size()) {
                        return;
                    }

                    m_currentItems[index].sourceName = sourceName;
                    this->renderList();
                    this->markChanged(nullptr);
                }
            );
            MappingComponentPopup::create(
                "Select Source",
                sourceSelect,
                { 320.f, sourceSelect->getContentHeight() + 95.f },
                [sourceSelect]() {
                    sourceSelect->emitSelectedSource();
                }
            )->show();
        }

        void onEditFormat(CCObject* sender) {
            size_t index = static_cast<size_t>(sender->getTag());
            if (index >= m_currentItems.size()) {
                return;
            }

            FormatEdit* formatEdit = FormatEdit::create(m_currentItems[index].formattedText);
            MappingComponentPopup::create(
                m_currentItems[index].sourceName,
                formatEdit,
                { 320.f, 145.f },
                [this, index, formatEdit]() {
                    if (index >= m_currentItems.size()) {
                        return;
                    }

                    m_currentItems[index].formattedText = formatEdit->getFormattedText();
                    this->markChanged(nullptr);
                }
            )->show();
        }

        void onDeleteMapping(CCObject* sender) {
            size_t index = static_cast<size_t>(sender->getTag());
            if (index >= m_currentItems.size()) {
                return;
            }

            m_currentItems.erase(m_currentItems.begin() + index);
            this->renderList();
            this->markChanged(nullptr);
        }

        void onCommit() override {
            this->getListSetting()->setItems(m_currentItems);
        }

        void onResetToDefault() override {
            m_currentItems = this->getListSetting()->getDefaultItems();
            this->renderList();
        }

        std::shared_ptr<SourceMappingsSettingV3> getListSetting() const {
            return std::static_pointer_cast<SourceMappingsSettingV3>(this->getSetting());
        }

    public:
        static SourceMappingsSettingNodeV3* create(
            std::shared_ptr<SourceMappingsSettingV3> setting,
            float width
        ) {
            SourceMappingsSettingNodeV3* ret = new SourceMappingsSettingNodeV3();
            if (ret->init(setting, width)) {
                ret->autorelease();
                return ret;
            }

            delete ret;
            return nullptr;
        }

        bool hasUncommittedChanges() const override {
            return m_currentItems != this->getListSetting()->getItems();
        }

        bool hasNonDefaultValue() const override {
            return m_currentItems != this->getListSetting()->getDefaultItems();
        }
    };

    SettingNodeV3* SourceMappingsSettingV3::createNode(float width) {
        return SourceMappingsSettingNodeV3::create(
            std::static_pointer_cast<SourceMappingsSettingV3>(shared_from_this()),
            width
        );
    }
}

namespace obs_stats {
    std::vector<SourceMapping> getSourceMappings() {
        std::shared_ptr<SourceMappingsSettingV3> setting = geode::cast::typeinfo_pointer_cast<SourceMappingsSettingV3>(
            Mod::get()->getSetting("source-mappings")
        );
        if (!setting) {
            return {};
        }

        return setting->getItems();
    }
}

$on_mod(Loaded) {
    (void)Mod::get()->registerCustomSettingType(
        "source-mappings",
        &SourceMappingsSettingV3::parse
    );
}
