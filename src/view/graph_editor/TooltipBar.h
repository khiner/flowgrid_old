#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

struct TooltipBar : public Component, private Timer {
    TooltipBar();

    void paint(Graphics &g) override;
    void timerCallback() override;

    String tip;
private:
    inline static const String DEFAULT_TOOLTIP = "Hover over anything for info";
};
