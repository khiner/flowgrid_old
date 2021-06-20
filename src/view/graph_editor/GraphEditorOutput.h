#pragma once

#include "view/graph_editor/processor/LabelGraphEditorProcessor.h"
#include "ProcessorGraph.h"
#include "GraphEditorChannel.h"

class GraphEditorOutput : public Component, private Output::Listener {
public:
    GraphEditorOutput(Output &, View &, ProcessorGraph &, PluginManager &, ConnectorDragListener &);

    ~GraphEditorOutput() override;

    void resized() override;

    GraphEditorChannel *findChannelAt(const MouseEvent &e) const;
    LabelGraphEditorProcessor *findProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const;

    int compareElements(const LabelGraphEditorProcessor *first, const LabelGraphEditorProcessor *second) const {
        return midiOutputProcessors.indexOf(first) - midiOutputProcessors.indexOf(second);
    }
private:
    std::unique_ptr<LabelGraphEditorProcessor> audioOutputProcessor;
    OwnedArray<LabelGraphEditorProcessor> midiOutputProcessors;

    Output &output;
    View &view;
    ProcessorGraph &graph;
    ConnectorDragListener &connectorDragListener;

    void processorRemoved(Processor *processor, int oldIndex) override {
        if (processor->isAudioOutputProcessor()) {
            audioOutputProcessor = nullptr;
        }
        if (processor->isMidiOutputProcessor()) {
            midiOutputProcessors.removeObject(findProcessorForNodeId(processor->getNodeId()));
            resized();
        }
    }
    void processorOrderChanged() override {
        midiOutputProcessors.sort(*this);
        resized();
    }

    void processorPropertyChanged(Processor *processor, const Identifier &i) override {
        if (i == ProcessorIDs::initialized) {
            if (processor->isAudioOutputProcessor()) {
                addAndMakeVisible(*(audioOutputProcessor = std::make_unique<LabelGraphEditorProcessor>(processor, nullptr, view, graph.getProcessorWrappers(), connectorDragListener)), 0);
                resized();
            } else if (processor->isMidiOutputProcessor()) {
                auto *midiOutputProcessor = new LabelGraphEditorProcessor(processor, nullptr, view, graph.getProcessorWrappers(), connectorDragListener);
                addAndMakeVisible(midiOutputProcessor, 0);
                midiOutputProcessors.addSorted(*this, midiOutputProcessor);
                resized();
            }
        }
    }
};
