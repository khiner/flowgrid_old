#include "Push2Animator.h"

void Push2Animator::timerCallback() {
    //auto t1 = std::chrono::high_resolution_clock::now();
    drawFrame();
    //std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1).count() << '\n';
}

void Push2Animator::drawFrame() {
    static const juce::Colour CLEAR_COLOR = juce::Colour(0xff000000);

    auto &g = displayBridge.getGraphics();
    g.fillAll(CLEAR_COLOR);
    paintEntireComponent(g, true);
    displayBridge.writeFrameToDisplay();
}
