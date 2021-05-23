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

    ValueTree getTrack() const { return Track::getTrackForProcessor(getState()); }

    AudioProcessorGraph::NodeID getNodeId() const { return Processor::getNodeId(state); }

    virtual bool isInView() { return Processor::isIoProcessor(state) || tracks.isProcessorSlotInView(getTrack(), Processor::getSlot(state)); }

    int getTrackIndex() const { return tracks.indexOfTrack(getTrack()); }
    int getNumInputChannels() const { return state.getChildWithName(InputChannelsIDs::INPUT_CHANNELS).getNumChildren(); }
    int getNumOutputChannels() const { return state.getChildWithName(OutputChannelsIDs::OUTPUT_CHANNELS).getNumChildren(); }
    bool isSelected() { return Track::isProcessorSelected(state); }
    StatefulAudioProcessorWrapper *getProcessorWrapper() const { return processorGraph.getProcessorWrapperForState(state); }

    void paint(Graphics &g) override;
    bool hitTest(int x, int y) override;
    void resized() override;

    virtual juce::Point<float> getConnectorDirectionVector(bool isInput) const;

    juce::Point<float> getChannelConnectPosition(int index, bool isInput) const {
        for (auto *channel : channels)
            if (fg::Channel::getChannelIndex(channel->getState()) == index && isInput == channel->isInput())
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
