#include "LabelGraphEditorProcessor.h"

LabelGraphEditorProcessor::LabelGraphEditorProcessor(Processor *processor, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(processor, track, view, processorWrappers, connectorDragListener) {
    valueTreePropertyChanged(this->processor->getState(), ProcessorIDs::name);
    if (this->processor->getState().hasProperty(ProcessorIDs::deviceName))
        valueTreePropertyChanged(this->processor->getState(), ProcessorIDs::deviceName);

    nameLabel.setColour(findColour(TextEditor::textColourId));
    nameLabel.setFontHeight(largeFontHeight);
    nameLabel.setJustification(Justification::centred);
    addAndMakeVisible(nameLabel);
}

void LabelGraphEditorProcessor::resized() {
    BaseGraphEditorProcessor::resized();
    auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();

    nameLabel.setBoundingBox(GraphEditorChannel::rotateRectIfNarrow(boxBoundsFloat));
}

void LabelGraphEditorProcessor::valueTreePropertyChanged(ValueTree &v, const Identifier &i) {
    if (v != processor->getState()) return;

    if (i == ProcessorIDs::deviceName) {
        setName(Processor::getDeviceName(v));
        nameLabel.setText(getName());
    } else if (i == ProcessorIDs::name) {
        setName(Processor::getName(v));
        nameLabel.setText(getName());
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}
