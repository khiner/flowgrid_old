#include "StatefulAudioProcessorWrapper.h"

#include "DefaultAudioProcessor.h"
#include "DeviceManagerUtilities.h"

StatefulAudioProcessorWrapper::Parameter::Parameter(AudioProcessorParameter *parameter, StatefulAudioProcessorWrapper *processorWrapper)
        : AudioProcessorParameterWithID(parameter->getName(32), parameter->getName(32),
                                        parameter->getLabel(), parameter->getCategory()),
          sourceParameter(parameter),
          defaultValue(parameter->getDefaultValue()), value(parameter->getDefaultValue()),
          valueToTextFunction([this](float value) {
              return sourceParameter->getCurrentValueAsText() +
                     (sourceParameter->getLabel().isEmpty() ? "" : " " + sourceParameter->getLabel());
          }),
          textToValueFunction([this](const String &text) {
              const String &trimmedText = sourceParameter->getLabel().isEmpty()
                                          ? text
                                          : text.upToFirstOccurrenceOf(sourceParameter->getLabel(), false, true).trim();
              return range.snapToLegalValue(sourceParameter->getValueForText(trimmedText));
          }),
          processorWrapper(processorWrapper) {
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

StatefulAudioProcessorWrapper::Parameter::~Parameter() {
    listeners.call(&Listener::parameterWillBeDestroyed, this);

    if (state.isValid())
        state.removeListener(this);
    sourceParameter->removeListener(this);
    for (auto *label : attachedLabels) {
        label->onTextChange = nullptr;
    }
    attachedLabels.clear(false);
    for (auto *slider : attachedSliders) {
        slider->removeListener(this);
    }
    attachedSliders.clear(false);
    for (auto *button : attachedButtons) {
        button->removeListener(this);
    }
    attachedButtons.clear(false);
    for (auto *comboBox : attachedComboBoxes) {
        comboBox->removeListener(this);
    }
    attachedComboBoxes.clear(false);
    for (auto *parameterSwitch : attachedSwitches) {
        parameterSwitch->removeListener(this);
    }
    attachedSwitches.clear(false);
    for (auto *levelMeter : attachedParameterControls) {
        levelMeter->removeListener(this);
    }
    attachedParameterControls.clear(false);
}

void StatefulAudioProcessorWrapper::Parameter::setValue(float newValue) {
    ScopedValueSetter<bool> svs(ignoreCallbacks, true);
    newValue = range.convertFrom0to1(newValue);

    if (value != newValue || listenersNeedCalling) {
        value = newValue;
        postUnnormalizedValue(value);
        setAttachedComponentValues(value);
        listenersNeedCalling = false;
        needsUpdate = true;
    }
}

void StatefulAudioProcessorWrapper::Parameter::setUnnormalizedValue(float unnormalizedValue) {
    setValue(range.convertTo0to1(unnormalizedValue));
}

void StatefulAudioProcessorWrapper::Parameter::setAttachedComponentValues(float newValue) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    {
        ScopedValueSetter<bool> svs(ignoreCallbacks, true);
        for (auto *label : attachedLabels) {
            label->setText(valueToTextFunction(newValue), dontSendNotification);
        }
        for (auto *slider : attachedSliders) {
            slider->setValue(newValue, sendNotificationSync);
        }
        for (auto *button : attachedButtons) {
            button->setToggleState(newValue >= 0.5f, sendNotificationSync);
        }
        for (auto *comboBox : attachedComboBoxes) {
            auto index = roundToInt(newValue * (comboBox->getNumItems() - 1));
            comboBox->setSelectedItemIndex(index, sendNotificationSync);
        }
        for (auto *parameterSwitch : attachedSwitches) {
            auto index = roundToInt(newValue * (parameterSwitch->getNumItems() - 1));
            parameterSwitch->setSelectedItemIndex(index, sendNotificationSync);
        }
        for (auto *levelMeter : attachedParameterControls) {
            levelMeter->setValue(newValue, sendNotificationSync);
        }
    }
}

void StatefulAudioProcessorWrapper::Parameter::postUnnormalizedValue(float unnormalizedValue) {
    ScopedValueSetter<bool> svs(ignoreCallbacks, true);
    if (convertNormalizedToUnnormalized(sourceParameter->getValue()) != unnormalizedValue) {
        sourceParameter->setValueNotifyingHost(range.convertTo0to1(unnormalizedValue));
    }
}

void StatefulAudioProcessorWrapper::Parameter::updateFromValueTree() {
    const float unnormalizedValue = float(state.getProperty(TracksStateIDs::value, defaultValue));
    setUnnormalizedValue(unnormalizedValue);
}

