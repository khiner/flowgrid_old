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
    explicit GraphEditorProcessors(Project& project, ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              project(project), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
        for (auto* object : objects) {
            if (object->isSelected())
                mostRecentlySelectedProcessor = object;
        }
    }

    ~GraphEditorProcessors() override {
        freeObjects();
    }

    bool isMasterTrack() const { return parent.hasProperty(IDs::isMasterTrack); }

    int getNumAvailableSlots() const { return project.maxSlotForTrack(parent) + 1; }

    int getSlotOffset() const { return isMasterTrack() ? project.getMasterViewSlotOffset() : project.getGridViewSlotOffset(); }

    void mouseDown(const MouseEvent &e) override {
        setSelected(true);
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2) {
            showPopupMenu(e.position.toInt());
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        auto slotOffset = getSlotOffset();
        for (int slot = 0; slot < getNumAvailableSlots(); slot++) {
            if (slot == slotOffset) {
                if (isMasterTrack())
                    r.removeFromLeft(32); // todo constant
                else
                    r.removeFromTop(32); // todo constant
            }
            auto processorBounds = isMasterTrack() ? r.removeFromLeft(getCellSize()) : r.removeFromTop(getCellSize());
            if (auto *processor = findProcessorAtSlot(slot)) {
                processor->setBounds(processorBounds);
            }
        }
        if (auto *processor = findProcessorAtSlot(Project::MIXER_CHANNEL_SLOT)) {
            auto processorBounds = isMasterTrack() ? r.removeFromLeft(getCellSize()) : r.removeFromTop(getCellSize());
            processor->setBounds(processorBounds);
        }
    }

    void paint(Graphics &g) override {
        auto r = getLocalBounds();
        auto slotOffset = getSlotOffset();
        g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.15));
        for (int slot = 0; slot < getNumAvailableSlots() + 1; slot++) {
            if (slot == slotOffset) {
                if (isMasterTrack())
                    r.removeFromLeft(32); // todo constant
                else
                    r.removeFromTop(32); // todo constant
            }
            auto cellBounds = isMasterTrack() ? r.removeFromLeft(getCellSize()) : r.removeFromTop(getCellSize());
            g.drawRect(cellBounds);
            if (slot == getNumAvailableSlots()) {
                g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.1));
                g.fillRect(cellBounds);
            }
        }
    }

    void slotOffsetChanged() {
        resized();
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &v) override {
        GraphEditorProcessor *processor = currentlyMovingProcessor != nullptr ? currentlyMovingProcessor : new GraphEditorProcessor(v, connectorDragListener, graph);
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

    GraphEditorPin *findPinAt(const MouseEvent &e) {
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
    
    void update() {
        for (auto *processor : objects) {
            processor->update();
        }
    }

    GraphEditorProcessor *getCurrentlyMovingProcessor() const {
        return currentlyMovingProcessor;
    }

    void setCurrentlyMovingProcessor(GraphEditorProcessor *currentlyMovingProcessor) {
        this->currentlyMovingProcessor = currentlyMovingProcessor;
    }

    bool anySelected() const {
        for (auto *processor : objects) {
            if (processor->isSelected())
                return true;
        }
        return false;
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

    static constexpr float MIXER_CHANNEL_SLOT_RATIO = 0.2f;

    Project &project;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    std::unique_ptr<PopupMenu> menu;
    GraphEditorProcessor *currentlyMovingProcessor {};
    GraphEditorProcessor* mostRecentlySelectedProcessor {};

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (isSuitableType(v)) {
            if (i == IDs::processorSlot)
                resized();
            else if (i == IDs::selected && v[IDs::selected]) {
                for (auto* processor : objects) {
                    if (processor->getState() == v)
                        mostRecentlySelectedProcessor = processor;
                    else
                        processor->setSelected(false);
                }
            }
        }

        Utilities::ValueTreeObjectList<GraphEditorProcessor>::valueTreePropertyChanged(v, i);
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
        menu = std::make_unique<PopupMenu>();
        if (slot == Project::MIXER_CHANNEL_SLOT) {
            menu->addItem(ADD_MIXER_CHANNEL_MENU_ID, "Add mixer channel");
        } else {
            project.addPluginsToMenu(*menu, parent);
        }

        menu->showMenuAsync({}, ModalCallbackFunction::create([this, slot](int r) {
            if (slot == Project::MIXER_CHANNEL_SLOT) {
                getCommandManager().invokeDirectly(CommandIDs::addMixerChannel, false);
            } else if (r == ADD_MIXER_CHANNEL_MENU_ID) {
                if (auto *description = project.getChosenType(r)) {
                    project.createAndAddProcessor(*description, parent, &project.getUndoManager(), slot);
                }
            }
        }));
    }

    int getCellSize() const {
        return isMasterTrack() ? ((getWidth() - 32) / (getNumAvailableSlots() + 1)) : project.getProcessorHeight(); // +1 for mixer
    }

    int findSlotAt(const Point<int> relativePosition) {
        int slot = isMasterTrack() ? relativePosition.x / getCellSize() : relativePosition.y / getCellSize();
        slot += getSlotOffset();
        if (slot >= getNumAvailableSlots())
            return Project::MIXER_CHANNEL_SLOT;
        else
            return jlimit(0, getNumAvailableSlots() - 1, slot);
    }
};
