#include "StatefulAudioProcessorWrapper.h"

#include "DefaultAudioProcessor.h"

StatefulAudioProcessorWrapper::Parameter::Parameter(AudioProcessorParameter *parameter, StatefulAudioProcessorWrapper *processorWrapper)
        : AudioProcessorParameterWithID(parameter->getName(32), parameter->getName(32),
                                        parameter->getLabel(), parameter->getCategory()),
          sourceParameter(parameter),
          defaultValue(parameter->getDefaultValue()), value(parameter->getDefaultValue()),
          valueToTextFunction([this](float value) {
              return sourceParameter->getCurrentValueAsText() + (sourceParameter->getLabel().isEmpty() ? "" : " " + sourceParameter->getLabel());
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
    if (state.isValid()) state.removeListener(this);
    listeners.call(&Listener::parameterWillBeDestroyed, this);

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
    setUnnormalizedValue(Param::getValue(state, defaultValue));
}

void StatefulAudioProcessorWrapper::Parameter::setNewState(const ValueTree &v, UndoManager *undoManager) {
    state = v;
    this->undoManager = undoManager;
    this->state.addListener(this);
    updateFromValueTree();
    copyValueToValueTree();
}

void StatefulAudioProcessorWrapper::Parameter::copyValueToValueTree() {
    if (auto *valueProperty = state.getPropertyPointer(ParamIDs::value)) {
        if ((float) *valueProperty != value) {
            ScopedValueSetter<bool> svs(ignoreParameterChangedCallbacks, true);
            state.setProperty(ParamIDs::value, value, undoManager);
            // TODO uncomment and make it work
//                    if (!processorWrapper->isSelected()) {
            // If we're looking at something else, change the focus so we know what's changing.
//                        processorWrapper->select();
//                    }
        }
    } else {
        Param::setValue(state, value);
    }
}

void StatefulAudioProcessorWrapper::Parameter::attachLabel(Label *valueLabel) {
    if (valueLabel == nullptr) return;

    attachedLabels.add(valueLabel);
    valueLabel->onTextChange = [this, valueLabel] { textChanged(valueLabel); };

    setAttachedComponentValues(value);
}

void StatefulAudioProcessorWrapper::Parameter::detachLabel(Label *valueLabel) {
    if (valueLabel == nullptr) return;

    attachedLabels.removeObject(valueLabel, false);
}

static NormalisableRange<double> doubleRangeFromFloatRange(NormalisableRange<float> &floatRange) {
    return NormalisableRange<double>(floatRange.start, floatRange.end, floatRange.interval, floatRange.skew, floatRange.symmetricSkew);
}

void StatefulAudioProcessorWrapper::Parameter::attachSlider(Slider *slider) {
    if (slider == nullptr) return;

    slider->textFromValueFunction = nullptr;
    slider->valueFromTextFunction = nullptr;
    slider->setNormalisableRange(doubleRangeFromFloatRange(range));
    slider->textFromValueFunction = valueToTextFunction;
    slider->valueFromTextFunction = textToValueFunction;
    attachedSliders.add(slider);
    slider->addListener(this);

    setAttachedComponentValues(value);
}

void StatefulAudioProcessorWrapper::Parameter::detachSlider(Slider *slider) {
    if (slider == nullptr) return;

    slider->removeListener(this);
    slider->textFromValueFunction = nullptr;
    slider->valueFromTextFunction = nullptr;
    attachedSliders.removeObject(slider, false);
}

void StatefulAudioProcessorWrapper::Parameter::attachButton(Button *button) {
    if (button == nullptr) return;

    attachedButtons.add(button);
    button->addListener(this);

    setAttachedComponentValues(value);
}

void StatefulAudioProcessorWrapper::Parameter::detachButton(Button *button) {
    if (button == nullptr) return;

    button->removeListener(this);
    attachedButtons.removeObject(button, false);
}

void StatefulAudioProcessorWrapper::Parameter::attachComboBox(ComboBox *comboBox) {
    if (comboBox == nullptr) return;

    attachedComboBoxes.add(comboBox);
    comboBox->addListener(this);

    setAttachedComponentValues(value);
}

void StatefulAudioProcessorWrapper::Parameter::detachComboBox(ComboBox *comboBox) {
    if (comboBox == nullptr) return;

    comboBox->removeListener(this);
    attachedComboBoxes.removeObject(comboBox, false);
}

void StatefulAudioProcessorWrapper::Parameter::attachSwitch(SwitchParameterComponent *parameterSwitch) {
    if (parameterSwitch == nullptr) return;

    attachedSwitches.add(parameterSwitch);
    parameterSwitch->addListener(this);

    setAttachedComponentValues(value);
}

void StatefulAudioProcessorWrapper::Parameter::detachSwitch(SwitchParameterComponent *parameterSwitch) {
    if (parameterSwitch == nullptr) return;

    parameterSwitch->removeListener(this);
    attachedSwitches.removeObject(parameterSwitch, false);
}

void StatefulAudioProcessorWrapper::Parameter::attachParameterControl(ParameterControl *parameterControl) {
    if (parameterControl == nullptr) return;

    parameterControl->setNormalisableRange(range);
    attachedParameterControls.add(parameterControl);
    parameterControl->addListener(this);

    setAttachedComponentValues(value);
}

void StatefulAudioProcessorWrapper::Parameter::detachParameterControl(ParameterControl *parameterControl) {
    if (parameterControl == nullptr) return;

    parameterControl->removeListener(this);
    attachedParameterControls.removeObject(parameterControl, false);
}

LevelMeterSource *StatefulAudioProcessorWrapper::Parameter::getLevelMeterSource() const {
    if (auto *defaultProcessor = dynamic_cast<DefaultAudioProcessor *>(processorWrapper->audioProcessor))
        if (sourceParameter == defaultProcessor->getMeteredParameter())
            return defaultProcessor->getMeterSource();

    return nullptr;
}

void StatefulAudioProcessorWrapper::Parameter::valueTreePropertyChanged(ValueTree &tree, const Identifier &p) {
    if (ignoreParameterChangedCallbacks) return;
    if (p == ParamIDs::value) {
        updateFromValueTree();
    }
}

void StatefulAudioProcessorWrapper::Parameter::textChanged(Label *valueLabel) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    if (ignoreCallbacks) return;

    beginParameterChange();
    auto newValue = convertNormalizedToUnnormalized(textToValueFunction(valueLabel->getText()));
    setUnnormalizedValue(newValue);
    endParameterChange();
}

