#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "GraphEditorProcessor.h"
#include "GraphEditorProcessorContainer.h"
#include "ConnectorDragListener.h"
#include "ProcessorGraph.h"
#include "GraphEditorPin.h"
#include "view/CustomColourIds.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor>,
                              public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessors(Project& project, const ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              project(project), tracksManager(project.getTracksManager()), viewStateManager(project.getViewStateManager()),
              connectionsStateManager(project.getConnectionStateManager()),
              viewState(viewStateManager.getState()), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
        viewState.addListener(this);
        valueTreePropertyChanged(viewState, isMasterTrack() ? IDs::numMasterProcessorSlots : IDs::numProcessorSlots);
    }

    ~GraphEditorProcessors() override {
        freeObjects();
    }

    bool isMasterTrack() const { return TracksStateManager::isMasterTrack(parent); }

    int getNumAvailableSlots() const { return tracksManager.getNumAvailableSlotsForTrack(parent); }

    int getSlotOffset() const { return tracksManager.getSlotOffsetForTrack(parent); }

    void mouseDown(const MouseEvent &e) override {
        int slot = findSlotAt(e.getEventRelativeTo(this));
        bool isSlotSelected = tracksManager.isSlotSelected(parent, slot);
        if (e.mods.isCommandDown() && isSlotSelected) {
            project.deselectProcessorSlot(parent, slot);
        } else if (!isSlotSelected) {
            // selects and focuses
            project.selectProcessorSlot(parent, slot, !e.mods.isCommandDown());
        } else {
            // mouse-down on something already selected focuses
            project.focusOnProcessorSlot(parent, slot);
        }
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2) {
            showPopupMenu(slot);
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        auto slotOffset = getSlotOffset();
        auto nonMixerCellSize = getNonMixerCellSize();

        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            bool isMixerChannel = slot == processorSlotRectangles.size() - 1;
            if (slot == slotOffset) {
                if (isMasterTrack())
                    r.removeFromLeft(TracksStateManager::TRACK_LABEL_HEIGHT);
                else
                    r.removeFromTop(TracksStateManager::TRACK_LABEL_HEIGHT);
            }
            auto cellSize = isMixerChannel ? nonMixerCellSize * 2 : nonMixerCellSize;
            auto processorBounds = isMasterTrack() ? r.removeFromLeft(cellSize) : r.removeFromTop(cellSize);
            processorSlotRectangles.getUnchecked(slot)->setRectangle(processorBounds.reduced(1).toFloat());
            if (auto *processor = findProcessorAtSlot(slot)) {
                processor->setBounds(processorBounds);
            }
        }
    }

    void updateProcessorSlotColours() {
        const auto& baseColour = findColour(ResizableWindow::backgroundColourId);
        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            auto fillColour = baseColour.brighter(0.13);
            if (viewStateManager.isProcessorSlotInView(parent, slot)) {
                fillColour = fillColour.brighter(0.3);
                if (tracksManager.isTrackSelected(parent))
                    fillColour = fillColour.brighter(0.2);
                if (tracksManager.isSlotSelected(parent, slot))
                        fillColour = tracksManager.getTrackColour(parent);
            }
            processorSlotRectangles.getUnchecked(slot)->setFill(fillColour);
        }
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &tree) override {
        GraphEditorProcessor *processor = currentlyMovingProcessor != nullptr
                                          ? currentlyMovingProcessor
                                          : new GraphEditorProcessor(project, tracksManager, tree, connectorDragListener, graph);
        addAndMakeVisible(processor);
        return processor;
    }

    void deleteObject(GraphEditorProcessor *processor) override {
        if (currentlyMovingProcessor == nullptr) {
            delete processor;
        } else {
            removeChildComponent(processor);
        }
    }

    void newObjectAdded(GraphEditorProcessor *processor) override { processor->addMouseListener(this, true); resized(); }

    void objectRemoved(GraphEditorProcessor *processor) override { processor->removeMouseListener(this); resized(); }

    void objectOrderChanged() override { resized(); }

    GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *processor : objects) {
            if (processor->getNodeId() == nodeId) {
                return processor;
            }
        }
        return nullptr;
    }

    GraphEditorPin *findPinAt(const MouseEvent &e) const {
        for (auto *processor : objects) {
            if (auto* pin = processor->findPinAt(e)) {
                return pin;
            }
        }
        return nullptr;
    }

    int findSlotAt(const MouseEvent &e) {
        const MouseEvent &relative = e.getEventRelativeTo(this);
        return findSlotAt(relative.getPosition());
    }

    GraphEditorProcessor *getCurrentlyMovingProcessor() const {
        return currentlyMovingProcessor;
    }

    void setCurrentlyMovingProcessor(GraphEditorProcessor *currentlyMovingProcessor) {
        this->currentlyMovingProcessor = currentlyMovingProcessor;
    }

private:
    static constexpr int
            DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, ENABLE_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
            DISABLE_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
            SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12,
            ADD_MIXER_CHANNEL_MENU_ID = 13;

    Project &project;
    TracksStateManager &tracksManager;
    ViewStateManager &viewStateManager;
    ConnectionsStateManager &connectionsStateManager;
    ValueTree viewState;

    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    GraphEditorProcessor *currentlyMovingProcessor {};

    OwnedArray<DrawableRectangle> processorSlotRectangles;

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (isSuitableType(tree) && i == IDs::processorSlot) {
            resized();
        } else if ((tree.hasType(IDs::TRACK) && i == IDs::selected) || i == IDs::gridViewTrackOffset) {
            updateProcessorSlotColours();
        } else if (i == IDs::gridViewSlotOffset || (i == IDs::masterViewSlotOffset && isMasterTrack())) {
            resized();
            updateProcessorSlotColours();
        } else if (i == IDs::numProcessorSlots || (i == IDs::numMasterProcessorSlots && isMasterTrack())) {
            auto numSlots = getNumAvailableSlots();
            while (processorSlotRectangles.size() < numSlots) {
                auto* rect = new DrawableRectangle();
                processorSlotRectangles.add(rect);
                addAndMakeVisible(rect);
                rect->setCornerSize({3, 3});
                rect->toBack();
            }
            processorSlotRectangles.removeLast(processorSlotRectangles.size() - numSlots, true);
            resized();
            updateProcessorSlotColours();
        } else if (i == IDs::selectedSlotsMask) {
            updateProcessorSlotColours();
        }

        Utilities::ValueTreeObjectList<GraphEditorProcessor>::valueTreePropertyChanged(tree, i);
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &tree) override {
        ValueTreeObjectList::valueTreeChildAdded(parent, tree);
        if (this->parent == parent && isSuitableType(tree)) {
            resized();
        }
    }

    GraphEditorProcessor* findProcessorAtSlot(int slot) const {
        for (auto* processor : objects) {
            if (processor->getSlot() == slot)
                return processor;
        }
        return nullptr;
    }

    void showPopupMenu(int slot) {
        PopupMenu menu;
        auto* processor = findProcessorAtSlot(slot);
        bool isMixerChannel = slot == tracksManager.getMixerChannelSlotForTrack(parent);

        if (processor != nullptr) {
            if (!isMixerChannel) {
                PopupMenu processorSelectorSubmenu;
                project.addPluginsToMenu(processorSelectorSubmenu, parent);
                menu.addSubMenu("Insert new processor", processorSelectorSubmenu);
                menu.addSeparator();
            }

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

            if (processor->getAudioProcessor()->hasEditor()) {
                menu.addSeparator();
                menu.addItem(SHOW_PLUGIN_GUI_MENU_ID, "Show plugin GUI");
                menu.addItem(SHOW_ALL_PROGRAMS_MENU_ID, "Show all programs");
            }

            menu.showMenuAsync({}, ModalCallbackFunction::create
                    ([this, processor, slot](int result) {
                        if (auto *description = project.getChosenType(result)) {
                            project.createProcessor(*description, slot);
                            return;
                        }
                        // TODO remove direct graph dependency and do everything through state managers instead
                        switch (result) {
                            case DELETE_MENU_ID:
                                getCommandManager().invokeDirectly(CommandIDs::deleteSelected, false);
                                break;
                            case TOGGLE_BYPASS_MENU_ID:
                                processor->toggleBypass();
                                break;
                            case ENABLE_DEFAULTS_MENU_ID:
                                project.setDefaultConnectionsAllowed(processor->getState(), true);
                                break;
                            case DISABLE_DEFAULTS_MENU_ID:
                                project.setDefaultConnectionsAllowed(processor->getState(), false);
                                break;
                            case DISCONNECT_ALL_MENU_ID:
                                project.disconnectProcessor(processor->getState());
                                break;
                            case DISCONNECT_CUSTOM_MENU_ID:
                                project.disconnectCustom(processor->getState());
                                break;
                            case SHOW_PLUGIN_GUI_MENU_ID:
                                processor->showWindow(PluginWindow::Type::normal);
                                break;
                            case SHOW_ALL_PROGRAMS_MENU_ID:
                                processor->showWindow(PluginWindow::Type::programs);
                                break;
                            case CONFIGURE_AUDIO_MIDI_MENU_ID:
                                getCommandManager().invokeDirectly(CommandIDs::showAudioMidiSettings, false);
                                break;
                            default:
                                break;
                        }
                    }));
        } else { // no processor in this slot
            if (isMixerChannel) {
                menu.addItem(ADD_MIXER_CHANNEL_MENU_ID, "Add mixer channel");
            } else {
                project.addPluginsToMenu(menu, parent);
            }

            menu.showMenuAsync({}, ModalCallbackFunction::create([this, slot, isMixerChannel](int result) {
                if (isMixerChannel) {
                    if (result == ADD_MIXER_CHANNEL_MENU_ID) {
                        getCommandManager().invokeDirectly(CommandIDs::addMixerChannel, false);
                    }
                } else {
                    if (auto *description = project.getChosenType(result)) {
                        project.createProcessor(*description, slot);
                    }
                }
            }));
        }
    }

    int getNonMixerCellSize() const {
        return isMasterTrack() ? tracksManager.getTrackWidth() : tracksManager.getProcessorHeight();
    }

    int findSlotAt(const Point<int> relativePosition) const {
        int length = isMasterTrack() ? relativePosition.x : relativePosition.y;
        int slot = (length - TracksStateManager::TRACK_LABEL_HEIGHT) / getNonMixerCellSize();
        return jlimit(0, getNumAvailableSlots() - 1, slot);
    }
};
