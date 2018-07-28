#pragma once

#include "JuceHeader.h"

namespace IDs
{
    #define DECLARE_ID(name) const juce::Identifier name (#name);

    DECLARE_ID(PARAM)
    DECLARE_ID(value)
    DECLARE_ID(id)

    DECLARE_ID(TRACKS)
    DECLARE_ID(INPUT)
    DECLARE_ID(OUTPUT)

    DECLARE_ID(MASTER_TRACK)

    DECLARE_ID(PROJECT)
    DECLARE_ID(uuid)
    DECLARE_ID(mediaId)

    DECLARE_ID(TRACK)
    DECLARE_ID(colour)
    DECLARE_ID(name)
    DECLARE_ID(image)
    DECLARE_ID(selected)

    DECLARE_ID(PROCESSOR)
    DECLARE_ID(processorSlot)
    DECLARE_ID(nodeId)
    DECLARE_ID(bypassed)
    DECLARE_ID(numInputChannels)
    DECLARE_ID(numOutputChannels)

    DECLARE_ID(PARAM_NA)

    DECLARE_ID(CONNECTIONS)
    DECLARE_ID(CONNECTION)
    DECLARE_ID(SOURCE)
    DECLARE_ID(DESTINATION)
    DECLARE_ID(channel)
    DECLARE_ID(isCustomConnection)

    #undef DECLARE_ID
}
