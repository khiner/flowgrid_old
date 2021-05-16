#include "LabelGraphEditorProcessor.h"

LabelGraphEditorProcessor::LabelGraphEditorProcessor(const ValueTree &state, View &view, Tracks &tracks, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(state, view, tracks, processorGraph, connectorDragListener) {
    valueTreePropertyChanged(this->state, ProcessorIDs::name);
    if (this->state.hasProperty(ProcessorIDs::deviceName))
        valueTreePropertyChanged(this->state, ProcessorIDs::deviceName);

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
    if (v != state) return;

    if (i == ProcessorIDs::deviceName) {
        setName(v[ProcessorIDs::deviceName]);
        nameLabel.setText(getName());
    } else if (i == ProcessorIDs::name) {
        setName(v[ProcessorIDs::name]);
        nameLabel.setText(getName());
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}
