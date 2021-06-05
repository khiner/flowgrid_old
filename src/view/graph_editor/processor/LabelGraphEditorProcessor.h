#pragma once

#include "BaseGraphEditorProcessor.h"

class LabelGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    LabelGraphEditorProcessor(Processor *processor, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener);

    void resized() override;

private:
    DrawableText nameLabel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
