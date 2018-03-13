#include "Push2Animator.h"
#include "chrono"

using namespace std;

Push2Animator::Push2Animator(): fakeElapsedTime(0) {
    startTimerHz(60); // animation timer
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

    const auto height = Push2Display::HEIGHT;
    const auto width = Push2Display::WIDTH;

    Path wavePath;

    const float waveStep = 10.0f;
    const float waveY = height * 0.44f;
    int i = 0;

    for (float x = waveStep * 0.5f; x < width; x += waveStep) {
        const float y1 = waveY + height * 0.10f * std::sin(i * 0.38f + fakeElapsedTime);
        const float y2 = waveY + height * 0.20f * std::sin(i * 0.20f + fakeElapsedTime * 2.0f);

        wavePath.addLineSegment(Line<float>(x, y1, x, y2), 2.0f);
        wavePath.addEllipse(x - waveStep * 0.3f, y1 - waveStep * 0.3f, waveStep * 0.6f, waveStep * 0.6f);
        wavePath.addEllipse(x - waveStep * 0.3f, y2 - waveStep * 0.3f, waveStep * 0.6f, waveStep * 0.6f);

        ++i;
    }

    g.setColour(juce::Colour::greyLevel(0.5f));
    g.fillPath(wavePath);

    auto logo = ImageCache::getFromMemory(BinaryData::PushStartup_png, BinaryData::PushStartup_pngSize);
    g.drawImageAt(logo, (width - logo.getWidth()) / 2, (height - logo.getHeight()) / 2);

    displayBridge.writeFrameToDisplay();
}
