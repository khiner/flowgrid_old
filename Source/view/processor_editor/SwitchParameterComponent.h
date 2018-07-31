#pragma once

#include "JuceHeader.h"

class SwitchParameterComponent : public Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void switchChanged(SwitchParameterComponent* switchThatHasChanged) = 0;
    };

    explicit SwitchParameterComponent(const String& firstButtonText, const String& secondButtonText) {
        auto *firstButton = buttons.add(new TextButton());
        auto *secondButton = buttons.add(new TextButton());

        for (auto *button : buttons) {
            button->setRadioGroupId(293847);
            button->setClickingTogglesState(true);
        }

        firstButton->setButtonText(firstButtonText);
        secondButton->setButtonText(secondButtonText);

        firstButton->setConnectedEdges(Button::ConnectedOnBottom);
        secondButton->setConnectedEdges(Button::ConnectedOnTop);
        firstButton->setToggleState(true, dontSendNotification);

        secondButton->onStateChange = [this]() { secondButtonChanged(); };

        for (auto *button : buttons)
            addAndMakeVisible(button);
    }

    bool getToggleState() const {
        return buttons[1]->getToggleState();
    }

    void setToggleState(bool newState, NotificationType notificationType) {
        if (buttons[1]->getToggleState() != newState) {
            buttons[1]->setToggleState(newState, notificationType);
            buttons[0]->setToggleState(!newState, notificationType);
        }
    }

    String getText() const {
        return getToggleState() ? buttons[1]->getButtonText() : buttons[0]->getButtonText();
    }

    void addListener(Listener* l) { listeners.add (l); }
    void removeListener(Listener* l) { listeners.remove (l); }

    void resized() override {
        auto area = getLocalBounds();
        auto buttonHeight = getHeight() / buttons.size();
        for (auto *button : buttons)
            button->setBounds(area.removeFromTop(buttonHeight));
    }

private:
    void secondButtonChanged() {
        listeners.call([this] (Listener& l) { l.switchChanged(this); });
    }

    OwnedArray<TextButton> buttons;
    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchParameterComponent)
};
