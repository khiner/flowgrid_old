#include "GraphEditorOutput.h"

GraphEditorOutput::GraphEditorOutput(Output &output, View &view, ProcessorGraph &processorGraph, PluginManager &pluginManager, ConnectorDragListener &connectorDragListener)
        : output(output), view(view), graph(processorGraph), connectorDragListener(connectorDragListener) {
    output.addOutputListener(this);
}

GraphEditorOutput::~GraphEditorOutput() {
    output.removeOutputListener(this);
}


void GraphEditorOutput::resized() {
    auto r = getLocalBounds();
    int midiOutputProcessorWidthInChannels = midiOutputProcessors.size() * 2;
    float audioOutputWidthRatio = audioOutputProcessor
                                  ? float(audioOutputProcessor->getNumInputChannels()) / float(audioOutputProcessor->getNumInputChannels() + midiOutputProcessorWidthInChannels)
                                  : 0;
    if (audioOutputProcessor != nullptr) {
        audioOutputProcessor->setBounds(r.removeFromLeft(int(getWidth() * audioOutputWidthRatio)));
    }
    for (auto *midiOutputProcessor : midiOutputProcessors) {
        midiOutputProcessor->setBounds(r.removeFromLeft(int(getWidth() * (1 - audioOutputWidthRatio) / midiOutputProcessors.size())));
    }
}

GraphEditorChannel *GraphEditorOutput::findChannelAt(const MouseEvent &e) const {
    if (auto *channel = audioOutputProcessor->findChannelAt(e))
        return channel;
    for (auto *midiOutputProcessor : midiOutputProcessors) {
        if (auto *channel = midiOutputProcessor->findChannelAt(e))
            return channel;
    }
    return nullptr;
}

LabelGraphEditorProcessor *GraphEditorOutput::findProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
    if (audioOutputProcessor != nullptr && audioOutputProcessor->getNodeId() == nodeId) return audioOutputProcessor.get();
    for (auto *midiOutputProcessor : midiOutputProcessors) {
        if (midiOutputProcessor->getNodeId() == nodeId)
            return midiOutputProcessor;
    }
    return nullptr;
}
