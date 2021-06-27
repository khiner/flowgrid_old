#pragma once

#include "StatefulList.h"
#include "Channel.h"

namespace ChannelsIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CHANNELS)
ID(type)
#undef ID
}

struct Channels : public Stateful<Channels>, public StatefulList<fg::Channel> {
    enum Type { input, output };

    static Identifier getIdentifier() { return ChannelsIDs::CHANNELS; }
    bool isChildType(const ValueTree &i) const override { return fg::Channel::isType(i); }

    void setType(Type type) { state.setProperty(ChannelsIDs::type, int(type), nullptr); }
    Type getType() {
        int typeInt = state[ChannelsIDs::type];
        return static_cast<Type>(typeInt);
    }

    static void setType(ValueTree &state, Type type) { state.setProperty(ChannelsIDs::type, int(type), nullptr); }
    static Type getType(ValueTree &state) {
        int typeInt = state[ChannelsIDs::type];
        return static_cast<Type>(typeInt);
    }

    static bool isInput(const ValueTree &state) { return state.isValid() && int(state[ChannelsIDs::type]) == int(Type::input); }
    static bool isOutput(const ValueTree &state) { return state.isValid() && int(state[ChannelsIDs::type]) == int(Type::output); }

protected:
    fg::Channel *createNewObject(const ValueTree &tree) override { return new fg::Channel(tree); }

public:

};
