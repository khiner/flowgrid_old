#pragma once

#include <ApplicationPropertiesAndCommandManager.h>
#include "processors/MidiKeyboardProcessor.h"

class PluginWindow : public DocumentWindow {
public:
    enum class Type {
        none = -1,
        normal = 0,
        programs,
        audioIO,
    };

    PluginWindow(ValueTree &processorState, AudioProcessor *processor, Type type)
            : DocumentWindow(processorState[IDs::name],
                             LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                             DocumentWindow::minimiseButton | DocumentWindow::closeButton),
              processor(processorState), type(type) {
        setSize(400, 300);

        Component *keyboardComponent{};
        if (auto *midiKeyboardProcessor = dynamic_cast<MidiKeyboardProcessor *>(processor)) {
            keyboardComponent = midiKeyboardProcessor->createKeyboard();
            keyboardComponent->setSize(800, 1);
            setContentOwned(keyboardComponent, true);
        } else if (auto *ui = createProcessorEditor(*processor, type)) {
            setContentOwned(ui, true);
        }

        int xPosition = processorState.hasProperty(IDs::pluginWindowX) ? int(processorState[IDs::pluginWindowX]) : Random::getSystemRandom().nextInt(500);
        int yPosition = processorState.hasProperty(IDs::pluginWindowX) ? int(processorState[IDs::pluginWindowY]) : Random::getSystemRandom().nextInt(500);
        setTopLeftPosition(xPosition, yPosition);
        setAlwaysOnTop(true);
        setVisible(true);
        if (keyboardComponent != nullptr)
            keyboardComponent->grabKeyboardFocus();
        addKeyListener(getCommandManager().getKeyMappings());
    }

    ~PluginWindow() override {
        clearContentComponent();
    }

    void moved() override {
        processor.setProperty(IDs::pluginWindowX, getX(), nullptr);
        processor.setProperty(IDs::pluginWindowY, getY(), nullptr);
    }

    void closeButtonPressed() override {
        processor.setProperty(IDs::pluginWindowType, static_cast<int>(Type::none), nullptr);
    }

    ValueTree processor;
    const Type type;

private:

    float getDesktopScaleFactor() const override { return 1.0f; }

    static AudioProcessorEditor *createProcessorEditor(AudioProcessor &processor, PluginWindow::Type type) {
        if (type == PluginWindow::Type::normal)
            if (auto *ui = processor.createEditorIfNeeded())
                return ui;

        if (type == PluginWindow::Type::programs)
            return new ProgramAudioProcessorEditor(processor);

//        if (type == PluginWindow::Type::audioIO)
//            return new FilterIOConfigurationWindow (processor);

        jassertfalse;
        return {};
    }

    static String getTypeName(Type type) {
        switch (type) {
            case Type::normal:
                return "Normal";
            case Type::programs:
                return "Programs";
            case Type::audioIO:
                return "IO";
            default:
                return {};
        }
    }

    //==============================================================================
    struct ProgramAudioProcessorEditor : public AudioProcessorEditor {
        explicit ProgramAudioProcessorEditor(AudioProcessor &p) : AudioProcessorEditor(p) {
            setOpaque(true);

            addAndMakeVisible(panel);

            Array<PropertyComponent *> programs;

            auto numPrograms = p.getNumPrograms();
            int totalHeight = 0;

            for (int i = 0; i < numPrograms; ++i) {
                auto name = p.getProgramName(i).trim();

                if (name.isEmpty())
                    name = "Unnamed";

                auto pc = new PropertyComp(name, p);
                programs.add(pc);
                totalHeight += pc->getPreferredHeight();
            }

            panel.addProperties(programs);

            setSize(400, std::clamp(totalHeight, 25, 400));
        }

        void paint(Graphics &g) override {
            g.fillAll(Colours::grey);
        }

        void resized() override {
            panel.setBounds(getLocalBounds());
        }

    private:
        struct PropertyComp : public PropertyComponent,
                              private AudioProcessorListener {
            PropertyComp(const String &name, AudioProcessor &p) : PropertyComponent(name), owner(p) {
                owner.addListener(this);
            }

            ~PropertyComp() override {
                owner.removeListener(this);
            }

            void refresh() override {}

            void audioProcessorChanged(AudioProcessor *, const ChangeDetails& details) override {}

            void audioProcessorParameterChanged(AudioProcessor *, int, float) override {}

            AudioProcessor &owner;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertyComp)
        };

        PropertyPanel panel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
