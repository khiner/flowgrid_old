#pragma once

#include "BaseGraphEditorProcessor.h"

class LabelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    LabelGraphEditorProcessor(const ValueTree &state, ViewState &view, TracksState &tracks, ProcessorGraph &processorGraph, ConnectorDragListener &connectorDragListener);

    void resized() override;

private:
    DrawableText nameLabel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
