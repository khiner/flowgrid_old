#include "ApplicationPropertiesAndCommandManager.h"
#include "PluginWindow.h"

#include "model/Processor.h"
#include "processors/MidiKeyboardProcessor.h"

PluginWindow::PluginWindow(Processor *processor, AudioProcessor *audioProcessor, PluginWindowType type)
        : DocumentWindow(processor->getName(), LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId), DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          processor(processor), type(type) {
    setSize(400, 300);

    Component *keyboardComponent{};
    if (auto *midiKeyboardProcessor = dynamic_cast<MidiKeyboardProcessor *>(audioProcessor)) {
        keyboardComponent = midiKeyboardProcessor->createKeyboard();
        keyboardComponent->setSize(800, 1);
        setContentOwned(keyboardComponent, true);
    } else if (auto *ui = createProcessorEditor(*audioProcessor, type)) {
        setContentOwned(ui, true);
    }

    int xPosition = processor->hasPluginWindowX() ? processor->getPluginWindowX() : Random::getSystemRandom().nextInt(500);
    int yPosition = processor->hasPluginWindowY() ? processor->getPluginWindowY() : Random::getSystemRandom().nextInt(500);
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

AudioProcessorEditor *PluginWindow::createProcessorEditor(AudioProcessor &processor, PluginWindowType type) {
    if (type == PluginWindowType::normal)
        if (auto *ui = processor.createEditorIfNeeded())
            return ui;

    if (type == PluginWindowType::programs)
        return new ProgramAudioProcessorEditor(processor);

//        if (type == PluginWindow::Type::audioIO)
//            return new FilterIOConfigurationWindow (processor);

    jassertfalse;
    return {};
}

void PluginWindow::moved() {
    processor->setPluginWindowX(getX());
    processor->setPluginWindowY(getY());
}

void PluginWindow::closeButtonPressed() {
    processor->setPluginWindowType(static_cast<int>(PluginWindowType::none));
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
