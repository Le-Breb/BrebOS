#ifndef BREBOS_USB_H
#define BREBOS_USB_H

#include "xHCI.h"

class USB
{
public:
    [[nodiscard]] static USB* get_instance();
};


#endif //BREBOS_USB_H
