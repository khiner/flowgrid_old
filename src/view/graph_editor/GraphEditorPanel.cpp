#include "GraphEditorPanel.h"

#include "view/CustomColourIds.h"
#include "ApplicationPropertiesAndCommandManager.h"

GraphEditorPanel::GraphEditorPanel(View &view, Tracks &tracks, Connections &connections, Input &input, Output &output, ProcessorGraph &processorGraph, Project &project, PluginManager &pluginManager)
        : view(view), tracks(tracks), connections(connections),
          input(input), output(output), graph(processorGraph), project(project),
          graphEditorInput(input, view, processorGraph, pluginManager, *this),
          graphEditorOutput(output, view, processorGraph, pluginManager, *this) {
    tracks.addTracksListener(this);
    view.addListener(this);
    connections.addListener(this);
    // TODO I think we can get away with only having GraphEditorInput/Output listening (needed for connections updates now)
    input.addListener(this);
    output.addListener(this);

    addAndMakeVisible(graphEditorInput);
    addAndMakeVisible(graphEditorOutput);
    addAndMakeVisible(*(graphEditorTracks = std::make_unique<GraphEditorTracks>(view, tracks, project, processorGraph.getProcessorWrappers(), pluginManager, *this)));
    addAndMakeVisible(*(connectors = std::make_unique<GraphEditorConnectors>(connections, *this, *this)));
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
    tracks.removeTracksListener(this);
}


void GraphEditorPanel::mouseDown(const MouseEvent &e) {
    const auto &trackAndSlot = trackAndSlotAt(e);
    auto *track = tracks.getChild(trackAndSlot.x);

    view.focusOnGridPane();

    bool isSlotSelected = track->isSlotSelected(trackAndSlot.y);
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
    auto *track = tracks.getChild(trackAndSlot.x);
    if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
        // single click deselects other tracks/processors
        project.setProcessorSlotSelected(track, trackAndSlot.y, true);
    }

    if (e.getNumberOfClicks() == 2) {
        auto *processor = track->getProcessorAtSlot(trackAndSlot.y);
        if (processor != nullptr && graph.getProcessorWrappers().getAudioProcessorForProcessor(processor)->hasEditor()) {
            tracks.showWindow(processor, PluginWindowType::normal);
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
    graphEditorTracks->setBounds(r.removeFromTop(processorHeight * (View::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + 2) + View::TRACK_LABEL_HEIGHT + View::TRACK_INPUT_HEIGHT));

    auto ioProcessorWidth = getWidth() - View::TRACKS_MARGIN * 2;
    int trackXOffset = View::TRACKS_MARGIN;
    top.setX(trackXOffset);
    top.setWidth(ioProcessorWidth);
    graphEditorInput.setBounds(top);

    auto bottom = r.removeFromTop(processorHeight);
    bottom.setX(trackXOffset);
    bottom.setWidth(ioProcessorWidth);
    graphEditorOutput.setBounds(bottom);

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
    if (auto *inputProcessor = graphEditorInput.findProcessorForNodeId(nodeId)) return inputProcessor;
    if (auto *outputProcessor = graphEditorOutput.findProcessorForNodeId(nodeId)) return outputProcessor;
    return graphEditorTracks->getProcessorForNodeId(nodeId);
}

bool GraphEditorPanel::closeAnyOpenPluginWindows() {
    bool wasEmpty = activePluginWindows.isEmpty();
    activePluginWindows.clear();
    return !wasEmpty;
}


GraphEditorChannel *GraphEditorPanel::findChannelAt(const MouseEvent &e) const {
    if (auto *channel = graphEditorInput.findChannelAt(e)) return channel;
    if (auto *channel = graphEditorOutput.findChannelAt(e)) return channel;
    return graphEditorTracks->findChannelAt(e);
}

ResizableWindow *GraphEditorPanel::getOrCreateWindowFor(Processor *processor, PluginWindowType type) {
    auto nodeId = processor->getNodeId();
    for (auto *pluginWindow : activePluginWindows)
        if (pluginWindow->processor->getNodeId() == nodeId && pluginWindow->type == type)
            return pluginWindow;

    if (auto *audioProcessor = graph.getProcessorWrappers().getProcessorWrapperForNodeId(nodeId)->audioProcessor)
        return activePluginWindows.add(new PluginWindow(processor, audioProcessor, type));

    return nullptr;
}

void GraphEditorPanel::closeWindowFor(const Processor *processor) {
    auto nodeId = processor->getNodeId();
    for (int i = activePluginWindows.size(); --i >= 0;)
        if (activePluginWindows.getUnchecked(i)->processor->getNodeId() == nodeId)
            activePluginWindows.remove(i);
}

juce::Point<int> GraphEditorPanel::trackAndSlotAt(const MouseEvent &e) {
    auto position = e.getEventRelativeTo(graphEditorTracks.get()).position.toInt();
    auto *track = graphEditorTracks->findTrackAt(position);
    return {tracks.indexOf(track), tracks.findSlotAt(position, track)};
}

static constexpr int
        DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, ENABLE_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
        DISABLE_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
        SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12;

void GraphEditorPanel::showPopupMenu(const Track *track, int slot) {
    PopupMenu menu;
    auto *processor = track->getProcessorAtSlot(slot);
    auto &pluginManager = project.getPluginManager();

    if (processor != nullptr) {
        PopupMenu processorSelectorSubmenu;
        pluginManager.addPluginsToMenu(processorSelectorSubmenu);
        menu.addSubMenu("Insert new processor", processorSelectorSubmenu);
        menu.addSeparator();

        if (processor->isIoProcessor()) {
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

        if (graph.getProcessorWrappers().getAudioProcessorForProcessor(processor)->hasEditor()) {
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
                            tracks.showWindow(processor, PluginWindowType::normal);
                            break;
                        case SHOW_ALL_PROGRAMS_MENU_ID:
                            tracks.showWindow(processor, PluginWindowType::programs);
                            break;
                        case CONFIGURE_AUDIO_MIDI_MENU_ID:
                            getCommandManager().invokeDirectly(CommandIDs::showAudioMidiSettings, false);
                            break;
                        default:
                            break;
                    }
                }));
    } else { // no processor in this slot
        pluginManager.addPluginsToMenu(menu);

        menu.showMenuAsync({}, ModalCallbackFunction::create([this, slot, &pluginManager](int result) {
            const auto &description = pluginManager.getChosenType(result);
            if (!description.name.isEmpty()) {
                project.createProcessor(description, slot);
            }
        }));
    }
}
