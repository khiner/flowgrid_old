#pragma once

#include "JuceHeader.h"

class InternalPluginFormat : public AudioPluginFormat {
public:
    //==============================================================================
    InternalPluginFormat() {
        {
            AudioProcessorGraph::AudioGraphIOProcessor p (AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
            p.fillInPluginDescription (audioOutDesc);
        }

        {
            AudioProcessorGraph::AudioGraphIOProcessor p (AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
            p.fillInPluginDescription (audioInDesc);
        }

        {
            AudioProcessorGraph::AudioGraphIOProcessor p (AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode);
            p.fillInPluginDescription (midiInDesc);
        }
    }

    //==============================================================================
    PluginDescription audioInDesc, audioOutDesc, midiInDesc;

    void getAllTypes (OwnedArray<PluginDescription>& results) {
        results.add (new PluginDescription (audioInDesc));
        results.add (new PluginDescription (audioOutDesc));
        results.add (new PluginDescription (midiInDesc));
//        results.add (new PluginDescription (SineWaveSynth::getPluginDescription()));
//        results.add (new PluginDescription (ReverbFilter::getPluginDescription()));
    }

    //==============================================================================
    String getName() const override                                                     { return "Internal"; }
    bool fileMightContainThisPluginType (const String&) override                        { return true; }
    FileSearchPath getDefaultLocationsToSearch() override                               { return {}; }
    bool canScanForPlugins() const override                                             { return false; }
    void findAllTypesForFile (OwnedArray <PluginDescription>&, const String&) override  {}
    bool doesPluginStillExist (const PluginDescription&) override                       { return true; }
    String getNameOfPluginFromIdentifier (const String& fileOrIdentifier) override      { return fileOrIdentifier; }
    bool pluginNeedsRescanning (const PluginDescription&) override                      { return false; }
    StringArray searchPathsForPlugins (const FileSearchPath&, bool, bool) override      { return {}; }

private:
    //==============================================================================
    void createPluginInstance (const PluginDescription& desc, double initialSampleRate, int initialBufferSize,
                               void* userData, void (*callback) (void*, AudioPluginInstance*, const String&)) override {
        auto* p = createInstance (desc.name);
        callback(userData, p, p == nullptr ? NEEDS_TRANS ("Invalid internal filter name") : String());
    }

    AudioPluginInstance* createInstance (const String& name) {
        if (name == audioOutDesc.name) return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode);
        if (name == audioInDesc.name)  return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode);
        if (name == midiInDesc.name)   return new AudioProcessorGraph::AudioGraphIOProcessor(AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode);
//        if (name == SineWaveSynth::getIdentifier()) return new SineWaveSynth (SineWaveSynth::getPluginDescription());
//        if (name == ReverbFilter::getIdentifier())  return new ReverbFilter  (ReverbFilter::getPluginDescription());

        return nullptr;
    }

    bool requiresUnblockedMessageThreadDuringCreation (const PluginDescription&) const noexcept override {
        return false;
    }
};