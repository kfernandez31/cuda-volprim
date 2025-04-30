#include "vec_math.h"
#include <iostream>

int main()
{
    // Just try to call a few functions to see if they compile

    float2 a = make_float2(1.0f, 2.0f);
    float2 b = make_float2(3.0f, 4.0f);

    float2 c = a;
    c.x += b.x;
    c.y += b.y;

    std::cout << "c.x = " << c.x << ", c.y = " << c.y << std::endl;

    float2 d = -c; // Test operator-

    std::cout << "d.x = " << d.x << ", d.y = " << d.y << std::endl;

    return 0;
}