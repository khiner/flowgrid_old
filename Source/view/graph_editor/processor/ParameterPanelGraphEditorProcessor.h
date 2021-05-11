#pragma once

#include "state/Project.h"
#include "BaseGraphEditorProcessor.h"
#include "view/processor_editor/ParametersPanel.h"

class ParameterPanelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    ParameterPanelGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                                       const ValueTree &state, ConnectorDragListener &connectorDragListener);

    ~ParameterPanelGraphEditorProcessor() override;

    void resized() override;

private:
    Project &project;
    std::unique_ptr<ParametersPanel> parametersPanel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
