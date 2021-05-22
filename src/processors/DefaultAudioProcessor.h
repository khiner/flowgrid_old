#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "view/parameter_control/level_meter/LevelMeterSource.h"
#include "FlowGridConfig.h"

using namespace juce;

class DefaultAudioProcessor : public AudioPluginInstance, public AudioProcessorParameter::Listener {
public:
    explicit DefaultAudioProcessor(const PluginDescription &description,
                                   const AudioChannelSet &channelSetToUse = AudioChannelSet::stereo())
            : AudioPluginInstance(getBusProperties(description.numInputChannels == 0, channelSetToUse)),
              name(description.name),
              state(description.fileOrIdentifier.fromFirstOccurrenceOf(":", false, false)),
              isGenerator(description.numInputChannels == 0),
              hasMidi(description.isInstrument),
              channelSet(channelSetToUse) {
        jassert(channelSetToUse.size() == description.numOutputChannels);
    }

    virtual void parameterChanged(AudioProcessorParameter *parameter, float newValue) {}

    void parameterValueChanged(int parameterIndex, float newValue) override {
        AudioProcessorParameter *parameter = getParameters().getUnchecked(parameterIndex);
        if (auto *rangedParameter = dynamic_cast<RangedAudioParameter *>(parameter))
            parameterChanged(parameter, rangedParameter->convertFrom0to1(newValue));
        else
            parameterChanged(parameter, newValue);
    }

    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

    virtual LevelMeterSource *getMeterSource() { return nullptr; }

    virtual AudioProcessorParameter *getMeteredParameter() { return nullptr; } // TODO should be a new MeteredParameter type

    // TODO can remove?
    void prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) override {
        this->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, estimatedSamplesPerBlock);
    }

    AudioProcessorEditor *createEditor() override { return nullptr; }

    const String getName() const override { return name; }
    int getNumParameters() override { return getParameters().size(); }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return hasMidi; }
    bool producesMidi() const override { return hasMidi; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    const String getProgramName(int) override { return {}; }
    void getStateInformation(juce::MemoryBlock &) override {}
    bool isBusesLayoutSupported(const BusesLayout &layout) const override {
        return !(!isGenerator && layout.getMainOutputChannelSet() != channelSet) && !(layout.getMainInputChannelSet() != channelSet);
    }
    static PluginDescription getPluginDescription(const String &identifier, bool registerAsGenerator, bool acceptsMidi,
                                                  const AudioChannelSet &channelSetToUse = AudioChannelSet::stereo());

    void setCurrentProgram(int) override {}
    void changeProgramName(int, const String &) override {}
    void setStateInformation(const void *, int) override {}
    void releaseResources() override {}
    void fillInPluginDescription(PluginDescription &description) const override {
        description = getPluginDescription(name + ":" + state, isGenerator, hasMidi, channelSet);
    }
    static AudioParameterFloat *createDefaultGainParameter(const String &id, const String &name, float defaultValue = 0.0f) {
        return new AudioParameterFloat(id, name, NormalisableRange<float>(float(Decibels::defaultMinusInfinitydB), 6.0f, 0.0f, 4.0f), defaultValue, "dB",
                                       AudioProcessorParameter::genericParameter, defaultStringFromDbValue, defaultValueFromDbString);
    }

    const static std::function<String(float, int)> defaultStringFromValue;
    const static std::function<String(float, int)> defaultStringFromDbValue;
    const static std::function<float(const String &)> defaultValueFromString;
    const static std::function<float(const String &)> defaultValueFromDbString;

private:
    static BusesProperties getBusProperties(bool registerAsGenerator, const AudioChannelSet &channelSetToUse);

    String name, state;
    bool isGenerator, hasMidi;
    AudioChannelSet channelSet;
};
