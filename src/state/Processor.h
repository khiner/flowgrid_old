#pragma once

#include "Stateful.h"
#include "InputChannels.h"
#include "OutputChannels.h"
#include "Param.h"

namespace ProcessorIDs {
#define ID(name) const juce::Identifier name(#name);
ID(PROCESSOR)
ID(id)
ID(name)
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
#undef ID
}

class Processor : public Stateful<Processor> {
    static Identifier getIdentifier() { return ProcessorIDs::PROCESSOR; }
};
