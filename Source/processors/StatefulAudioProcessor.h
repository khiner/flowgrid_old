#pragma  once

#include "DefaultAudioProcessor.h"
#include <unordered_map>
#include <Utilities.h>
#include "Identifiers.h"

class StatefulAudioProcessor : public DefaultAudioProcessor, public Utilities::ValueTreePropertyChangeListener {
public:
    struct Parameter : public AudioProcessorParameterWithID {
        Parameter(ValueTree state, UndoManager &undoManager, String parameterID, String paramName, const String &labelText, NormalisableRange<double> range,
                  float defaultVal, std::function<String (const float)> valueToTextFunction,
                  std::function<float(const String &)> textToValueFunction)
                : AudioProcessorParameterWithID(parameterID, paramName, labelText, Category::genericParameter),
                  state(std::move(state)), undoManager(undoManager), paramId(std::move(parameterID)), paramName(std::move(paramName)),
                  labelText(labelText), range(std::move(range)),
                  valueToTextFunction(std::move(valueToTextFunction)),
                  textToValueFunction(std::move(textToValueFunction)), defaultValue(defaultVal) {}

        ValueTree getState() const {
            return state.getChildWithProperty(IDs::id, paramId);
        }

        float getValue() const override {
            return (float) range.convertTo0to1(getRawValue());
        }

        void setValue(float newValue) override {
            float clampedNewValue = newValue < 0 ? 0 : (newValue > 1 ? 1 : newValue);
            getState().setProperty(IDs::value, range.convertFrom0to1(clampedNewValue), &undoManager);
        }

        float getRawValue() const {
            return getState().getProperty(IDs::value);
        }

        Value getValueObject() const {
            return getState().getPropertyAsValue(IDs::value, &undoManager);
        }

        void attachSlider(Slider *slider, Label *label=nullptr) {
            if (label != nullptr) {
                label->setText(paramName, dontSendNotification);
            }
            slider->getValueObject().referTo({});
            slider->setNormalisableRange(range);
            slider->textFromValueFunction = valueToTextFunction;
            slider->valueFromTextFunction = textToValueFunction;
            slider->getValueObject().referTo(getValueObject());
        }

        float getDefaultValue() const override {
            //return range.convertTo0to1 (defaultValue);
            return defaultValue;
        }

        float getValueForText(const String& text) const override {
            return (float) range.convertTo0to1(textToValueFunction != nullptr ? textToValueFunction(text) : text.getFloatValue());
        }

        ValueTree state;
        UndoManager &undoManager;

        const String paramId;
        const String paramName;
        const String labelText;
        NormalisableRange<double> range;
        std::function<String(const float)> valueToTextFunction;
        std::function<float(const String &)> textToValueFunction;

    private:
        float defaultValue;
    };

    StatefulAudioProcessor(const PluginDescription& description, ValueTree state, UndoManager &undoManager) :
            DefaultAudioProcessor(description), state(std::move(state)), undoManager(undoManager) {
        this->state.addListener(this);
    }

    ~StatefulAudioProcessor() override {
        state.removeListener(this);
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            parameterChanged(parameterId, tree[IDs::value]);
        }
    }

    Parameter *getParameterObject(int parameterIndex) {
        return dynamic_cast<Parameter *>(getParameters()[parameterIndex]);
    }

    void updateValueTree() {
        for (auto parameter : getParameters()) {
            auto *parameterObject = dynamic_cast<Parameter *>(parameter);
            ValueTree v = getOrCreateChildValueTree(parameterObject->paramId);
            if (!v.hasProperty(IDs::value)) {
                v.setProperty(IDs::value, parameterObject->getDefaultValue(), nullptr);
            }
            v.sendPropertyChangeMessage(IDs::value);
        }
    }

    ValueTree getOrCreateChildValueTree(const String& paramID) {
        ValueTree v(state.getChildWithProperty(IDs::id, paramID));

        if (!v.isValid()) {
            v = ValueTree(IDs::PARAM);
            v.setProperty(IDs::id, paramID, nullptr);
            state.appendChild(v, nullptr);
        }

        return v;
    }

    ValueTree state;
    UndoManager &undoManager;
};
