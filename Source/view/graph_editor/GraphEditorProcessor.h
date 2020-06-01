#pragma once

#include "BaseGraphEditorProcessor.h"

class GraphEditorProcessor : public BaseGraphEditorProcessor {
public:
    GraphEditorProcessor(Project &project, TracksState &tracks, ViewState &view,
                         const ValueTree &state, ConnectorDragListener &connectorDragListener,
                         bool showChannelLabels = false) :
            BaseGraphEditorProcessor(project, tracks, view, state, connectorDragListener, showChannelLabels) {
        valueTreePropertyChanged(this->state, IDs::name);
        if (this->state.hasProperty(IDs::deviceName))
            valueTreePropertyChanged(this->state, IDs::deviceName);

        if (!showChannelLabels) {
            nameLabel.setColour(findColour(TextEditor::textColourId));
            nameLabel.setFontHeight(largeFontHeight);
            nameLabel.setJustification(Justification::centred);
            addAndMakeVisible(nameLabel);
        }
    }

    ~GraphEditorProcessor() override {
        if (parametersPanel != nullptr)
            parametersPanel->removeMouseListener(this);
    }

    void resized() override {
        BaseGraphEditorProcessor::resized();
        if (auto *processor = getAudioProcessor()) {
            auto boxBoundsFloat = getBoxBounds().reduced(4).toFloat();

            if (!showChannelLabels) {
                nameLabel.setBoundingBox(rotateRectIfNarrow(boxBoundsFloat));
            }

            if (parametersPanel != nullptr) {
                parametersPanel->setBounds(boxBoundsFloat.toNearestInt());
            }
        }
    }

private:
    DrawableText nameLabel;
    std::unique_ptr<ParametersPanel> parametersPanel;

    void valueTreePropertyChanged(ValueTree &v, const Identifier &i) override {
        if (v != state)
            return;

        if (parametersPanel == nullptr) {
            if (auto *processorWrapper = getProcessorWrapper()) {
                if (auto *defaultProcessor = dynamic_cast<DefaultAudioProcessor *>(processorWrapper->processor)) {
                    if (defaultProcessor->showInlineEditor()) {
                        addAndMakeVisible((parametersPanel = std::make_unique<ParametersPanel>(1, processorWrapper->getNumParameters())).get());
                        parametersPanel->addMouseListener(this, true);
                        parametersPanel->addParameter(processorWrapper->getParameter(0));
                        parametersPanel->addParameter(processorWrapper->getParameter(1));
                        removeChildComponent(&nameLabel);
                    }
                }
            }
        }

        if (i == IDs::deviceName) {
            setName(v[IDs::deviceName]);
            nameLabel.setText(getName());
        } else if (i == IDs::name) {
            setName(v[IDs::name]);
            nameLabel.setText(getName());
        }
        if (i == IDs::processorInitialized)
            resized();
    }
};
