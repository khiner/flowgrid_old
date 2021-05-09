#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

class SwitchParameterComponent : public Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;

        virtual void switchChanged(SwitchParameterComponent *switchThatHasChanged) = 0;
    };

    explicit SwitchParameterComponent(const StringArray &labels);

    int getNumItems() const { return buttons.size(); }

    int getSelectedItemIndex() const;

    void setSelectedItemIndex(int index, NotificationType notificationType);

    String getText() const;

    void addListener(Listener *l) { listeners.add(l); }

    void removeListener(Listener *l) { listeners.remove(l); }

    void resized() override {
        auto area = getLocalBounds();
        auto buttonHeight = getHeight() / buttons.size();
        for (auto *button : buttons)
            button->setBounds(area.removeFromTop(buttonHeight));
    }

private:
    void buttonChanged(TextButton *button);

    OwnedArray<TextButton> buttons;
    ListenerList<Listener> listeners;
};
