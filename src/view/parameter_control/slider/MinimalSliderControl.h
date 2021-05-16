#pragma once

#include "SliderControl.h"

class MinimalSliderControl : public SliderControl {
public:
    explicit MinimalSliderControl(Orientation orientation, bool fromCenter = false);

    void resized() override;

    void paint(Graphics &g) override;

private:
    static constexpr int THUMB_WIDTH = 4;

    Rectangle<int> getSliderBounds();

    Rectangle<int> getSliderBarFillBounds(const Rectangle<int> &sliderBarBounds);
};
