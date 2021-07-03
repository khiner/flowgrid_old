#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Stateful.h"

namespace ConnectionIDs {
#define ID(name) const juce::Identifier name(#name);
ID(CONNECTION)
ID(sourceChannelIndex)
ID(destinationChannelIndex)
ID(sourceNodeId)
ID(destinationNodeId)
ID(isCustom)
#undef ID
}

namespace fg {
struct Connection : public Stateful<Connection> {
    explicit Connection(ValueTree state) : Stateful<Connection>(std::move(state)) {}
    Connection(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination) {
        setCustom(true);
        setSourceNodeAndChannel(source);
        setDestinationNodeAndChannel(destination);
    }
    explicit Connection(const AudioProcessorGraph::Connection &connection, bool isDefault = true) : Connection(connection.source, connection.destination) {
        if (!isDefault) setCustom(!isDefault);
    }
    explicit Connection(const Connection *other) : Connection(other->toAudioConnection(), !other->isCustom()) {}

    AudioProcessorGraph::Connection toAudioConnection() const {
        return {{getSourceNodeId(),      getSourceChannel()},
                {getDestinationNodeId(), getDestinationChannel()}};
    }

    AudioProcessorGraph::NodeAndChannel getSourceNodeAndChannel() const {
        return {getSourceNodeId(), getSourceChannel()};
    }

    AudioProcessorGraph::NodeAndChannel getDestinationNodeAndChannel() const {
        return {getDestinationNodeId(), getDestinationChannel()};
    }

    void setSourceNodeAndChannel(AudioProcessorGraph::NodeAndChannel source) {
        setSourceNodeId(source.nodeID);
        setSourceChannel(source.channelIndex);
    }

    void setDestinationNodeAndChannel(AudioProcessorGraph::NodeAndChannel destination) {
        setDestinationNodeId(destination.nodeID);
        setDestinationChannel(destination.channelIndex);
    }

    int getIndex() const { return state.getParent().indexOf(state); }

    static Identifier getIdentifier() { return ConnectionIDs::CONNECTION; }

    bool sourceEquals(AudioProcessorGraph::NodeAndChannel nodeAndChannel) const {
        return getSourceNodeId() == nodeAndChannel.nodeID && getSourceChannel() == nodeAndChannel.channelIndex;
    }

    bool destinationEquals(AudioProcessorGraph::NodeAndChannel nodeAndChannel) const {
        return getDestinationNodeId() == nodeAndChannel.nodeID && getDestinationChannel() == nodeAndChannel.channelIndex;
    }

    bool equals(const AudioProcessorGraph::Connection &connection) const {
        return sourceEquals(connection.source) && destinationEquals(connection.destination);
    }

    void clearSource() {
        setSourceNodeId({});
        setSourceChannel(0); // -1?
    }

    void clearDestination() {
        setDestinationNodeId({});
        setDestinationChannel(0); // -1?
    }

    bool isMIDI() const noexcept {
        return getSourceChannel() == AudioProcessorGraph::midiChannelIndex ||
               getDestinationChannel() == AudioProcessorGraph::midiChannelIndex;
    }
    bool isCustom() const { return state.hasProperty(ConnectionIDs::isCustom) && state[ConnectionIDs::isCustom]; }
    int getSourceChannel() const { return state[ConnectionIDs::sourceChannelIndex]; }
    int getDestinationChannel() const { return state[ConnectionIDs::destinationChannelIndex]; }
    AudioProcessorGraph::NodeID getSourceNodeId() const {
        return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::sourceNodeId]))) : AudioProcessorGraph::NodeID{};
    }
    AudioProcessorGraph::NodeID getDestinationNodeId() const {
        return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::destinationNodeId]))) : AudioProcessorGraph::NodeID{};
    }

    void setSourceChannel(int channel) { state.setProperty(ConnectionIDs::sourceChannelIndex, channel, nullptr); }
    void setCustom(bool isCustom) { state.setProperty(ConnectionIDs::isCustom, isCustom, nullptr); }
    void setDestinationChannel(int channel) { state.setProperty(ConnectionIDs::destinationChannelIndex, channel, nullptr); }
    void setSourceNodeId(AudioProcessorGraph::NodeID sourceNodeId) { state.setProperty(ConnectionIDs::sourceNodeId, int(sourceNodeId.uid), nullptr); }
    void setDestinationNodeId(AudioProcessorGraph::NodeID destinationNodeId) { state.setProperty(ConnectionIDs::destinationNodeId, int(destinationNodeId.uid), nullptr); }

    static bool isCustom(const ValueTree &state) { return state.hasProperty(ConnectionIDs::isCustom) && state[ConnectionIDs::isCustom]; }
    static int getSourceChannel(const ValueTree &state) { return state[ConnectionIDs::sourceChannelIndex]; }
    static void setSourceChannel(ValueTree &state, int channel) { state.setProperty(ConnectionIDs::sourceChannelIndex, channel, nullptr); }
    static int getDestinationChannel(const ValueTree &state) { return state[ConnectionIDs::destinationChannelIndex]; }
    static AudioProcessorGraph::NodeID getSourceNodeId(const ValueTree &state) {
        return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::sourceNodeId]))) : AudioProcessorGraph::NodeID{};
    }
    static AudioProcessorGraph::NodeID getDestinationNodeId(const ValueTree &state) {
        return state.isValid() ? AudioProcessorGraph::NodeID(static_cast<uint32>(int(state[ConnectionIDs::destinationNodeId]))) : AudioProcessorGraph::NodeID{};
    }

    static void setCustom(ValueTree &state, bool isCustom) { state.setProperty(ConnectionIDs::isCustom, isCustom, nullptr); }
    static void setDestinationChannel(ValueTree &state, int channel) { state.setProperty(ConnectionIDs::destinationChannelIndex, channel, nullptr); }
    static void setSourceNodeId(ValueTree &state, AudioProcessorGraph::NodeID sourceNodeId) { state.setProperty(ConnectionIDs::sourceNodeId, int(sourceNodeId.uid), nullptr); }
    static void setDestinationNodeId(ValueTree &state, AudioProcessorGraph::NodeID destinationNodeId) { state.setProperty(ConnectionIDs::destinationNodeId, int(destinationNodeId.uid), nullptr); }
};
}
