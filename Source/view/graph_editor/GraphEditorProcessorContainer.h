#pragma once

#include "GraphEditorProcessor.h"

class GraphEditorProcessorContainer {
public:
    virtual GraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID) const = 0;
    virtual ~GraphEditorProcessorContainer() {};
};
