#pragma once

#include "StatefulList.h"
#include "Channel.h"

namespace InputChannelsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(INPUT_CHANNELS)
#undef ID
}

struct InputChannels : public Stateful<InputChannels>, public StatefulList<fg::Channel> {
    static Identifier getIdentifier() { return InputChannelsIDs::INPUT_CHANNELS; }
    bool isChildType(const ValueTree &i) const override { return fg::Channel::isType(i); }

protected:
    fg::Channel *createNewObject(const ValueTree &tree) override { return new fg::Channel(tree); }

public:

};
