#include <ApplicationPropertiesAndCommandManager.h>
#include "PluginWindow.h"

#include "processors/MidiKeyboardProcessor.h"

PluginWindow::PluginWindow(ValueTree &processorState, AudioProcessor *processor, Type type)
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