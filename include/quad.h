#pragma once

#include "flat.h"

class Quad : public Flat {
    bool contains(float t, float alpha, float beta) const override {
        static const Interval unit_interval(0, 1);
        return unit_interval.contains(alpha) && unit_interval.contains(beta);
    }
};
