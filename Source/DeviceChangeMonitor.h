#pragma once

#include "JuceHeader.h"

class DeviceChangeMonitor : private Timer {
public:
    explicit DeviceChangeMonitor(AudioDeviceManager &audioDeviceManager) : audioDeviceManager(audioDeviceManager) {
        startTimer(1000);
    }

private:
    AudioDeviceManager &audioDeviceManager;

    int numInputDevices{0}, numOutputDevices{0};

    void timerCallback() override {
        int newNumInputDevices = MidiInput::getDevices().size();
        int newNumOutputDevices = MidiOutput::getDevices().size();

        if (numInputDevices != newNumInputDevices || numOutputDevices != newNumOutputDevices)
            audioDeviceManager.sendChangeMessage();

        numInputDevices = newNumInputDevices;
        numOutputDevices = newNumOutputDevices;
    }
};
