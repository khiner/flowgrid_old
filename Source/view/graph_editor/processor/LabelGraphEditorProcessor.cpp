#include "LabelGraphEditorProcessor.h"

LabelGraphEditorProcessor::LabelGraphEditorProcessor(SAPC &audioProcessorContainer, TracksState &tracks, ViewState &view, const ValueTree &state, ConnectorDragListener &connectorDragListener) :
        BaseGraphEditorProcessor(audioProcessorContainer, tracks, view, state, connectorDragListener) {
    valueTreePropertyChanged(this->state, IDs::name);
    if (this->state.hasProperty(IDs::deviceName))
        valueTreePropertyChanged(this->state, IDs::deviceName);

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

    if (i == IDs::deviceName) {
        setName(v[IDs::deviceName]);
        nameLabel.setText(getName());
    } else if (i == IDs::name) {
        setName(v[IDs::name]);
        nameLabel.setText(getName());
    }

    BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
}
