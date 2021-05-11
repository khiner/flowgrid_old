#pragma once

#include "state/Project.h"
#include "view/graph_editor/ConnectorDragListener.h"
#include "view/graph_editor/GraphEditorChannel.h"
#include "StatefulAudioProcessorContainer.h"

class BaseGraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    BaseGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                             const ValueTree &state, ConnectorDragListener &connectorDragListener);

    ~BaseGraphEditorProcessor() override;

    const ValueTree &getState() const { return state; }

    ValueTree getTrack() const {
        return TracksState::getTrackForProcessor(getState());
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        return StatefulAudioProcessorContainer::getNodeIdForState(state);
    }

    virtual bool isInView() {
        return isIoProcessor() || view.isProcessorSlotInView(getTrack(), getSlot());
    }

    bool isMasterTrack() const { return TracksState::isMasterTrack(getTrack()); }

    int getTrackIndex() const { return tracks.indexOf(getTrack()); }

    int getSlot() const { return state[IDs::processorSlot]; }

    int getNumInputChannels() const { return state.getChildWithName(IDs::INPUT_CHANNELS).getNumChildren(); }

    int getNumOutputChannels() const { return state.getChildWithName(IDs::OUTPUT_CHANNELS).getNumChildren(); }

    bool acceptsMidi() const { return state[IDs::acceptsMidi]; }

    bool producesMidi() const { return state[IDs::producesMidi]; }

    bool isIoProcessor() const { return InternalPluginFormat::isIoProcessor(state[IDs::name]); }

    bool isSelected() { return TracksState::isProcessorSelected(state); }

    StatefulAudioProcessorWrapper *getProcessorWrapper() const {
        return audioProcessorContainer.getProcessorWrapperForState(state);
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

    virtual Rectangle<int> getBoxBounds();

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

    virtual void layoutChannel(AudioProcessor *processor, GraphEditorChannel *channel, const Rectangle<float> &boxBounds) const;

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override;

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;

private:
    TracksState &tracks;
    ViewState &view;
    ConnectorDragListener &connectorDragListener;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;

    GraphEditorChannel *findChannelWithState(const ValueTree &state) {
        for (auto *channel : channels)
            if (channel->getState() == state)
                return channel;

        return nullptr;
    }
};
