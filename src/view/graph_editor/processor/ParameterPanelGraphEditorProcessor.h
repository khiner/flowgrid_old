#pragma once

#include "BaseGraphEditorProcessor.h"
#include "view/processor_editor/ParametersPanel.h"

class ParameterPanelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    ParameterPanelGraphEditorProcessor(const ValueTree &state, View &view, Tracks &tracks, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener);

    ~ParameterPanelGraphEditorProcessor() override;

    void resized() override;

private:
    std::unique_ptr<ParametersPanel> parametersPanel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
