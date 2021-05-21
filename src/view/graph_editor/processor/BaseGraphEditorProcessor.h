#pragma once

#include "view/graph_editor/ConnectorDragListener.h"
#include "view/graph_editor/GraphEditorChannel.h"
#include "ProcessorGraph.h"

class BaseGraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    BaseGraphEditorProcessor(const ValueTree &state, View &view, Tracks &tracks,
                             ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener);

    ~BaseGraphEditorProcessor() override;

    const ValueTree &getState() const { return state; }

    ValueTree getTrack() const {
        return Tracks::getTrackForProcessor(getState());
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        return Processor::getNodeId(state);
    }

    virtual bool isInView() {
        return isIoProcessor() || tracks.isProcessorSlotInView(getTrack(), getSlot());
    }

    bool isMasterTrack() const { return Tracks::isMasterTrack(getTrack()); }

    int getTrackIndex() const { return tracks.indexOf(getTrack()); }

    int getSlot() const { return state[ProcessorIDs::processorSlot]; }

    int getNumInputChannels() const { return state.getChildWithName(InputChannelsIDs::INPUT_CHANNELS).getNumChildren(); }

    int getNumOutputChannels() const { return state.getChildWithName(OutputChannelsIDs::OUTPUT_CHANNELS).getNumChildren(); }

    bool acceptsMidi() const { return state[ProcessorIDs::acceptsMidi]; }

    bool producesMidi() const { return state[ProcessorIDs::producesMidi]; }

    bool isIoProcessor() const { return InternalPluginFormat::isIoProcessor(state[ProcessorIDs::name]); }

    bool isSelected() { return Tracks::isProcessorSelected(state); }

    StatefulAudioProcessorWrapper *getProcessorWrapper() const {
        return processorGraph.getProcessorWrapperForState(state);
    }

    void paint(Graphics &g) override;

    bool hitTest(int x, int y) override;

    void resized() override;

    virtual juce::Point<float> getConnectorDirectionVector(bool isInput) const;

    juce::Point<float> getChannelConnectPosition(int index, bool isInput) const {
        for (auto *channel : channels)
            if (channel->getChannelIndex() == index && isInput == channel->isInput())
                return channel->getConnectPosition();

        return {};
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) {
        for (auto *channel : channels)
            if (channel->contains(e.getEventRelativeTo(channel).getPosition()))
                return channel;

        return nullptr;
    }

    virtual Rectangle<int> getBoxBounds() const;

    class ElementComparator {
    public:
        static int compareElements(BaseGraphEditorProcessor *first, BaseGraphEditorProcessor *second) {
            return first->getName().compare(second->getName());
        }
    };

protected:
    ValueTree state;
    static constexpr float largeFontHeight = 18.0f;
    static constexpr int channelSize = 10;
    OwnedArray<GraphEditorChannel> channels;

    virtual void layoutChannel(AudioProcessor *processor, GraphEditorChannel *channel) const;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;

protected:
    View &view;
    Tracks &tracks;
    ProcessorGraph &processorGraph;
    ConnectorDragListener &connectorDragListener;

private:
    GraphEditorChannel *findChannelWithState(const ValueTree &state) {
        for (auto *channel : channels)
            if (channel->getState() == state)
                return channel;

        return nullptr;
    }
};
