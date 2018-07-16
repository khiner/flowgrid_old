#pragma  once

#include "DefaultAudioProcessor.h"
#include <unordered_map>
#include <Utilities.h>
#include "Identifiers.h"

class StatefulAudioProcessorWrapper : public Utilities::ValueTreePropertyChangeListener {
public:
    struct Parameter : public AudioProcessorParameterWithID {
        Parameter(String parameterID, String paramName, const String &labelText, NormalisableRange<double> range,
                  float defaultVal, std::function<String (const float)> valueToTextFunction,
                  std::function<float(const String &)> textToValueFunction)
                : AudioProcessorParameterWithID(parameterID, paramName, labelText, Category::genericParameter),
                  paramId(std::move(parameterID)), paramName(std::move(paramName)),
                  labelText(labelText), range(std::move(range)),
                  valueToTextFunction(std::move(valueToTextFunction)),
                  textToValueFunction(std::move(textToValueFunction)), defaultValue(defaultVal) {}

        float getValue() const override {
            return (float) range.convertTo0to1(getRawValue());
        }

        void setValue(float newValue) override {
            float clampedNewValue = newValue < 0 ? 0 : (newValue > 1 ? 1 : newValue);
            auto convertedValue = (float) range.convertFrom0to1(clampedNewValue);
            listeners.call ([=] (AudioProcessorValueTreeState::Listener& l) { l.parameterChanged(paramID, convertedValue); });

            state.setProperty(IDs::value, convertedValue, undoManager);
        }

        void setUnnormalisedValue(float newUnnormalisedValue) {
            setValue((float) range.convertTo0to1(newUnnormalisedValue));
        }

        float getRawValue() const {
            return state.getProperty(IDs::value, defaultValue);
        }

        Value getValueObject() {
            return state.getPropertyAsValue(IDs::value, undoManager);
        }

        void setNewState(const ValueTree& v, UndoManager *undoManager) {
            state = v;
            //updateFromValueTree(); TODO
            if (!state.hasProperty(IDs::value)) {
                state.setProperty(IDs::value, defaultValue, nullptr);
            }
            state.sendPropertyChangeMessage(IDs::value);

            this->undoManager = undoManager;
        }

        void updateFromValueTree() {
            setUnnormalisedValue(getRawValue());
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

        void addListener(AudioProcessorValueTreeState::Listener* listener) {
            listeners.add(listener);
        }

        void removeListener(AudioProcessorValueTreeState::Listener* listener) {
            listeners.remove(listener);
        }

        float getDefaultValue() const override {
            return (float) range.convertTo0to1 (defaultValue);
        }

        float getValueForText(const String& text) const override {
            return (float) range.convertTo0to1(textToValueFunction != nullptr ? textToValueFunction(text) : text.getFloatValue());
        }

        static Parameter* getParameterForID(AudioProcessor& processor, const StringRef &paramID) noexcept {
            for (auto* ap : processor.getParameters()) {
                auto* p = dynamic_cast<Parameter*> (ap);

                if (paramID == p->paramID)
                    return p;
            }

            return nullptr;
        }

        const String paramId;
        const String paramName;
        const String labelText;
        NormalisableRange<double> range;
        std::function<String(const float)> valueToTextFunction;
        std::function<float(const String &)> textToValueFunction;
        float defaultValue;

        ValueTree state;
        UndoManager *undoManager;
    private:

        ListenerList<AudioProcessorValueTreeState::Listener> listeners;
    };

    StatefulAudioProcessorWrapper(AudioPluginInstance *processor, ValueTree state, UndoManager &undoManager) :
            processor(processor), state(std::move(state)), undoManager(undoManager) {
        this->state.addListener(this);
        processor->enableAllBuses();
        updateValueTree();
    }

    ~StatefulAudioProcessorWrapper() override {
        state.removeListener(this);
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
        if (p == IDs::value) {
            String parameterId = tree.getProperty(IDs::id);
            if (auto *parameter = Parameter::getParameterForID(*processor, parameterId)) {
                parameter->setUnnormalisedValue(float(tree[IDs::value]));
            }
        }
    }

    Parameter *getParameterObject(int parameterIndex) {
        return dynamic_cast<Parameter *>(processor->getParameters()[parameterIndex]);
    }

    void updateValueTree() {
        for (auto parameter : processor->getParameters()) {
            auto *parameterObject = dynamic_cast<Parameter *>(parameter);
            parameterObject->setNewState(getOrCreateChildValueTree(parameterObject->paramId), &undoManager);
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

    AudioPluginInstance *processor;
    ValueTree state;
    UndoManager &undoManager;
};
