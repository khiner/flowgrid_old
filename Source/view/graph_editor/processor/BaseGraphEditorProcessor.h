#pragma once

#include <Utilities.h>
#include <state/Project.h>
#include <StatefulAudioProcessorContainer.h>
#include "JuceHeader.h"
#include "view/graph_editor/ConnectorDragListener.h"
#include "view/graph_editor/GraphEditorChannel.h"

class BaseGraphEditorProcessor : public Component, public ValueTree::Listener {
public:
    BaseGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                         const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : state(state), project(project), tracks(tracks), view(view),
              connectorDragListener(connectorDragListener), audioProcessorContainer(project), pluginManager(project.getPluginManager()) {
        this->state.addListener(this);

        for (auto child : state) {
            if (child.hasType(IDs::INPUT_CHANNELS) || child.hasType(IDs::OUTPUT_CHANNELS)) {
                for (auto channel : child) {
                    valueTreeChildAdded(child, channel);
                }
            }
        }
    }

    ~BaseGraphEditorProcessor() override {
        state.removeListener(this);
    }

    const ValueTree &getState() const {
        return state;
    }

    ValueTree getTrack() const {
        return TracksState::getTrackForProcessor(getState());
    }

    AudioProcessorGraph::NodeID getNodeId() const {
        if (!state.isValid())
            return {};
        return ProcessorGraph::getNodeIdForState(state);
    }

    virtual bool isInView() {
        return isIoProcessor() || view.isProcessorSlotInView(getTrack(), getSlot());
    }

    inline bool isMasterTrack() const { return TracksState::isMasterTrack(getTrack()); }

    inline int getTrackIndex() const { return tracks.indexOf(getTrack()); }

    inline int getSlot() const { return state[IDs::processorSlot]; }

    inline int getNumInputChannels() const { return state.getChildWithName(IDs::INPUT_CHANNELS).getNumChildren(); }

    inline int getNumOutputChannels() const { return state.getChildWithName(IDs::OUTPUT_CHANNELS).getNumChildren(); }

    inline bool acceptsMidi() const { return state[IDs::acceptsMidi]; }

    inline bool producesMidi() const { return state[IDs::producesMidi]; }

    inline bool isIoProcessor() const { return InternalPluginFormat::isIoProcessorName(state[IDs::name]); }

    inline bool isSelected() { return tracks.isProcessorSelected(state); }

    StatefulAudioProcessorWrapper *getProcessorWrapper() const {
        return audioProcessorContainer.getProcessorWrapperForState(state);
    }

    AudioProcessor *getAudioProcessor() const {
        if (auto *processorWrapper = getProcessorWrapper())
            return processorWrapper->processor;

        return {};
    }

    void showWindow(PluginWindow::Type type) {
        state.setProperty(IDs::pluginWindowType, int(type),  &project.getUndoManager());
    }

    void paint(Graphics &g) override {
        auto boxColour = findColour(TextEditor::backgroundColourId);
        if (state[IDs::bypassed])
            boxColour = boxColour.brighter();
        else if (isSelected())
            boxColour = boxColour.brighter(0.02);

        g.setColour(boxColour);
        g.fillRect(getBoxBounds());
    }

    void mouseDown(const MouseEvent &e) override {
        project.beginDragging({getTrackIndex(), getSlot()});
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.mouseWasDraggedSinceMouseDown()) {
        } else if (e.getNumberOfClicks() == 2) {
            if (getAudioProcessor()->hasEditor()) {
                showWindow(PluginWindow::Type::normal);
            }
        }
    }

    bool hitTest(int x, int y) override {
        for (auto *child : getChildren())
            if (child->getBounds().contains(x, y))
                return true;

        return x >= 3 && x < getWidth() - 6 && y >= channelSize && y < getHeight() - channelSize;
    }

    void resized() override {
        if (auto *processor = getAudioProcessor()) {
            auto boxBoundsFloat = getBoxBounds().toFloat();
            for (auto *channel : channels)
                layoutChannel(processor, channel, boxBoundsFloat);
            repaint();
            connectorDragListener.update();
        }
    }

    virtual void layoutChannel(AudioProcessor *processor, GraphEditorChannel *channel, const Rectangle<float> &boxBounds) const {
        const bool isInput = channel->isInput();
        auto channelIndex = channel->getChannelIndex();
        int busIndex = 0;
        processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIndex);
        int total = isInput ? getNumInputChannels() : getNumOutputChannels();
        const int index = channel->isMidi() ? (total - 1) : channelIndex;
        auto indexPosition = index + busIndex * 0.5f;

        int x = boxBounds.getX() + indexPosition * channelSize;
        int y = boxBounds.getY() + indexPosition * channelSize;
        if (TracksState::isProcessorLeftToRightFlowing(getState()))
            channel->setSize(boxBounds.getWidth() / 2, channelSize);
        else
            channel->setSize(channelSize, boxBounds.getHeight() / 2);

        const auto &connectionDirection = getConnectorDirectionVector(channel->isInput());
        if (connectionDirection == juce::Point(-1, 0))
            channel->setTopLeftPosition(boxBounds.getX(), y);
        else if (connectionDirection == juce::Point(1, 0))
            channel->setTopRightPosition(boxBounds.getRight(), y);
        else if (connectionDirection == juce::Point(0, -1))
            channel->setTopLeftPosition(x, boxBounds.getY());
        else
            channel->setTopLeftPosition(x, boxBounds.getBottom() - channel->getHeight());
    }

    virtual juce::Point<int> getConnectorDirectionVector(bool isInput) const {
        bool isLeftToRight = TracksState::isProcessorLeftToRightFlowing(getState());
        if (isInput) {
            if (isLeftToRight)
                return {-1, 0};
            else
                return {0, -1};
        } else {
            if (isLeftToRight)
                return {1, 0};
            else
                return {0, 1};
        }
    }

    juce::Point<float> getChannelConnectPosition(int index, bool isInput) const {
        for (auto *channel : channels)
            if (channel->getChannelIndex() == index && isInput == channel->isInput())
                return channel->getConnectPosition();

        return {};
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) {
        auto *comp = getComponentAt(e.getEventRelativeTo(this).position.toInt());
        return dynamic_cast<GraphEditorChannel *>(comp);
    }

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

    virtual Rectangle<int> getBoxBounds() {
        auto r = getLocalBounds().reduced(1);
        bool isLeftToRight = TracksState::isProcessorLeftToRightFlowing(getState());
        if (isLeftToRight) {
            if (getNumInputChannels() > 0)
                r.setLeft(channelSize);
            if (getNumOutputChannels() > 0)
                r.removeFromRight(channelSize);
        } else {
            if (getNumInputChannels() > 0)
                r.setTop(channelSize);
            if (getNumOutputChannels() > 0)
                r.removeFromBottom(channelSize);
        }
        return r;
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *channel = new GraphEditorChannel(child, connectorDragListener);
            addAndMakeVisible(channel);
            channels.add(channel);
            resized();
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int) override {
        if (child.hasType(IDs::CHANNEL)) {
            auto *channelToRemove = findChannelWithState(child);
            channels.removeObject(channelToRemove);
            resized();
        }
    }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (i == IDs::processorInitialized)
            resized();
    }

private:
    Project &project;
    TracksState &tracks;
    ViewState &view;
    ConnectorDragListener &connectorDragListener;
    StatefulAudioProcessorContainer &audioProcessorContainer;
    PluginManager &pluginManager;
    OwnedArray<GraphEditorChannel> channels;

    GraphEditorChannel *findChannelWithState(const ValueTree &state) {
        for (auto *channel : channels)
            if (channel->getState() == state)
                return channel;

        return nullptr;
    }
};
