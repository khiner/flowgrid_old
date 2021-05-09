#pragma once

struct TooltipBar : public Component, private Timer {
    TooltipBar() {
        startTimer(100);
    }

    void paint(Graphics &g) override {
        g.setColour(findColour(ResizableWindow::backgroundColourId));
        g.drawRect(getLocalBounds(), 2);
        g.setFont(Font(getHeight() * 0.7f, Font::bold));
        const auto &textColour = findColour(TextEditor::textColourId);
        g.setColour(tip == DEFAULT_TOOLTIP ? textColour.withAlpha(0.5f) : textColour);
        g.drawFittedText(tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
    }

    void timerCallback() override {
        String newTip = DEFAULT_TOOLTIP;

        if (auto *underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (auto *ttc = dynamic_cast<TooltipClient *> (underMouse))
                if (!(underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                    newTip = ttc->getTooltip();

        if (newTip != tip) {
            tip = newTip;
            repaint();
        }
    }

    String tip;
    String DEFAULT_TOOLTIP = "Hover over anything for info";
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TooltipBar)
};
