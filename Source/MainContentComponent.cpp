#pragma once

#include <audio_processors/MainProcessor.h>

#include "Push2Animator.h"
#include "AudioSettings.h"

/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainContentComponent : public Component {
public:
    MainContentComponent():
            audioSettings(deviceManager), mainProcessor(2, 2), instrumentViewComponent(mainProcessor.getCurrentInstrument()), push2Animator(&push2ViewComponent) {

        setSize(960, 600);
        deviceManager.initialiseWithDefaultDevices(2, 2);
        player.setProcessor(&mainProcessor);
        deviceManager.addAudioCallback(&player);

        status.setColour(Label::textColourId, Colours::white);
        status.setJustificationType(Justification::bottomLeft);
        addAndMakeVisible(status);

        push2MidiCommunicator.setMidiInputCallback([this](const MidiMessage &message) {
            mainProcessor.handleControlMidi(message);
        });

        push2ViewComponent.addAndMakeVisible(instrumentViewComponent);
        addAndMakeVisible(push2ViewComponent);
        auto audioDeviceSelectorComponent = audioSettings.getAudioDeviceSelectorComponent().get();
        addAndMakeVisible(audioDeviceSelectorComponent);
        audioDeviceSelectorComponent->setTopLeftPosition(0, push2ViewComponent.getHeight());
   }

    ~MainContentComponent() override {
        deviceManager.removeAudioCallback(&player);
    }

    void paint(Graphics &g) override {
        g.fillAll(backgroundColor);
    }

    void resized() override {
        const int kStatusSize = 100;
        status.setBounds(0, getHeight() - kStatusSize, getWidth(), kStatusSize);

        auto r =  getLocalBounds().reduced (4);
        audioSettings.getAudioDeviceSelectorComponent()->setBounds(r.removeFromTop (proportionOfHeight (0.65f)));
        audioSettings.getDiagnosticsBox().setBounds(r);
    }

    void lookAndFeelChanged() override {
        audioSettings.getDiagnosticsBox().applyFontToAllText(audioSettings.getDiagnosticsBox().getFont());
    }

private:
    const Colour backgroundColor = dynamic_cast<LookAndFeel_V4&>(LookAndFeel::getDefaultLookAndFeel()).getCurrentColourScheme().getUIColour((LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    AudioDeviceManager deviceManager;
    Push2MidiCommunicator push2MidiCommunicator;
    Label status;

    AudioSettings audioSettings;
    MainProcessor mainProcessor;
    InstrumentViewComponent instrumentViewComponent;
    Push2ViewComponent push2ViewComponent;
    Push2Animator push2Animator;
    AudioProcessorPlayer player;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


// (This function is called by the app startup code to create our main component)
Component *createMainContentComponent() { return new MainContentComponent(); }
