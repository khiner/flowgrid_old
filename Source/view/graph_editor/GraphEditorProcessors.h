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

    int getNumAvailableSlots() const { return Project::NUM_AVAILABLE_PROCESSOR_SLOTS + (isMasterTrack() ? 1 : 0); }

    void mouseDown(const MouseEvent &e) override {
        setSelected(true);
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() == 2) {
            showPopupMenu(e.position.toInt());
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        int totalSize = isMasterTrack() ? getWidth() : getHeight();

        int mixerChannelSize = int(totalSize * MIXER_CHANNEL_SLOT_RATIO);
        for (int slot = 0; slot < getNumAvailableSlots(); slot++) {
            cellSizes[slot] = (slot != getNumAvailableSlots() - 1)
                              ? (totalSize - mixerChannelSize) / (getNumAvailableSlots() - 1)
                              : mixerChannelSize;
        }

        for (int slot = 0; slot < getNumAvailableSlots(); slot++) {
            auto processorBounds = isMasterTrack() ? r.removeFromLeft(getCellSize()) : r.removeFromTop(cellSizes[slot]);
            if (auto *processor = findProcessorAtSlot(slot)) {
                processor->setBounds(processorBounds);
            }
        }
    }

    void paint(Graphics &g) override {
        auto r = getLocalBounds();
        g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.15));
        for (int i = 0; i < getNumAvailableSlots(); i++)
            g.drawRect(isMasterTrack() ? r.removeFromLeft(getCellSize()) : r.removeFromTop(cellSizes[i]));
        if (!project.getMixerChannelProcessorForTrack(parent).isValid()) {
            auto r = getLocalBounds().reduced(1);
            g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.05));
            g.fillRect(isMasterTrack()
                       ? r.removeFromRight(getCellSize())
                       : r.removeFromBottom(cellSizes[getNumAvailableSlots() - 1]));
        }
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

    int cellSizes[Project::NUM_AVAILABLE_PROCESSOR_SLOTS];

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
        if (slot != project.maxSlotForTrack(parent)) {
            project.addPluginsToMenu(*menu, parent);
        } else {
            menu->addItem(ADD_MIXER_CHANNEL_MENU_ID, "Add mixer channel");
        }
        menu->showMenuAsync({}, ModalCallbackFunction::create([this, slot](int r) {
            if (slot != project.maxSlotForTrack(parent)) {
                if (auto *description = project.getChosenType(r)) {
                    project.createAndAddProcessor(*description, parent, slot);
                }
            } else if (r == ADD_MIXER_CHANNEL_MENU_ID) {
                getCommandManager().invokeDirectly(CommandIDs::addMixerChannel, false);
            }
        }));
    }

    int getCellSize() const {
        return (isMasterTrack() ? getWidth() : getHeight()) / getNumAvailableSlots();
    }

    int findSlotAt(const Point<int> relativePosition) {
        int slot = isMasterTrack() ? relativePosition.x / getCellSize() : relativePosition.y / getCellSize();
        // master track has one more available slot than other tracks.
        return jlimit(0, getNumAvailableSlots() - 1, slot);
    }
};
