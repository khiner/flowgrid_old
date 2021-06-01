#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginWindowType.h"

using namespace juce;

class PluginWindow : public DocumentWindow {
public:
    PluginWindow(ValueTree &processorState, AudioProcessor *processor, PluginWindowType type);

    ~PluginWindow() override;

    void moved() override;

    void closeButtonPressed() override;

    ValueTree processor;
    const PluginWindowType type;

private:
    float getDesktopScaleFactor() const override { return 1.0f; }

    static AudioProcessorEditor *createProcessorEditor(AudioProcessor &processor, PluginWindowType type);

    static String getTypeName(PluginWindowType type) {
        switch (type) {
            case PluginWindowType::normal:
                return "Normal";
            case PluginWindowType::programs:
                return "Programs";
            case PluginWindowType::audioIO:
                return "IO";
            case PluginWindowType::none:
            default:
                return {};
        }
    }

    struct ProgramAudioProcessorEditor : public AudioProcessorEditor {
        explicit ProgramAudioProcessorEditor(AudioProcessor &p);

        void paint(Graphics &g) override;
        void resized() override;

    private:
        struct PropertyComp : public PropertyComponent,
                              private AudioProcessorListener {
            PropertyComp(const String &name, AudioProcessor &p);

            ~PropertyComp() override;

            void refresh() override {}

            void audioProcessorChanged(AudioProcessor *, const ChangeDetails &details) override {}
            void audioProcessorParameterChanged(AudioProcessor *, int, float) override {}

            AudioProcessor &owner;
        };

        PropertyPanel panel;
    };
};