void StatefulAudioProcessorWrapper::Parameter::setNewState(const ValueTree &v, UndoManager *undoManager) {
    state = v;
    this->undoManager = undoManager;
    this->state.addListener(this);
    updateFromValueTree();
    copyValueToValueTree();
}

void StatefulAudioProcessorWrapper::Parameter::copyValueToValueTree() {
    if (auto *valueProperty = state.getPropertyPointer(TracksStateIDs::value)) {
        if ((float) *valueProperty != value) {
            ScopedValueSetter<bool> svs(ignoreParameterChangedCallbacks, true);
            state.setProperty(TracksStateIDs::value, value, undoManager);
            // TODO uncomment and make it work
//                    if (!processorWrapper->isSelected()) {
            // If we're looking at something else, change the focus so we know what's changing.
//                        processorWrapper->select();
//                    }
        }
    } else {
        state.setProperty(TracksStateIDs::value, value, nullptr);
    }
}

void StatefulAudioProcessorWrapper::Parameter::attachLabel(Label *valueLabel) {
    if (valueLabel != nullptr) {
        attachedLabels.add(valueLabel);
        valueLabel->onTextChange = [this, valueLabel] { textChanged(valueLabel); };

        setAttachedComponentValues(value);
    }
}

void StatefulAudioProcessorWrapper::Parameter::detachLabel(Label *valueLabel) {
    if (valueLabel != nullptr)
        attachedLabels.removeObject(valueLabel, false);
}

static NormalisableRange<double> doubleRangeFromFloatRange(NormalisableRange<float> &floatRange) {
    return NormalisableRange<double>(floatRange.start, floatRange.end, floatRange.interval, floatRange.skew, floatRange.symmetricSkew);
}

void StatefulAudioProcessorWrapper::Parameter::attachSlider(Slider *slider) {
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

void StatefulAudioProcessorWrapper::Parameter::detachSlider(Slider *slider) {
    if (slider != nullptr) {
        slider->removeListener(this);
        slider->textFromValueFunction = nullptr;
        slider->valueFromTextFunction = nullptr;
        attachedSliders.removeObject(slider, false);
    }
}

void StatefulAudioProcessorWrapper::Parameter::attachButton(Button *button) {
    if (button != nullptr) {
        attachedButtons.add(button);
        button->addListener(this);

        setAttachedComponentValues(value);
    }
}

void StatefulAudioProcessorWrapper::Parameter::detachButton(Button *button) {
    if (button != nullptr) {
        button->removeListener(this);
        attachedButtons.removeObject(button, false);
    }
}

void StatefulAudioProcessorWrapper::Parameter::attachComboBox(ComboBox *comboBox) {
    if (comboBox != nullptr) {
        attachedComboBoxes.add(comboBox);
        comboBox->addListener(this);

        setAttachedComponentValues(value);
    }
}

void StatefulAudioProcessorWrapper::Parameter::detachComboBox(ComboBox *comboBox) {
    if (comboBox != nullptr) {
        comboBox->removeListener(this);
        attachedComboBoxes.removeObject(comboBox, false);
    }
}

void StatefulAudioProcessorWrapper::Parameter::attachSwitch(SwitchParameterComponent *parameterSwitch) {
    if (parameterSwitch != nullptr) {
        attachedSwitches.add(parameterSwitch);
        parameterSwitch->addListener(this);

        setAttachedComponentValues(value);
    }
}

void StatefulAudioProcessorWrapper::Parameter::detachSwitch(SwitchParameterComponent *parameterSwitch) {
    if (parameterSwitch != nullptr) {
        parameterSwitch->removeListener(this);
        attachedSwitches.removeObject(parameterSwitch, false);
    }
}

void StatefulAudioProcessorWrapper::Parameter::attachParameterControl(ParameterControl *parameterControl) {
    if (parameterControl != nullptr) {
        parameterControl->setNormalisableRange(range);
        attachedParameterControls.add(parameterControl);
        parameterControl->addListener(this);

        setAttachedComponentValues(value);
    }
}

void StatefulAudioProcessorWrapper::Parameter::detachParameterControl(ParameterControl *parameterControl) {
    if (parameterControl != nullptr) {
        parameterControl->removeListener(this);
        attachedParameterControls.removeObject(parameterControl, false);
    }
}

LevelMeterSource *StatefulAudioProcessorWrapper::Parameter::getLevelMeterSource() const {
    if (auto *defaultProcessor = dynamic_cast<DefaultAudioProcessor *>(processorWrapper->processor))
        if (sourceParameter == defaultProcessor->getMeteredParameter())
            return defaultProcessor->getMeterSource();

    return nullptr;
}

void StatefulAudioProcessorWrapper::Parameter::valueTreePropertyChanged(ValueTree &tree, const Identifier &p) {
    if (ignoreParameterChangedCallbacks) return;

    if (p == TracksStateIDs::value) {
        updateFromValueTree();
    }
}

void StatefulAudioProcessorWrapper::Parameter::textChanged(Label *valueLabel) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);

    auto newValue = convertNormalizedToUnnormalized(textToValueFunction(valueLabel->getText()));
    if (!ignoreCallbacks) {
        beginParameterChange();
        setUnnormalizedValue(newValue);
        endParameterChange();
    }
}

void StatefulAudioProcessorWrapper::Parameter::sliderValueChanged(Slider *slider) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);

    if (!ignoreCallbacks)
        setUnnormalizedValue((float) slider->getValue());
}

void StatefulAudioProcessorWrapper::Parameter::buttonClicked(Button *button) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);

    if (!ignoreCallbacks) {
        beginParameterChange();
        setUnnormalizedValue(button->getToggleState() ? 1.0f : 0.0f);
        endParameterChange();
    }
}

void StatefulAudioProcessorWrapper::Parameter::comboBoxChanged(ComboBox *comboBox) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);

    if (!ignoreCallbacks) {
        if (sourceParameter->getCurrentValueAsText() != comboBox->getText()) {
            beginParameterChange();
            setUnnormalizedValue(sourceParameter->getValueForText(comboBox->getText()));
            endParameterChange();
        }
    }
}

void StatefulAudioProcessorWrapper::Parameter::switchChanged(SwitchParameterComponent *parameterSwitch) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);

    if (!ignoreCallbacks) {
        beginParameterChange();
        float newValue = float(parameterSwitch->getSelectedItemIndex()) / float((parameterSwitch->getNumItems() - 1));
        setUnnormalizedValue(newValue);
        endParameterChange();
    }
}

void StatefulAudioProcessorWrapper::Parameter::parameterControlValueChanged(ParameterControl *control) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);

    if (!ignoreCallbacks)
        setUnnormalizedValue(control->getValue());
}

StatefulAudioProcessorWrapper::StatefulAudioProcessorWrapper(AudioPluginInstance *processor, AudioProcessorGraph::NodeID nodeId, ValueTree state, UndoManager &undoManager, AudioDeviceManager &deviceManager) :
        processor(processor), state(std::move(state)), undoManager(undoManager), deviceManager(deviceManager) {
    this->state.setProperty(ProcessorStateIDs::nodeId, int(nodeId.uid), nullptr);
    processor->enableAllBuses();
    if (auto *ioProcessor = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor *> (processor)) {
        if (ioProcessor->isInput()) {
            processor->setPlayConfigDetails(0, processor->getTotalNumOutputChannels(), processor->getSampleRate(), processor->getBlockSize());
        } else if (ioProcessor->isOutput()) {
            processor->setPlayConfigDetails(processor->getTotalNumInputChannels(), 0, processor->getSampleRate(), processor->getBlockSize());
        }
    }

    updateValueTree();
    processor->addListener(this);
}

StatefulAudioProcessorWrapper::~StatefulAudioProcessorWrapper() {
    processor->removeListener(this);
    automatableParameters.clear(false);
}

void StatefulAudioProcessorWrapper::updateValueTree() {
    for (auto parameter : processor->getParameters()) {
        auto *parameterWrapper = new Parameter(parameter, this);
        parameterWrapper->setNewState(getOrCreateChildValueTree(parameterWrapper->paramID), &undoManager);
        parameters.add(parameterWrapper);
        if (parameter->isAutomatable())
            automatableParameters.add(parameterWrapper);
    }
    audioProcessorChanged(processor, AudioProcessorListener::ChangeDetails().withParameterInfoChanged(true));
    // Also a little hacky, but maybe the best we can do.
    // If we're loading from state, bypass state needs to make its way to the processor graph to actually mute.
    state.sendPropertyChangeMessage(ProcessorStateIDs::bypassed);
}

ValueTree StatefulAudioProcessorWrapper::getOrCreateChildValueTree(const String &paramID) {
    ValueTree v(state.getChildWithProperty(TracksStateIDs::id, paramID));

    if (!v.isValid()) {
        v = ValueTree(TracksStateIDs::PARAM);
        v.setProperty(TracksStateIDs::id, paramID, nullptr);
        state.appendChild(v, nullptr);
    }

    return v;
}

