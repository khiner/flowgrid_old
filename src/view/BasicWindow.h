#pragma once

#include "view/UiColours.h"

class BasicWindow : public DocumentWindow {
public:
    explicit BasicWindow(const String &name, Component *contentComponent, bool owned, std::function<void()> onClose = {}) :
            DocumentWindow(name, Colours::lightgrey, DocumentWindow::allButtons), onClose(std::move(onClose)) {
        if (owned)
            setContentOwned(contentComponent, true);
        else
            setContentNonOwned(contentComponent, true);
        setResizable(true, true);
        setVisible(true);
        setBackgroundColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void closeButtonPressed() override {
        if (onClose)
            onClose();
        else
            delete this;
    }

private:
    std::function<void()> onClose;
};
