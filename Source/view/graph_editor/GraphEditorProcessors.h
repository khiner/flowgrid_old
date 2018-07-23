#pragma once

#include <ValueTreeObjectList.h>
#include <Project.h>
#include "GraphEditorProcessor.h"

class GraphEditorProcessors : public Component,
                              public Utilities::ValueTreeObjectList<GraphEditorProcessor> {
public:
    explicit GraphEditorProcessors(GraphEditorTrack &at, ValueTree& state, ConnectorDragListener &connectorDragListener, ProcessorGraph& graph)
            : Utilities::ValueTreeObjectList<GraphEditorProcessor>(state),
              track(at), connectorDragListener(connectorDragListener), graph(graph) {
        rebuildObjects();
    }

    ~GraphEditorProcessors() override {
        freeObjects();
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
        auto *ac = new GraphEditorProcessor(v, track, connectorDragListener, graph);
        addAndMakeVisible(ac);
        ac->update();
        return ac;
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
    GraphEditorTrack &track;
    ConnectorDragListener &connectorDragListener;
    ProcessorGraph &graph;

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
};
