#pragma once

#include <ProcessorGraph.h>
#include "GraphEditorPin.h"
#include "GraphEditorTracks.h"
#include "GraphEditorConnectors.h"
#include "view/CustomColourIds.h"

class GraphEditorPanel
        : public Component, public ConnectorDragListener, public GraphEditorProcessorContainer,
          private ValueTree::Listener {
public:
    GraphEditorPanel(ProcessorGraph &g, Project &project, Viewport &parentViewport)
            : graph(g), project(project), tracksManager(project.getTracksManager()),
              viewManager(project.getViewStateManager()), parentViewport(parentViewport) {
        project.getState().addListener(this);

        addAndMakeVisible(*(tracks = std::make_unique<GraphEditorTracks>(project, project.getTracks(), *this, graph)));
        addAndMakeVisible(*(connectors = std::make_unique<GraphEditorConnectors>(project.getConnections(), *this, *this, graph)));
        unfocusOverlay.setFill(findColour(CustomColourIds::unfocusedOverlayColourId));
        addChildComponent(unfocusOverlay);
        addMouseListener(this, true);
    }

    ~GraphEditorPanel() override {
        removeMouseListener(this);
        draggingConnector = nullptr;
    }

    void mouseDown(const MouseEvent& event) override {
        viewManager.focusOnGridPane();
    }

    // Call this method when the parent viewport size has changed or when the number of tracks has changed.
    void resize() {
        tracksManager.setProcessorHeight(getProcessorHeight());
        tracksManager.setTrackWidth(getTrackWidth());
        int newWidth = getTrackWidth() * jmax(ViewStateManager::NUM_VISIBLE_TRACKS, tracksManager.getNumNonMasterTracks(),
                                              viewManager.getNumMasterProcessorSlots()) + TracksStateManager::TRACK_LABEL_HEIGHT * 2;
        int newHeight = getProcessorHeight() * (jmax(ViewStateManager::NUM_VISIBLE_TRACKS,
                                                     viewManager.getNumTrackProcessorSlots() + 1) + 3) + TracksStateManager::TRACK_LABEL_HEIGHT;
        setSize(newWidth, newHeight);
        updateViewPosition();
        if (newWidth == getWidth() && newHeight == getHeight()) {
            tracks->resized();
            connectors->updateConnectors();
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        unfocusOverlay.setRectangle(r.toFloat());
        connectors->setBounds(r);

        auto processorHeight = getProcessorHeight();
        auto top = r.removeFromTop(processorHeight);

        tracks->setBounds(r.removeFromTop(processorHeight * (viewManager.getNumTrackProcessorSlots() + 2) + TracksStateManager::TRACK_LABEL_HEIGHT));

        auto ioProcessorWidth = parentViewport.getWidth() - TracksStateManager::TRACK_LABEL_HEIGHT * 2;
        int trackXOffset = parentViewport.getViewPositionX() + TracksStateManager::TRACK_LABEL_HEIGHT;
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

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const override {
        if (audioInputProcessor && nodeId == audioInputProcessor->getNodeId()) return audioInputProcessor.get();
        else if (audioOutputProcessor && nodeId == audioOutputProcessor->getNodeId()) return audioOutputProcessor.get();
        else if (auto *midiInputProcessor = findMidiInputProcessorForNodeId(nodeId)) return midiInputProcessor;
        else if (auto *midiOutputProcessor = findMidiOutputProcessorForNodeId(nodeId)) return midiOutputProcessor;
        else return tracks->getProcessorForNodeId(nodeId);
    }

private:
    ProcessorGraph &graph;
    Project& project;
    TracksStateManager& tracksManager;
    ViewStateManager& viewManager;

    const AudioProcessorGraph::Connection EMPTY_CONNECTION {{ProcessorGraph::NodeID(0), 0}, {ProcessorGraph::NodeID(0), 0}};
    std::unique_ptr<GraphEditorConnectors> connectors;
    GraphEditorConnector *draggingConnector {};
    std::unique_ptr<GraphEditorTracks> tracks;
    std::unique_ptr<GraphEditorProcessor> audioInputProcessor;
    std::unique_ptr<GraphEditorProcessor> audioOutputProcessor;
    OwnedArray<GraphEditorProcessor> midiInputProcessors;
    OwnedArray<GraphEditorProcessor> midiOutputProcessors;

    AudioProcessorGraph::Connection initialDraggingConnection { EMPTY_CONNECTION };

    GraphEditorProcessor::ElementComparator processorComparator;

    DrawableRectangle unfocusOverlay;

    // Ideally we wouldn't call up to the parent to manage its view offsets, and the parent would instead manage
    // all of this stuff on its own. However, I don't think that cleanliness is quite worth adding yet another
    // project state listener.
    Viewport &parentViewport;

    int getTrackWidth() { return (parentViewport.getWidth() - TracksStateManager::TRACK_LABEL_HEIGHT * 2) / ViewStateManager::NUM_VISIBLE_TRACKS; }

    int getProcessorHeight() { return (parentViewport.getHeight() - TracksStateManager::TRACK_LABEL_HEIGHT) / (ViewStateManager::NUM_VISIBLE_PROCESSOR_SLOTS + 1); }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        if (auto *pin = audioInputProcessor->findPinAt(e))
            return pin;
        else if ((pin = audioOutputProcessor->findPinAt(e)))
            return pin;
        for (auto *midiInputProcessor : midiInputProcessors) {
            if (auto* pin = midiInputProcessor->findPinAt(e))
                return pin;
        }
        for (auto *midiOutputProcessor : midiOutputProcessors) {
            if (auto* pin = midiOutputProcessor->findPinAt(e))
                return pin;
        }
        return tracks->findPinAt(e);
    }

    GraphEditorProcessor* findMidiInputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *midiInputProcessor : midiInputProcessors) {
            if (midiInputProcessor->getNodeId() == nodeId)
                return midiInputProcessor;
        }
        return nullptr;
    }

    GraphEditorProcessor* findMidiOutputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *midiOutputProcessor : midiOutputProcessors) {
            if (midiOutputProcessor->getNodeId() == nodeId)
                return midiOutputProcessor;
        }
        return nullptr;
    }

    void updateViewPosition() {
        auto masterSlotOffset = viewManager.getMasterViewSlotOffset();
        auto trackSlotOffset = viewManager.getGridViewTrackOffset();
        parentViewport.setViewPosition(
                jmax(masterSlotOffset, trackSlotOffset) * getTrackWidth(),
                viewManager.getGridViewSlotOffset() * getProcessorHeight()
        );
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& i) override {
        if (tree.hasType(IDs::PROCESSOR) && i == IDs::processorSlot) {
            connectors->updateConnectors();
        } else if (i == IDs::numMasterProcessorSlots || i == IDs::numProcessorSlots) {
            resize();
        } else if (i == IDs::gridViewTrackOffset || i == IDs::masterViewSlotOffset) {
            updateViewPosition();
            resized();
        } else if (i == IDs::gridViewSlotOffset) {
            updateViewPosition();
            connectors->updateConnectors();
        } else if (i == IDs::focusedPane) {
            unfocusOverlay.setVisible(!viewManager.isGridPaneFocused());
            unfocusOverlay.toFront(false);
        }
    }

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::TRACK)) {
            resize();
        } else if (child.hasType(IDs::PROCESSOR)) {
            // TODO this should use the project-listener methods (only ProcessorGraph should listen to this directly),
            //  but currently this is an order-dependent snowflake
            if (child[IDs::name] == MidiInputProcessor::name()) {
                auto *midiInputProcessor = new GraphEditorProcessor(project, tracksManager, child, *this, graph);
                addAndMakeVisible(midiInputProcessor);
                midiInputProcessors.addSorted(processorComparator, midiInputProcessor);
                resized();
            } else if (child[IDs::name] == MidiOutputProcessor::name()) {
                auto *midiOutputProcessor = new GraphEditorProcessor(project, tracksManager, child, *this, graph);
                addAndMakeVisible(midiOutputProcessor);
                midiOutputProcessors.addSorted(processorComparator, midiOutputProcessor);
                resized();
            } else if (child[IDs::name] == "Audio Input") {
                addAndMakeVisible(*(audioInputProcessor = std::make_unique<GraphEditorProcessor>(project, tracksManager, child, *this, graph, true)));
                resized();
            } else if (child[IDs::name] == "Audio Output") {
                addAndMakeVisible(*(audioOutputProcessor = std::make_unique<GraphEditorProcessor>(project, tracksManager, child, *this, graph, true)));
                resized();
            }
            connectors->updateConnectors();
        } else if (child.hasType(IDs::CONNECTION)) {
            connectors->updateConnectors();
        } else if (child.hasType(IDs::CHANNEL)) {
            resized();
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::TRACK)) {
            resize();
        } else if (child.hasType(IDs::PROCESSOR)) {
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

    void valueTreeChildOrderChanged(ValueTree& parent, int oldIndex, int newIndex) override {
        if (parent.hasType(IDs::TRACKS)) {
            connectors->updateConnectors();
        }
    }

    void valueTreeParentChanged(ValueTree& treeWhoseParentHasChanged) override {}

    void valueTreeRedirected(ValueTree& treeWhichHasBeenChanged) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
