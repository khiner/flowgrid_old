#pragma once

#include "BaseGraphEditorProcessor.h"
#include "view/processor_editor/ParametersPanel.h"

class ParameterPanelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    ParameterPanelGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                              const ValueTree &state, ConnectorDragListener &connectorDragListener,
                              bool showChannelLabels = false) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener, showChannelLabels) {
//        parametersPanel->addMouseListener(this);
    }

    ~ParameterPanelGraphEditorProcessor() override {
        if (parametersPanel != nullptr)
            parametersPanel->removeMouseListener(this);
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();

        if (parametersPanel != nullptr) {
            parametersPanel->setBounds(boxBoundsFloat.toNearestInt());
        }
    }

private:
    std::unique_ptr<ParametersPanel> parametersPanel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

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
};
