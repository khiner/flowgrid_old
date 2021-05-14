#pragma once

#include <juce_core/juce_core.h>

namespace IDs {
#define ID(name) const juce::Identifier name(#name);
    ID(PARAM)
    ID(id)
    ID(value)

    ID(TRACKS)
    ID(INPUT)
    ID(OUTPUT)
    ID(MIDI_INPUT)
    ID(MIDI_OUTPUT)

    ID(PROJECT)

    ID(TRACK)
    ID(uuid)
    ID(colour)
    ID(name)
    ID(selected)
    ID(isMasterTrack)

    ID(PROCESSOR_LANES)
    ID(PROCESSOR_LANE)
    ID(selectedSlotsMask)

    ID(PROCESSOR)
    ID(processorInitialized)
    ID(processorSlot)
    ID(nodeId)
    ID(bypassed)
    ID(acceptsMidi)
    ID(producesMidi)
    ID(deviceName)
    ID(state)
    ID(allowDefaultConnections)
    ID(pluginWindowType)
    ID(pluginWindowX)
    ID(pluginWindowY)

    ID(INPUT_CHANNELS)
    ID(OUTPUT_CHANNELS)
    ID(CHANNEL)
    ID(channelIndex)
    ID(abbreviatedName)
#undef ID
}
