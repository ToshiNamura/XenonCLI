// examples/sqrt_demo.ixx
// Использует внешнюю функцию sqrt из math.hint.
// Собрать:  xenon compile examples/sqrt_demo.ixx examples/math.hint
// Запустить: xenon run examples/sqrt_demo.module

export module SqrtDemo;

double main()
{
	return sqrt(81.0);
}
