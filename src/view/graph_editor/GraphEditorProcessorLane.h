#pragma once

#include "model/StatefulList.h"
#include "GraphEditorProcessorContainer.h"
#include "ConnectorDragListener.h"
#include "GraphEditorChannel.h"

class GraphEditorProcessorLane : public Component, GraphEditorProcessorContainer, public ProcessorLane::Listener, public ValueTree::Listener {
public:
    explicit GraphEditorProcessorLane(ProcessorLane *lane, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener);

    ~GraphEditorProcessorLane() override;

    void processorAdded(Processor *processor) override {
        addAndMakeVisible(children.insert(processor->getIndex(), createEditorForProcessor(processor)));
        children.getUnchecked(processor->getIndex())->addMouseListener(this, true);
        resized();
    }
    void processorRemoved(Processor *processor, int oldIndex) override {
        children.getUnchecked(oldIndex)->removeMouseListener(this);
        children.remove(oldIndex);
        resized();
    }
    void processorOrderChanged() override {
        children.sort(*this);
        resized();
        connectorDragListener.update();
    }

    void processorPropertyChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::slot) {
            resized();
        }
    }

    void updateProcessorSlotColours();

    ProcessorLane *getLane() const { return lane; }
    Track *getTrack() const { return track; }
    int getNumSlots() const { return view.getNumProcessorSlots(track->isMaster()); }
    int getSlotOffset() const { return view.getSlotOffset(track->isMaster()); }

    void resized() override;

    BaseGraphEditorProcessor *createEditorForProcessor(Processor *processor);

    BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID nodeId) const override {
        for (auto *processor : children)
            if (processor->getNodeId() == nodeId)
                return processor;
        return nullptr;
    }

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const {
        for (auto *processor : children)
            if (auto *channel = processor->findChannelAt(e))
                return channel;
        return nullptr;
    }

    int compareElements(BaseGraphEditorProcessor *first, BaseGraphEditorProcessor *second) const {
        return lane->indexOf(first->getProcessor()) - lane->indexOf(second->getProcessor());
    }

private:
    OwnedArray<BaseGraphEditorProcessor> children;

    ProcessorLane *lane;
    Track *track;
    View &view;
    StatefulAudioProcessorWrappers &processorWrappers;
    ConnectorDragListener &connectorDragListener;

    DrawableRectangle laneDragRectangle;
    OwnedArray<DrawableRectangle> processorSlotRectangles;

    BaseGraphEditorProcessor *findProcessorAtSlot(int slot) const {
        for (auto *processor : children)
            if (processor->getProcessor()->getSlot() == slot)
                return processor;
        return nullptr;
    }

    void valueTreePropertyChanged(ValueTree &tree, const Identifier &i) override;
};
