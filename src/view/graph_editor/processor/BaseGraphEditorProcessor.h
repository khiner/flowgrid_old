#pragma once

#include "view/graph_editor/ConnectorDragListener.h"
#include "view/graph_editor/GraphEditorChannel.h"
#include "model/View.h"
#include "model/StatefulAudioProcessorWrappers.h"

class BaseGraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    BaseGraphEditorProcessor(Processor *processor, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener  &connectorDragListener);
    ~BaseGraphEditorProcessor() override;

    Processor *getProcessor() const { return processor; }
    Track *getTrack() const { return track; }

    AudioProcessorGraph::NodeID getNodeId() const {
        if (processor == nullptr) return {};
        return processor->getNodeId();
    }

    virtual bool isInView() {
        return processor->isIoProcessor() || view.isProcessorSlotInView(track->getIndex(), track->isMaster(), processor->getSlot());
    }

    int getTrackIndex() const { return track->getIndex(); }
    int getNumInputChannels() const { return processor->getState().getChildWithName(InputChannelsIDs::INPUT_CHANNELS).getNumChildren(); }
    int getNumOutputChannels() const { return processor->getState().getChildWithName(OutputChannelsIDs::OUTPUT_CHANNELS).getNumChildren(); }
    bool isSelected() { return track != nullptr && track->isProcessorSelected(processor); }
    StatefulAudioProcessorWrapper *getProcessorWrapper() const { return processorWrappers.getProcessorWrapperForProcessor(processor); }

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

protected:
    Processor *processor;
    static constexpr float largeFontHeight = 18.0f;
    static constexpr int channelSize = 10;
    OwnedArray<GraphEditorChannel> channels;

    virtual void layoutChannel(AudioProcessor *audioProcessor, GraphEditorChannel *channel) const;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;
    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override;
    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;

protected:
    Track *track;
    View &view;
    StatefulAudioProcessorWrappers &processorWrappers;
    ConnectorDragListener &connectorDragListener;

private:
    GraphEditorChannel *findChannelWithState(const ValueTree &state) {
        for (auto *channel : channels)
            if (channel->getState() == state)
                return channel;
        return nullptr;
    }
};
