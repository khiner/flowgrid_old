#pragma once

#include "BaseGraphEditorProcessor.h"

class GraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    GraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                         const ValueTree &state, ConnectorDragListener &connectorDragListener,
                         bool showChannelLabels = false) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener, showChannelLabels) {
    }
};
