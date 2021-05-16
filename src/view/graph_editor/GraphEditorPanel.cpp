#include "GraphEditorPanel.h"

#include "view/CustomColourIds.h"
#include "ApplicationPropertiesAndCommandManager.h"

GraphEditorPanel::GraphEditorPanel(ViewState &view, TracksState &tracks, ConnectionsState &connections, InputState &input, OutputState &output, ProcessorGraph &processorGraph, Project &project, PluginManager &pluginManager)
        : view(view), tracks(tracks), connections(connections),
          input(input), output(output), graph(processorGraph), project(project) {
    tracks.addListener(this);
    view.addListener(this);
    connections.addListener(this);
    input.addListener(this);
    output.addListener(this);

    addAndMakeVisible(*(graphEditorTracks = std::make_unique<GraphEditorTracks>(view, tracks, project, processorGraph, pluginManager, *this)));
    addAndMakeVisible(*(connectors = std::make_unique<GraphEditorConnectors>(connections, *this, *this, processorGraph)));
    unfocusOverlay.setFill(findColour(CustomColourIds::unfocusedOverlayColourId));
    addChildComponent(unfocusOverlay);
    addMouseListener(this, true);
}

GraphEditorPanel::~GraphEditorPanel() {
    removeMouseListener(this);
    draggingConnector = nullptr;

    output.removeListener(this);
    input.removeListener(this);
    connections.removeListener(this);
    view.removeListener(this);
    tracks.removeListener(this);
}


void GraphEditorPanel::mouseDown(const MouseEvent &e) {
    const auto &trackAndSlot = trackAndSlotAt(e);
    const auto &track = tracks.getTrack(trackAndSlot.x);
    view.focusOnGridPane();

    bool isSlotSelected = TracksState::isSlotSelected(track, trackAndSlot.y);
    project.setProcessorSlotSelected(track, trackAndSlot.y, !(isSlotSelected && e.mods.isCommandDown()), !(isSlotSelected || e.mods.isCommandDown()));
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2)
        showPopupMenu(track, trackAndSlot.y);

    project.beginDragging(trackAndSlot);
}

void GraphEditorPanel::mouseDrag(const MouseEvent &e) {
    if (project.isCurrentlyDraggingProcessor() && !e.mods.isRightButtonDown())
        project.dragToPosition(trackAndSlotAt(e));
}

void GraphEditorPanel::mouseUp(const MouseEvent &e) {
    const auto &trackAndSlot = trackAndSlotAt(e);
    const auto &track = tracks.getTrack(trackAndSlot.x);
    if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
        // single click deselects other tracks/processors
        project.setProcessorSlotSelected(track, trackAndSlot.y, true);
    }

    if (e.getNumberOfClicks() == 2) {
        const auto &processor = TracksState::getProcessorAtSlot(track, trackAndSlot.y);
        if (processor.isValid() && graph.getAudioProcessorForState(processor)->hasEditor()) {
            tracks.showWindow(processor, PluginWindow::Type::normal);
        }
    }

    project.endDraggingProcessor();
}

