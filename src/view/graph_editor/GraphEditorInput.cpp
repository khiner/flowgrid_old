#include "GraphEditorInput.h"

GraphEditorInput::GraphEditorInput(Input &input, View &view, ProcessorGraph &processorGraph, PluginManager &pluginManager, ConnectorDragListener &connectorDragListener)
    : input(input), view(view), graph(processorGraph), connectorDragListener(connectorDragListener) {
    input.addChildListener(this);
}

GraphEditorInput::~GraphEditorInput() {
    removeMouseListener(this);
    input.removeChildListener(this);
}


void GraphEditorInput::resized() {
    auto r = getLocalBounds();
    int midiInputProcessorWidthInChannels = midiInputProcessors.size() * 2;
    float audioInputWidthRatio = audioInputProcessor
                                 ? float(audioInputProcessor->getNumOutputChannels()) / float(audioInputProcessor->getNumOutputChannels() + midiInputProcessorWidthInChannels)
                                 : 0;
    if (audioInputProcessor != nullptr) {
        audioInputProcessor->setBounds(r.removeFromLeft(int(getWidth() * audioInputWidthRatio)));
    }
    for (auto *midiInputProcessor: midiInputProcessors) {
        midiInputProcessor->setBounds(r.removeFromLeft(int(getWidth() * (1 - audioInputWidthRatio) / midiInputProcessors.size())));
    }
}

GraphEditorChannel *GraphEditorInput::findChannelAt(const MouseEvent &e) const {
    if (auto *channel = audioInputProcessor->findChannelAt(e))
        return channel;
    for (auto *midiInputProcessor: midiInputProcessors) {
        if (auto *channel = midiInputProcessor->findChannelAt(e))
            return channel;
    }
    return nullptr;
}

LabelGraphEditorProcessor *GraphEditorInput::findProcessorForNodeId(const AudioProcessorGraph::NodeID nodeId) const {
    if (audioInputProcessor != nullptr && audioInputProcessor->getNodeId() == nodeId) return audioInputProcessor.get();
    for (auto *midiInputProcessor: midiInputProcessors) {
        if (midiInputProcessor->getNodeId() == nodeId)
            return midiInputProcessor;
    }
    return nullptr;
}
