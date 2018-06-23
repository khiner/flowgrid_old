#include <push2/Push2Display.h>
#include "InstrumentViewComponent.h"

InstrumentViewComponent::InstrumentViewComponent(Instrument *instrument) {
    setSize(Push2Display::WIDTH, Push2Display::HEIGHT);

    for (int sliderIndex = 0; sliderIndex < instrument->getNumParameters(); sliderIndex++) {
        auto slider = instrument->getSlider(sliderIndex);
        addAndMakeVisible(slider);
        slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        slider->setBounds(sliderIndex * Push2Display::WIDTH / 8, 0, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
        slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxAbove, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);
    }
}
