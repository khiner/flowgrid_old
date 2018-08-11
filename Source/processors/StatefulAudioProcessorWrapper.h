#include <utility>

#pragma once

#include <Utilities.h>
#include "Identifiers.h"
#include "view/processor_editor/SwitchParameterComponent.h"
#include "Project.h"

class StatefulAudioProcessorWrapper : private AudioProcessorListener {
public:
    struct Parameter
            : public AudioProcessorParameterWithID,
              private Utilities::ValueTreePropertyChangeListener,
              public AudioProcessorParameter::Listener, public Slider::Listener, public Button::Listener,
              public ComboBox::Listener, public SwitchParameterComponent::Listener, public LevelMeter::Listener {
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
                else if (!sourceParameter->getAllValueStrings().isEmpty())
                    range = NormalisableRange<float>(0.0, 1.0, 1.0f / (sourceParameter->getAllValueStrings().size() - 1.0f));
                else
                    range = NormalisableRange<float>(0.0, 1.0);

            }
            value = defaultValue = convertNormalizedToUnnormalized(parameter->getDefaultValue());
            sourceParameter->addListener(this);
        }

        ~Parameter() override {
            if (state.isValid())
                state.removeListener(this);
            sourceParameter->removeListener(this);
            for (auto* label : attachedLabels) {
                label->onTextChange = nullptr;
            }
            attachedLabels.clear(false);
            for (auto* slider : attachedSliders) {
                slider->removeListener(this);
            }
            attachedSliders.clear(false);
            for (auto* button : attachedButtons) {
                button->removeListener(this);
            }
            attachedButtons.clear(false);
            for (auto* comboBox : attachedComboBoxes) {
                comboBox->removeListener(this);
            }
            attachedComboBoxes.clear(false);
            for (auto* parameterSwitch : attachedSwitches) {
                parameterSwitch->removeListener(this);
            }
            attachedSwitches.clear(false);
        }

        String getText(float v, int length) const override {
            return valueToTextFunction != nullptr ?
                   valueToTextFunction(convertNormalizedToUnnormalized(v)) :
                   AudioProcessorParameter::getText(v, length);
        }

        void parameterValueChanged(int parameterIndex, float newValue) override {
            if (!ignoreCallbacks) { setValue(newValue); }
        }

        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

        float convertNormalizedToUnnormalized(float value) const {
            return range.snapToLegalValue(range.convertFrom0to1(value));
        }

        float getValue() const override {
            return range.convertTo0to1(value);
        }

        void setValue(float newValue) override {
            ScopedValueSetter<bool> svs(ignoreCallbacks, true);
            newValue = range.convertFrom0to1(newValue);

            if (value != newValue || listenersNeedCalling) {
                value = newValue;
                postUnnormalisedValue(value);
                setAttachedComponentValues(value);
                listenersNeedCalling = false;
                needsUpdate = true;
            }
        }

        void setUnnormalizedValue(float unnormalisedValue) {
            setValue(range.convertTo0to1(unnormalisedValue));
        }

        void setAttachedComponentValues(float newValue) {
            const ScopedLock selfCallbackLock(selfCallbackMutex);
            {
                ScopedValueSetter<bool> svs(ignoreCallbacks, true);
                for (auto* label : attachedLabels) {
                    label->setText(valueToTextFunction(newValue), dontSendNotification);
                }
                for (auto* slider : attachedSliders) {
                    slider->setValue(newValue, sendNotificationSync);
                }
                for (auto* button : attachedButtons) {
                    button->setToggleState(newValue >= 0.5f, sendNotificationSync);
                }
                for (auto* comboBox : attachedComboBoxes) {
                    auto index = roundToInt(newValue * (comboBox->getNumItems() - 1));
                    comboBox->setSelectedItemIndex(index, sendNotificationSync);
                }
                for (auto* parameterSwitch : attachedSwitches) {
                    auto index = roundToInt(newValue * (parameterSwitch->getNumItems() - 1));
                    parameterSwitch->setSelectedItemIndex(index, sendNotificationSync);
                }
                for (auto* levelMeter : attachedLevelMeters) {
                    levelMeter->setValue(newValue, sendNotificationSync);
                }
            }
        }

        void postUnnormalisedValue(float unnormalisedValue) {
            ScopedValueSetter<bool> svs(ignoreCallbacks, true);
            if (convertNormalizedToUnnormalized(sourceParameter->getValue()) != unnormalisedValue) {
                sourceParameter->setValueNotifyingHost(range.convertTo0to1(unnormalisedValue));
            }
        }

        void updateFromValueTree() {
            const float unnormalisedValue = float(state.getProperty(IDs::value, defaultValue));
            setUnnormalizedValue(unnormalisedValue);
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

        void attachLabel(Label *valueLabel) {
            if (valueLabel != nullptr) {
                attachedLabels.add(valueLabel);
                valueLabel->onTextChange = [this, valueLabel] { textChanged(valueLabel); };

                setAttachedComponentValues(value);
            }
        }

        void detachLabel(Label *valueLabel) {
            if (valueLabel != nullptr)
                attachedLabels.removeObject(valueLabel, false);
        }

        void attachSlider(Slider *slider) {
            if (slider != nullptr) {
                slider->textFromValueFunction = nullptr;
                slider->valueFromTextFunction = nullptr;
                slider->setNormalisableRange(doubleRangeFromFloatRange(range));
                slider->textFromValueFunction = valueToTextFunction;
                slider->valueFromTextFunction = textToValueFunction;
                attachedSliders.add(slider);
                slider->addListener(this);

                setAttachedComponentValues(value);
            }
        }

        void detachSlider(Slider *slider) {
            if (slider != nullptr) {
                slider->removeListener(this);
                slider->textFromValueFunction = nullptr;
                slider->valueFromTextFunction = nullptr;
                attachedSliders.removeObject(slider, false);
            }
        }

        void attachButton(Button *button) {
            if (button != nullptr) {
                attachedButtons.add(button);
                button->addListener(this);

                setAttachedComponentValues(value);
            }
        }

        void detachButton(Button *button) {
            if (button != nullptr) {
                button->removeListener(this);
                attachedButtons.removeObject(button, false);
            }
        }

        void attachComboBox(ComboBox *comboBox) {
            if (comboBox != nullptr) {
                attachedComboBoxes.add(comboBox);
                comboBox->addListener(this);

                setAttachedComponentValues(value);
            }
        }

        void detachComboBox(ComboBox *comboBox) {
            if (comboBox != nullptr) {
                comboBox->removeListener(this);
                attachedComboBoxes.removeObject(comboBox, false);
            }
        }

        void attachSwitch(SwitchParameterComponent *parameterSwitch) {
            if (parameterSwitch != nullptr) {
                attachedSwitches.add(parameterSwitch);
                parameterSwitch->addListener(this);

                setAttachedComponentValues(value);
            }
        }

        void detachSwitch(SwitchParameterComponent *parameterSwitch) {
            if (parameterSwitch != nullptr) {
                parameterSwitch->removeListener(this);
                attachedSwitches.removeObject(parameterSwitch, false);
            }
        }

        void attachLevelMeter(LevelMeter *levelMeter) {
            if (levelMeter != nullptr) {
                attachedLevelMeters.add(levelMeter);
                levelMeter->addListener(this);

                setAttachedComponentValues(value);
            }
        }

        void detachLevelMeter(LevelMeter *levelMeter) {
            if (levelMeter != nullptr) {
                levelMeter->removeListener(this);
                attachedLevelMeters.removeObject(levelMeter, false);
            }
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

        OwnedArray<Label> attachedLabels {};
        OwnedArray<Slider> attachedSliders {};
        OwnedArray<Button> attachedButtons {};
        OwnedArray<ComboBox> attachedComboBoxes {};
        OwnedArray<SwitchParameterComponent> attachedSwitches {};
        OwnedArray<LevelMeter> attachedLevelMeters {};

        void valueTreePropertyChanged(ValueTree &tree, const Identifier &p) override {
            if (ignoreParameterChangedCallbacks)
                return;

            if (p == IDs::value) {
                updateFromValueTree();
            }
        }

        void textChanged(Label *valueLabel) {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            auto newValue = convertNormalizedToUnnormalized(textToValueFunction(valueLabel->getText()));
            if (!ignoreCallbacks) {
                beginParameterChange();
                setUnnormalizedValue(newValue);
                endParameterChange();
            }
        }

        void sliderValueChanged(Slider* slider) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks)
                setUnnormalizedValue((float) slider->getValue());
        }

        void buttonClicked(Button* button) override {
            const ScopedLock selfCallbackLock (selfCallbackMutex);

            if (!ignoreCallbacks) {
                beginParameterChange();
                setUnnormalizedValue(button->getToggleState() ? 1.0f : 0.0f);
                endParameterChange();
            }
        }

        void comboBoxChanged(ComboBox* comboBox) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks) {
                if (sourceParameter->getCurrentValueAsText() != comboBox->getText()) {
                    beginParameterChange();
                    setUnnormalizedValue(sourceParameter->getValueForText(comboBox->getText()));
                    endParameterChange();
                }
            }
        }

        void switchChanged(SwitchParameterComponent* parameterSwitch) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks) {
                beginParameterChange();
                float newValue = float(parameterSwitch->getSelectedItemIndex()) / float((parameterSwitch->getNumItems() - 1));
                setUnnormalizedValue(newValue);
                endParameterChange();
            }
        }

        void levelMeterValueChanged(LevelMeter* levelMeter) override {
            const ScopedLock selfCallbackLock(selfCallbackMutex);

            if (!ignoreCallbacks)
                setUnnormalizedValue(levelMeter->getValue());
        }

        void beginParameterChange() {
            if (undoManager != nullptr)
                undoManager->beginNewTransaction();
            sourceParameter->beginChangeGesture();
        }

        void endParameterChange() {
            sourceParameter->endChangeGesture();
        }

        void sliderDragStarted (Slider* slider) override { beginParameterChange(); }
        void sliderDragEnded   (Slider* slider) override { endParameterChange();   }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Parameter)
    };

    StatefulAudioProcessorWrapper(AudioPluginInstance *processor, AudioProcessorGraph::NodeID nodeId, ValueTree state, Project& project, UndoManager &undoManager) :
            processor(processor), state(std::move(state)), project(project), undoManager(undoManager) {
        this->state.setProperty(IDs::nodeId, int(nodeId), nullptr);
        processor->enableAllBuses();
        if (auto* ioProcessor = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor*> (processor)) {
            if (ioProcessor->isInput()) {
                processor->setPlayConfigDetails(0, processor->getTotalNumOutputChannels(), processor->getSampleRate(), processor->getBlockSize());
            } else if (ioProcessor->isOutput()) {
                processor->setPlayConfigDetails(processor->getTotalNumInputChannels(), 0, processor->getSampleRate(), processor->getBlockSize());
            }
        }

        updateValueTree();
        processor->addListener(this);
    }

    ~StatefulAudioProcessorWrapper() override {
        processor->removeListener(this);
        automatableParameters.clear(false);
    }

    int getNumParameters() { return parameters.size(); }

    Parameter *getParameter(int parameterIndex) { return parameters[parameterIndex]; }

    Parameter *getAutomatableParameter(int parameterIndex) {
        return automatableParameters[parameterIndex];
    }

    void updateValueTree() {
        for (auto parameter : processor->getParameters()) {
            auto *parameterWrapper = new Parameter(parameter);
            parameterWrapper->setNewState(getOrCreateChildValueTree(parameterWrapper->paramID), &undoManager);
            parameters.add(parameterWrapper);
            if (parameter->isAutomatable())
                automatableParameters.add(parameterWrapper);
        }
        audioProcessorChanged(processor);
        // Also a little hacky, but maybe the best we can do.
        // If we're loading from state, bypass state needs to make its way to the processor graph to actually mute.
        state.sendPropertyChangeMessage(IDs::bypassed);
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

    AudioPluginInstance *processor;
    ValueTree state;
private:
    Project &project;
    UndoManager &undoManager;
    OwnedArray<Parameter> parameters;
    OwnedArray<Parameter> automatableParameters;

    CriticalSection valueTreeChanging;

    void updateStateForProcessor(AudioProcessor *processor) {
        Array<String> newInputs, newOutputs;
        for (int i = 0; i < processor->getTotalNumInputChannels(); i++)
            newInputs.add(getChannelName(i, true));
        if (processor->acceptsMidi())
            newInputs.add(getChannelName(AudioProcessorGraph::midiChannelIndex, true));
        for (int i = 0; i < processor->getTotalNumOutputChannels(); i++)
            newOutputs.add(getChannelName(i, false));
        if (processor->producesMidi())
            newOutputs.add(getChannelName(AudioProcessorGraph::midiChannelIndex, false));

        ValueTree inputChannels = state.getChildWithName(IDs::INPUT_CHANNELS);
        ValueTree outputChannels = state.getChildWithName(IDs::OUTPUT_CHANNELS);
        if (!inputChannels.isValid()) {
            inputChannels = ValueTree(IDs::INPUT_CHANNELS);
            state.addChild(inputChannels, -1, nullptr);
        }
        if (!outputChannels.isValid()) {
            outputChannels = ValueTree(IDs::OUTPUT_CHANNELS);
            state.addChild(outputChannels, -1, nullptr);
        }

        Array<String> oldInputs, oldOutputs;
        for (int i = 0; i < inputChannels.getNumChildren(); i++) {
            const auto& channel = inputChannels.getChild(i);
            oldInputs.add(channel[IDs::name]);
        }
        for (int i = 0; i < outputChannels.getNumChildren(); i++) {
            const auto& channel = outputChannels.getChild(i);
            oldOutputs.add(channel[IDs::name]);
        }

        if (processor->acceptsMidi())
            state.setProperty(IDs::acceptsMidi, true, nullptr);
        if (processor->producesMidi())
            state.setProperty(IDs::producesMidi, true, nullptr);

        updateChannels(oldInputs, newInputs, inputChannels, true);
        updateChannels(oldOutputs, newOutputs, outputChannels, false);
    }

    void updateChannels(Array<String>& oldChannels, Array<String>& newChannels, ValueTree& channelsState, bool isInput) {
        for (int i = 0; i < oldChannels.size(); i++) {
            const auto& oldChannel = oldChannels.getUnchecked(i);
            if (!newChannels.contains(oldChannel)) {
                channelsState.removeChild(channelsState.getChildWithProperty(IDs::name, oldChannel), &undoManager);
            }
        }
        for (int i = 0; i < newChannels.size(); i++) {
            const auto& newChannel = newChannels.getUnchecked(i);
            if (!oldChannels.contains(newChannel)) {
                ValueTree channelState(IDs::CHANNEL);
                channelState.setProperty(IDs::name, newChannel, nullptr);
                channelsState.addChild(channelState, i, &undoManager);
            }
        }
    }

    String getChannelName(int channelIndex, bool isInput) {
        bool isMidi = channelIndex == AudioProcessorGraph::midiChannelIndex;
        String channelName;
        if (processor->getName() == "Audio Input" || processor->getName() == "Audio Output") {
            channelName = project.getAudioChannelName(channelIndex, processor->getName() == "Audio Input");
        } else {
            int busIndex = 0;
            if (isMidi) {
                channelName = isInput ? "MIDI Input" : "MIDI Output";
            } else {
                auto channel = processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIndex);
                if (auto *bus = processor->getBus(isInput, busIndex))
                    channelName = bus->getName() + ": " + AudioChannelSet::getAbbreviatedChannelTypeName(
                            bus->getCurrentLayout().getTypeOfChannel(channel));
                else
                    channelName = (isInput ? "Main Input: " : "Main Output: ") + String(channelIndex + 1);
            }
        }
        return channelName;
    }

    void audioProcessorParameterChanged(AudioProcessor *processor, int parameterIndex, float newValue) override {}

    void audioProcessorChanged(AudioProcessor *processor) override {
        if (processor == nullptr)
            return;
        if (MessageManager::getInstance()->isThisTheMessageThread())
            updateStateForProcessor(processor);
        else
            MessageManager::callAsync([this, processor] { updateStateForProcessor(processor); });
    }

    void audioProcessorParameterChangeGestureBegin(AudioProcessor *processor, int parameterIndex) override {}

    void audioProcessorParameterChangeGestureEnd(AudioProcessor *processor, int parameterIndex) override {}
};
