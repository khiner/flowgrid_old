#pragma once

#include <ProcessorGraph.h>
#include "GraphEditorChannel.h"
#include "GraphEditorTracks.h"
#include "GraphEditorConnectors.h"
#include "view/CustomColourIds.h"
#include <view/PluginWindow.h>

class GraphEditorPanel
        : public Component, public ConnectorDragListener, public GraphEditorProcessorContainer,
          private ValueTree::Listener {
public:
    GraphEditorPanel(ProcessorGraph &graph, Project &project)
            : graph(graph), project(project), audioProcessorContainer(project), tracks(project.getTracks()), connections(project.getConnections()),
              view(project.getView()), input(project.getInput()), output(project.getOutput()) {
        tracks.addListener(this);
        view.addListener(this);
        connections.addListener(this);
        input.addListener(this);
        output.addListener(this);

        addAndMakeVisible(*(graphEditorTracks = std::make_unique<GraphEditorTracks>(project, tracks, *this)));
        addAndMakeVisible(*(connectors = std::make_unique<GraphEditorConnectors>(connections, *this, *this, graph)));
        unfocusOverlay.setFill(findColour(CustomColourIds::unfocusedOverlayColourId));
        addChildComponent(unfocusOverlay);
        addMouseListener(this, true);
    }

    ~GraphEditorPanel() override {
        removeMouseListener(this);
        draggingConnector = nullptr;

        output.removeListener(this);
        input.removeListener(this);
        connections.removeListener(this);
        view.removeListener(this);
        tracks.removeListener(this);
    }

    void mouseDown(const MouseEvent &e) override {
        view.focusOnGridPane();
    }

    void mouseDrag(const MouseEvent &e) override {
        if (project.isCurrentlyDraggingProcessor() && !e.mods.isRightButtonDown())
            project.dragToPosition(trackAndSlotAt(e));
    }

    void mouseUp(const MouseEvent &e) override {
        project.endDraggingProcessor();
    }

    void resized() override {
        auto processorHeight = getProcessorHeight();
        view.setProcessorHeight(processorHeight);
        view.setTrackWidth(getTrackWidth());

        auto r = getLocalBounds();
        unfocusOverlay.setRectangle(r.toFloat());
        connectors->setBounds(r);

        auto top = r.removeFromTop(processorHeight);

        graphEditorTracks->setBounds(r.removeFromTop(processorHeight * (ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 2) + ViewState::TRACK_LABEL_HEIGHT * 2 + ViewState::TRACKS_MARGIN));

        auto ioProcessorWidth = getWidth() - ViewState::TRACKS_MARGIN * 2;
        int trackXOffset = ViewState::TRACKS_MARGIN;
        top.setX(trackXOffset);
        top.setWidth(ioProcessorWidth);

        auto bottom = r.removeFromTop(processorHeight);
        bottom.setX(trackXOffset);
        bottom.setWidth(ioProcessorWidth);

        int midiInputProcessorWidthInChannels = midiInputProcessors.size() * 2;
        float audioInputWidthRatio = audioInputProcessor
                                     ? float(audioInputProcessor->getNumOutputChannels()) / float(audioInputProcessor->getNumOutputChannels() + midiInputProcessorWidthInChannels)
                                     : 0;
        if (audioInputProcessor != nullptr) {
            audioInputProcessor->setBounds(top.removeFromLeft(int(ioProcessorWidth * audioInputWidthRatio)));
        }
        for (auto *midiInputProcessor : midiInputProcessors) {
            midiInputProcessor->setBounds(top.removeFromLeft(int(ioProcessorWidth * (1 - audioInputWidthRatio) / midiInputProcessors.size())));
        }

        int midiOutputProcessorWidthInChannels = midiOutputProcessors.size() * 2;
        float audioOutputWidthRatio = audioOutputProcessor
                                      ? float(audioOutputProcessor->getNumInputChannels()) / float(audioOutputProcessor->getNumInputChannels() + midiOutputProcessorWidthInChannels)
                                      : 0;
        if (audioOutputProcessor != nullptr) {
            audioOutputProcessor->setBounds(bottom.removeFromLeft(int(ioProcessorWidth * audioOutputWidthRatio)));
            for (auto *midiOutputProcessor : midiOutputProcessors) {
                midiOutputProcessor->setBounds(bottom.removeFromLeft(int(ioProcessorWidth * (1 - audioOutputWidthRatio) / midiOutputProcessors.size())));
            }
        }
        connectors->updateConnectors();
    }

    void update() override {
        connectors->updateConnectors();
    }

    void beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) override {
        auto *c = dynamic_cast<GraphEditorConnector *> (e.originalComponent);
        draggingConnector = c;

        if (draggingConnector == nullptr)
            draggingConnector = new GraphEditorConnector(ValueTree(), *this, *this, source, destination);
        else
            initialDraggingConnection = draggingConnector->getConnection();

        addAndMakeVisible(draggingConnector);

        dragConnector(e);
    }

    void dragConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        draggingConnector->setTooltip({});

        if (auto *channel = findChannelAt(e)) {
            draggingConnector->dragTo(channel->getNodeAndChannel(), channel->isInput());

            auto connection = draggingConnector->getConnection();
            if (graph.canConnect(connection) || graph.isConnected(connection)) {
                draggingConnector->setTooltip(channel->getTooltip());
                return;
            }
        }
        const auto &position = e.getEventRelativeTo(this).position;
        draggingConnector->dragTo(position);
    }

    void endDraggingConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        auto newConnection = draggingConnector->getConnection();

        draggingConnector->setTooltip({});
        if (initialDraggingConnection == EMPTY_CONNECTION)
            delete draggingConnector;
        else
            draggingConnector = nullptr;

        if (newConnection != initialDraggingConnection) {
            if (newConnection.source.nodeID.uid != 0 && newConnection.destination.nodeID.uid != 0)
                project.addConnection(newConnection);
            if (initialDraggingConnection != EMPTY_CONNECTION)
                project.removeConnection(initialDraggingConnection);
        }
        initialDraggingConnection = EMPTY_CONNECTION;
    }

    BaseGraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (audioInputProcessor && nodeId == audioInputProcessor->getNodeId()) return audioInputProcessor.get();
        else if (audioOutputProcessor && nodeId == audioOutputProcessor->getNodeId()) return audioOutputProcessor.get();
        else if (auto *midiInputProcessor = findMidiInputProcessorForNodeId(nodeId)) return midiInputProcessor;
        else if (auto *midiOutputProcessor = findMidiOutputProcessorForNodeId(nodeId)) return midiOutputProcessor;
        else return graphEditorTracks->getProcessorForNodeId(nodeId);
    }

    bool closeAnyOpenPluginWindows() {
        bool wasEmpty = activePluginWindows.isEmpty();
        activePluginWindows.clear();
        return !wasEmpty;
    }

private:
    ProcessorGraph &graph;
    Project &project;
    StatefulAudioProcessorContainer &audioProcessorContainer;

    TracksState &tracks;
    ConnectionsState &connections;
    ViewState &view;
    InputState &input;
    OutputState &output;

    const AudioProcessorGraph::Connection EMPTY_CONNECTION{{ProcessorGraph::NodeID(0), 0},
                                                           {ProcessorGraph::NodeID(0), 0}};
    std::unique_ptr<GraphEditorConnectors> connectors;
    GraphEditorConnector *draggingConnector{};
    std::unique_ptr<GraphEditorTracks> graphEditorTracks;
    std::unique_ptr<LabelGraphEditorProcessor> audioInputProcessor;
    std::unique_ptr<LabelGraphEditorProcessor> audioOutputProcessor;
    OwnedArray<LabelGraphEditorProcessor> midiInputProcessors;
    OwnedArray<LabelGraphEditorProcessor> midiOutputProcessors;

    AudioProcessorGraph::Connection initialDraggingConnection{EMPTY_CONNECTION};

    LabelGraphEditorProcessor::ElementComparator processorComparator;

    DrawableRectangle unfocusOverlay;

    OwnedArray<PluginWindow> activePluginWindows;

    int getTrackWidth() { return (getWidth() - ViewState::TRACKS_MARGIN * 2) / ViewState::NUM_VISIBLE_TRACKS; }

    int getProcessorHeight() { return (getHeight() - ViewState::TRACK_LABEL_HEIGHT * 2) / (ViewState::NUM_VISIBLE_PROCESSOR_SLOTS + 1); }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const {
        if (auto *channel = audioInputProcessor->findChannelAt(e))
            return channel;
        else if ((channel = audioOutputProcessor->findChannelAt(e)))
            return channel;
        for (auto *midiInputProcessor : midiInputProcessors) {
            if (auto *channel = midiInputProcessor->findChannelAt(e))
                return channel;
        }
        for (auto *midiOutputProcessor : midiOutputProcessors) {
            if (auto *channel = midiOutputProcessor->findChannelAt(e))
                return channel;
        }
        return graphEditorTracks->findChannelAt(e);
    }

    LabelGraphEditorProcessor *findMidiInputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *midiInputProcessor : midiInputProcessors) {
            if (midiInputProcessor->getNodeId() == nodeId)
                return midiInputProcessor;
        }
        return nullptr;
    }

    LabelGraphEditorProcessor *findMidiOutputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *midiOutputProcessor : midiOutputProcessors) {
            if (midiOutputProcessor->getNodeId() == nodeId)
                return midiOutputProcessor;
        }
        return nullptr;
    }

    ResizableWindow *getOrCreateWindowFor(ValueTree &processorState, PluginWindow::Type type) {
        auto nodeId = audioProcessorContainer.getNodeIdForState(processorState);
        for (auto *pluginWindow : activePluginWindows)
            if (audioProcessorContainer.getNodeIdForState(pluginWindow->processor) == nodeId && pluginWindow->type == type)
                return pluginWindow;

        if (auto *processor = audioProcessorContainer.getProcessorWrapperForNodeId(nodeId)->processor)
            return activePluginWindows.add(new PluginWindow(processorState, processor, type));

        return nullptr;
    }

    void closeWindowFor(ValueTree &processor) {
        auto nodeId = audioProcessorContainer.getNodeIdForState(processor);
        for (int i = activePluginWindows.size(); --i >= 0;)
            if (audioProcessorContainer.getNodeIdForState(activePluginWindows.getUnchecked(i)->processor) == nodeId)
                activePluginWindows.remove(i);
    }

    juce::Point<int> trackAndSlotAt(const MouseEvent &e) {
        auto position = e.getEventRelativeTo(graphEditorTracks.get()).position.toInt();
        const auto &track = graphEditorTracks->findTrackAt(position);
        return {tracks.indexOf(track), view.findSlotAt(position, track)};
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (tree.hasType(IDs::PROCESSOR) && i == IDs::processorSlot) {
            connectors->updateConnectors();
        } else if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) {
            resized();
        } else if (i == IDs::gridViewSlotOffset) {
            connectors->updateConnectors();
        } else if (i == IDs::focusedPane) {
            unfocusOverlay.setVisible(!view.isGridPaneFocused());
            unfocusOverlay.toFront(false);
        } else if (i == IDs::pluginWindowType) {
            const auto type = static_cast<PluginWindow::Type>(int(tree[i]));
            if (type == PluginWindow::Type::none)
                closeWindowFor(tree);
            else if (auto *w = getOrCreateWindowFor(tree, type))
                w->toFront(true);
        }
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (child[IDs::name] == MidiInputProcessor::name()) {
                auto *midiInputProcessor = new LabelGraphEditorProcessor(project, tracks, view, child, *this);
                addAndMakeVisible(midiInputProcessor, 0);
                midiInputProcessors.addSorted(processorComparator, midiInputProcessor);
                resized();
            } else if (child[IDs::name] == MidiOutputProcessor::name()) {
                auto *midiOutputProcessor = new LabelGraphEditorProcessor(project, tracks, view, child, *this);
                addAndMakeVisible(midiOutputProcessor, 0);
                midiOutputProcessors.addSorted(processorComparator, midiOutputProcessor);
                resized();
            } else if (child[IDs::name] == "Audio Input") {
                addAndMakeVisible(*(audioInputProcessor = std::make_unique<LabelGraphEditorProcessor>(project, tracks, view, child, *this)), 0);
                resized();
            } else if (child[IDs::name] == "Audio Output") {
                addAndMakeVisible(*(audioOutputProcessor = std::make_unique<LabelGraphEditorProcessor>(project, tracks, view, child, *this)), 0);
                resized();
            }
            connectors->updateConnectors();
        } else if (child.hasType(IDs::CONNECTION)) {
            connectors->updateConnectors();
        }
    }

    void valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::PROCESSOR)) {
            if (child[IDs::name] == MidiInputProcessor::name()) {
                midiInputProcessors.removeObject(findMidiInputProcessorForNodeId(ProcessorGraph::getNodeIdForState(child)));
                resized();
            } else if (child[IDs::name] == MidiOutputProcessor::name()) {
                midiOutputProcessors.removeObject(findMidiOutputProcessorForNodeId(ProcessorGraph::getNodeIdForState(child)));
                resized();
            }
            connectors->updateConnectors();
        } else if (child.hasType(IDs::CONNECTION)) {
            connectors->updateConnectors();
        }
    }

    void valueTreeChildOrderChanged(ValueTree &parent, int oldIndex, int newIndex) override {
        if (parent.hasType(IDs::TRACKS))
            connectors->updateConnectors();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
