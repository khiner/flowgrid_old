#pragma once

#include "view/parameter_control/slider/SliderControl.h"
#include "view/parameter_control/level_meter/LevelMeter.h"
#include "BaseGraphEditorProcessor.h"

class TrackOutputGraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    TrackOutputGraphEditorProcessor(const ValueTree &state, Track *track, View &view, StatefulAudioProcessorWrappers &processorWrappers, ConnectorDragListener &connectorDragListener);

    ~TrackOutputGraphEditorProcessor() override;

    void resized() override;
    void paint(Graphics &g) override;

    bool isInView() override { return true; }

    Rectangle<int> getBoxBounds() const override;

private:
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<SliderControl> panSlider;
    StatefulAudioProcessorWrapper::Parameter *gainParameter;
    StatefulAudioProcessorWrapper::Parameter *panParameter;
    void colourChanged() override { repaint(); }

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override;
};
