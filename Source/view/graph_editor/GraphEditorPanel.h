#pragma once

#include <ProcessorGraph.h>
#include "GraphEditorPin.h"
#include "GraphEditorTracks.h"
#include "GraphEditorConnectors.h"

class GraphEditorPanel
        : public Component, public ConnectorDragListener, public GraphEditorProcessorContainer,
          private ValueTree::Listener {
public:
    GraphEditorPanel(ProcessorGraph &g, Project &project, Viewport &parentViewport)
            : graph(g), project(project), parentViewport(parentViewport) {
        project.getState().addListener(this);

        addAndMakeVisible(*(tracks = std::make_unique<GraphEditorTracks>(project, project.getTracks(), *this, graph)));
        addAndMakeVisible(*(connectors = std::make_unique<GraphEditorConnectors>(project.getConnections(), *this, *this, graph)));
    }

    ~GraphEditorPanel() override {
        draggingConnector = nullptr;
    }

    // Call this method when the parent viewport size has changed or when the number of tracks has changed.
    void resize() {
        project.setProcessorHeight(getProcessorHeight());
        project.setTrackWidth(getTrackWidth());
        setSize(getTrackWidth() * jmax(Project::NUM_VISIBLE_TRACKS, project.getNumNonMasterTracks(), project.getNumMasterProcessorSlots()) + GraphEditorTrack::LABEL_HEIGHT * 2,
                getProcessorHeight() * (jmax(Project::NUM_VISIBLE_TRACK_PROCESSOR_SLOTS, project.getNumTrackProcessorSlots()) + 3) + GraphEditorTrack::LABEL_HEIGHT);
        updateViewPosition();
    }

    void resized() override {
        auto r = getLocalBounds();
        connectors->setBounds(r);

        auto processorHeight = getProcessorHeight();
        auto top = r.removeFromTop(processorHeight);

        tracks->setBounds(r.removeFromTop(processorHeight * (project.getNumTrackProcessorSlots() + 1) + GraphEditorTrack::LABEL_HEIGHT));

        auto ioProcessorWidth = parentViewport.getWidth() - GraphEditorTrack::LABEL_HEIGHT * 2;
        int trackXOffset = parentViewport.getViewPositionX() + GraphEditorTrack::LABEL_HEIGHT;
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
        updateComponents();
    }

    void update() override {
        updateComponents();
    }

    void updateComponents() {
        if (audioInputProcessor != nullptr)
            audioInputProcessor->update();
        if (audioOutputProcessor != nullptr)
            audioOutputProcessor->update();
        for (auto* midiInputProcessor : midiInputProcessors) midiInputProcessor->update();
        for (auto* midiOutputProcessor : midiOutputProcessors) midiOutputProcessor->update();
        tracks->updateProcessors();
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
            if (connection.source.nodeID == 0 && !pin->isInput())
                connection.source = pin->getPin();
            else if (connection.destination.nodeID == 0 && pin->isInput())
                connection.destination = pin->getPin();

            if (graph.canConnect(connection) || graph.isConnected(connection)) {
                pos = getLocalPoint(pin->getParentComponent(), pin->getBounds().getCentre()).toFloat();
                draggingConnector->setTooltip(pin->getTooltip());
            }
        }

        if (draggingConnector->getConnection().source.nodeID == 0)
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
            if (newConnection.source.nodeID == 0) {
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
            graph.addConnection(newConnection);

        if (initialDraggingConnection != EMPTY_CONNECTION) {
            graph.removeConnection(initialDraggingConnection);
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

    ProcessorGraph &graph;
    Project &project;

private:
    const AudioProcessorGraph::Connection EMPTY_CONNECTION {{0, 0}, {0, 0}};
    std::unique_ptr<GraphEditorConnectors> connectors;
    GraphEditorConnector *draggingConnector {};
    std::unique_ptr<GraphEditorTracks> tracks;
    std::unique_ptr<GraphEditorProcessor> audioInputProcessor;
    std::unique_ptr<GraphEditorProcessor> audioOutputProcessor;
    OwnedArray<GraphEditorProcessor> midiInputProcessors;
    OwnedArray<GraphEditorProcessor> midiOutputProcessors;

    AudioProcessorGraph::Connection initialDraggingConnection { EMPTY_CONNECTION };

    GraphEditorProcessor::ElementComparator processorComparator;

    // Ideally we wouldn't call up to the parent to manage its view offsets, and the parent would instead manage
    // all of this stuff on its own. However, I don't think that cleanliness is quite worth adding yet another
    // project state listener.
    Viewport &parentViewport;

    int getTrackWidth() { return (parentViewport.getWidth() - GraphEditorTrack::LABEL_HEIGHT * 2) / Project::NUM_VISIBLE_TRACKS; }

    int getProcessorHeight() { return (parentViewport.getHeight() - GraphEditorTrack::LABEL_HEIGHT) / Project::NUM_VISIBLE_PROCESSOR_SLOTS; }
    
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
        auto masterSlotOffset = project.getMasterViewSlotOffset();
        auto trackSlotOffset = project.getGridViewTrackOffset();
        parentViewport.setViewPosition(
                jmax(masterSlotOffset, trackSlotOffset) * getTrackWidth(),
                project.getGridViewSlotOffset() * getProcessorHeight()
        );
    }

    void valueTreePropertyChanged(ValueTree& tree, const Identifier& i) override {
        if (tree.hasType(IDs::PROCESSOR) && i == IDs::processorSlot) {
            updateComponents();
        } else if (i == IDs::numMasterProcessorSlots || i == IDs::numProcessorSlots) {
            resize();
        } else if (i == IDs::gridViewTrackOffset) {
            updateViewPosition();
            tracks->masterSlotOffsetChanged();
            resized();
        } else if (i == IDs::gridViewSlotOffset) {
            updateViewPosition();
            tracks->slotOffsetChanged();
            connectors->updateConnectors();
        } else if (i == IDs::masterViewSlotOffset) {
            updateViewPosition();
            tracks->masterSlotOffsetChanged();
            resized();
        }
    }

    void valueTreeChildAdded(ValueTree& parent, ValueTree& child) override {
        if (child.hasType(IDs::TRACK)) {
            resize();
        } else if (child.hasType(IDs::PROCESSOR)) {
            if (child[IDs::name] == MidiInputProcessor::name()) {
                auto *midiInputProcessor = new GraphEditorProcessor(child, *this, graph);
                addAndMakeVisible(midiInputProcessor);
                midiInputProcessors.addSorted(processorComparator, midiInputProcessor);
                resized();
            } else if (child[IDs::name] == MidiOutputProcessor::name()) {
                auto *midiOutputProcessor = new GraphEditorProcessor(child, *this, graph);
                addAndMakeVisible(midiOutputProcessor);
                midiOutputProcessors.addSorted(processorComparator, midiOutputProcessor);
                resized();
            } else if (child[IDs::name] == "Audio Input") {
                addAndMakeVisible(*(audioInputProcessor = std::make_unique<GraphEditorProcessor>(child, *this, graph, true)));
                resized();
            } else if (child[IDs::name] == "Audio Output") {
                addAndMakeVisible(*(audioOutputProcessor = std::make_unique<GraphEditorProcessor>(child, *this, graph, true)));
                resized();
            } else {
                updateComponents();
            }
        } else if (child.hasType(IDs::CONNECTION)) {
            connectors->updateConnectors();
        }
    }

    void valueTreeChildRemoved(ValueTree& parent, ValueTree& child, int indexFromWhichChildWasRemoved) override {
        if (child.hasType(IDs::TRACK)) {
            resize();
        } else if (child.hasType(IDs::PROCESSOR)) {
            if (child[IDs::name] == MidiInputProcessor::name()) {
                midiInputProcessors.removeObject(findMidiInputProcessorForNodeId(ProcessorGraph::getNodeIdForState(child)));
            } else if (child[IDs::name] == MidiOutputProcessor::name()) {
                midiOutputProcessors.removeObject(findMidiOutputProcessorForNodeId(ProcessorGraph::getNodeIdForState(child)));
            }
            updateComponents();
        } else if (child.hasType(IDs::CONNECTION)) {
            connectors->updateConnectors();
        }
    }

    void valueTreeChildOrderChanged(ValueTree& parent, int oldIndex, int newIndex) override {
        if (parent.hasType(IDs::TRACKS) || parent.hasType(IDs::TRACK)) {
            updateComponents();
        }
    }

    void valueTreeParentChanged(ValueTree& treeWhoseParentHasChanged) override {}

    void valueTreeRedirected(ValueTree& treeWhichHasBeenChanged) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorPanel)
};
