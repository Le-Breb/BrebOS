#ifndef NETWORK_H
#define NETWORK_H
#include "E1000.h"


class Network {
    static E1000* nic;
public:
    static void init();

    static void run();
};



#endif //NETWORK_H
