#pragma once

#include "JuceHeader.h"
#include "StatefulAudioProcessorWrapper.h"

using SParameter = StatefulAudioProcessorWrapper::Parameter;

class DefaultAudioProcessor : public AudioPluginInstance, public AudioProcessorValueTreeState::Listener {
public:
    explicit DefaultAudioProcessor(const PluginDescription& description,
                                   const AudioChannelSet& channelSetToUse = AudioChannelSet::stereo())
            : AudioPluginInstance(getBusProperties(description.numInputChannels == 0, channelSetToUse)),
              name(description.name),
              state(description.fileOrIdentifier.fromFirstOccurrenceOf(":", false, false)),
              isGenerator(description.numInputChannels == 0),
              hasMidi(description.isInstrument),
              channelSet(channelSetToUse) {
        jassert(channelSetToUse.size() == description.numOutputChannels);
    };

    // TODO can remove?
    void prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) override {
        this->setPlayConfigDetails(getTotalNumInputChannels(), getTotalNumOutputChannels(), sampleRate, estimatedSamplesPerBlock);
    }

    const String getName() const override { return name; }
    int getNumParameters() override { return getParameters().size(); }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return hasMidi; }
    bool producesMidi() const override { return hasMidi; }
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const String getProgramName(int) override { return {}; }
    void changeProgramName(int, const String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layout) const override {
        if (!isGenerator) {
            if (layout.getMainOutputChannelSet() != channelSet)
                return false;
        }

        return !(layout.getMainInputChannelSet() != channelSet);
    }


    void fillInPluginDescription(PluginDescription& description) const override {
        description = getPluginDescription(name + ":" + state, isGenerator, hasMidi, channelSet);
    }

    static PluginDescription getPluginDescription(const String& identifier, bool registerAsGenerator, bool acceptsMidi,
                                                  const AudioChannelSet& channelSetToUse = AudioChannelSet::stereo()) {
        PluginDescription descr;
        auto pluginName  = identifier.upToFirstOccurrenceOf(":", false, false);
        auto pluginState = identifier.fromFirstOccurrenceOf(":", false, false);

        descr.name              = pluginName;
        descr.descriptiveName   = pluginName;
        descr.pluginFormatName  = "Internal";
        descr.category          = (registerAsGenerator ? (acceptsMidi ? "Synth" : "Generator") : "Effect");
        descr.manufacturerName  = "Odang Ludo Productions";
        descr.version           = ProjectInfo::versionString;
        descr.fileOrIdentifier  = pluginName + ":" + pluginState;
        descr.uid               = pluginName.hashCode();
        descr.isInstrument      = (acceptsMidi && registerAsGenerator);
        descr.numInputChannels  = (registerAsGenerator ? 0 : channelSetToUse.size());
        descr.numOutputChannels = channelSetToUse.size();

        return descr;
    }

private:
    static BusesProperties getBusProperties(bool registerAsGenerator,
                                             const AudioChannelSet& channelSetToUse) {
        return registerAsGenerator ? BusesProperties().withOutput("Output", channelSetToUse)
                                   : BusesProperties().withInput("Input",  channelSetToUse).withOutput("Output", channelSetToUse);
    }

    String name, state;
    bool isGenerator, hasMidi;
    AudioChannelSet channelSet;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DefaultAudioProcessor)
};