void StatefulAudioProcessorWrapper::Parameter::sliderValueChanged(Slider *slider) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    if (ignoreCallbacks) return;

    setUnnormalizedValue((float) slider->getValue());
}

void StatefulAudioProcessorWrapper::Parameter::buttonClicked(Button *button) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    if (ignoreCallbacks) return;

    beginParameterChange();
    setUnnormalizedValue(button->getToggleState() ? 1.0f : 0.0f);
    endParameterChange();
}

void StatefulAudioProcessorWrapper::Parameter::comboBoxChanged(ComboBox *comboBox) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    if (ignoreCallbacks || sourceParameter->getCurrentValueAsText() == comboBox->getText()) return;

    beginParameterChange();
    setUnnormalizedValue(sourceParameter->getValueForText(comboBox->getText()));
    endParameterChange();
}

void StatefulAudioProcessorWrapper::Parameter::switchChanged(SwitchParameterComponent *parameterSwitch) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    if (ignoreCallbacks) return;

    beginParameterChange();
    float newValue = float(parameterSwitch->getSelectedItemIndex()) / float((parameterSwitch->getNumItems() - 1));
    setUnnormalizedValue(newValue);
    endParameterChange();
}

void StatefulAudioProcessorWrapper::Parameter::parameterControlValueChanged(ParameterControl *control) {
    const ScopedLock selfCallbackLock(selfCallbackMutex);
    if (ignoreCallbacks) return;

    setUnnormalizedValue(control->getValue());
}

StatefulAudioProcessorWrapper::StatefulAudioProcessorWrapper(AudioPluginInstance *audioProcessor, Processor *processor, UndoManager &undoManager) :
        audioProcessor(audioProcessor), nodeId(processor->getNodeId()) {
    audioProcessor->enableAllBuses();
    if (auto *ioProcessor = dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor *>(audioProcessor)) {
        if (ioProcessor->isInput()) {
            audioProcessor->setPlayConfigDetails(0, audioProcessor->getTotalNumOutputChannels(), audioProcessor->getSampleRate(), audioProcessor->getBlockSize());
        } else if (ioProcessor->isOutput()) {
            audioProcessor->setPlayConfigDetails(audioProcessor->getTotalNumInputChannels(), 0, audioProcessor->getSampleRate(), audioProcessor->getBlockSize());
        }
    }

    for (auto parameter : audioProcessor->getParameters()) {
        auto *parameterWrapper = new Parameter(parameter, this);
        auto paramState = processor->getState().getChildWithProperty(ParamIDs::id, parameterWrapper->paramID);
        if (!paramState.isValid()) {
            paramState = ValueTree(ParamIDs::PARAM);
            Param::setId(paramState, parameterWrapper->paramID);
            processor->getState().appendChild(paramState, nullptr);
        }
        parameterWrapper->setNewState(paramState, &undoManager);
        parameters.add(parameterWrapper);
        if (parameter->isAutomatable())
            automatableParameters.add(parameterWrapper);
    }
    processor->audioProcessorChanged(audioProcessor, AudioProcessorListener::ChangeDetails().withParameterInfoChanged(true));
    // XXX A little hacky, but maybe the best we can do.
    //  If we're loading from state, bypass state needs to make its way to the processor graph to actually mute.
    processor->getState().sendPropertyChangeMessage(ProcessorIDs::bypassed);
    audioProcessor->addListener(processor);
}

StatefulAudioProcessorWrapper::~StatefulAudioProcessorWrapper() {
    automatableParameters.clear(false);
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
