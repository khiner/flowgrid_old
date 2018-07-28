#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "GraphEditorProcessor.h"
#include "GraphEditorProcessorContainer.h"
#include "ConnectorDragListener.h"
#include "ProcessorGraph.h"
#include "PinComponent.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor>,
                              public GraphEditorProcessorContainer {
public:
    explicit GraphEditorProcessors(Project& project, ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              project(project), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
    }

    ~GraphEditorProcessors() override {
        freeObjects();
    }

    bool isMasterTrack() const {
        return parent.hasType(IDs::MASTER_TRACK);
    }

    void mouseDown(const MouseEvent &e) override {
        if (e.mods.isPopupMenu()) {
            showPopupMenu(e.position.toInt());
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        const int cellSize = getCellSize();

        for (int slot = 0; slot < Project::NUM_AVAILABLE_PROCESSOR_SLOTS; slot++) {
            auto processorBounds = isMasterTrack() ? r.removeFromLeft(cellSize) : r.removeFromTop(cellSize);
            if (auto *processor = findProcessorAtSlot(slot)) {
                processor->setBounds(processorBounds);
            }
        }
    }

    void paint(Graphics &g) override {
        auto r = getLocalBounds();
        const int cellSize = getCellSize();

        g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.15));
        for (int i = 0; i < Project::NUM_AVAILABLE_PROCESSOR_SLOTS; i++)
            g.drawRect(isMasterTrack() ? r.removeFromLeft(cellSize) : r.removeFromTop(cellSize));
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &v) override {
        GraphEditorProcessor *processor = currentlyMovingProcessor != nullptr ? currentlyMovingProcessor : new GraphEditorProcessor(v, connectorDragListener, graph);
        addAndMakeVisible(processor);
        processor->update();
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

    void objectRemoved(GraphEditorProcessor *) override { resized(); }

    void objectOrderChanged() override { resized(); }

    GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *processor : objects) {
            if (processor->getNodeId() == nodeId) {
                return processor;
            }
        }
        return nullptr;
    }

    PinComponent *findPinAt(const MouseEvent &e) {
        for (auto *processor : objects) {
            if (auto* pin = processor->findPinAt(e)) {
                return pin;
            }
        }
        return nullptr;
    }

    int findSlotAt(const MouseEvent &e) {
        const MouseEvent &relative = e.getEventRelativeTo(this);
        int slot = isMasterTrack() ? relative.x / getCellSize() : relative.y / getCellSize();
        return jlimit(0, Project::NUM_AVAILABLE_PROCESSOR_SLOTS - 1, slot);
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
            if (processor->isSelected()) {
                return processor;
            }
        }
        return false;
    }

private:
    Project &project;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    std::unique_ptr<PopupMenu> menu;
    GraphEditorProcessor *currentlyMovingProcessor {};

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (isSuitableType(v))
            if (i == IDs::processorSlot)
                resized();

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
        int slot = isMasterTrack() ? mousePos.x / getCellSize() : mousePos.y / getCellSize();
        menu = std::make_unique<PopupMenu>();
        project.addPluginsToMenu(*menu, parent);
        menu->showMenuAsync({}, ModalCallbackFunction::create([this, slot](int r) {
            if (auto *description = project.getChosenType(r)) {
                project.createAndAddProcessor(*description, parent, slot);
            }
        }));
    }

    int getCellSize() const {
        return (isMasterTrack() ? getWidth() : getHeight()) / Project::NUM_AVAILABLE_PROCESSOR_SLOTS;
    }
};
