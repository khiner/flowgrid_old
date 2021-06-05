#include "ParameterPanelGraphEditorProcessor.h"

ParameterPanelGraphEditorProcessor::ParameterPanelGraphEditorProcessor(Processor *processor, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(processor, track, view, processorWrappers, connectorDragListener) {
//        parametersPanel->addMouseListener(this);
}

ParameterPanelGraphEditorProcessor::~ParameterPanelGraphEditorProcessor() {
    if (parametersPanel != nullptr)
        parametersPanel->removeMouseListener(this);
}

void ParameterPanelGraphEditorProcessor::resized() {
    BaseGraphEditorProcessor::resized();
    auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();

    if (parametersPanel != nullptr) {
        parametersPanel->setBounds(boxBoundsFloat.toNearestInt());
    }
}

void ParameterPanelGraphEditorProcessor::valueTreePropertyChanged(ValueTree &v, const Identifier &i) {
    if (v != processor->getState()) return;

    if (parametersPanel == nullptr) {
        if (auto *processorWrapper = getProcessorWrapper()) {
            addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1, processorWrapper->getNumParameters())).get());
            parametersPanel->addMouseListener(this, true);
            parametersPanel->addParameter(processorWrapper->getParameter(0));
            parametersPanel->addParameter(processorWrapper->getParameter(1));
        }
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}
