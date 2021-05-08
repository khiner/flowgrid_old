#pragma once

#include "SliderControl.h"

class MinimalSliderControl : public SliderControl {
public:
    MinimalSliderControl(Orientation orientation, bool fromCenter = false) : SliderControl(orientation, fromCenter) {
        thumb.setColours(findColour(thumbColourId), findColour(thumbColourId), findColour(thumbColourId));
        thumb.setOutline(findColour(thumbColourId), 0);
    }

    void resized() override {
        const auto &sliderBounds = getSliderBounds();
        const auto &localBounds = getLocalBounds();
        int thumbPosition = orientation == vertical ?
                            sliderBounds.getRelativePoint(0.0f, 1.0f - value).y :
                            sliderBounds.getRelativePoint(value, 0.0f).x;

        const auto &thumbBounds = orientation == vertical ?
                                  localBounds.withHeight(THUMB_WIDTH).withY(thumbPosition - THUMB_WIDTH / 2) :
                                  localBounds.withWidth(THUMB_WIDTH).withX(thumbPosition - THUMB_WIDTH / 2);
        Path p;
        p.addRectangle(thumbBounds);
        thumb.setBounds(thumbBounds);
        thumb.setShape(p, false, false, false);
    }

    void paint(Graphics &g) override {
        Graphics::ScopedSaveState saved(g);
        drawSlider(g);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MinimalSliderControl)

    static constexpr int THUMB_WIDTH = 4;

    Rectangle<float> getSliderBounds() {
        const auto &r = getLocalBounds().toFloat();
        return orientation == vertical ?
               r.reduced(getWidth() * 0.2f, THUMB_WIDTH) :
               r.reduced(THUMB_WIDTH, getHeight() * 0.2f);
    }

    Rectangle<float> getSliderBarFillBounds(const Rectangle<float> &sliderBarBounds) {
        if (fromCenter) {
            float fromCenterValue = std::abs(0.5f - value);
            bool extendUp = value >= 0.5f;
            if (orientation == vertical) {
                const auto &fillBar = sliderBarBounds.withHeight(fromCenterValue * sliderBarBounds.getHeight());
                if (extendUp)
                    return fillBar.withBottomY(sliderBarBounds.getCentreY());
                else
                    return fillBar.withY(sliderBarBounds.getCentreY());
            } else {
                const auto &fillBar = sliderBarBounds.withWidth(fromCenterValue * sliderBarBounds.getWidth());
                if (extendUp)
                    return fillBar.withX(sliderBarBounds.getCentreX());
                else
                    return fillBar.withRightX(sliderBarBounds.getCentreX());
            }
        } else {
            return orientation == vertical ?
                   sliderBarBounds.withHeight(value * sliderBarBounds.getHeight()) :
                   sliderBarBounds.withWidth(value * sliderBarBounds.getWidth());
        }
    }

    void drawSlider(Graphics &g) {
        auto sliderBounds = getSliderBounds();
        const float shortDimension = orientation == vertical ? sliderBounds.getWidth() : sliderBounds.getHeight();
        const auto &sliderBarBounds = orientation == vertical ?
                                      sliderBounds.removeFromLeft(shortDimension).reduced(shortDimension * 0.1f, 0.0f) :
                                      sliderBounds.removeFromTop(shortDimension).reduced(0.0f, shortDimension * 0.1f);
        g.setColour(findColour(backgroundColourId));
        g.fillRect(sliderBarBounds);
        const auto &fillBounds = getSliderBarFillBounds(sliderBarBounds);
        g.setColour(findColour(foregroundColourId));
        g.fillRect(fillBounds);
    }
};
