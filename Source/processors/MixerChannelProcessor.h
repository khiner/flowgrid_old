#pragma once

#include "JuceHeader.h"
#include "DefaultAudioProcessor.h"
#include "view/level_meter/LevelMeter.h"

class MixerChannelProcessor : public DefaultAudioProcessor {
public:
    explicit MixerChannelProcessor() :
            DefaultAudioProcessor(getPluginDescription()),
            balanceParameter(new AudioParameterFloat("balance", "Balance", NormalisableRange<float>(0.0f, 1.0f), balance.getTargetValue(), "",
                                                     AudioProcessorParameter::genericParameter, defaultStringFromValue, defaultValueFromString)),
            gainParameter(new AudioParameterFloat("gain", "Gain", NormalisableRange<float>(0.0f, 1.0f), gain.getTargetValue(), "dB",
                                                  AudioProcessorParameter::genericParameter, defaultStringFromDbValue, defaultValueFromDbString)) {
        balanceParameter->addListener(this);
        gainParameter->addListener(this);

        addParameter(balanceParameter);
        addParameter(gainParameter);
        inlineEditor = std::make_unique<InlineEditor>(*this);
    }

    ~MixerChannelProcessor() override {
        gainParameter->removeListener(this);
        balanceParameter->removeListener(this);
    }
    static const String name() { return "Mixer Channel"; }

    static PluginDescription getPluginDescription() {
        return DefaultAudioProcessor::getPluginDescription(name(), false, false);
    }

    Component* getInlineEditor() override {
        return inlineEditor.get();
    }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override {
        balance.reset(getSampleRate(), 0.05);
        gain.reset(getSampleRate(), 0.05);
    }

    void parameterChanged(AudioProcessorParameter *parameter, float newValue) override {
        if (parameter == balanceParameter) {
            balance.setValue(newValue);
        } else if (parameter == gainParameter) {
            gain.setValue(newValue);
        }
    }

    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override {
        if (buffer.getNumChannels() == 2) {
            // 0db at center, linear stereo balance control
            // http://www.kvraudio.com/forum/viewtopic.php?t=148865
            for (int i = 0; i < buffer.getNumSamples(); i++) {
                const float balanceValue = balance.getNextValue();

                float leftChannelGain, rightChannelGain;
                if (balanceValue < 0.5) {
                    leftChannelGain = 1;
                    rightChannelGain = balanceValue * 2;
                } else {
                    leftChannelGain = (1 - balanceValue) * 2;
                    rightChannelGain = 1;
                }

                buffer.setSample(0, i, buffer.getSample(0, i) * leftChannelGain);
                buffer.setSample(1, i, buffer.getSample(1, i) * rightChannelGain);
            }
        }
        gain.applyGain(buffer, buffer.getNumSamples());
        meterSource.measureBlock(buffer);
    }

    LevelMeterSource* getMeterSource() {
        return &meterSource;
    }

    AudioParameterFloat *getGainParameter() {
        return gainParameter;
    }

    AudioParameterFloat *getBalanceParameter() {
        return balanceParameter;
    }

class InlineEditor : public Component, private AudioProcessorListener, private LevelMeter::Listener, private Slider::Listener {
    public:
        explicit InlineEditor(MixerChannelProcessor &processor) : processor(processor) {
            meter.setMeterSource(processor.getMeterSource());
            balanceSlider.setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
            balanceSlider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);
            balanceSlider.setNormalisableRange(NormalisableRange<double>(0.0, 1.0));
            balanceSliderLabel.setText("Balance", dontSendNotification);
            balanceSliderLabel.setJustificationType(Justification::centred);
            balanceSliderLabel.attachToComponent(&balanceSlider, false);
            addAndMakeVisible(meter);
            addAndMakeVisible(balanceSlider);

            balanceSlider.addListener(this);
            balanceSlider.setColour(Slider::rotarySliderOutlineColourId, Colours::darkgrey);
            meter.addListener(this);
            processor.addListener(this);
            processor.getGainParameter()->sendValueChangedMessageToListeners(processor.getGainParameter()->get());
            processor.getBalanceParameter()->sendValueChangedMessageToListeners(processor.getBalanceParameter()->get());
        }

        ~InlineEditor() override {
            meter.removeListener(this);
            processor.removeListener(this);
        }

        void resized() override {
            meter.setBounds(getLocalBounds().removeFromLeft(getWidth() / 3).withX(getWidth() / 6));
            balanceSlider.setBounds(getLocalBounds().removeFromRight(getWidth() / 2).removeFromBottom(getWidth() / 2));
        }

    private:
        MixerChannelProcessor &processor;
        LevelMeter meter;
        Slider balanceSlider;
        Label balanceSliderLabel;

        void sliderValueChanged(Slider* slider) override {
            processor.getBalanceParameter()->setValueNotifyingHost(float(slider->getValue()));
        }

        void levelMeterValueChanged(LevelMeter* levelMeter) override {
            processor.getGainParameter()->setValueNotifyingHost(levelMeter->getValue());
        }

        void audioProcessorParameterChanged(AudioProcessor* processor, int parameterIndex, float newValue) override {
            if (parameterIndex == 0)
                balanceSlider.setValue(newValue, dontSendNotification);
            else if (parameterIndex == 1)
                meter.setValue(newValue, dontSendNotification);
        }

        void audioProcessorChanged(AudioProcessor* processor) override {}
        void audioProcessorParameterChangeGestureBegin(AudioProcessor* processor, int parameterIndex) override {}
        void audioProcessorParameterChangeGestureEnd(AudioProcessor* processor, int parameterIndex) override {}
    };

private:
    std::unique_ptr<InlineEditor> inlineEditor;

    LinearSmoothedValue<float> balance { 0.5 };
    LinearSmoothedValue<float> gain { 0.5 };

    AudioParameterFloat *balanceParameter;
    AudioParameterFloat *gainParameter;
    LevelMeterSource meterSource;
};
