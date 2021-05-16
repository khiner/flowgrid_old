#include "LabelGraphEditorProcessor.h"

LabelGraphEditorProcessor::LabelGraphEditorProcessor(const ValueTree &state, ViewState &view, TracksState &tracks, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(state, view, tracks, processorGraph, connectorDragListener) {
    valueTreePropertyChanged(this->state, TrackStateIDs::name);
    if (this->state.hasProperty(ProcessorStateIDs::deviceName))
        valueTreePropertyChanged(this->state, ProcessorStateIDs::deviceName);

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

    if (i == ProcessorStateIDs::deviceName) {
        setName(v[ProcessorStateIDs::deviceName]);
        nameLabel.setText(getName());
    } else if (i == TrackStateIDs::name) {
        setName(v[TrackStateIDs::name]);
        nameLabel.setText(getName());
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}
