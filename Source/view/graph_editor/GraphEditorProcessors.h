#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "GraphEditorProcessor.h"
#include "GraphEditorProcessorContainer.h"
#include "ConnectorDragListener.h"
#include "ProcessorGraph.h"
#include "GraphEditorPin.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor>,
                              public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessors(Project& project, const ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              viewState(project.getViewState()), project(project), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
        for (auto* object : objects) {
            if (object->isSelected())
                mostRecentlySelectedProcessor = object;
        }
        viewState.addListener(this);
        valueTreePropertyChanged(viewState, isMasterTrack() ? IDs::numMasterProcessorSlots : IDs::numProcessorSlots);
    }

    ~GraphEditorProcessors() override {
        freeObjects();
    }

    bool isMasterTrack() const { return parent.hasProperty(IDs::isMasterTrack); }

    int getNumAvailableSlots() const { return project.getNumAvailableSlotsForTrack(parent); }

    int getSlotOffset() const { return project.getSlotOffsetForTrack(parent); }

    void mouseDown(const MouseEvent &e) override {
        setSelected(true);
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2) {
            showPopupMenu(e.position.toInt());
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        auto slotOffset = getSlotOffset();
        auto nonMixerCellSize = getNonMixerCellSize(), mixerCellSize = getMixerCellSize();

        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            bool isMixerChannel = slot == processorSlotRectangles.size() - 1;
            if (slot == slotOffset) {
                if (isMasterTrack())
                    r.removeFromLeft(Project::TRACK_LABEL_HEIGHT);
                else
                    r.removeFromTop(Project::TRACK_LABEL_HEIGHT);
            }
            auto cellSize = isMixerChannel ? mixerCellSize : nonMixerCellSize;
            auto processorBounds = isMasterTrack() ? r.removeFromLeft(cellSize) : r.removeFromTop(cellSize);
            processorSlotRectangles.getUnchecked(slot)->setRectangle(processorBounds.reduced(1).toFloat());
            if (auto *processor = findProcessorAtSlot(slot)) {
                processor->setBounds(processorBounds);
            }
        }
    }

    void updateProcessorSlotColours() {
        for (int slot = 0; slot < processorSlotRectangles.size(); slot++) {
            static const auto& baseColour = findColour(TextEditor::backgroundColourId);
            auto fillColour = baseColour.brighter(0.13);
            if (project.isProcessorSlotInView(parent, slot)) {
                fillColour = fillColour.brighter(0.3);
                if (project.isTrackSelected(parent))
                    fillColour = fillColour.brighter(0.2);
                if (auto *processor = findProcessorAtSlot(slot)) {
                    if (processor->isSelected())
                        fillColour = processor->getTrackColour();
                }
            }
            processorSlotRectangles.getUnchecked(slot)->setFill(fillColour);
        }
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &v) override {
        GraphEditorProcessor *processor = currentlyMovingProcessor != nullptr
                                          ? currentlyMovingProcessor
                                          : new GraphEditorProcessor(project, v, connectorDragListener, graph);
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

    void newObjectAdded(GraphEditorProcessor *) override { resized(); }

    void objectRemoved(GraphEditorProcessor *object) override {
        resized();
        if (object == mostRecentlySelectedProcessor) {
            mostRecentlySelectedProcessor = nullptr;
            setSelected(currentlyMovingProcessor == nullptr);
        }
    }

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

    void setSelected(bool selected, ValueTree::Listener *listenerToExclude=nullptr) {
        if (selected) {
            if (mostRecentlySelectedProcessor != nullptr) {
                mostRecentlySelectedProcessor->setSelected(true, listenerToExclude);
            } else if (auto *firstProcessor = objects.getFirst()) {
                firstProcessor->setSelected(true, listenerToExclude);
            } else {
                parent.setPropertyExcludingListener(listenerToExclude, IDs::selected, true, nullptr);
            }
        } else {
            for (auto *processor : objects) {
                processor->setSelected(false, listenerToExclude);
            }
        }
    }

private:
    static constexpr int ADD_MIXER_CHANNEL_MENU_ID = 1;

    ValueTree viewState;

    Project &project;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    GraphEditorProcessor *currentlyMovingProcessor {};
    GraphEditorProcessor* mostRecentlySelectedProcessor {};
    OwnedArray<DrawableRectangle> processorSlotRectangles;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (isSuitableType(v)) {
            if (i == IDs::processorSlot) {
                resized();
                updateProcessorSlotColours();
            } else if (i == IDs::selected && v[IDs::selected]) {
                for (auto* processor : objects) {
                    if (processor->getState() == v)
                        mostRecentlySelectedProcessor = processor;
                    else
                        processor->setSelected(false);
                }
                updateProcessorSlotColours();
            }
        } else if ((v.hasType(IDs::TRACK) && i == IDs::selected) || i == IDs::gridViewTrackOffset) {
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
        }

        Utilities::ValueTreeObjectList<GraphEditorProcessor>::valueTreePropertyChanged(v, i);
    }

    void valueTreeChildAdded(ValueTree &parent, ValueTree &tree) override {
        ValueTreeObjectList::valueTreeChildAdded(parent, tree);
        if (this->parent == parent && isSuitableType(tree)) {
            resized();
            updateProcessorSlotColours();
        }
    }

    void valueTreeChildRemoved(ValueTree &exParent, ValueTree &tree, int index) override {
        ValueTreeObjectList::valueTreeChildRemoved(exParent, tree, index);
        if (parent == exParent && isSuitableType(tree)) {
            updateProcessorSlotColours();
        }
    }

    GraphEditorProcessor* findProcessorAtSlot(int slot) const {
        for (auto* processor : objects) {
            if (processor->getSlot() == slot) {
                return processor;
            }
        }
        return nullptr;
    }

    void showPopupMenu(const Point<int> &mousePos) {
        int slot = findSlotAt(mousePos);
        PopupMenu menu;
        bool isMixerChannel = slot == project.getMixerChannelSlotForTrack(parent);

        if (isMixerChannel) {
            menu.addItem(ADD_MIXER_CHANNEL_MENU_ID, "Add mixer channel");
        } else {
            project.addPluginsToMenu(menu, parent);
        }

        menu.showMenuAsync({}, ModalCallbackFunction::create([this, slot, isMixerChannel](int r) {
            if (isMixerChannel) {
                getCommandManager().invokeDirectly(CommandIDs::addMixerChannel, false);
            } else {
                if (auto *description = project.getChosenType(r)) {
                    project.createAndAddProcessor(*description, parent, &project.getUndoManager(), slot);
                }
            }
        }));
    }

    int getNonMixerCellSize() const {
        return isMasterTrack() ? project.getTrackWidth() : project.getProcessorHeight();
    }

    int getMixerCellSize() const {
        return isMasterTrack() ? project.getTrackWidth() : (project.getProcessorHeight() * 2);
    }

    // todo use processorslotrectangles
    int findSlotAt(const Point<int> relativePosition) const {
        int slot = isMasterTrack() ? (relativePosition.x - 32) / getNonMixerCellSize() : (relativePosition.y - 32) /
                getNonMixerCellSize();
        return jlimit(0, getNumAvailableSlots() - 1, slot);
    }
};