void GraphEditorPanel::resized() {
    auto processorHeight = getProcessorHeight();
    view.setProcessorHeight(processorHeight);
    view.setTrackWidth(getTrackWidth());

    auto r = getLocalBounds();
    unfocusOverlay.setRectangle(r.toFloat());
    connectors->setBounds(r);

    auto top = r.removeFromTop(processorHeight);

    graphEditorTracks->setBounds(r.removeFromTop(processorHeight * (ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 2) + ViewState::TRACK_LABEL_HEIGHT + ViewState::TRACK_INPUT_HEIGHT));

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

void GraphEditorPanel::update() { connectors->updateConnectors(); }

void GraphEditorPanel::beginConnectorDrag(AudioProcessorGraph::NodeAndChannel source, AudioProcessorGraph::NodeAndChannel destination, const MouseEvent &e) {
    auto *c = dynamic_cast<GraphEditorConnector *> (e.originalComponent);
    draggingConnector = c;

    if (draggingConnector == nullptr)
        draggingConnector = new GraphEditorConnector(ValueTree(), *this, *this, source, destination);
    else
        initialDraggingConnection = draggingConnector->getConnection();

    addAndMakeVisible(draggingConnector);

    dragConnector(e);
}

void GraphEditorPanel::dragConnector(const MouseEvent &e) {
    if (draggingConnector == nullptr) return;

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

void GraphEditorPanel::endDraggingConnector(const MouseEvent &e) {
    if (draggingConnector == nullptr) return;

    auto newConnection = draggingConnector->getConnection();
    draggingConnector->setTooltip({});
    if (initialDraggingConnection == EMPTY_CONNECTION)
        delete draggingConnector;
    else
        draggingConnector = nullptr;

    if (newConnection != initialDraggingConnection) {
        if (newConnection.source.nodeID.uid != 0 && newConnection.destination.nodeID.uid != 0)
            graph.addConnection(newConnection);
        if (initialDraggingConnection != EMPTY_CONNECTION)
            graph.removeConnection(initialDraggingConnection);
    }
    initialDraggingConnection = EMPTY_CONNECTION;
}

BaseGraphEditorProcessor *GraphEditorPanel::getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
    if (audioInputProcessor && nodeId == audioInputProcessor->getNodeId()) return audioInputProcessor.get();
    else if (audioOutputProcessor && nodeId == audioOutputProcessor->getNodeId()) return audioOutputProcessor.get();
    else if (auto *midiInputProcessor = findMidiInputProcessorForNodeId(nodeId)) return midiInputProcessor;
    else if (auto *midiOutputProcessor = findMidiOutputProcessorForNodeId(nodeId)) return midiOutputProcessor;
    else return graphEditorTracks->getProcessorForNodeId(nodeId);
}

bool GraphEditorPanel::closeAnyOpenPluginWindows() {
    bool wasEmpty = activePluginWindows.isEmpty();
    activePluginWindows.clear();
    return !wasEmpty;
}


GraphEditorChannel *GraphEditorPanel::findChannelAt(const MouseEvent &e) const {
    if (auto *channel = audioInputProcessor->findChannelAt(e))
        return channel;
    if (auto *channel = audioOutputProcessor->findChannelAt(e))
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

LabelGraphEditorProcessor *GraphEditorPanel::findMidiInputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
    for (auto *midiInputProcessor : midiInputProcessors) {
        if (midiInputProcessor->getNodeId() == nodeId)
            return midiInputProcessor;
    }
    return nullptr;
}

LabelGraphEditorProcessor *GraphEditorPanel::findMidiOutputProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
    for (auto *midiOutputProcessor : midiOutputProcessors) {
        if (midiOutputProcessor->getNodeId() == nodeId)
            return midiOutputProcessor;
    }
    return nullptr;
}

ResizableWindow *GraphEditorPanel::getOrCreateWindowFor(ValueTree &processorState, PluginWindow::Type type) {
    auto nodeId = TracksState::getNodeIdForProcessor(processorState);
    for (auto *pluginWindow : activePluginWindows)
        if (TracksState::getNodeIdForProcessor(pluginWindow->processor) == nodeId && pluginWindow->type == type)
            return pluginWindow;

    if (auto *processor = graph.getProcessorWrapperForNodeId(nodeId)->processor)
        return activePluginWindows.add(new PluginWindow(processorState, processor, type));

    return nullptr;
}

void GraphEditorPanel::closeWindowFor(ValueTree &processor) {
    auto nodeId = TracksState::getNodeIdForProcessor(processor);
    for (int i = activePluginWindows.size(); --i >= 0;)
        if (TracksState::getNodeIdForProcessor(activePluginWindows.getUnchecked(i)->processor) == nodeId)
            activePluginWindows.remove(i);
}

juce::Point<int> GraphEditorPanel::trackAndSlotAt(const MouseEvent &e) {
    auto position = e.getEventRelativeTo(graphEditorTracks.get()).position.toInt();
    const auto &track = graphEditorTracks->findTrackAt(position);
    return {tracks.indexOf(track), tracks.findSlotAt(position, track)};
}

static constexpr int
        DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, ENABLE_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
        DISABLE_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
        SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12;

void GraphEditorPanel::showPopupMenu(const ValueTree &track, int slot) {
    PopupMenu menu;
    const auto &processor = TracksState::getProcessorAtSlot(track, slot);
    auto &pluginManager = project.getPluginManager();

    if (processor.isValid()) {
        PopupMenu processorSelectorSubmenu;
        pluginManager.addPluginsToMenu(processorSelectorSubmenu, track);
        menu.addSubMenu("Insert new processor", processorSelectorSubmenu);
        menu.addSeparator();

        if (InternalPluginFormat::isIoProcessor(processor[ProcessorStateIDs::name])) {
            menu.addItem(CONFIGURE_AUDIO_MIDI_MENU_ID, "Configure audio/MIDI IO");
        } else {
            menu.addItem(DELETE_MENU_ID, "Delete this processor");
            menu.addItem(TOGGLE_BYPASS_MENU_ID, "Toggle Bypass");
        }

        menu.addSeparator();
        // todo single, stateful, menu item for enable/disable default connections
        menu.addItem(ENABLE_DEFAULTS_MENU_ID, "Enable default connections");
        menu.addItem(DISABLE_DEFAULTS_MENU_ID, "Disable default connections");
        menu.addItem(DISCONNECT_ALL_MENU_ID, "Disconnect all");
        menu.addItem(DISCONNECT_CUSTOM_MENU_ID, "Disconnect all custom");

        if (graph.getAudioProcessorForState(processor)->hasEditor()) {
            menu.addSeparator();
            menu.addItem(SHOW_PLUGIN_GUI_MENU_ID, "Show plugin GUI");
            menu.addItem(SHOW_ALL_PROGRAMS_MENU_ID, "Show all programs");
        }

        menu.showMenuAsync({}, ModalCallbackFunction::create
                ([this, processor, slot, &pluginManager](int result) {
                    const auto &description = pluginManager.getChosenType(result);
                    if (!description.name.isEmpty()) {
                        project.createProcessor(description, slot);
                        return;
                    }

                    switch (result) {
                        case DELETE_MENU_ID:
                            getCommandManager().invokeDirectly(CommandIDs::deleteSelected, false);
                            break;
                        case TOGGLE_BYPASS_MENU_ID:
                            project.toggleProcessorBypass(processor);
                            break;
                        case ENABLE_DEFAULTS_MENU_ID:
                            project.setDefaultConnectionsAllowed(processor, true);
                            break;
                        case DISABLE_DEFAULTS_MENU_ID:
                            project.setDefaultConnectionsAllowed(processor, false);
                            break;
                        case DISCONNECT_ALL_MENU_ID:
                            graph.disconnectProcessor(processor);
                            break;
                        case DISCONNECT_CUSTOM_MENU_ID:
                            project.disconnectCustom(processor);
                            break;
                        case SHOW_PLUGIN_GUI_MENU_ID:
                            tracks.showWindow(processor, PluginWindow::Type::normal);
                            break;
                        case SHOW_ALL_PROGRAMS_MENU_ID:
                            tracks.showWindow(processor, PluginWindow::Type::programs);
                            break;
                        case CONFIGURE_AUDIO_MIDI_MENU_ID:
                            getCommandManager().invokeDirectly(CommandIDs::showAudioMidiSettings, false);
                            break;
                        default:
                            break;
                    }
                }));
    } else { // no processor in this slot
        pluginManager.addPluginsToMenu(menu, track);

        menu.showMenuAsync({}, ModalCallbackFunction::create([this, slot, &pluginManager](int result) {
            const auto &description = pluginManager.getChosenType(result);
            if (!description.name.isEmpty()) {
                project.createProcessor(description, slot);
            }
        }));
    }
}

void GraphEditorPanel::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    if ((tree.hasType(ProcessorStateIDs::PROCESSOR) && i == ProcessorStateIDs::processorSlot) || i == ViewStateIDs::gridViewSlotOffset) {
        connectors->updateConnectors();
    } else if (i == ViewStateIDs::gridViewTrackOffset || i == ViewStateIDs::masterViewSlotOffset) {
        resized();
    } else if (i == ViewStateIDs::focusedPane) {
        unfocusOverlay.setVisible(!view.isGridPaneFocused());
        unfocusOverlay.toFront(false);
    } else if (i == ProcessorStateIDs::pluginWindowType) {
        const auto type = static_cast<PluginWindow::Type>(int(tree[i]));
        if (type == PluginWindow::Type::none)
            closeWindowFor(tree);
        else if (auto *w = getOrCreateWindowFor(tree, type))
            w->toFront(true);
    } else if (i == ProcessorStateIDs::processorInitialized) {
        if (tree[ProcessorStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName()) {
            auto *midiInputProcessor = new LabelGraphEditorProcessor(tree, view, tracks, graph, *this);
            addAndMakeVisible(midiInputProcessor, 0);
            midiInputProcessors.addSorted(processorComparator, midiInputProcessor);
            resized();
        } else if (tree[ProcessorStateIDs::name] == InternalPluginFormat::getMidiOutputProcessorName()) {
            auto *midiOutputProcessor = new LabelGraphEditorProcessor(tree, view, tracks, graph, *this);
            addAndMakeVisible(midiOutputProcessor, 0);
            midiOutputProcessors.addSorted(processorComparator, midiOutputProcessor);
            resized();
        } else if (tree[ProcessorStateIDs::name] == "Audio Input") {
            addAndMakeVisible(*(audioInputProcessor = std::make_unique<LabelGraphEditorProcessor>(tree, view, tracks, graph, *this)), 0);
            resized();
        } else if (tree[ProcessorStateIDs::name] == "Audio Output") {
            addAndMakeVisible(*(audioOutputProcessor = std::make_unique<LabelGraphEditorProcessor>(tree, view, tracks, graph, *this)), 0);
            resized();
        }
    }
}

void GraphEditorPanel::valueTreeChildAdded(ValueTree &parent, ValueTree &child) {
    if (TrackState::isType(child) || child.hasType(ProcessorStateIDs::PROCESSOR) || child.hasType(ConnectionStateIDs::CONNECTION)) {
        connectors->updateConnectors();
    }
}

void GraphEditorPanel::valueTreeChildRemoved(ValueTree &parent, ValueTree &child, int indexFromWhichChildWasRemoved) {
    if (TrackState::isType(child)) {
        connectors->updateConnectors();
    } else if (child.hasType(ProcessorStateIDs::PROCESSOR)) {
        if (child[ProcessorStateIDs::name] == InternalPluginFormat::getMidiInputProcessorName()) {
            midiInputProcessors.removeObject(findMidiInputProcessorForNodeId(TracksState::getNodeIdForProcessor(child)));
            resized();
        } else if (child[ProcessorStateIDs::name] == InternalPluginFormat::getMidiOutputProcessorName()) {
            midiOutputProcessors.removeObject(findMidiOutputProcessorForNodeId(TracksState::getNodeIdForProcessor(child)));
            resized();
        } else {
            connectors->updateConnectors();
        }
    } else if (child.hasType(ConnectionStateIDs::CONNECTION)) {
        connectors->updateConnectors();
    }
}

void GraphEditorPanel::valueTreeChildOrderChanged(ValueTree &parent, int oldIndex, int newIndex) {
    if (TracksState::isType(parent)) {
        connectors->updateConnectors();
    }
}
