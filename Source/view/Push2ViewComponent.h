#ifndef SOUND_MACHINE_PUSH2VIEWCOMPONENT_H
#define SOUND_MACHINE_PUSH2VIEWCOMPONENT_H

#include "JuceHeader.h"
#include "InstrumentViewComponent.h"
#include <push2/Push2Display.h>

class Push2ViewComponent : public Component {
public:
    Push2ViewComponent() {
        setSize(Push2Display::WIDTH, Push2Display::HEIGHT);
    }
};


#endif //SOUND_MACHINE_PUSH2VIEWCOMPONENT_H
