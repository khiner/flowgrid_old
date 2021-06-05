#include "InternalPluginFormat.h"

#include "Arpeggiator.h"
#include "BalanceProcessor.h"
#include "GainProcessor.h"
#include "MidiInputProcessor.h"
#include "MidiKeyboardProcessor.h"
#include "MidiOutputProcessor.h"
#include "MixerChannelProcessor.h"
#include "ParameterTypesTestProcessor.h"
#include "SineBank.h"
#include "SineSynth.h"
#include "TrackInputProcessor.h"
#include "TrackOutputProcessor.h"

InternalPluginFormat::InternalPluginFormat() : internalPluginDescriptions{
        TrackInputProcessor::getPluginDescription(),
        TrackOutputProcessor::getPluginDescription(),
        Arpeggiator::getPluginDescription(),
        BalanceProcessor::getPluginDescription(),
        GainProcessor::getPluginDescription(),
        MidiInputProcessor::getPluginDescription(),
        MidiKeyboardProcessor::getPluginDescription(),
        MidiOutputProcessor::getPluginDescription(),
        MixerChannelProcessor::getPluginDescription(),
        ParameterTypesTestProcessor::getPluginDescription(),
        SineBank::getPluginDescription(),
        SineSynth::getPluginDescription(),
} {
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

std::unique_ptr<AudioPluginInstance> InternalPluginFormat::createInstance(const String &name) const {
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

bool InternalPluginFormat::isAudioInputProcessor(const String &name) {
    return name == "Audio Output";
}

bool InternalPluginFormat::isAudioOutputProcessor(const String &name) {
    return name == "Audio Output";
}

bool InternalPluginFormat::isMidiInputProcessor(const String &name) {
    return name == MidiInputProcessor::name();
}

bool InternalPluginFormat::isMidiOutputProcessor(const String &name) {
    return name == MidiOutputProcessor::name();
}

bool InternalPluginFormat::isIoProcessor(const String &name) {
    return isAudioInputProcessor(name) || isAudioOutputProcessor(name) || name == MidiInputProcessor::name() || name == MidiOutputProcessor::name();
}

bool InternalPluginFormat::isTrackIOProcessor(const String &name) {
    return name == TrackInputProcessor::name() || name == TrackOutputProcessor::name();
}

String InternalPluginFormat::getTrackInputProcessorName() { return TrackInputProcessor::name(); }

String InternalPluginFormat::getTrackOutputProcessorName() { return TrackOutputProcessor::name(); }

String InternalPluginFormat::getMidiInputProcessorName() { return MidiInputProcessor::name(); }

String InternalPluginFormat::getMidiOutputProcessorName() { return MidiOutputProcessor::name(); }

String InternalPluginFormat::getMixerChannelProcessorName() { return MixerChannelProcessor::name(); }

void InternalPluginFormat::createPluginInstance(const PluginDescription &desc, double initialSampleRate, int initialBufferSize, AudioPluginFormat::PluginCreationCallback callback) {
    if (auto pluginInstance = createInstance(desc.name))
        callback(std::move(pluginInstance), {});
    else
        callback(nullptr, NEEDS_TRANS("Invalid internal plugin name"));
}
