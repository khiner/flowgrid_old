#pragma once

#include "view/graph_editor/processor/BaseGraphEditorProcessor.h"

class GraphEditorProcessorContainer {
public:
    virtual BaseGraphEditorProcessor *getProcessorForNodeId(AudioProcessorGraph::NodeID) const = 0;

    virtual ~GraphEditorProcessorContainer() = default;
};
