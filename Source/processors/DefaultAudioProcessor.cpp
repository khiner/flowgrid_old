#include "DefaultAudioProcessor.h"

const std::function<String (float, int)> DefaultAudioProcessor::defaultStringFromValue = [](float value, int radix) { return String(value, jmin(3, radix)); };
const std::function<String (float, int)> DefaultAudioProcessor::defaultStringFromDbValue = [](float value, int radix) { return value <= Decibels::defaultMinusInfinitydB ? "-inf" : String(value, jmin(2, radix)); };
const std::function<float (const String&)> DefaultAudioProcessor::defaultValueFromString = [](const String &text) {
    auto t = text.trimStart();
    while (t.startsWithChar ('+'))
        t = t.substring (1).trimStart();

    return t.initialSectionContainingOnly("0123456789.,-").getFloatValue();
};

const std::function<float (const String&)> DefaultAudioProcessor::defaultValueFromDbString = [](const String &text) {
    auto decibelText = text.upToFirstOccurrenceOf("dB", false, false).trim();
    return decibelText == "-inf" ? Decibels::defaultMinusInfinitydB : DefaultAudioProcessor::defaultValueFromString(decibelText);
};
