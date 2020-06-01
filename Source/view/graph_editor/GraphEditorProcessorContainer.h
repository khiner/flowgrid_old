#pragma once

#include "BaseGraphEditorProcessor.h"

class GraphEditorProcessorContainer {
public:
    virtual BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID) const = 0;

    virtual ~GraphEditorProcessorContainer() = default;
};
