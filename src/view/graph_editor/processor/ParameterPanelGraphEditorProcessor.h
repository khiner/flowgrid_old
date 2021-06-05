#pragma once

#include "BaseGraphEditorProcessor.h"
#include "view/processor_editor/ParametersPanel.h"

class ParameterPanelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    ParameterPanelGraphEditorProcessor(Processor *processor, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener);

    ~ParameterPanelGraphEditorProcessor() override;

    void resized() override;

private:
    std::unique_ptr<ParametersPanel> parametersPanel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
