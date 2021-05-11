#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "state/Identifiers.h"

using namespace juce;

class PluginWindow : public DocumentWindow {
public:
    enum class Type {
        none = -1,
        normal = 0,
        programs,
        audioIO,
    };

    PluginWindow(ValueTree &processorState, AudioProcessor *processor, Type type);

    ~PluginWindow() override;

    void moved() override;

    void closeButtonPressed() override;

    ValueTree processor;
    const Type type;

private:
    float getDesktopScaleFactor() const override { return 1.0f; }

    static AudioProcessorEditor *createProcessorEditor(AudioProcessor &processor, PluginWindow::Type type);

    static String getTypeName(Type type) {
        switch (type) {
            case Type::normal:
                return "Normal";
            case Type::programs:
                return "Programs";
            case Type::audioIO:
                return "IO";
            case Type::none:
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
