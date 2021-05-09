#pragma once

#include <view/parameter_control/ParameterControl.h>

class SliderControl : public ParameterControl {
public:
    enum Orientation {
        horizontal,
        vertical,
    };

    explicit SliderControl(Orientation orientation, bool fromCenter = false) :
            ParameterControl(), orientation(orientation), fromCenter(fromCenter),
            thumb("", {}, {}, {}) {
        addAndMakeVisible(thumb);
        thumb.addMouseListener(this, true);
    }

    ~SliderControl() override {
        thumb.removeMouseListener(this);
    }

protected:
    SliderControl::Orientation orientation;
    bool fromCenter;
    ShapeButton thumb;

    float getValueForPosition(juce::Point<int> localPosition) const override {
        return orientation == vertical ?
               std::clamp(1.0f - float(localPosition.y) / float(getHeight()), 0.0f, 1.0f) :
               std::clamp(float(localPosition.x) / float(getWidth()), 0.0f, 1.0f);
    }
};
