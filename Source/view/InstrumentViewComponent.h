#ifndef SOUND_MACHINE_INSTRUMENTVIEWCOMPONENT_H
#define SOUND_MACHINE_INSTRUMENTVIEWCOMPONENT_H

#include "JuceHeader.h"

class InstrumentViewComponent : public Component {
public:
    explicit InstrumentViewComponent(const std::vector<std::unique_ptr<Slider> >& sliders);
};


#endif //SOUND_MACHINE_INSTRUMENTVIEWCOMPONENT_H
