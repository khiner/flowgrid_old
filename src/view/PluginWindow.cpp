#include "ApplicationPropertiesAndCommandManager.h"
#include "PluginWindow.h"

#include "processors/MidiKeyboardProcessor.h"
#include "model/Tracks.h"

PluginWindow::PluginWindow(ValueTree &processorState, AudioProcessor *processor, Type type)
        : DocumentWindow(processorState[ProcessorIDs::name],
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

    int xPosition = processorState.hasProperty(ProcessorIDs::pluginWindowX) ? int(processorState[ProcessorIDs::pluginWindowX]) : Random::getSystemRandom().nextInt(500);
    int yPosition = processorState.hasProperty(ProcessorIDs::pluginWindowX) ? int(processorState[ProcessorIDs::pluginWindowY]) : Random::getSystemRandom().nextInt(500);
    setTopLeftPosition(xPosition, yPosition);
    setAlwaysOnTop(true);
    setVisible(true);
    if (keyboardComponent != nullptr)
        keyboardComponent->grabKeyboardFocus();
    addKeyListener(getCommandManager().getKeyMappings());
}

PluginWindow::~PluginWindow() {
    clearContentComponent();
}

AudioProcessorEditor *PluginWindow::createProcessorEditor(AudioProcessor &processor, PluginWindow::Type type) {
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

void PluginWindow::moved() {
    processor.setProperty(ProcessorIDs::pluginWindowX, getX(), nullptr);
    processor.setProperty(ProcessorIDs::pluginWindowY, getY(), nullptr);
}

void PluginWindow::closeButtonPressed() {
    processor.setProperty(ProcessorIDs::pluginWindowType, static_cast<int>(Type::none), nullptr);
}

PluginWindow::ProgramAudioProcessorEditor::ProgramAudioProcessorEditor(AudioProcessor &p) : AudioProcessorEditor(p) {
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

PluginWindow::ProgramAudioProcessorEditor::PropertyComp::PropertyComp(const String &name, AudioProcessor &p) : PropertyComponent(name), owner(p) {
    owner.addListener(this);
}

PluginWindow::ProgramAudioProcessorEditor::PropertyComp::~PropertyComp() {
    owner.removeListener(this);
}

void PluginWindow::ProgramAudioProcessorEditor::paint(Graphics &g) {
    g.fillAll(Colours::grey);
}

void PluginWindow::ProgramAudioProcessorEditor::resized() {
    panel.setBounds(getLocalBounds());
}
