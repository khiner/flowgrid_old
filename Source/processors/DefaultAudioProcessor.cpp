#include "DefaultAudioProcessor.h"

const std::function<String (float, int)> DefaultAudioProcessor::defaultStringFromValue = [](float value, int radix) { return String(value, jmin(4, radix)); };
const std::function<String (float, int)> DefaultAudioProcessor::defaultStringFromDbValue = [](float value, int radix) { return String(Decibels::gainToDecibels(value), jmin(4, radix)); };
const std::function<float (const String&)> DefaultAudioProcessor::defaultValueFromString = [](const String &text) {
    auto t = text.trimStart();
    while (t.startsWithChar ('+'))
        t = t.substring (1).trimStart();

    return t.initialSectionContainingOnly("0123456789.,-").getFloatValue();
};

const std::function<float (const String&)> DefaultAudioProcessor::defaultValueFromDbString = [](const String &text) {
    auto t = text.trimStart();
    if (t.endsWithIgnoreCase("db"))
        t = t.substring (0, t.length() - 2);
    return Decibels::decibelsToGain(DefaultAudioProcessor::defaultValueFromString(t));
};
