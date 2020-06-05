#pragma once

#include "BaseGraphEditorProcessor.h"

class LabelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    LabelGraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                              const ValueTree &state, ConnectorDragListener &connectorDragListener) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener) {
        valueTreePropertyChanged(this->state, IDs::name);
        if (this->state.hasProperty(IDs::deviceName))
            valueTreePropertyChanged(this->state, IDs::deviceName);

        nameLabel.setColour(findColour(TextEditor::textColourId));
        nameLabel.setFontHeight(largeFontHeight);
        nameLabel.setJustification(Justification::centred);
        addAndMakeVisible(nameLabel);
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();

        nameLabel.setBoundingBox(GraphEditorChannel::rotateRectIfNarrow(boxBoundsFloat));
    }

private:
    DrawableText nameLabel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (i == IDs::deviceName) {
            setName(v[IDs::deviceName]);
            nameLabel.setText(getName());
        } else if (i == IDs::name) {
            setName(v[IDs::name]);
            nameLabel.setText(getName());
        }

        BaseGraphEditorProcessor::valueTreePropertyChanged(v, i);
    }
};
