#pragma once

#include "LevelMeterSource.h"
#include "LevelMeter.h"

class MinimalLevelMeter : public LevelMeter {
public:
    explicit MinimalLevelMeter(Orientation orientation);

    void resized() override;

private:
    static constexpr int THUMB_WIDTH = 4;

    Rectangle<float> getMeterBounds();

    void drawMeterBars(Graphics &g, const LevelMeterSource *source) override;
};
