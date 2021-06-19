#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "model/Channel.h"
#include "model/Processor.h"
#include "view/parameter_control/ParameterControl.h"
#include "view/parameter_control/level_meter/LevelMeterSource.h"
#include "view/processor_editor/SwitchParameterComponent.h"

struct StatefulAudioProcessorWrapper {
    struct Parameter
            : public AudioProcessorParameterWithID,
              private ValueTree::Listener,
              public AudioProcessorParameter::Listener, public Slider::Listener, public Button::Listener,
              public ComboBox::Listener, public SwitchParameterComponent::Listener, public ParameterControl::Listener {
        class Listener {
        public:
            virtual ~Listener() = default;

            virtual void parameterWillBeDestroyed(Parameter *parameter) = 0;
        };

        explicit Parameter(AudioProcessorParameter *parameter, StatefulAudioProcessorWrapper *processorWrapper);

        ~Parameter() override;

        void addListener(Listener *listener) { listeners.add(listener); }
        void removeListener(Listener *listener) { listeners.remove(listener); }

        String getText(float v, int length) const override {
            return valueToTextFunction != nullptr ?
                   valueToTextFunction(convertNormalizedToUnnormalized(v)) :
                   AudioProcessorParameter::getText(v, length);
        }

        void parameterValueChanged(int parameterIndex, float newValue) override { if (!ignoreCallbacks) { setValue(newValue); }}
        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

        float convertNormalizedToUnnormalized(float value) const { return range.snapToLegalValue(range.convertFrom0to1(value)); }

        float getValue() const override { return range.convertTo0to1(value); }
        void setValue(float newValue) override;
        void setUnnormalizedValue(float unnormalizedValue);
        void setAttachedComponentValues(float newValue);
        void postUnnormalizedValue(float unnormalizedValue);
        void setNewState(const ValueTree &v, UndoManager *undoManager);
        void updateFromValueTree();
        void copyValueToValueTree();
        void attachLabel(Label *valueLabel);
        void detachLabel(Label *valueLabel);
        void attachSlider(Slider *slider);
        void detachSlider(Slider *slider);
        void attachButton(Button *button);
        void detachButton(Button *button);
        void attachComboBox(ComboBox *comboBox);
        void detachComboBox(ComboBox *comboBox);
        void attachSwitch(SwitchParameterComponent *parameterSwitch);
        void detachSwitch(SwitchParameterComponent *parameterSwitch);
        void attachParameterControl(ParameterControl *parameterControl);
        void detachParameterControl(ParameterControl *parameterControl);

        float getDefaultValue() const override { return range.convertTo0to1(defaultValue); }
        float getValueForText(const String &text) const override {
            return range.convertTo0to1(textToValueFunction != nullptr ? textToValueFunction(text) : text.getFloatValue());
        }

        LevelMeterSource *getLevelMeterSource() const;

        AudioProcessorParameter *sourceParameter{nullptr};
        float defaultValue, value;
        std::function<String(const float)> valueToTextFunction;
        std::function<float(const String &)> textToValueFunction;
        NormalisableRange<float> range;

        std::atomic<bool> needsUpdate{true};
        ValueTree state;
        UndoManager *undoManager{nullptr};
        StatefulAudioProcessorWrapper *processorWrapper;
    private:
        ListenerList<Listener> listeners;
        bool listenersNeedCalling{true};
        bool ignoreParameterChangedCallbacks = false;
        bool ignoreCallbacks{false};
        CriticalSection selfCallbackMutex;

        OwnedArray<Label> attachedLabels{};
        OwnedArray<Slider> attachedSliders{};
        OwnedArray<Button> attachedButtons{};
        OwnedArray<ComboBox> attachedComboBoxes{};
        OwnedArray<SwitchParameterComponent> attachedSwitches{};
        OwnedArray<ParameterControl> attachedParameterControls{};

        void valueTreePropertyChanged(ValueTree &tree, const Identifier &p) override;
        void textChanged(Label *valueLabel);
        void sliderValueChanged(Slider *slider) override;
        void sliderDragStarted(Slider *slider) override { beginParameterChange(); }
        void sliderDragEnded(Slider *slider) override { endParameterChange(); }
        void buttonClicked(Button *button) override;
        void comboBoxChanged(ComboBox *comboBox) override;
        void switchChanged(SwitchParameterComponent *parameterSwitch) override;
        void parameterControlValueChanged(ParameterControl *control) override;
        void parameterControlDragStarted(ParameterControl *control) override { beginParameterChange(); }
        void parameterControlDragEnded(ParameterControl *control) override { endParameterChange(); }

        void beginParameterChange() const {
            if (undoManager != nullptr) undoManager->beginNewTransaction();
            sourceParameter->beginChangeGesture();
        }
        void endParameterChange() const {
            sourceParameter->endChangeGesture();
        }
    };

    StatefulAudioProcessorWrapper(AudioPluginInstance *audioProcessor, Processor *processor, UndoManager &undoManager);

    ~StatefulAudioProcessorWrapper();

    juce::AudioProcessorGraph::NodeID getNodeId() const { return nodeId; }

    int getNumParameters() const { return parameters.size(); }
    int getNumAutomatableParameters() const { return automatableParameters.size(); }

    Parameter *getParameter(int parameterIndex) { return parameters[parameterIndex]; }
    Parameter *getAutomatableParameter(int parameterIndex) { return automatableParameters[parameterIndex]; }

    bool flushParameterValuesToValueTree();

    AudioPluginInstance *audioProcessor;

private:
    juce::AudioProcessorGraph::NodeID nodeId;

    OwnedArray<Parameter> parameters;
    OwnedArray<Parameter> automatableParameters;

    CriticalSection valueTreeChanging;
};
