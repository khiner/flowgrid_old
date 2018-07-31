#pragma once

#include <processors/StatefulAudioProcessorWrapper.h>
#include "JuceHeader.h"
#include "Push2ComponentBase.h"

class Push2ProcessorView : public Push2ComponentBase {
public:
    Push2ProcessorView(Project& project, Push2MidiCommunicator& push2MidiCommunicator)
            : Push2ComponentBase(project, push2MidiCommunicator) {
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

    ~Push2ProcessorView() override {
        detachAllParameters();
    }

    void resized() override {
        for (int i = 0; i < sliders.size(); i++) {
            Slider *slider = sliders.getUnchecked(i);
            slider->setBounds(i * getWidth() / 8, 20, getWidth() / 8, getWidth() / 8);
            slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxBelow, false, getHeight(), getHeight() / 5);
        }
    }

    void setProcessor(StatefulAudioProcessorWrapper *processor) {
        if (this->processor != processor) {
            detachAllParameters();
        }
        this->processor = processor;
        for (auto *c : getChildren())
            c->setVisible(false);
        if (processor != nullptr) {
            for (int i = 0; i < jmin(sliders.size(), processor->processor->getParameters().size()); i++) {
                Slider *slider = sliders.getUnchecked(i);
                if (auto *parameter = processor->getParameter(i)) {
                    parameter->attachSlider(slider);
                    labels.getUnchecked(i)->setText(parameter->name, dontSendNotification);
                }
                slider->setVisible(true);
            }
        }
    }
    
private:
    const static int MAX_PROCESSOR_PARAMS_TO_DISPLAY = 8;

    StatefulAudioProcessorWrapper *processor {};
    OwnedArray<Slider> sliders;
    OwnedArray<Label> labels;

    void detachAllParameters() {
        if (processor != nullptr) {
            for (int i = 0; i < jmin(sliders.size(), processor->processor->getParameters().size()); i++) {
                Slider *slider = sliders.getUnchecked(i);
                if (auto *parameter = processor->getParameter(i)) {
                    parameter->detachSlider(slider);
                }
            }
        }
    }
};
