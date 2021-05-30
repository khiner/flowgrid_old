#include "Push2ProcessorView.h"

Push2ProcessorView::Push2ProcessorView(View &view, Tracks &tracks, Project &project, Push2MidiCommunicator &push2MidiCommunicator)
        : Push2TrackManagingView(view, tracks, project, push2MidiCommunicator),
          escapeProcessorFocusButton("Back", 0.5, Colours::white),
          parameterPageLeftButton("Page parameters left", 0.5, Colours::white),
          parameterPageRightButton("Page parameters right", 0.0, Colours::white),
          processorPageLeftButton("Page processors left", 0.5, Colours::white),
          processorPageRightButton("Page processors right", 0.0, Colours::white) {

    for (int i = 0; i < NUM_COLUMNS; i++) {
        addChildComponent(processorLabels.add(new Push2Label(i, true, push2MidiCommunicator)));
    }

    addChildComponent(escapeProcessorFocusButton);
    addChildComponent(parameterPageLeftButton);
    addChildComponent(parameterPageRightButton);
    addChildComponent(processorPageLeftButton);
    addChildComponent(processorPageRightButton);

    addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1)).get());
}

void Push2ProcessorView::resized() {
    Push2TrackManagingView::resized();

    auto r = getLocalBounds();
    auto top = r.removeFromTop(HEADER_FOOTER_HEIGHT);
    auto labelWidth = getWidth() / NUM_COLUMNS;
    escapeProcessorFocusButton.setBounds(top.removeFromLeft(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
    parameterPageRightButton.setBounds(top.removeFromRight(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
    parameterPageLeftButton.setBounds(top.removeFromRight(labelWidth).withSizeKeepingCentre(top.getHeight(), top.getHeight()));
    processorPageLeftButton.setBounds(escapeProcessorFocusButton.getBounds());
    processorPageRightButton.setBounds(parameterPageRightButton.getBounds());

    r.removeFromBottom(HEADER_FOOTER_HEIGHT);
    parametersPanel->setBounds(r);

    top = getLocalBounds().removeFromTop(HEADER_FOOTER_HEIGHT);
    for (auto *processorLabel : processorLabels) {
        processorLabel->setBounds(top.removeFromLeft(labelWidth));
    }
}

void Push2ProcessorView::trackSelected(Track *track) {
    Push2TrackManagingView::trackSelected(track);
    if (track->getProcessorLane().getNumChildren() == 0) {
        parametersPanel->setProcessorWrapper(nullptr);
    }
}

void Push2ProcessorView::processorFocused(StatefulAudioProcessorWrapper *processorWrapper) {
    parametersPanel->setProcessorWrapper(processorWrapper);
    if (processorWrapper == nullptr) {
        updateEnabledPush2Buttons();
        return;
    }
    const auto processorIndex = processorWrapper->state.getParent().indexOf(processorWrapper->state);
    bool hasPaged = false;
    while ((getButtonIndexForProcessorIndex(processorIndex)) <= 0 && canPageProcessorsLeft()) {
        pageProcessorsLeft();
        hasPaged = true;
    }
    while ((getButtonIndexForProcessorIndex(processorIndex)) >= processorLabels.size() - 1 && canPageProcessorsRight()) {
        pageProcessorsRight();
        hasPaged = true;
    }
    if (!hasPaged)
        updateEnabledPush2Buttons();
}

void Push2ProcessorView::aboveScreenButtonPressed(int buttonIndex) {
    if (processorHasFocus) {
        if (buttonIndex == 0)
            focusOnProcessor(false);
        else if (buttonIndex == NUM_COLUMNS - 2)
            pageParametersLeft();
        else if (buttonIndex == NUM_COLUMNS - 1)
            pageParametersRight();
    } else {
        if (buttonIndex == 0 && canPageProcessorsLeft())
            pageProcessorsLeft();
        else if (buttonIndex == NUM_COLUMNS - 1 && canPageProcessorsRight())
            pageProcessorsRight();
        else {
            if (processorLabels.getUnchecked(buttonIndex)->isSelected())
                focusOnProcessor(true);
            else
                selectProcessor(getProcessorIndexForButtonIndex(buttonIndex));
        }
    }
}

void Push2ProcessorView::encoderRotated(int encoderIndex, float changeAmount) {
    if (auto *parameter = parametersPanel->getParameterOnCurrentPageAt(encoderIndex)) {
        parameter->setValue(std::clamp(parameter->getValue() + changeAmount, 0.0f, 1.0f));
    }
}

void Push2ProcessorView::updateEnabledPush2Buttons() {
    Push2TrackManagingView::updateEnabledPush2Buttons();
    updateProcessorButtons();
    updatePageButtonVisibility();
}

void Push2ProcessorView::updatePageButtonVisibility() {
    const auto &trackColour = getColourForTrack(tracks.getFocusedTrack());
    if (processorHasFocus) { // TODO reset when processor changes
        processorPageLeftButton.setVisible(false);
        processorPageRightButton.setVisible(false);

        for (auto *label : processorLabels)
            label->setVisible(false);

        // TODO use Push2Label to unite these two sets of updates into one automagically
        push2.setAboveScreenButtonColour(0, trackColour); // back button
        push2.setAboveScreenButtonColour(NUM_COLUMNS - 2, parametersPanel->canPageLeft() ? trackColour : Colours::black);
        push2.setAboveScreenButtonColour(NUM_COLUMNS - 1, parametersPanel->canPageRight() ? trackColour : Colours::black);

        escapeProcessorFocusButton.setVisible(true);
        parameterPageLeftButton.setVisible(parametersPanel->canPageLeft());
        parameterPageRightButton.setVisible(parametersPanel->canPageRight());
    } else {
        escapeProcessorFocusButton.setVisible(false);
        parameterPageLeftButton.setVisible(false);
        parameterPageRightButton.setVisible(false);

        processorPageLeftButton.setVisible(canPageProcessorsLeft());
        processorPageRightButton.setVisible(canPageProcessorsRight());
        if (canPageProcessorsLeft())
            push2.setAboveScreenButtonColour(0, trackColour);
        if (canPageProcessorsRight())
            push2.setAboveScreenButtonColour(NUM_COLUMNS - 1, trackColour);
    }
}

void Push2ProcessorView::updateProcessorButtons() {
    const auto &focusedTrack = tracks.getFocusedTrackState();
    const auto &focusedProcessorLane = Track::getProcessorLane(focusedTrack);

    if (processorHasFocus || !focusedTrack.isValid()) { // TODO reset when processor changes
        for (auto *label : processorLabels)
            label->setVisible(false);
    } else {
        for (int buttonIndex = 0; buttonIndex < processorLabels.size(); buttonIndex++) {
            auto *label = processorLabels.getUnchecked(buttonIndex);
            auto processorIndex = getProcessorIndexForButtonIndex(buttonIndex);
            if ((buttonIndex == 0 && canPageProcessorsLeft()) ||
                (buttonIndex == NUM_COLUMNS - 1 && canPageProcessorsRight())) {
                label->setVisible(false);
            } else if (processorIndex < focusedProcessorLane.getNumChildren()) {
                const auto &processor = focusedProcessorLane.getChild(processorIndex);
                if (Processor::isType(processor)) {
                    label->setVisible(true);
                    label->setText(Processor::getName(processor), dontSendNotification);
                    label->setSelected(tracks.isProcessorFocused(processor));
                }
            } else if (buttonIndex == 0 && focusedProcessorLane.getNumChildren() == 0) {
                label->setVisible(true);
                label->setText("No processors", dontSendNotification);
                label->setSelected(false);
            } else {
                label->setVisible(false);
            }
        }
        updateColours();
    }
}

void Push2ProcessorView::updateColours() {
    const auto &colour = getColourForTrack(tracks.getFocusedTrack());
    for (auto *processorLabel : processorLabels) {
        processorLabel->setMainColour(colour);
    }
    escapeProcessorFocusButton.setColour(colour);
    parameterPageLeftButton.setColour(colour);
    parameterPageRightButton.setColour(colour);
    processorPageLeftButton.setColour(colour);
    processorPageRightButton.setColour(colour);
}

void Push2ProcessorView::pageParametersLeft() {
    parametersPanel->pageLeft();
    updatePageButtonVisibility();
}

void Push2ProcessorView::pageParametersRight() {
    parametersPanel->pageRight();
    updatePageButtonVisibility();
}

void Push2ProcessorView::pageProcessorsLeft() {
    processorLabelOffset -= NUM_COLUMNS - 1;
    updateProcessorButtons();
    updatePageButtonVisibility();
}

void Push2ProcessorView::pageProcessorsRight() {
    processorLabelOffset += NUM_COLUMNS - 1;
    updateProcessorButtons();
    updatePageButtonVisibility();
}

void Push2ProcessorView::valueTreePropertyChanged(ValueTree &tree, const Identifier &i) {
    Push2TrackManagingView::valueTreePropertyChanged(tree, i);
    if (Processor::isType(tree)) {
        int processorIndex = tree.getParent().indexOf(tree);
        if (i == ProcessorIDs::name) {
            auto buttonIndex = getButtonIndexForProcessorIndex(processorIndex);
            if (auto *processorLabel = processorLabels[buttonIndex]) {
                processorLabel->setText(Processor::getName(tree), dontSendNotification);
            }
        }
    }
}

void Push2ProcessorView::trackColourChanged(const String &trackUuid, const Colour &colour) {
    Push2TrackManagingView::trackColourChanged(trackUuid, colour);
    auto *track = tracks.findTrackWithUuid(trackUuid);
    if (track != nullptr && track->hasSelections()) {
        updateColours();
    }
}

void Push2ProcessorView::selectProcessor(int processorIndex) {
    const auto &focusedLane = Track::getProcessorLane(tracks.getFocusedTrackState());
    if (focusedLane.isValid() && processorIndex < focusedLane.getNumChildren()) {
        project.selectProcessor(focusedLane.getChild(processorIndex));
    }
}
