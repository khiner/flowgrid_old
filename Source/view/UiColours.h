#pragma once

#include "JuceHeader.h"

Colour getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour uiColour, const Colour &fallback = Colour(0xff4d4d4d)) {
    if (auto *v4 = dynamic_cast<LookAndFeel_V4 *> (&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour(uiColour);

    return fallback;
}
