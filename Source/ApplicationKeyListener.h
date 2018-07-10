#pragma once

#include "JuceHeader.h"
#include "ValueTreeItems.h"

class ApplicationKeyListener : public KeyListener {
public:
    ApplicationKeyListener(Project &project, UndoManager &undoManager)
            : project(project), undoManager(undoManager) {}

    bool keyPressed(const KeyPress &key, Component* originatingComponent) override {
        if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
            project.deleteSelectedItems();
            return true;
        }

        if (key == KeyPress('z', ModifierKeys::commandModifier, 0)) {
            undoManager.undo();
            return true;
        }

        if (key == KeyPress('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)) {
            undoManager.redo();
            return true;
        }

        return false;
    }
private:
    Project &project;
    UndoManager &undoManager;
};