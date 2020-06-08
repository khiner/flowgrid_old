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

    bool fileMightContainThisPluginType(const String &) override { return true; }

    FileSearchPath getDefaultLocationsToSearch() override { return {}; }

    bool canScanForPlugins() const override { return false; }

    void findAllTypesForFile(OwnedArray<PluginDescription> &, const String &) override {}

    bool doesPluginStillExist(const PluginDescription &) override { return true; }

    String getNameOfPluginFromIdentifier(const String &fileOrIdentifier) override { return fileOrIdentifier; }

    bool pluginNeedsRescanning(const PluginDescription &) override { return false; }

    StringArray searchPathsForPlugins(const FileSearchPath &, bool, bool) override { return {}; }

    bool isTrivialToScan() const override { return true; }

    static bool isIoProcessor(const String &name) {
        return name == "Audio Output" || name == "Audio Input" || name == MidiInputProcessor::name() || name == MidiOutputProcessor::name();
    }

    static bool isTrackIOProcessor(const String &name) {
        return isTrackInputProcessor(name) || isTrackOutputProcessor(name);
    }

    static bool isTrackInputProcessor(const String &name) {
        return name == TrackInputProcessor::name();
    }

    static bool isTrackOutputProcessor(const String &name) {
        return name == TrackOutputProcessor::name();
    }

private:
    void createPluginInstance(const PluginDescription &desc, double initialSampleRate, int initialBufferSize,
                              PluginCreationCallback callback) override {
        if (auto pluginInstance = createInstance(desc.name))
            callback(std::move(pluginInstance), {});
        else
            callback(nullptr, NEEDS_TRANS("Invalid internal plugin name"));
    }

    std::unique_ptr<AudioPluginInstance> createInstance(const String &name) {
        if (name == audioOutDesc.name) return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
        if (name == audioInDesc.name) return std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
        if (name == TrackInputProcessor::name()) return std::make_unique<TrackInputProcessor>();
        if (name == TrackOutputProcessor::name()) return std::make_unique<TrackOutputProcessor>();
        if (name == MidiInputProcessor::name()) return std::make_unique<MidiInputProcessor>();
        if (name == MidiKeyboardProcessor::name()) return std::make_unique<MidiKeyboardProcessor>();
        if (name == MidiOutputProcessor::name()) return std::make_unique<MidiOutputProcessor>();
        if (name == Arpeggiator::name()) return std::make_unique<Arpeggiator>();
        if (name == BalanceProcessor::name()) return std::make_unique<BalanceProcessor>();
        if (name == GainProcessor::name()) return std::make_unique<GainProcessor>();
        if (name == MixerChannelProcessor::name()) return std::make_unique<MixerChannelProcessor>();
        if (name == ParameterTypesTestProcessor::name()) return std::make_unique<ParameterTypesTestProcessor>();
        if (name == SineBank::name()) return std::make_unique<SineBank>();
        if (name == SineSynth::name()) return std::make_unique<SineSynth>();

        return {};
    }

    bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription &) const noexcept override {
        return false;
    }
};
