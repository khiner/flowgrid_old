#pragma once

#include <audio_processors/StatefulAudioProcessor.h>
#include "JuceHeader.h"

class Push2ProcessorViewComponent : public Component {
public:
    Push2ProcessorViewComponent();

    void setStatefulAudioProcessor(StatefulAudioProcessor *statefulAudioProcessor);

private:
    std::vector<std::unique_ptr<Slider> > sliders;
    std::vector<std::unique_ptr<Label> > labels;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> > sliderAttachments;
};
