#pragma once

#include "JuceHeader.h"

class AudioSettings : public ChangeListener {
public:
    AudioSettings(AudioDeviceManager &audioDeviceManager): audioDeviceManager(audioDeviceManager) {

        audioSetupComp.reset (new AudioDeviceSelectorComponent (audioDeviceManager,
                                                                0, 256, 0, 256, true, true, true, false));
        diagnosticsBox.setMultiLine (true);
        diagnosticsBox.setReturnKeyStartsNewLine (true);
        diagnosticsBox.setReadOnly (true);
        diagnosticsBox.setScrollbarsShown (true);
        diagnosticsBox.setCaretVisible (false);
        diagnosticsBox.setPopupMenuEnabled (true);

        audioDeviceManager.addChangeListener (this);
    }

    std::unique_ptr<AudioDeviceSelectorComponent>& getAudioDeviceSelectorComponent() {
        return audioSetupComp;
    }

    TextEditor& getDiagnosticsBox() {
        return diagnosticsBox;
    }

    void dumpDeviceInfo() {
        logMessage ("--------------------------------------");
        logMessage ("Current audio device type: " + (audioDeviceManager.getCurrentDeviceTypeObject() != nullptr
                                                     ? String (audioDeviceManager.getCurrentDeviceTypeObject()->getTypeName())
                                                     : "<none>"));

        if (AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
        {
            logMessage ("Current audio device: "   + device->getName().quoted());
            logMessage ("Sample rate: "    + String (device->getCurrentSampleRate()) + " Hz");
            logMessage ("Block size: "     + String (device->getCurrentBufferSizeSamples()) + " samples");
            logMessage ("Output Latency: " + String (device->getOutputLatencyInSamples())   + " samples");
            logMessage ("Input Latency: "  + String (device->getInputLatencyInSamples())    + " samples");
            logMessage ("Bit depth: "      + String (device->getCurrentBitDepth()));
            logMessage ("Input channel names: "    + device->getInputChannelNames().joinIntoString (", "));
            logMessage ("Active input channels: "  + getListOfActiveBits (device->getActiveInputChannels()));
            logMessage ("Output channel names: "   + device->getOutputChannelNames().joinIntoString (", "));
            logMessage ("Active output channels: " + getListOfActiveBits (device->getActiveOutputChannels()));
        }
        else
        {
            logMessage ("No audio device open");
        }
    }

    void logMessage (const String& m) {
        diagnosticsBox.moveCaretToEnd();
        diagnosticsBox.insertTextAtCaret (m + newLine);
    }
private:
    AudioDeviceManager& audioDeviceManager;
    std::unique_ptr<AudioDeviceSelectorComponent> audioSetupComp;
    TextEditor diagnosticsBox;

    void changeListenerCallback (ChangeBroadcaster*) override {
        dumpDeviceInfo();
    }

    static String getListOfActiveBits (const BigInteger& b)
    {
        StringArray bits;

        for (int i = 0; i <= b.getHighestBit(); ++i)
            if (b[i])
                bits.add (String (i));

        return bits.joinIntoString (", ");
    }
};


