#pragma once

#include "JuceHeader.h"
#include <intruments/InstrumentSource.h>

class InstrumentViewComponent : public Component {
public:
    InstrumentViewComponent();

    void setInstrument(InstrumentSource *instrument);

private:
    std::vector<std::unique_ptr<Slider> > sliders;
    std::vector<std::unique_ptr<Label> > labels;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> > sliderAttachments;
};
