#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

#include "Push2Animator.h"
#include "AppState.h"
#include "MidiParameterTranslator.h"
#include "AudioSettings.h"

/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainContentComponent : public AudioAppComponent {
public:
    MainContentComponent(): audioSettings(deviceManager), midiParameterTranslator(appState) {
        setSize(960, 600);
        setAudioChannels(2, 2);

        status.setColour(Label::textColourId, Colours::white);
        status.setJustificationType(Justification::bottomLeft);
        addAndMakeVisible(status);

        push2MidiCommunicator.setMidiInputCallback([this](const MidiMessage &message) {
            AudioProcessorParameter *parameter = midiParameterTranslator.translate(message);
        });

        addAndMakeVisible(audioSettings.getAudioDeviceSelectorComponent().get());
   }

    ~MainContentComponent() override {
        shutdownAudio();
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        // This function will be called when the audio device is started, or when
        // its settings (i.e. sample rate, block size, etc) are changed.

        // You can use this function to initialise any resources you might need,
        // but be careful - it will be called on the audio thread, not the GUI thread.

        // For more details, see the help for AudioProcessor::prepareToPlay()
    }

    void getNextAudioBlock(const AudioSourceChannelInfo &bufferToFill) override {
        // Your audio-processing code goes here!

        // For more details, see the help for AudioProcessor::getNextAudioBlock()

        // Right now we are not producing any data, in which case we need to clear the buffer
        // (to prevent the output of random noise)
        bufferToFill.clearActiveBufferRegion();
    }

    void releaseResources() override {
        // This will be called when the audio device stops, or when it is being
        // restarted due to a setting change.

        // For more details, see the help for AudioProcessor::releaseResources()
    }

    inline Colour getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour uiColour, Colour fallback = Colour (0xff4d4d4d)) noexcept
    {
        if (auto* v4 = dynamic_cast<LookAndFeel_V4*> (&LookAndFeel::getDefaultLookAndFeel()))
            return v4->getCurrentColourScheme().getUIColour (uiColour);

        return fallback;
    }

    void paint(Graphics &g) override {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
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
    Push2Animator push2Animator;
    Push2MidiCommunicator push2MidiCommunicator;
    Label status;

    AppState appState;
    MidiParameterTranslator midiParameterTranslator;

    AudioSettings audioSettings;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


// (This function is called by the app startup code to create our main component)
Component *createMainContentComponent() { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED
