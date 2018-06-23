#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

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
            audioSettings(deviceManager), mainProcessor(2, 2), push2Animator(mainProcessor.getSliders()) {

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

        addAndMakeVisible(audioSettings.getAudioDeviceSelectorComponent().get());
        addAndMakeVisible(push2Animator);
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
    Push2Animator push2Animator;
    AudioProcessorPlayer player;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


// (This function is called by the app startup code to create our main component)
Component *createMainContentComponent() { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED
