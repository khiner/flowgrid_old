#pragma once

#include <ValueTreeObjectList.h>
#include <state/Project.h>
#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "GraphEditorProcessorContainer.h"
#include "ConnectorDragListener.h"
#include "GraphEditorChannel.h"
#include "view/CustomColourIds.h"
#include "view/graph_editor/processor/ParameterPanelGraphEditorProcessor.h"

class GraphEditorProcessorLane : public Component,
                                 public Utilities::ValueTreeObjectList<BaseGraphEditorProcessor>,
                                 public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessorLane(Project &project, const ValueTree &state, ConnectorDragListener &connectorDragListener)
            : Utilities::ValueTreeObjectList<BaseGraphEditorProcessor>(state),
              project(project), state(state), tracks(project.getTracks()), view(project.getView()),
              connections(project.getConnections()),
              pluginManager(project.getPluginManager()), connectorDragListener(connectorDragListener) {
        rebuildObjects();
        view.addListener(this);
        // TODO shouldn't need to do this
        valueTreePropertyChanged(view.getState(), isMasterTrack() ? IDs::numMasterProcessorSlots : IDs::numProcessorSlots);
    }

    ~GraphEditorProcessorLane() override {
        view.removeListener(this);
        freeObjects();
    }

    const ValueTree &getState() const { return state; }

    ValueTree getTrack() const {
        return parent.getParent().getParent();
    }

    bool isMasterTrack() const { return TracksState::isMasterTrack(getTrack()); }

    int getNumSlots() const { return view.getNumSlotsForTrack(getTrack()); }

    int getSlotOffset() const { return view.getSlotOffsetForTrack(getTrack()); }

    void mouseDown(const MouseEvent &e) override {
        int slot = findSlotAt(e.getEventRelativeTo(this));
        bool isSlotSelected = TracksState::isSlotSelected(getTrack(), slot);
        project.setProcessorSlotSelected(getTrack(), slot, !(isSlotSelected && e.mods.isCommandDown()), !(isSlotSelected || e.mods.isCommandDown()));
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2)
            showPopupMenu(slot);
    }

    void mouseUp(const MouseEvent &e) override {
        if (e.mouseWasClicked() && !e.mods.isCommandDown()) {
            // single click deselects other tracks/processors
            project.setProcessorSlotSelected(getTrack(), findSlotAt(e.getEventRelativeTo(this)), true);
        }
    }

    void resized() override {
        auto slotOffset = getSlotOffset();
        auto processorSlotSize = view.getProcessorSlotSize(getTrack());
        auto r = getLocalBounds();
        if (isMasterTrack()) {
            r.setWidth(processorSlotSize);
            r.setX(-slotOffset * processorSlotSize - ViewState::TRACK_LABEL_HEIGHT);
        } else {
            r.setHeight(processorSlotSize);
            r.setY(-slotOffset * processorSlotSize - ViewState::TRACK_LABEL_HEIGHT);
        }

        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            if (slot == slotOffset) {
                if (isMasterTrack())
                    r.setX(r.getX() + ViewState::TRACK_LABEL_HEIGHT);
                else
                    r.setY(r.getY() + ViewState::TRACK_LABEL_HEIGHT);
            } else if (slot == slotOffset + ViewState::NUM_VISIBLE_NON_MASTER_TRACK_SLOTS + (isMasterTrack() ? 1 : 0)) {
                if (isMasterTrack())
                    r.setX(r.getRight());
                else
                    r.setY(r.getBottom());
            }
            processorSlotRectangles.getUnchecked(slot)->setRectangle(r.reduced(1).toFloat());
            if (auto *processor = findProcessorAtSlot(slot))
                processor->setBounds(r);
            if (isMasterTrack())
                r.setX(r.getRight());
            else
                r.setY(r.getBottom());
        }
    }

    void updateProcessorSlotColours() {
        const static auto &baseColour = findColour(ResizableWindow::backgroundColourId).brighter(0.4);
        const auto &track = getTrack();

        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            auto fillColour = baseColour;
            if (TracksState::doesTrackHaveSelections(track))
                fillColour = fillColour.brighter(0.2);
            if (TracksState::isSlotSelected(track, slot))
                fillColour = TracksState::getTrackColour(track);
            if (tracks.isSlotFocused(track, slot))
                fillColour = fillColour.brighter(0.16);
            if (!view.isProcessorSlotInView(track, slot))
                fillColour = fillColour.darker(0.3);
            processorSlotRectangles.getUnchecked(slot)->setFill(fillColour);
        }
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    BaseGraphEditorProcessor *createEditorForProcessor(const ValueTree &processor) {
        if (processor[IDs::name] == MixerChannelProcessor::name()) {
            return new ParameterPanelGraphEditorProcessor(project, tracks, view, processor, connectorDragListener);
        }
        return new LabelGraphEditorProcessor(project, tracks, view, processor, connectorDragListener);
    }

    BaseGraphEditorProcessor *createNewObject(const ValueTree &processor) override {
        auto *graphEditorProcessor = currentlyMovingProcessor != nullptr ? currentlyMovingProcessor : createEditorForProcessor(processor);
        addAndMakeVisible(graphEditorProcessor);
        return graphEditorProcessor;
    }

    void deleteObject(BaseGraphEditorProcessor *graphEditorProcessor) override {
        if (currentlyMovingProcessor == nullptr)
            delete graphEditorProcessor;
        else
            removeChildComponent(graphEditorProcessor);
    }

    void newObjectAdded(BaseGraphEditorProcessor *processor) override {
        processor->addMouseListener(this, true);
        resized();
    }

    void objectRemoved(BaseGraphEditorProcessor *processor) override {
        processor->removeMouseListener(this);
        resized();
    }

    void objectOrderChanged() override { resized(); }

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        if (auto *currentlyMovingProcessor = getCurrentlyMovingProcessor())
            if (currentlyMovingProcessor->getNodeId() == nodeId)
                return currentlyMovingProcessor;

        for (auto *processor : objects)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const {
        for (auto *processor : objects)
            if (auto *channel = processor->findChannelAt(e))
                return channel;
        return nullptr;
    }

    int findSlotAt(const MouseEvent &e) {
        return view.findSlotAt(e.getEventRelativeTo(this->getParentComponent()).getPosition(), getTrack());
    }

    BaseGraphEditorProcessor *getCurrentlyMovingProcessor() const {
        return currentlyMovingProcessor;
    }

    void setCurrentlyMovingProcessor(BaseGraphEditorProcessor *currentlyMovingProcessor) {
        if (objects.contains(currentlyMovingProcessor))
            this->currentlyMovingProcessor = currentlyMovingProcessor;
    }

