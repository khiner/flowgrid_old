#pragma once

#include <ProcessorGraph.h>
#include "GraphEditorPin.h"
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
        view.setProcessorHeight(getProcessorHeight());
        view.setTrackWidth(getTrackWidth());

        auto r = getLocalBounds();
        unfocusOverlay.setRectangle(r.toFloat());
        connectors->setBounds(r);

        auto processorHeight = getProcessorHeight();
        auto top = r.removeFromTop(processorHeight);

        graphEditorTracks->setBounds(r.removeFromTop(processorHeight * (ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 1) + ViewState::TRACK_LABEL_HEIGHT + ViewState::TRACK_OUTPUT_HEIGHT));

        auto ioProcessorWidth = getWidth() - ViewState::TRACK_LABEL_HEIGHT - ViewState::TRACK_OUTPUT_HEIGHT;
        int trackXOffset = ViewState::TRACK_LABEL_HEIGHT;
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
            draggingConnector = new GraphEditorConnector(ValueTree(), *this, *this);
        else
            initialDraggingConnection = draggingConnector->getConnection();

        draggingConnector->setInput(source);
        draggingConnector->setOutput(destination);

        addAndMakeVisible(draggingConnector);
        draggingConnector->toFront(false);

        dragConnector(e);
    }

    void dragConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        draggingConnector->setTooltip({});

        auto pos = e.getEventRelativeTo(this).position;
        auto connection = draggingConnector->getConnection();

        if (auto *pin = findPinAt(e)) {
            if (connection.source.nodeID.uid == 0 && !pin->isInput())
                connection.source = pin->getPin();
            else if (connection.destination.nodeID.uid == 0 && pin->isInput())
                connection.destination = pin->getPin();

            if (graph.canConnect(connection) || graph.isConnected(connection)) {
                pos = getLocalPoint(pin->getParentComponent(), pin->getBounds().getCentre()).toFloat();
                draggingConnector->setTooltip(pin->getTooltip());
            }
        }

        if (draggingConnector->getConnection().source.nodeID.uid == 0)
            draggingConnector->dragStart(pos);
        else
            draggingConnector->dragEnd(pos);
    }

    void endDraggingConnector(const MouseEvent &e) override {
        if (draggingConnector == nullptr)
            return;

        auto newConnection = EMPTY_CONNECTION;
        if (auto *pin = findPinAt(e)) {
            newConnection = draggingConnector->getConnection();
            if (newConnection.source.nodeID.uid == 0) {
                if (pin->isInput())
                    newConnection = EMPTY_CONNECTION;
                else
                    newConnection.source = pin->getPin();
            } else {
                if (!pin->isInput())
                    newConnection = EMPTY_CONNECTION;
                else
                    newConnection.destination = pin->getPin();
            }
        }

        draggingConnector->setTooltip({});
        if (initialDraggingConnection == EMPTY_CONNECTION)
            delete draggingConnector;
        else {
            draggingConnector->setInput(initialDraggingConnection.source);
            draggingConnector->setOutput(initialDraggingConnection.destination);
            draggingConnector = nullptr;
        }

        if (newConnection == initialDraggingConnection) {
            initialDraggingConnection = EMPTY_CONNECTION;
            return;
        }

        if (newConnection != EMPTY_CONNECTION)
            project.addConnection(newConnection);

        if (initialDraggingConnection != EMPTY_CONNECTION) {
            project.removeConnection(initialDraggingConnection);
            initialDraggingConnection = EMPTY_CONNECTION;
        }
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
    std::unique_ptr<GraphEditorProcessor> audioInputProcessor;
    std::unique_ptr<GraphEditorProcessor> audioOutputProcessor;
    OwnedArray<GraphEditorProcessor> midiInputProcessors;
    OwnedArray<GraphEditorProcessor> midiOutputProcessors;

    AudioProcessorGraph::Connection initialDraggingConnection{EMPTY_CONNECTION};

    GraphEditorProcessor::ElementComparator processorComparator;

    DrawableRectangle unfocusOverlay;

    OwnedArray<PluginWindow> activePluginWindows;

    int getTrackWidth() { return (getWidth() - ViewState::TRACK_LABEL_HEIGHT - ViewState::TRACK_OUTPUT_HEIGHT) / ViewState::NUM_VISIBLE_TRACKS; }

    int getProcessorHeight() { return (getHeight() - ViewState::TRACK_LABEL_HEIGHT - ViewState::TRACK_OUTPUT_HEIGHT) / ViewState::NUM_VISIBLE_PROCESSOR_SLOTS; }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        if (auto *pin = audioInputProcessor->findPinAt(e))
            return pin;
        else if ((pin = audioOutputProcessor->findPinAt(e)))
            return pin;
        for (auto *midiInputProcessor : midiInputProcessors) {
            if (auto *pin = midiInputProcessor->findPinAt(e))
                return pin;
        }
        for (auto *midiOutputProcessor : midiOutputProcessors) {
            if (auto *pin = midiOutputProcessor->findPinAt(e))
                return pin;
        }
        return graphEditorTracks->findPinAt(e);
    }

    GraphEditorProcessor *findMidiInputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *midiInputProcessor : midiInputProcessors) {
            if (midiInputProcessor->getNodeId() == nodeId)
                return midiInputProcessor;
        }
        return nullptr;
    }

    GraphEditorProcessor *findMidiOutputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
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
        int trackIndex = view.findTrackIndexAt(position, tracks.getNumNonMasterTracks());
        const auto &track = tracks.getTrack(trackIndex);
        return {trackIndex, view.findSlotAt(position, track)};
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
                auto *midiInputProcessor = new GraphEditorProcessor(project, tracks, view, child, *this);
                addAndMakeVisible(midiInputProcessor);
                midiInputProcessors.addSorted(processorComparator, midiInputProcessor);
                resized();
            } else if (child[IDs::name] == MidiOutputProcessor::name()) {
                auto *midiOutputProcessor = new GraphEditorProcessor(project, tracks, view, child, *this);
                addAndMakeVisible(midiOutputProcessor);
                midiOutputProcessors.addSorted(processorComparator, midiOutputProcessor);
                resized();
            } else if (child[IDs::name] == "Audio Input") {
                addAndMakeVisible(*(audioInputProcessor = std::make_unique<GraphEditorProcessor>(project, tracks, view, child, *this, true)));
                resized();
            } else if (child[IDs::name] == "Audio Output") {
                addAndMakeVisible(*(audioOutputProcessor = std::make_unique<GraphEditorProcessor>(project, tracks, view, child, *this, true)));
                resized();
            }
            connectors->updateConnectors();
        } else if (child.hasType(IDs::CONNECTION)) {
            connectors->updateConnectors();
        } else if (child.hasType(IDs::CHANNEL)) {
            resized();
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
        } else if (child.hasType(IDs::CHANNEL)) {
            resized();
        }
    }

    void valueTreeChildOrderChanged(ValueTree &parent, int oldIndex, int newIndex) override {
        if (parent.hasType(IDs::TRACKS))
            connectors->updateConnectors();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
