//
// Created by Karl Hiner on 6/22/18.
//

#include <push2/Push2Display.h>
#include "InstrumentViewComponent.h"

InstrumentViewComponent::InstrumentViewComponent(const std::vector<std::unique_ptr<Slider> > &sliders) {
    setSize(Push2Display::WIDTH, Push2Display::HEIGHT);

    for (int sliderIndex = 0; sliderIndex < sliders.size(); sliderIndex++) {
        auto& slider = sliders[sliderIndex];
        addAndMakeVisible(slider.get());
        slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        slider->setBounds(sliderIndex * Push2Display::WIDTH / 8, 0, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
        slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxAbove, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);
    }
}