bool StatefulAudioProcessorWrapper::flushParameterValuesToValueTree() {
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

void StatefulAudioProcessorWrapper::updateStateForProcessor(AudioProcessor *processor) {
    Array<Channel> newInputs, newOutputs;
    for (int i = 0; i < processor->getTotalNumInputChannels(); i++)
        newInputs.add({processor, deviceManager, i, true});
    if (processor->acceptsMidi())
        newInputs.add({processor, deviceManager, AudioProcessorGraph::midiChannelIndex, true});
    for (int i = 0; i < processor->getTotalNumOutputChannels(); i++)
        newOutputs.add({processor, deviceManager, i, false});
    if (processor->producesMidi())
        newOutputs.add({processor, deviceManager, AudioProcessorGraph::midiChannelIndex, false});

    ValueTree inputChannels = state.getChildWithName(InputChannelsStateIDs::INPUT_CHANNELS);
    ValueTree outputChannels = state.getChildWithName(OutputChannelsStateIDs::OUTPUT_CHANNELS);
    if (!inputChannels.isValid()) {
        inputChannels = ValueTree(InputChannelsStateIDs::INPUT_CHANNELS);
        state.appendChild(inputChannels, nullptr);
    }
    if (!outputChannels.isValid()) {
        outputChannels = ValueTree(OutputChannelsStateIDs::OUTPUT_CHANNELS);
        state.appendChild(outputChannels, nullptr);
    }

    Array<Channel> oldInputs, oldOutputs;
    for (int i = 0; i < inputChannels.getNumChildren(); i++) {
        const auto &channel = inputChannels.getChild(i);
        oldInputs.add({channel});
    }
    for (int i = 0; i < outputChannels.getNumChildren(); i++) {
        const auto &channel = outputChannels.getChild(i);
        oldOutputs.add({channel});
    }

    if (processor->acceptsMidi())
        state.setProperty(ProcessorStateIDs::acceptsMidi, true, nullptr);
    if (processor->producesMidi())
        state.setProperty(ProcessorStateIDs::producesMidi, true, nullptr);

    updateChannels(oldInputs, newInputs, inputChannels);
    updateChannels(oldOutputs, newOutputs, outputChannels);
}

void StatefulAudioProcessorWrapper::updateChannels(Array<Channel> &oldChannels, Array<Channel> &newChannels, ValueTree &channelsState) {
    for (int i = 0; i < oldChannels.size(); i++) {
        const auto &oldChannel = oldChannels.getUnchecked(i);
        if (!newChannels.contains(oldChannel)) {
            channelsState.removeChild(channelsState.getChildWithProperty(TrackStateIDs::name, oldChannel.name), &undoManager);
        }
    }
    for (int i = 0; i < newChannels.size(); i++) {
        const auto &newChannel = newChannels.getUnchecked(i);
        if (!oldChannels.contains(newChannel)) {
            channelsState.addChild(newChannel.toState(), i, &undoManager);
        }
    }
}

void StatefulAudioProcessorWrapper::audioProcessorChanged(AudioProcessor *processor, const AudioProcessorListener::ChangeDetails &details) {
    if (processor == nullptr) return;

    if (MessageManager::getInstance()->isThisTheMessageThread())
        updateStateForProcessor(processor);
    else
        MessageManager::callAsync([this, processor] { updateStateForProcessor(processor); });
}

StatefulAudioProcessorWrapper::Channel::Channel(AudioProcessor *processor, AudioDeviceManager &deviceManager, int channelIndex, bool isInput) :
        channelIndex(channelIndex) {
    if (processor->getName() == "Audio Input" || processor->getName() == "Audio Output") {
        name = DeviceManagerUtilities::getAudioChannelName(deviceManager, channelIndex, processor->getName() == "Audio Input");
        abbreviatedName = name;
    } else {
        if (channelIndex == AudioProcessorGraph::midiChannelIndex) {
            name = isInput ? "MIDI Input" : "MIDI Output";
            abbreviatedName = isInput ? "MIDI In" : "MIDI Out";
        } else {
            int busIndex = 0;
            auto channel = processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIndex);
            if (auto *bus = processor->getBus(isInput, busIndex)) {
                abbreviatedName = AudioChannelSet::getAbbreviatedChannelTypeName(bus->getCurrentLayout().getTypeOfChannel(channel));
                name = bus->getName() + ": " + abbreviatedName;
            } else {
                name = (isInput ? "Main Input: " : "Main Output: ") + String(channelIndex + 1);
                abbreviatedName = (isInput ? "Main In: " : "Main Out: ") + String(channelIndex + 1);
            }
        }
    }
}

StatefulAudioProcessorWrapper::Channel::Channel(const ValueTree &channelState) :
        channelIndex(channelState[ChannelStateIDs::channelIndex]),
        name(channelState[TrackStateIDs::name]),
        abbreviatedName(channelState[ChannelStateIDs::abbreviatedName]) {
}
