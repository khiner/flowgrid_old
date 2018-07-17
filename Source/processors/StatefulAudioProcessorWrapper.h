#pragma  once

#include <unordered_map>
#include <Utilities.h>
#include "Identifiers.h"

class StatefulAudioProcessorWrapper : private Timer {
public:
struct Parameter : public AudioProcessorParameterWithID, private Utilities::ValueTreePropertyChangeListener, private AudioProcessorParameter::Listener {
        Parameter(const String &parameterID, const String &paramName, const String &labelText, NormalisableRange<double> range,
                  float defaultValue, std::function<String (const float)> valueToTextFunction,
                  std::function<float(const String &)> textToValueFunction)
                : AudioProcessorParameterWithID(parameterID, paramName, labelText, Category::genericParameter),
                  range(std::move(range)), valueToTextFunction(std::move(valueToTextFunction)),
                  textToValueFunction(std::move(textToValueFunction)), defaultValue(defaultValue), value(defaultValue) {}

    explicit Parameter(AudioProcessorParameter *parameter)
        : AudioProcessorParameterWithID(parameter->getName(32), parameter->getName(32),
                                        parameter->getLabel(), parameter->getCategory()),
          sourceParameter(parameter),
          defaultValue(parameter->getDefaultValue()), value(parameter->getDefaultValue()),
          valueToTextFunction([this](float value) { return sourceParameter->getText(value, 4) + " " + sourceParameter->getLabel(); }),
          textToValueFunction([this](const String& text) { return sourceParameter->getValueForText(text); }) {
            sourceParameter->addListener(this);
        }

        ~Parameter() override {
            if (state.isValid())
                state.removeListener(this);
            if (sourceParameter != nullptr) {
                sourceParameter->removeListener(this);
            }
        }

        void parameterValueChanged(int parameterIndex, float newValue) override {
            setValue(newValue);
        }

        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

        float getValue() const override {
            return (float) range.convertTo0to1(getRawValue());
        }

        void setValue(float newValue) override {
            newValue = (float) range.snapToLegalValue(range.convertFrom0to1(newValue));

            if (value != newValue || listenersNeedCalling) {
                value = newValue;

                listeners.call ([=] (AudioProcessorValueTreeState::Listener& l) { l.parameterChanged (paramID, value); });
                listenersNeedCalling = false;

                needsUpdate = true;
            }
        }

        void setUnnormalisedValue (float newUnnormalisedValue) {
            if (value != newUnnormalisedValue) {
                setValueNotifyingHost((float) range.convertTo0to1(newUnnormalisedValue));
            }
        }

        void updateFromValueTree() {
            setUnnormalisedValue(state.getProperty(IDs::value, defaultValue));
        }

        float getRawValue() const {
            return state.getProperty(IDs::value, defaultValue);
        }

        Value getValueObject() {
            return state.getPropertyAsValue(IDs::value, undoManager);
        }

        void setNewState(const ValueTree& v, UndoManager *undoManager) {
            state = v;
            this->state.addListener(this);
            this->undoManager = undoManager;
            updateFromValueTree();
        }


        void copyValueToValueTree() {
            if (auto* valueProperty = state.getPropertyPointer(IDs::value)) {
                if ((float) *valueProperty != value) {
                    ScopedValueSetter<bool> svs(ignoreParameterChangedCallbacks, true);
                    state.setProperty(IDs::value, value, undoManager);
                }
            } else {
                state.setProperty(IDs::value, value, nullptr);
            }
        }

        void attachSlider(Slider *slider, Label *label=nullptr) {
            if (label != nullptr) {
                label->setText(name, dontSendNotification);
            }
            // referTo({}) can call these value functions to notify listeners, and they may refer to dead params.
            slider->textFromValueFunction = nullptr;
            slider->valueFromTextFunction = nullptr;
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
            return (float) range.convertTo0to1(defaultValue);
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

        NormalisableRange<double> range;
        std::function<String(const float)> valueToTextFunction;
        std::function<float(const String &)> textToValueFunction;
        float value, defaultValue;
        std::atomic<bool> needsUpdate { true };

        ValueTree state;
        UndoManager *undoManager { nullptr };

    private:
        AudioProcessorParameter *sourceParameter { nullptr };
        ListenerList<AudioProcessorValueTreeState::Listener> listeners;
        bool listenersNeedCalling { true };
        bool ignoreParameterChangedCallbacks = false;

        void valueTreePropertyChanged(ValueTree& tree, const Identifier& p) override {
            if (ignoreParameterChangedCallbacks)
                return;

            if (p == IDs::value) {
                auto value = float(tree[IDs::value]);
                setUnnormalisedValue(value);
                if (sourceParameter != nullptr) {
                    sourceParameter->setValue(value);
                }
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Parameter)
    };

    StatefulAudioProcessorWrapper(AudioPluginInstance *processor, ValueTree state, UndoManager &undoManager) :
            processor(processor), state(std::move(state)), undoManager(undoManager) {
        processor->enableAllBuses();
        updateValueTree();
        startTimerHz(10);
    }

    Parameter *getParameterObject(int parameterIndex) {
        if (parameterIndex < parameters.size()) {
            return parameters[parameterIndex];
        } else {
            return dynamic_cast<Parameter *>(processor->getParameters()[parameterIndex]);
        }
    }

    void updateValueTree() {
        for (auto parameter : processor->getParameters()) {
            if (auto *parameterObject = dynamic_cast<Parameter *>(parameter)) {
                parameterObject->setNewState(getOrCreateChildValueTree(parameterObject->paramID), &undoManager);
            } else {
                auto *p = new Parameter(parameter);
                p->setNewState(getOrCreateChildValueTree(p->paramID), &undoManager);
                parameters.add(p);
            }
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

    bool flushParameterValuesToValueTree() {
        ScopedLock lock(valueTreeChanging);

        bool anythingUpdated = false;

        if (!parameters.isEmpty()) {
            for (auto *ap : parameters) {
                bool needsUpdateTestValue = true;

                if (ap->needsUpdate.compare_exchange_strong(needsUpdateTestValue, false)) {
                    ap->copyValueToValueTree();
                    anythingUpdated = true;
                }
            }
        } else {
            for (auto *ap : processor->getParameters()) {
                if (auto *p = dynamic_cast<Parameter *>(ap)) {
                    bool needsUpdateTestValue = true;

                    if (p->needsUpdate.compare_exchange_strong(needsUpdateTestValue, false)) {
                        p->copyValueToValueTree();
                        anythingUpdated = true;
                    }
                }
            }
        }

        return anythingUpdated;

    }

    // TODO only one timer callback for all processors
    void timerCallback() override {
        auto anythingUpdated = flushParameterValuesToValueTree();
        startTimer(anythingUpdated ? 1000 / 50 : jlimit(50, 500, getTimerInterval() + 20));
    }

    AudioPluginInstance *processor;
    ValueTree state;
    UndoManager &undoManager;
    OwnedArray<Parameter> parameters;

    CriticalSection valueTreeChanging;
};
