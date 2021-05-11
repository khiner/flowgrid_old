#include "MinimalSliderControl.h"


MinimalSliderControl::MinimalSliderControl(SliderControl::Orientation orientation, bool fromCenter) : SliderControl(orientation, fromCenter) {
    thumb.setColours(findColour(thumbColourId), findColour(thumbColourId), findColour(thumbColourId));
    thumb.setOutline(findColour(thumbColourId), 0);
}

void MinimalSliderControl::resized() {
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

void MinimalSliderControl::paint(Graphics &g) {
    Graphics::ScopedSaveState saved(g);
    auto sliderBounds = getSliderBounds();
    const int shortDimension = orientation == vertical ? sliderBounds.getWidth() : sliderBounds.getHeight();
    const auto &sliderBarBounds = orientation == vertical ?
                                  sliderBounds.removeFromLeft(shortDimension).reduced(shortDimension / 10, 0.0f) :
                                  sliderBounds.removeFromTop(shortDimension).reduced(0.0f, shortDimension / 10);
    g.setColour(findColour(backgroundColourId));
    g.fillRect(sliderBarBounds);
    const auto &fillBounds = getSliderBarFillBounds(sliderBarBounds);
    g.setColour(findColour(foregroundColourId));
    g.fillRect(fillBounds);
}

Rectangle<int> MinimalSliderControl::getSliderBarFillBounds(const Rectangle<int> &sliderBarBounds) {
    if (fromCenter) {
        float fromCenterValue = std::abs(0.5f - value);
        bool extendUp = value >= 0.5f;
        if (orientation == vertical) {
            const auto &fillBar = sliderBarBounds.withHeight(static_cast<int>(fromCenterValue * static_cast<float>(sliderBarBounds.getHeight())));
            return extendUp ? fillBar.withBottomY(sliderBarBounds.getCentreY()) : fillBar.withY(sliderBarBounds.getCentreY());
        } else {
            const auto &fillBar = sliderBarBounds.withWidth(static_cast<int>(fromCenterValue * static_cast<float>(sliderBarBounds.getWidth())));
            return extendUp ? fillBar.withX(sliderBarBounds.getCentreX()) : fillBar.withRightX(sliderBarBounds.getCentreX());
        }
    } else {
        return orientation == vertical ?
               sliderBarBounds.withHeight(static_cast<int>(value * static_cast<float>(sliderBarBounds.getHeight()))) :
               sliderBarBounds.withWidth(static_cast<int>(value * static_cast<float>(sliderBarBounds.getWidth())));
    }
}

Rectangle<int> MinimalSliderControl::getSliderBounds() {
    const auto &r = getLocalBounds();
    return orientation == vertical ?
           r.reduced(getWidth() / 5, THUMB_WIDTH) :
           r.reduced(THUMB_WIDTH, getHeight() / 5);
}

