#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "GraphEditorProcessor.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor> {
public:
    explicit GraphEditorProcessors(Project& project, GraphEditorTrack &at, ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              project(project), track(at), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
    }

    ~GraphEditorProcessors() override {
        freeObjects();
    }

    void mouseDown(const MouseEvent &e) override {
        if (e.mods.isPopupMenu()) {
            showPopupMenu(e.position.toInt());
        }
    }

    void resized() override {
        auto r = getLocalBounds();
        const int h = r.getHeight() / Project::NUM_AVAILABLE_PROCESSOR_SLOTS;

        for (int slot = 0; slot < Project::NUM_AVAILABLE_PROCESSOR_SLOTS; slot++) {
            auto processorBounds = r.removeFromTop(h);
            if (auto *processor = findProcessorAtSlot(slot)) {
                processor->setBounds(processorBounds);
            }
        }
    }

    void paint(Graphics &g) override {
        auto r = getLocalBounds();
        const int h = r.getHeight() / Project::NUM_AVAILABLE_PROCESSOR_SLOTS;

        g.setColour(findColour(ResizableWindow::backgroundColourId).brighter(0.15));
        for (int i = 0; i < Project::NUM_AVAILABLE_PROCESSOR_SLOTS; i++)
            g.drawRect(r.removeFromTop(h));
    }

    bool isSuitableType(const ValueTree &v) const override {
        return v.hasType(IDs::PROCESSOR);
    }

    GraphEditorProcessor *createNewObject(const ValueTree &v) override {
        auto *processor = new GraphEditorProcessor(v, connectorDragListener, graph);
        addAndMakeVisible(processor);
        processor->update();
        return processor;
    }

    void deleteObject(GraphEditorProcessor *ac) override {
        delete ac;
    }

    void newObjectAdded(GraphEditorProcessor *) override { resized(); }

    void objectRemoved(GraphEditorProcessor *) override { resized(); }

    void objectOrderChanged() override { resized(); }

    GraphEditorProcessor *getProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
        for (auto *processor : objects) {
            if (processor->getNodeId() == nodeId) {
                return processor;
            }
        }
        return nullptr;
    }

    PinComponent *findPinAt(const Point<float> &pos) const {
        for (auto *processor : objects) {
            auto *comp = processor->getComponentAt(pos.toInt() - processor->getPosition());
            if (auto *pin = dynamic_cast<PinComponent *> (comp)) {
                return pin;
            }
        }
        return nullptr;
    }

    void updateNodes() {
        for (auto *processor : objects) {
            processor->update();
        }
    }
private:
    Project &project;
    GraphEditorTrack &track;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;
    std::unique_ptr<PopupMenu> menu;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (isSuitableType(v))
            if (i == IDs::start || i == IDs::length)
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
        int slot = int(Project::NUM_VISIBLE_PROCESSOR_SLOTS * jlimit(0.0f, 0.99f, mousePos.y / float(getHeight())));
        menu = std::make_unique<PopupMenu>();
        project.addPluginsToMenu(*menu, parent);
        menu->showMenuAsync({}, ModalCallbackFunction::create([this, slot](int r) {
            if (auto *description = project.getChosenType(r)) {
                project.createAndAddProcessor(*description, parent, slot);
            }
        }));
    }
};
