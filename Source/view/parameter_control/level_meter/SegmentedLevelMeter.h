#pragma once

#include "LevelMeter.h"
#include "LevelMeterSource.h"

class SegmentedLevelMeter : public LevelMeter {
public:
    SegmentedLevelMeter();

    void resized() override {
        verticalGradient.clearColours();
    }

private:
    ColourGradient verticalGradient;

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) override;
};
