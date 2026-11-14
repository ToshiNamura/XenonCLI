// examples/operators.ixx
// Демонстрация операторов Xenon.
// Собрать:   xenon compile examples/operators.ixx
// Запустить: xenon run examples/operators.module

export module Operators;

int mask = 1 << 4;

double main()
{
	double total = 0.0;

	for (double i = 1.0; i <= 4.0; i++)
	{
		total += i;
	}

	double remainder = 10.0 % 3.0;
	double bits = (mask & 240.0) | (mask ^ 31.0);
	double shifted = ~0.0 >> 1.0;
	double magnitude = total > 0.0 ? total : -total;

	return total + remainder + bits + shifted + magnitude + mask;
}
