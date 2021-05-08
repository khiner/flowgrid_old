#pragma once

class SwitchParameterComponent : public Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void switchChanged(SwitchParameterComponent *switchThatHasChanged) = 0;
    };

    explicit SwitchParameterComponent(const StringArray &labels) {
        for (auto &label : labels) {
            auto *button = new TextButton();
            button->setRadioGroupId(293847);
            button->setClickingTogglesState(true);
            button->setButtonText(label);
            button->onStateChange = [this, button]() { buttonChanged(button); };
            buttons.add(button);
        }
        for (auto *button : buttons) {
            if (button == buttons.getFirst())
                button->setConnectedEdges(Button::ConnectedOnBottom);
            else if (button == buttons.getLast())
                button->setConnectedEdges(Button::ConnectedOnTop);
            else
                button->setConnectedEdges(Button::ConnectedOnTop | Button::ConnectedOnBottom);
        }
        if (!buttons.isEmpty()) {
            buttons.getFirst()->setToggleState(true, dontSendNotification);
        }

        for (auto *button : buttons)
            addAndMakeVisible(button);
    }

    int getSelectedItemIndex() const {
        for (int i = 0; i < buttons.size(); i++)
            if (buttons.getUnchecked(i)->getToggleState())
                return i;

        return 0;
    }

    int getNumItems() const {
        return buttons.size();
    }

    void setSelectedItemIndex(const int index, const NotificationType notificationType) {
        if (auto *selectedButton = buttons[index]) {
            for (auto *otherButton : buttons) {
                if (otherButton != selectedButton) {
                    otherButton->setToggleState(false, notificationType);
                }
            }
            selectedButton->setToggleState(true, notificationType);
        }
    }

    String getText() const {
        for (auto *button : buttons)
            if (button->getToggleState())
                return button->getButtonText();

        return "";
    }

    void addListener(Listener *l) { listeners.add(l); }

    void removeListener(Listener *l) { listeners.remove(l); }

    void resized() override {
        auto area = getLocalBounds();
        auto buttonHeight = getHeight() / buttons.size();
        for (auto *button : buttons)
            button->setBounds(area.removeFromTop(buttonHeight));
    }

private:
    void buttonChanged(TextButton *button) {
        if (button->getToggleState())
            listeners.call([this](Listener &l) { l.switchChanged(this); });
    }

    OwnedArray<TextButton> buttons;
    ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwitchParameterComponent)
};
