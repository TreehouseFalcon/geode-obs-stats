#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/ui/TextInput.hpp>

#include <memory>

using namespace geode::prelude;

namespace {
    class ObsWsPasswordSettingV3 : public SettingV3 {
    protected:
        std::string m_defaultValue;
        std::string m_value;

    public:
        static Result<std::shared_ptr<SettingV3>> parse(
            std::string const& key,
            std::string const& modID,
            matjson::Value const& json
        ) {
            std::shared_ptr<ObsWsPasswordSettingV3> res = std::make_shared<ObsWsPasswordSettingV3>();
            auto root = checkJson(json, "ObsWsPasswordSettingV3");

            res->init(key, modID, root);
            res->parseNameAndDescription(root);
            res->parseEnableIf(root);
            res->parseValueProperties(root);
            root.has("default").into(res->m_defaultValue);
            res->m_value = res->m_defaultValue;
            root.checkUnknownKeys();

            return root.ok(std::static_pointer_cast<SettingV3>(res));
        }

        bool load(matjson::Value const& json) override {
            auto value = json.asString();
            if (!value) return false;

            m_value = value.unwrap();
            return true;
        }

        bool save(matjson::Value& json) const override {
            json = m_value;
            return true;
        }

        bool isDefaultValue() const override {
            return m_value == m_defaultValue;
        }

        void reset() override {
            this->setValue(m_defaultValue);
        }

        SettingNodeV3* createNode(float width) override;

        std::string const& getValue() const {
            return m_value;
        }

        std::string const& getDefaultValue() const {
            return m_defaultValue;
        }

        void setValue(std::string value) {
            m_value = std::move(value);
            this->markChanged();
        }
    };

    class ObsWsPasswordSettingNodeV3 : public SettingNodeV3, public TextInputDelegate {
    protected:
        TextInput* m_input = nullptr;
        std::string m_currentValue;
        bool m_hasPendingEdit = false;

        bool init(std::shared_ptr<ObsWsPasswordSettingV3> setting, float width) {
            if (!SettingNodeV3::init(setting, width)) {
                return false;
            }

            m_currentValue = setting->getValue();

            m_input = TextInput::create(width / 2.f, "Password");
            m_input->setCommonFilter(CommonFilter::Any);
            m_input->setPasswordMode(true);
            m_input->setMaxCharCount(100);
            m_input->setTextAlign(TextInputAlign::Left);
            m_input->setScale(.7f);
            m_input->setString(m_currentValue);
            m_input->setDelegate(this);
            this->getButtonMenu()->addChildAtPosition(m_input, Anchor::Center);

            this->updateState(nullptr);

            return true;
        }

        void updateState(CCNode* invoker) override {
            SettingNodeV3::updateState(invoker);

            if (invoker != m_input && !m_hasPendingEdit) {
                m_currentValue = this->getPasswordSetting()->getValue();
                m_input->setString(m_currentValue);
            }

            m_input->setEnabled(this->getPasswordSetting()->shouldEnable());
        }

        void onCommit() override {
            this->getPasswordSetting()->setValue(m_currentValue);
            m_hasPendingEdit = false;
            m_input->setString(m_currentValue);
        }

        void onResetToDefault() override {
            m_currentValue = this->getPasswordSetting()->getDefaultValue();
            m_hasPendingEdit = false;
            m_input->setString(m_currentValue);
        }

        void textChanged(CCTextInputNode* input) override {
            if (input == m_input->getInputNode()) {
                m_currentValue = input->getString();
                m_hasPendingEdit = this->hasUncommittedChanges();
                if (m_hasPendingEdit) {
                    this->markChanged(m_input);
                }
            }
        }

        std::shared_ptr<ObsWsPasswordSettingV3> getPasswordSetting() const {
            return std::static_pointer_cast<ObsWsPasswordSettingV3>(this->getSetting());
        }

    public:
        static ObsWsPasswordSettingNodeV3* create(
            std::shared_ptr<ObsWsPasswordSettingV3> setting,
            float width
        ) {
            ObsWsPasswordSettingNodeV3* ret = new ObsWsPasswordSettingNodeV3();
            if (ret->init(setting, width)) {
                ret->autorelease();
                return ret;
            }

            delete ret;
            return nullptr;
        }

        bool hasUncommittedChanges() const override {
            return m_currentValue != this->getPasswordSetting()->getValue();
        }

        bool hasNonDefaultValue() const override {
            return m_currentValue != this->getPasswordSetting()->getDefaultValue();
        }
    };

    SettingNodeV3* ObsWsPasswordSettingV3::createNode(float width) {
        return ObsWsPasswordSettingNodeV3::create(
            std::static_pointer_cast<ObsWsPasswordSettingV3>(shared_from_this()),
            width
        );
    }
}

$on_mod(Loaded) {
    (void)Mod::get()->registerCustomSettingType(
        "obs-ws-password",
        &ObsWsPasswordSettingV3::parse
    );
}
