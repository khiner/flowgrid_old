#pragma once

#include "Components.h"
#include "ValueTreeItems.h"
#include "drow_Utilities.h"

class SelectionPanel : public Component,
                       private ChangeListener {
public:
    SelectionPanel(TreeView &tv, Edit &e)
            : treeView(tv), edit(e) {
        drow::addAndMakeVisible(*this, {&titleLabel, &nameEditor, &colourButton, &startSlider, &lengthSlider});

        colourButton.setButtonText("Set colour");

        startSlider.setRange(0.0, 10.0);
        lengthSlider.setRange(0.0, 10.0);

        drow::visitComponents({&nameEditor, &colourButton, &startSlider, &lengthSlider},
                              [this](Component *c) {
                                  labels.add(new Label(String(), c->getName()))->attachToComponent(c, true);
                              });

        edit.addChangeListener(this);

        refresh();
    }

    ~SelectionPanel() {
        edit.removeChangeListener(this);
    }

    void paint(Graphics &g) override {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override {
        auto r = getLocalBounds();

        titleLabel.setBounds(r.removeFromTop(22));
        r = r.withTrimmedLeft(70).withWidth(250);

        drow::visitComponents({&nameEditor, &colourButton, &startSlider, &lengthSlider},
                              [this, &r](Component *c) { if (c->isVisible()) c->setBounds(r.removeFromTop(22)); });
    }

private:
    TreeView &treeView;
    Edit &edit;

    Label titleLabel;
    TextEditor nameEditor{"Name: "};
    ColourChangeButton colourButton{"Colour: "};
    Slider startSlider{"Start: "}, lengthSlider{"Length: "};
    OwnedArray<Label> labels;

    template<typename Type>
    inline Type *getFirstSelectedItemOfType() const {
        const int numSelected = treeView.getNumSelectedItems();

        for (int i = 0; i < numSelected; ++i)
            if (auto *t = dynamic_cast<Type *> (treeView.getSelectedItem(i)))
                return t;

        return nullptr;
    }

    void refresh() {
        for (auto *c : getChildren())
            c->setVisible(false);

        titleLabel.setText("No item selected", dontSendNotification);
        titleLabel.setVisible(true);

        if (auto *clip = getFirstSelectedItemOfType<Clip>()) {
            titleLabel.setText("Clip Selected: " + clip->getDisplayText(), dontSendNotification);
            nameEditor.setVisible(true);
            startSlider.setVisible(true);
            lengthSlider.setVisible(true);

            nameEditor.getTextValue().referTo(clip->getState().getPropertyAsValue(IDs::name, clip->getUndoManager()));
            startSlider.getValueObject().referTo(
                    clip->getState().getPropertyAsValue(IDs::start, clip->getUndoManager()));
            lengthSlider.getValueObject().referTo(
                    clip->getState().getPropertyAsValue(IDs::length, clip->getUndoManager()));
        } else if (auto *track = getFirstSelectedItemOfType<Track>()) {
            titleLabel.setText("Track Selected: " + track->getDisplayText(), dontSendNotification);
            nameEditor.setVisible(true);
            colourButton.setVisible(true);

            nameEditor.getTextValue().referTo(track->getState().getPropertyAsValue(IDs::name, track->getUndoManager()));
            colourButton.getColourValueObject().referTo(
                    track->getState().getPropertyAsValue(IDs::colour, track->getUndoManager()));
        } else if (auto *ed = getFirstSelectedItemOfType<Edit>()) {
            titleLabel.setText("Edit Selected: " + ed->getDisplayText(), dontSendNotification);
            nameEditor.setVisible(true);

            nameEditor.getTextValue().referTo(ed->getState().getPropertyAsValue(IDs::name, ed->getUndoManager()));
        }

        repaint();
        resized();
    }

    void changeListenerCallback(ChangeBroadcaster *cb) override {
        refresh();
    }
};
