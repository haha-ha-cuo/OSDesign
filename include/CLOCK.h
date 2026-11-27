//
// Created by YuHao Shi on 2026/11/27.
//

#ifndef OSDESIGN_CLOCK_H
#define OSDESIGN_CLOCK_H

class CLOCK {
public:
    static int t;
    static int tick() { return t++; }
    CLOCK() { t = 0; }
};

#endif //OSDESIGN_CLOCK_H
