#pragma once

#include "processors/StatefulAudioProcessorWrapper.h"
#include "ParametersPanel.h"

class ProcessorEditor : public Component {
public:
    explicit ProcessorEditor(int maxRows = 1);

    void setSelected(bool selected) {
        parametersPanel->setBackgroundColour(findColour(ResizableWindow::backgroundColourId).brighter(selected ? 0.3f : 0.1f));
    }

    void setFocused(bool focused) {
        parametersPanel->setBackgroundColour(findColour(ResizableWindow::backgroundColourId).brighter(focused ? 0.6f : 0.1f));
    }

    Processor *getProcessor() const { return parametersPanel->getProcessor(); }

    void paint(Graphics &g) override;
    void resized() override;

    void setProcessor(StatefulAudioProcessorWrapper *processorWrapper);

    void updatePageButtonVisibility() {
        pageLeftButton.setVisible(parametersPanel->canPageLeft());
        pageRightButton.setVisible(parametersPanel->canPageRight());
    }

private:
    std::unique_ptr<ParametersPanel> parametersPanel;
    DrawableText titleLabel;
    ArrowButton pageLeftButton, pageRightButton;
};
