#include "SwitchParameterComponent.h"

SwitchParameterComponent::SwitchParameterComponent(const StringArray &labels) {
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

int SwitchParameterComponent::getSelectedItemIndex() const {
    for (int i = 0; i < buttons.size(); i++)
        if (buttons.getUnchecked(i)->getToggleState())
            return i;
    return 0;
}

void SwitchParameterComponent::setSelectedItemIndex(const int index, const NotificationType notificationType) {
    if (auto *selectedButton = buttons[index]) {
        for (auto *otherButton : buttons) {
            if (otherButton != selectedButton) {
                otherButton->setToggleState(false, notificationType);
            }
        }
        selectedButton->setToggleState(true, notificationType);
    }
}

String SwitchParameterComponent::getText() const {
    for (auto *button : buttons)
        if (button->getToggleState())
            return button->getButtonText();
    return "";
}

void SwitchParameterComponent::buttonChanged(TextButton *button) {
    if (button->getToggleState()) listeners.call(&Listener::switchChanged, this);
}
