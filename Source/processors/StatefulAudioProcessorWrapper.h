#pragma once

#include <Utilities.h>
#include "Identifiers.h"

class StatefulAudioProcessorWrapper : private AudioProcessorListener, private Timer {
public:
    struct Parameter
            : public AudioProcessorParameterWithID,
              private Utilities::ValueTreePropertyChangeListener,
              public AudioProcessorParameter::Listener,
              public Slider::Listener {
        explicit Parameter(AudioProcessorParameter *parameter)
                : AudioProcessorParameterWithID(parameter->getName(32), parameter->getName(32),
                                                parameter->getLabel(), parameter->getCategory()),
                  sourceParameter(parameter),
                  defaultValue(parameter->getDefaultValue()), value(parameter->getDefaultValue()),
                  valueToTextFunction([this](float value) { return sourceParameter->getCurrentValueAsText(); }),
                  textToValueFunction([this](const String &text) { return sourceParameter->getValueForText(text); }) {
            if (auto *p = dynamic_cast<AudioParameterFloat *>(sourceParameter)) {
                range = p->range;
            } else {
                if (sourceParameter->getNumSteps() != AudioProcessor::getDefaultNumParameterSteps())
                    range = NormalisableRange<float>(0.0, 1.0, 1.0f / (sourceParameter->getNumSteps() - 1.0f));
                else
                    range = NormalisableRange<float>(0.0, 1.0);
            }
            value = defaultValue = range.convertFrom0to1(parameter->getDefaultValue());
            sourceParameter->addListener(this);
        }

        ~Parameter() override {
            if (state.isValid())
                state.removeListener(this);
            sourceParameter->removeListener(this);
            for (auto* slider : attachedSliders) {
                detachSlider(slider);
            }
        }

        void parameterValueChanged(int parameterIndex, float newValue) override {
            setValue(newValue);
        }

        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

        float getValue() const override {
            return range.convertTo0to1(value);
        }

        void setValue(float newValue) override {
            newValue = range.snapToLegalValue(range.convertFrom0to1(newValue));

            if (value != newValue || listenersNeedCalling) {
                value = newValue;
                setAttachedComponentValues(value);
                listenersNeedCalling = false;
                needsUpdate = true;
            }
        }

        void setAttachedComponentValues(float newValue) {
            const ScopedLock selfCallbackLock(selfCallbackMutex);
            {
                ScopedValueSetter<bool> svs (ignoreCallbacks, true);
                for (auto* slider : attachedSliders) {
                    slider->setValue(newValue, sendNotificationSync);
                }
            }
        }

        void setUnnormalisedValue(float newUnnormalisedValue) {
            if (value != newUnnormalisedValue) {
                setValue(range.convertTo0to1(newUnnormalisedValue));
            }
        }

        void postUnnormalisedValue(float unnormalisedValue) {
            setUnnormalisedValue(unnormalisedValue);
            if (range.convertFrom0to1(sourceParameter->getValue()) != unnormalisedValue)
                sourceParameter->setValueNotifyingHost(range.convertTo0to1(unnormalisedValue));
        }

        void updateFromValueTree() {
            const float unnormalisedValue = float(state.getProperty(IDs::value, defaultValue));
            postUnnormalisedValue(unnormalisedValue);
        }

        void setNewState(const ValueTree &v, UndoManager *undoManager) {
            state = v;
            this->undoManager = undoManager;
            this->state.addListener(this);
            updateFromValueTree();
            copyValueToValueTree();
        }

        void copyValueToValueTree() {
            if (auto *valueProperty = state.getPropertyPointer(IDs::value)) {
                if ((float) *valueProperty != value) {
                    ScopedValueSetter<bool> svs(ignoreParameterChangedCallbacks, true);
                    state.setProperty(IDs::value, value, undoManager);
                }
            } else {
                state.setProperty(IDs::value, value, nullptr);
            }
        }

        void attachSlider(Slider *slider, Label *name=nullptr, Label *label=nullptr) {
            if (name != nullptr) {
                name->setText(this->name, dontSendNotification);
            }
            if (label != nullptr) {
                label->setText(this->label, dontSendNotification);
            }

            // referTo({}) can call these value functions to notify listeners, and they may refer to dead params.
            slider->setNormalisableRange(doubleRangeFromFloatRange(range));
            slider->textFromValueFunction = valueToTextFunction;
            slider->valueFromTextFunction = textToValueFunction;
            attachedSliders.add(slider);
            setAttachedComponentValues(value);
            slider->addListener(this);
        }

        void detachSlider(Slider *slider) {
            slider->removeListener(this);
            slider->textFromValueFunction = nullptr;
            slider->valueFromTextFunction = nullptr;
            attachedSliders.removeObject(slider, false);
        }

        float getDefaultValue() const override {
            return range.convertTo0to1(defaultValue);
        }

        float getValueForText(const String &text) const override {
            return range.convertTo0to1(
                    textToValueFunction != nullptr ? textToValueFunction(text) : text.getFloatValue());
        }

        static NormalisableRange<double> doubleRangeFromFloatRange(NormalisableRange<float> &floatRange) {
            return NormalisableRange<double>(floatRange.start, floatRange.end, floatRange.interval, floatRange.skew,
                                             floatRange.symmetricSkew);
        }

        AudioProcessorParameter *sourceParameter{nullptr};
        float defaultValue, value;
        std::function<String(const float)> valueToTextFunction;
        std::function<float(const String &)> textToValueFunction;
        NormalisableRange<float> range;

        std::atomic<bool> needsUpdate{true};
        ValueTree state;
        UndoManager *undoManager{nullptr};

    private:
        bool listenersNeedCalling{true};
        bool ignoreParameterChangedCallbacks = false;

        bool ignoreCallbacks { false };
        CriticalSection selfCallbackMutex;

        OwnedArray<Slider> attachedSliders {};

        void valueTreePropertyChanged(ValueTree &tree, const Identifier &p) override {
            if (ignoreParameterChangedCallbacks)
                return;

            if (p == IDs::value) {
                updateFromValueTree();
            }
        }

        void sliderValueChanged(Slider* s) override {
            const ScopedLock selfCallbackLock (selfCallbackMutex);

            if (!ignoreCallbacks)
                postUnnormalisedValue((float) s->getValue());
        }

        void beginParameterChange() {
            if (undoManager != nullptr)
                undoManager->beginNewTransaction();
            sourceParameter->beginChangeGesture();
        }

        void endParameterChange()
        {
            sourceParameter->endChangeGesture();
        }

        void sliderDragStarted (Slider* slider) override { beginParameterChange(); }
        void sliderDragEnded   (Slider* slider) override { endParameterChange();   }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Parameter)
    };

    StatefulAudioProcessorWrapper(AudioPluginInstance *processor, ValueTree state, UndoManager &undoManager) :
            processor(processor), state(std::move(state)), undoManager(undoManager) {
        processor->addListener(this);
        processor->enableAllBuses();
        audioProcessorChanged(processor);
        updateValueTree();
        startTimerHz(10);
    }

    Parameter *getParameter(int parameterIndex) {
        return parameters[parameterIndex];
    }

    void updateValueTree() {
        for (auto parameter : processor->getParameters()) {
            auto *parameterWrapper = new Parameter(parameter);
            parameterWrapper->setNewState(getOrCreateChildValueTree(parameterWrapper->paramID), &undoManager);
            parameters.add(parameterWrapper);
        }

        state.setProperty(IDs::numInputChannels, processor->getTotalNumInputChannels(), &undoManager);
        state.setProperty(IDs::numOutputChannels, processor->getTotalNumOutputChannels(), &undoManager);
    }

    ValueTree getOrCreateChildValueTree(const String &paramID) {
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

        for (auto *ap : parameters) {
            bool needsUpdateTestValue = true;

            if (ap->needsUpdate.compare_exchange_strong(needsUpdateTestValue, false)) {
                ap->copyValueToValueTree();
                anythingUpdated = true;
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
private:
    UndoManager &undoManager;
    OwnedArray<Parameter> parameters;

    CriticalSection valueTreeChanging;

    void audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) override {}

    void audioProcessorChanged(AudioProcessor *processor) override {
        MessageManager::callAsync([this, processor] {
            if (processor != nullptr) {
                // TODO should we use UndoManager and also support _setting_ playConfigDetails on state change?
                state.setProperty(IDs::numInputChannels, processor->getTotalNumInputChannels(), nullptr);
                state.setProperty(IDs::numOutputChannels, processor->getTotalNumOutputChannels(), nullptr);
                state.setProperty(IDs::acceptsMidi, processor->acceptsMidi(), nullptr);
                state.setProperty(IDs::producesMidi, processor->producesMidi(), nullptr);
            }
        });
    }

    void audioProcessorParameterChangeGestureBegin(AudioProcessor *processor, int parameterIndex) override {}

    void audioProcessorParameterChangeGestureEnd(AudioProcessor *processor, int parameterIndex) override {}
};
