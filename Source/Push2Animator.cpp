#include "Push2Animator.h"

Push2Animator::Push2Animator(): fakeElapsedTime() {
    startTimerHz(60); // animation timer

    setSize(Push2Display::WIDTH, Push2Display::HEIGHT);

    for (int sliderIndex = 0; sliderIndex < 8; sliderIndex++) {
        auto slider = std::make_unique<Slider>();
        addAndMakeVisible(slider.get());
        slider->setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        slider->setBounds(sliderIndex * Push2Display::WIDTH / 8, 0, Push2Display::WIDTH / 8, Push2Display::WIDTH / 8);
        slider->setTextBoxStyle(Slider::TextEntryBoxPosition::TextBoxAbove, false, Push2Display::HEIGHT, Push2Display::HEIGHT / 5);

        sliders.push_back(std::move(slider));
    }
}

void Push2Animator::timerCallback() {
    fakeElapsedTime += 0.02f;
    //auto t1 = std::chrono::high_resolution_clock::now();
    drawFrame();
    //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1).count() << '\n';
}

void Push2Animator::drawFrame() {
    static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

    auto &g = displayBridge.getGraphics();
    g.fillAll(CLEAR_COLOR);

    this->paintEntireComponent(g, true);
    displayBridge.writeFrameToDisplay();
}
