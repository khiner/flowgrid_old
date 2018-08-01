#include <memory>

#pragma once

#include "JuceHeader.h"
#include "processors/StatefulAudioProcessorWrapper.h"
#include "SwitchParameterComponent.h"
#include "ParametersPanel.h"

class ProcessorEditor : public Component {
public:
    explicit ProcessorEditor(int maxRows=2) :
            pageLeftButton("Page left", 0.5, findColour(ResizableWindow::backgroundColourId).brighter(0.75)),
            pageRightButton("Page right", 0.0, findColour(ResizableWindow::backgroundColourId).brighter(0.75)) {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(pageLeftButton);
        addAndMakeVisible(pageRightButton);

        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(maxRows)).get());

        pageLeftButton.onClick = [this]() {
            parametersPanel->pageLeft();
            updatePageButtonVisibility();
        };

        pageRightButton.onClick = [this]() {
            parametersPanel->pageRight();
            updatePageButtonVisibility();
        };
    }

    void paint(Graphics &g) override {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(30);
        pageRightButton.setBounds(top.removeFromRight(30).reduced(5));
        pageLeftButton.setBounds(top.removeFromRight(30).reduced(5));
        titleLabel.setBounds(top);

        parametersPanel->setBounds(r);
    }

    void setProcessor(StatefulAudioProcessorWrapper *const processorWrapper) {
        if (processorWrapper != nullptr) {
            titleLabel.setText(processorWrapper->processor->getName(), dontSendNotification);
        }
        parametersPanel->setProcessor(processorWrapper);
        updatePageButtonVisibility();
    }

    void updatePageButtonVisibility() {
        pageLeftButton.setVisible(parametersPanel->canPageLeft());
        pageRightButton.setVisible(parametersPanel->canPageRight());
    }

private:
    std::unique_ptr<ParametersPanel> parametersPanel;
    Label titleLabel;
    ArrowButton pageLeftButton, pageRightButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorEditor)
};
