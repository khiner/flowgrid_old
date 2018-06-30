#pragma once

#include <processors/StatefulAudioProcessor.h>
#include "JuceHeader.h"

class Push2ProcessorView : public Component {
public:
    Push2ProcessorView() {
        for (int paramIndex = 0; paramIndex < MAX_PROCESSOR_PARAMS_TO_DISPLAY; paramIndex++) {
            Slider *slider = new Slider("Param " + String(paramIndex) + ": ");
            addAndMakeVisible(slider);


            Label *label = new Label(String(), slider->getName());
            label->setJustificationType(Justification::centred);
            label->attachToComponent(slider, false);
            labels.add(label);

            slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
            sliders.add(slider);
        }
    }
    
    void resized() override {
        for (int i = 0; i < sliders.size(); i++) {
            Slider *slider = sliders.getUnchecked(i);
            slider->setBounds(i * getWidth() / 8, 20, getWidth() / 8, getWidth() / 8);
            slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, getHeight(), getHeight() / 5);
        }
    }

    void setProcessor(StatefulAudioProcessor *processor) {
        for (int i = 0; i < processor->getNumParameters(); i++) {
            processor->getParameterInfo(i)->attachSlider(sliders.getUnchecked(i), labels.getUnchecked(i));
        }
    }
    
private:
    const static int MAX_PROCESSOR_PARAMS_TO_DISPLAY = 8;

    OwnedArray<Slider> sliders;
    OwnedArray<Label> labels;
};
