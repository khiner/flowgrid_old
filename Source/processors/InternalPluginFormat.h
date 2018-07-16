#pragma once

#include "JuceHeader.h"

class InternalPluginFormat : public AudioPluginFormat {
public:
    //==============================================================================
    InternalPluginFormat() {
        {
            AudioProcessorGraph::AudioGraphIOProcessor p(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
            p.fillInPluginDescription(audioOutDesc);
        }

        {
            AudioProcessorGraph::AudioGraphIOProcessor p(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
            p.fillInPluginDescription(audioInDesc);
        }

        {
            AudioProcessorGraph::AudioGraphIOProcessor p(AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode);
            p.fillInPluginDescription(midiInDesc);
        }
    }

    PluginDescription audioInDesc, audioOutDesc, midiInDesc;

    void getAllTypes(OwnedArray<PluginDescription>& results) {
        results.add(new PluginDescription(audioInDesc));
        results.add(new PluginDescription(audioOutDesc));
        results.add(new PluginDescription(midiInDesc));
        results.add(new PluginDescription(MixerChannelProcessor::getPluginDescription()));
        results.add(new PluginDescription(GainProcessor::getPluginDescription()));
        results.add(new PluginDescription(BalanceProcessor::getPluginDescription()));
        results.add(new PluginDescription(SineBank::getPluginDescription()));
    }

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
        if (name == audioInDesc.name)  return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
        if (name == midiInDesc.name)   return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode);
        if (name == MixerChannelProcessor::getIdentifier()) return new MixerChannelProcessor(MixerChannelProcessor::getPluginDescription());
        if (name == GainProcessor::getIdentifier()) return new GainProcessor(GainProcessor::getPluginDescription());
        if (name == BalanceProcessor::getIdentifier()) return new BalanceProcessor(BalanceProcessor::getPluginDescription());
        if (name == SineBank::getIdentifier()) return new SineBank(SineBank::getPluginDescription());

        return nullptr;
    }

    bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription&) const noexcept override {
        return false;
    }
};
