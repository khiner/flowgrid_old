#include "ProcessorEditor.h"

ProcessorEditor::ProcessorEditor(int maxRows) :
        pageLeftButton("Page left", 0.5f, findColour(ResizableWindow::backgroundColourId).brighter(0.75f)),
        pageRightButton("Page right", 0.0f, findColour(ResizableWindow::backgroundColourId).brighter(0.75f)) {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(pageLeftButton);
    addAndMakeVisible(pageRightButton);
    addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(maxRows)).get());

    parametersPanel->setBackgroundColour(findColour(ResizableWindow::backgroundColourId).brighter(0.1f));
    parametersPanel->setOutlineColour(findColour(TextEditor::backgroundColourId));

    titleLabel.setColour(findColour(TextEditor::textColourId));
    titleLabel.setJustification(Justification::verticallyCentred);
    titleLabel.setFontHeight(18);

    pageLeftButton.onClick = [this]() {
        parametersPanel->pageLeft();
        updatePageButtonVisibility();
    };

    pageRightButton.onClick = [this]() {
        parametersPanel->pageRight();
        updatePageButtonVisibility();
    };
}

void ProcessorEditor::paint(Graphics &g) {
    auto r = getLocalBounds();
    g.setColour(findColour(TextEditor::backgroundColourId));
    g.fillRect(r.removeFromTop(38));
}

void ProcessorEditor::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(38);
    top.removeFromLeft(8);
    titleLabel.setBoundingBox(top.removeFromLeft(200).reduced(2).toFloat());

    top.reduce(4, 4);
    pageRightButton.setBounds(top.removeFromRight(30).reduced(5));
    pageLeftButton.setBounds(top.removeFromRight(30).reduced(5));

    parametersPanel->setBounds(r);
}

void ProcessorEditor::setProcessor(StatefulAudioProcessorWrapper *processorWrapper) {
    if (processorWrapper != nullptr) {
        titleLabel.setText(processorWrapper->processor->getName());
    }
    parametersPanel->setProcessorWrapper(processorWrapper);
    parametersPanel->resized();
    updatePageButtonVisibility();
}
