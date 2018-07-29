#include "DefaultAudioProcessor.h"

const std::function<String (float, int)> DefaultAudioProcessor::defaultStringFromValue = [](float value, int radix) { return String(value, jmin(4, radix)); };
const std::function<String (float, int)> DefaultAudioProcessor::defaultStringFromDbValue = [](float value, int radix) { return String(Decibels::gainToDecibels(value), jmin(4, radix)); };
