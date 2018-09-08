#pragma once

#include "JuceHeader.h"
#include "processors/MidiKeyboardProcessor.h"

/**
    A desktop window containing a plugin's GUI.
*/
class PluginWindow : public DocumentWindow {
public:
    enum class Type {
        normal = 0,
        programs,
        audioIO,
    };

    PluginWindow(AudioProcessorGraph::Node *n, Type t, OwnedArray<PluginWindow> &windowList)
            : DocumentWindow(n->getProcessor()->getName(),
                             LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId),
                             DocumentWindow::minimiseButton | DocumentWindow::closeButton),
              activeWindowList(windowList),
              node(n), type(t) {
        setSize(400, 300);

        Component* keyboardComponent {};
        if (auto *midiKeyboardProcessor = dynamic_cast<MidiKeyboardProcessor *>(n->getProcessor())) {
            keyboardComponent = midiKeyboardProcessor->createKeyboard();
            keyboardComponent->setSize(800, 1);
            setContentOwned(keyboardComponent, true);
            keyboardComponent->grabKeyboardFocus();
        } else if (auto *ui = createProcessorEditor(*node->getProcessor(), type))
            setContentOwned(ui, true);

        setTopLeftPosition(node->properties.getWithDefault(getLastXProp(type), Random::getSystemRandom().nextInt(500)),
                           node->properties.getWithDefault(getLastYProp(type), Random::getSystemRandom().nextInt(500)));

        node->properties.set(getOpenProp(type), true);

        setAlwaysOnTop(true);
        setVisible(true);
        if (keyboardComponent != nullptr)
            keyboardComponent->grabKeyboardFocus();
    }

    ~PluginWindow() override {
        clearContentComponent();
    }

    void moved() override {
        node->properties.set(getLastXProp(type), getX());
        node->properties.set(getLastYProp(type), getY());
    }

    void closeButtonPressed() override {
        node->properties.set(getOpenProp(type), false);
        activeWindowList.removeObject(this);
    }

    static String getLastXProp(Type type) { return "uiLastX_" + getTypeName(type); }

    static String getLastYProp(Type type) { return "uiLastY_" + getTypeName(type); }

    static String getOpenProp(Type type) { return "uiopen_" + getTypeName(type); }

    OwnedArray<PluginWindow> &activeWindowList;
    const AudioProcessorGraph::Node::Ptr node;
    const Type type;

private:
    float getDesktopScaleFactor() const override { return 1.0f; }

    static AudioProcessorEditor *createProcessorEditor(AudioProcessor &processor, PluginWindow::Type type) {
        if (type == PluginWindow::Type::normal) {
            if (auto *ui = processor.createEditorIfNeeded())
                return ui;
        }

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

            setSize(400, jlimit(25, 400, totalHeight));
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

            void audioProcessorChanged(AudioProcessor *) override {}

            void audioProcessorParameterChanged(AudioProcessor *, int, float) override {}

            AudioProcessor &owner;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PropertyComp)
        };

        PropertyPanel panel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
};
