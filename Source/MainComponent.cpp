#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

#include "Push2Demo.h"

#include <iomanip>
#include <deque>
#include <sstream>

/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainContentComponent : public AudioAppComponent {
public:
    MainContentComponent() {
        setSize(960, 600);

        // specify the number of input and output channels that we want to open
        setAudioChannels(2, 2);

        // Initialize a label to show the status of the connection
        status.setColour(Label::textColourId, Colours::white);
        status.setJustificationType(Justification::bottomLeft);
        addAndMakeVisible(status);

        status.setText("Push 2 connected", juce::dontSendNotification);
        demo.SetMidiInputCallback(
                [this](const MidiMessage &message) {
                    this->processMidiInput(message);
                });
    }

    ~MainContentComponent() override {
        shutdownAudio();
    }

    void processMidiInput(const MidiMessage &message) {
        if (message.isAftertouch()) {
            const MessageManagerLock lock;
            status.setText("Aftertouch", juce::dontSendNotification);
            return;
        } else if (message.isChannelPressure()) {
            const MessageManagerLock lock;
            status.setText("Channel pressure", juce::dontSendNotification);
            return;
        }
        if (message.getRawDataSize() == 3) {
            auto data = message.getRawData();
            std::ostringstream oss;
            oss << "Midi ("
                << message.getTimeStamp() << ") :"
                << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << int(data[0]) << " - "
                << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << int(data[1]) << " - "
                << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << int(data[2]);

            midiMessageStack.push_back(oss.str());
            if (midiMessageStack.size() > 4) {
                midiMessageStack.pop_front();
            }

            const MessageManagerLock lock;

            std::string statusStr;
            for (const auto &s : midiMessageStack) {
                statusStr += s + "\n";
            }
            status.setText(statusStr, juce::dontSendNotification);
        } else {
            const MessageManagerLock lock;
            status.setText("MIDI!!!", juce::dontSendNotification);
        }
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

    void paint(Graphics &g) override {
        // (Our component is opaque, so we must completely fill the background with a solid colour)
        g.fillAll(Colours::black);

        auto logo = ImageCache::getFromMemory(BinaryData::PushStartup_png, BinaryData::PushStartup_pngSize);
        g.drawImageAt(logo, (getWidth() - logo.getWidth()) / 2, (getHeight() - logo.getHeight()) / 2);
    }

    void resized() override {
        const int kStatusSize = 100;
        status.setBounds(0, getHeight() - kStatusSize, getWidth(), kStatusSize);
    }


private:
    Demo demo;
    Label status;
    std::deque<std::string> midiMessageStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


// (This function is called by the app startup code to create our main component)
Component *createMainContentComponent() { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED
