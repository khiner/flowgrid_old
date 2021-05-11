#include "DefaultAudioProcessor.h"

const std::function<String(float, int)> DefaultAudioProcessor::defaultStringFromValue = [](float value, int radix) { return String(value, jmin(3, radix)); };
const std::function<String(float, int)> DefaultAudioProcessor::defaultStringFromDbValue = [](float value, int radix) { return value <= Decibels::defaultMinusInfinitydB ? "-inf" : String(value, jmin(2, radix)); };
const std::function<float(const String &)> DefaultAudioProcessor::defaultValueFromString = [](const String &text) {
    auto t = text.trimStart();
    while (t.startsWithChar('+'))
        t = t.substring(1).trimStart();

    return t.initialSectionContainingOnly("0123456789.,-").getFloatValue();
};

const std::function<float(const String &)> DefaultAudioProcessor::defaultValueFromDbString = [](const String &text) {
    auto decibelText = text.upToFirstOccurrenceOf("dB", false, false).trim();
    return decibelText == "-inf" ? Decibels::defaultMinusInfinitydB : DefaultAudioProcessor::defaultValueFromString(decibelText);
};

PluginDescription DefaultAudioProcessor::getPluginDescription(const String &identifier, bool registerAsGenerator, bool acceptsMidi, const AudioChannelSet &channelSetToUse) {
    PluginDescription descr;
    auto pluginName = identifier.upToFirstOccurrenceOf(":", false, false);
    auto pluginState = identifier.fromFirstOccurrenceOf(":", false, false);

    descr.name = pluginName;
    descr.descriptiveName = pluginName;
    descr.pluginFormatName = "Internal";
    descr.category = (registerAsGenerator ? (acceptsMidi ? "Synth" : "Generator") : "Effect");
    descr.manufacturerName = "Odang Ludo Productions";

    descr.version = PROJECT_VERSION;

    descr.fileOrIdentifier = pluginName + ":" + pluginState;
    descr.uid = pluginName.hashCode();
    descr.isInstrument = (acceptsMidi && registerAsGenerator);
    descr.numInputChannels = (registerAsGenerator ? 0 : channelSetToUse.size());
    descr.numOutputChannels = channelSetToUse.size();

    return descr;
}

AudioProcessor::BusesProperties DefaultAudioProcessor::getBusProperties(bool registerAsGenerator, const AudioChannelSet &channelSetToUse) {
    if (channelSetToUse == AudioChannelSet::disabled())
        return BusesProperties();
    return registerAsGenerator ? BusesProperties().withOutput("Output", channelSetToUse)
                               : BusesProperties().withInput("Input", channelSetToUse).withOutput("Output", channelSetToUse);
}
