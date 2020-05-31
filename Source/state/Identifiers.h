#pragma once

#include "JuceHeader.h"

namespace IDs {
#define DECLARE_ID(name) const juce::Identifier name(#name);


    DECLARE_ID(PARAM)
    DECLARE_ID(id)
    DECLARE_ID(value)

    DECLARE_ID(TRACKS)
    DECLARE_ID(INPUT)
    DECLARE_ID(OUTPUT)
    DECLARE_ID(MIDI_INPUT)
    DECLARE_ID(MIDI_OUTPUT)

    DECLARE_ID(PROJECT)

    DECLARE_ID(TRACK)
    DECLARE_ID(uuid)
    DECLARE_ID(colour)
    DECLARE_ID(name)
    DECLARE_ID(selected)
    DECLARE_ID(isMasterTrack)

    DECLARE_ID(PROCESSOR_LANE)
    DECLARE_ID(selectedSlotsMask)

    DECLARE_ID(PROCESSOR)
    DECLARE_ID(processorInitialized)
    DECLARE_ID(processorSlot)
    DECLARE_ID(nodeId)
    DECLARE_ID(bypassed)
    DECLARE_ID(acceptsMidi)
    DECLARE_ID(producesMidi)
    DECLARE_ID(deviceName)
    DECLARE_ID(state)
    DECLARE_ID(allowDefaultConnections)
    DECLARE_ID(pluginWindowType)
    DECLARE_ID(pluginWindowX)
    DECLARE_ID(pluginWindowY)

    DECLARE_ID(INPUT_CHANNELS)
    DECLARE_ID(OUTPUT_CHANNELS)
    DECLARE_ID(CHANNEL)

    DECLARE_ID(CONNECTIONS)
    DECLARE_ID(CONNECTION)
    DECLARE_ID(SOURCE)
    DECLARE_ID(DESTINATION)
    DECLARE_ID(channel)
    DECLARE_ID(isCustomConnection)

    DECLARE_ID(VIEW_STATE)
    DECLARE_ID(controlMode)
    DECLARE_ID(focusedPane)
    DECLARE_ID(focusedTrackIndex)
    DECLARE_ID(focusedProcessorSlot)
    DECLARE_ID(gridViewSlotOffset)
    DECLARE_ID(gridViewTrackOffset)
    DECLARE_ID(masterViewSlotOffset)
    DECLARE_ID(numMasterProcessorSlots)
    DECLARE_ID(numProcessorSlots)

#undef DECLARE_ID
}
