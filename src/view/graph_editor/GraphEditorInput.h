#pragma once

#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "ProcessorGraph.h"
#include "GraphEditorChannel.h"

class GraphEditorInput : public Component, private Input::Listener {
public:
    GraphEditorInput(Input &, View &, ProcessorGraph &, PluginManager &, ConnectorDragListener &);

    ~GraphEditorInput() override;

    void resized() override;

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const;
    LabelGraphEditorProcessor *findProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const;

    int compareElements(const LabelGraphEditorProcessor *first, const LabelGraphEditorProcessor *second) const {
        return midiInputProcessors.indexOf(first) - midiInputProcessors.indexOf(second);
    }
private:
    std::unique_ptr<LabelGraphEditorProcessor> audioInputProcessor;
    OwnedArray<LabelGraphEditorProcessor> midiInputProcessors;

    Input &input;
    View &view;
    ProcessorGraph &graph;
    ConnectorDragListener &connectorDragListener;

    void processorAdded(Processor *processor) override {}
    void processorRemoved(Processor *processor, int oldIndex) override {
        if (processor->isAudioInputProcessor()) {
            audioInputProcessor = nullptr;
        }
        if (processor->isMidiInputProcessor()) {
            midiInputProcessors.removeObject(findProcessorForNodeId(processor->getNodeId()));
            resized();
        }
    }
    void processorOrderChanged() override {
        midiInputProcessors.sort(*this);
        resized();
    }

    void processorPropertyChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::initialized) {
            if (processor->isAudioInputProcessor()) {
                addAndMakeVisible(*(audioInputProcessor = std::make_unique<LabelGraphEditorProcessor>(processor, nullptr, view, graph.getProcessorWrappers(), connectorDragListener)), 0);
                resized();
            } else if (processor->isMidiInputProcessor()) {
                auto *midiInputProcessor = new LabelGraphEditorProcessor(processor, nullptr, view, graph.getProcessorWrappers(), connectorDragListener);
                addAndMakeVisible(midiInputProcessor, 0);
                midiInputProcessors.addSorted(*this, midiInputProcessor);
                resized();
            }
        }
    }
};
