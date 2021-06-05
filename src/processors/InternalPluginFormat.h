#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

using namespace juce;

class InternalPluginFormat : public AudioPluginFormat {
public:
    InternalPluginFormat();

    PluginDescription audioInDesc, audioOutDesc;

    String getName() const override { return "Internal"; }
    Array<PluginDescription> &getInternalPluginDescriptions() { return internalPluginDescriptions; }
    bool fileMightContainThisPluginType(const String &) override { return true; }
    FileSearchPath getDefaultLocationsToSearch() override { return {}; }
    bool canScanForPlugins() const override { return false; }
    void findAllTypesForFile(OwnedArray<PluginDescription> &, const String &) override {}
    bool doesPluginStillExist(const PluginDescription &) override { return true; }
    String getNameOfPluginFromIdentifier(const String &fileOrIdentifier) override { return fileOrIdentifier; }
    bool pluginNeedsRescanning(const PluginDescription &) override { return false; }
    StringArray searchPathsForPlugins(const FileSearchPath &, bool, bool) override { return {}; }
    bool isTrivialToScan() const override { return true; }
    static bool isAudioInputProcessor(const String &name);
    static bool isAudioOutputProcessor(const String &name);
    static bool isMidiInputProcessor(const String &name);
    static bool isMidiOutputProcessor(const String &name);
    static bool isIoProcessor(const String &name);
    static bool isTrackIOProcessor(const String &name);
    static String getTrackInputProcessorName();
    static String getTrackOutputProcessorName();
    static String getMidiInputProcessorName();
    static String getMidiOutputProcessorName();
    static String getMixerChannelProcessorName();

private:
    Array<PluginDescription> internalPluginDescriptions;

    void createPluginInstance(const PluginDescription &desc, double initialSampleRate, int initialBufferSize, PluginCreationCallback callback) override;
    std::unique_ptr<AudioPluginInstance> createInstance(const String &name) const;

    bool requiresUnblockedMessageThreadDuringCreation(const PluginDescription &) const noexcept override { return false; }
};
