// examples/example.ixx
// A Xenon module: a C++-like module interface unit.

export module Example;

import Math;
import IO.Core;

#include "xenon/xenon.hpp"

double add(double a, double b)
{
    return a + b;
}

double factorial(double n)
{
    double result = 1.0;
    double i = 1.0;
    while (i <= n)
    {
        result = result * i;
        i = i + 1.0;
    }
    return result;
}

double main()
{
    double x = add(20.0, 22.0);
    return x + factorial(5.0);
}