private:
    static constexpr int
            DELETE_MENU_ID = 1, TOGGLE_BYPASS_MENU_ID = 2, ENABLE_DEFAULTS_MENU_ID = 3, DISCONNECT_ALL_MENU_ID = 4,
            DISABLE_DEFAULTS_MENU_ID = 5, DISCONNECT_CUSTOM_MENU_ID = 6,
            SHOW_PLUGIN_GUI_MENU_ID = 10, SHOW_ALL_PROGRAMS_MENU_ID = 11, CONFIGURE_AUDIO_MIDI_MENU_ID = 12;

    Project &project;
    ValueTree state;

    TracksState &tracks;
    ViewState &view;
    ConnectionsState &connections;

    PluginManager &pluginManager;
    ConnectorDragListener &connectorDragListener;
    BaseGraphEditorProcessor *currentlyMovingProcessor{};

    OwnedArray<DrawableRectangle> processorSlotRectangles;

    BaseGraphEditorProcessor *findProcessorAtSlot(int slot) const {
        for (auto *processor : objects)
            if (processor->getSlot() == slot)
                return processor;
        return nullptr;
    }

    void showPopupMenu(int slot) {
        PopupMenu menu;
        const auto &track = getTrack();
        auto *processor = findProcessorAtSlot(slot);

        if (processor != nullptr) {
            PopupMenu processorSelectorSubmenu;
            pluginManager.addPluginsToMenu(processorSelectorSubmenu, track);
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

            if (processor->getAudioProcessor()->hasEditor()) {
                menu.addSeparator();
                menu.addItem(SHOW_PLUGIN_GUI_MENU_ID, "Show plugin GUI");
                menu.addItem(SHOW_ALL_PROGRAMS_MENU_ID, "Show all programs");
            }

            menu.showMenuAsync({}, ModalCallbackFunction::create
                    ([this, processor, slot](int result) {
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
                                project.toggleProcessorBypass(processor->getState());
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
            pluginManager.addPluginsToMenu(menu, track);

            menu.showMenuAsync({}, ModalCallbackFunction::create([this, slot](int result) {
                auto &description = pluginManager.getChosenType(result);
                if (!description.name.isEmpty()) {
                    project.createProcessor(description, slot);
                }
            }));
        }
    }

    // TODO only instantiate 64 slot rects (and maybe another set for the boundary perimeter)
    //  might be an over-early optimization though
    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override {
        if (isSuitableType(tree) && i == IDs::processorSlot) {
            resized();
        } else if (i == IDs::selected || i == IDs::colour ||
                   i == IDs::selectedSlotsMask || i == IDs::focusedTrackIndex || i == IDs::focusedProcessorSlot ||
                   i == IDs::gridViewTrackOffset) {
            updateProcessorSlotColours();
        } else if (i == IDs::gridViewSlotOffset || (i == IDs::masterViewSlotOffset && isMasterTrack())) {
            resized();
            updateProcessorSlotColours();
        } else if (i == IDs::numProcessorSlots || (i == IDs::numMasterProcessorSlots && isMasterTrack())) {
            auto numSlots = getNumSlots();
            while (processorSlotRectangles.size() < numSlots) {
                auto *rect = new DrawableRectangle();
                processorSlotRectangles.add(rect);
                addAndMakeVisible(rect);
                rect->setCornerSize({3, 3});
                rect->toBack();
            }
            processorSlotRectangles.removeLast(processorSlotRectangles.size() - numSlots, true);
            resized();
            updateProcessorSlotColours();
        }
        Utilities::ValueTreeObjectList<BaseGraphEditorProcessor>::valueTreePropertyChanged(tree, i);
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &child) override {
        ValueTreeObjectList::valueTreeChildAdded(parent, child);
        if (this->parent == parent && isSuitableType(child))
            resized();
    }
};
