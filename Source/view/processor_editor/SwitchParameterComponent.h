#pragma once

#include "JuceHeader.h"

class SwitchParameterComponent : public Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void switchChanged(SwitchParameterComponent* switchThatHasChanged) = 0;
    };

    explicit SwitchParameterComponent(const String& leftButtonText, const String& rightButtonText) {
        auto *leftButton = buttons.add(new TextButton());
        auto *rightButton = buttons.add(new TextButton());

        for (auto *button : buttons) {
            button->setRadioGroupId(293847);
            button->setClickingTogglesState(true);
        }

        leftButton->setButtonText(leftButtonText);
        rightButton->setButtonText(rightButtonText);

        leftButton->setConnectedEdges(Button::ConnectedOnRight);
        rightButton->setConnectedEdges(Button::ConnectedOnLeft);
        leftButton->setToggleState(true, dontSendNotification);

        rightButton->onStateChange = [this]() { rightButtonChanged(); };

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
        auto buttonWidth = getWidth() / buttons.size();
        for (auto *button : buttons)
            button->setBounds(area.removeFromLeft(buttonWidth));
    }

private:
    void rightButtonChanged() {
        listeners.call([this] (Listener& l) { l.switchChanged(this); });
    }

    OwnedArray<TextButton> buttons;
    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchParameterComponent)
};
