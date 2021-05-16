#pragma once

#include <juce_core/juce_core.h>

struct Push2MidiDevice {
    static juce::String getDeviceName() {
        return "Ableton Push 2 Live Port";
    }
};
