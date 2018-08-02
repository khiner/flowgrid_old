#pragma once

#include "JuceHeader.h"
#include "InternalProcessorIncludes.h"

class InternalPluginFormat : public AudioPluginFormat {
public:
    //==============================================================================
    InternalPluginFormat() {
        {
            AudioProcessorGraph::AudioGraphIOProcessor p(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
            p.fillInPluginDescription(audioInDesc);
        }
        {
            AudioProcessorGraph::AudioGraphIOProcessor p(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
            p.fillInPluginDescription(audioOutDesc);
        }

        internalPluginDescriptions.add(audioInDesc, audioOutDesc);
    }

    PluginDescription audioInDesc, audioOutDesc;

    String getName() const override { return "Internal"; }
    bool fileMightContainThisPluginType(const String&) override { return true; }
    FileSearchPath getDefaultLocationsToSearch() override { return {}; }
    bool canScanForPlugins() const override { return false; }
    void findAllTypesForFile(OwnedArray <PluginDescription>&, const String&) override  {}
    bool doesPluginStillExist(const PluginDescription&) override { return true; }
    String getNameOfPluginFromIdentifier(const String& fileOrIdentifier) override { return fileOrIdentifier; }
    bool pluginNeedsRescanning(const PluginDescription&) override { return false; }
    StringArray searchPathsForPlugins(const FileSearchPath&, bool, bool) override { return {}; }

private:
    void createPluginInstance(const PluginDescription& desc, double initialSampleRate, int initialBufferSize,
                               void* userData, void (*callback) (void*, AudioPluginInstance*, const String&)) override {
        auto* p = createInstance(desc.name);
        callback(userData, p, p == nullptr ? NEEDS_TRANS("Invalid internal filter name") : String());
    }

    AudioPluginInstance* createInstance(const String& name) {
        if (name == audioOutDesc.name) return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
        if (name == audioInDesc.name) return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
        if (name == MidiInputProcessor::name()) return new MidiInputProcessor();
        if (name == MidiOutputProcessor::name()) return new MidiOutputProcessor();
        if (name == Arpeggiator::name()) return new Arpeggiator();
        if (name == BalanceProcessor::name()) return new BalanceProcessor();
        if (name == GainProcessor::name()) return new GainProcessor();
        if (name == MixerChannelProcessor::name()) return new MixerChannelProcessor();
        if (name == ParameterTypesTestProcessor::name()) return new ParameterTypesTestProcessor();
        if (name == SineBank::name()) return new SineBank();
        if (name == SineSynth::name()) return new SineSynth();
        return nullptr;
    }

    bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription&) const noexcept override {
        return false;
    }
};
