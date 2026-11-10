# Соглашения о стилистике кода

Настоящее соглашение распространяется на весь проект XenonCLI:

- исходный код на C++ (`include/**`, `src/**`, `tests/**`);
- скрипты на языке Xenon (`.ixx`);
- файлы внешних объявлений (`.hint`).

Цель — единообразный, предсказуемый и легко читаемый код.

## Общие правила

- **Отступы** — символы табуляции, один уровень = одна табуляция. Пробелы не используются для отступов.
- **Фигурные скобки** — всегда с новой строки (стиль Allman), для всех конструкций: функций, классов, пространств имён, `if`, `for`, `while`, `switch` и т. д.
- **Длина строки** — без жёсткого лимита; при переносе продолжение выравнивается по смыслу (аргументы, операторы).
- **Конец файла** — пустая строка в конце; без висячих пробелов.

## Заголовочные файлы

Каждый заголовок защищён от повторного включения двумя механизмами:

```cpp
#pragma once

#ifndef XENON__MODULE_HPP
#define XENON__MODULE_HPP 1

// ... содержимое ...

#endif // !XENON__MODULE_HPP
```

Имя макроса — `XENON__<ИМЯ_ФАЙЛА_В_ВЕРХНЕМ_РЕГИСТРЕ>` с суффиксом `_HPP`.
Для совпадающих имён файлов в макрос добавляется имя каталога
(например, `XENON__COMPILER_MODULE_HPP` и `XENON__MODULE_MODULE_HPP`).

Порядок директив `#include` (группы разделяются пустой строкой):

1. собственный заголовок этого файла;
2. заголовки стандартной библиотеки;
3. заголовки проекта.

```cpp
#include <xenon/compiler/compiler.hpp>

#include <string>
#include <vector>

#include <xenon/compiler/codegen.hpp>
#include <xenon/compiler/parser.hpp>
```

Заголовки проекта включаются по пути `<xenon/...>`.

## Именование

| Сущность | Стиль | Пример |
| --- | --- | --- |
| Пространства имён | `camelCase` (нижний) | `xenon` |
| Классы, структуры, перечисления | `PascalCase` | `Compiler`, `ModuleHeader`, `TokenType` |
| Члены перечислений | `PascalCase` | `Negate`, `JumpIfFalse`, `Function` |
| Функции и методы | `camelCase` | `compile`, `parseStatement`, `getField` |
| Локальные переменные и параметры | `camelCase` | `headerParser`, `sourcePath` |
| Приватные поля класса | `_camelCase` | `_header`, `_error`, `_source` |
| Открытые поля структур | `PascalCase` | `Name`, `Value`, `Operation` |
| Константы уровня файла | `kPascalCase` | `kExitUsage`, `kExitIo` |
| Макросы препроцессора | `XENON_*` | `XENON_VERSION_STRING` |

```cpp
class Compiler
{
public:
	bool compile(std::string_view source, Module& out);

private:
	ModuleHeader _header;
	std::string  _error;
};
```

## Форматирование

- Пробелы: после ключевых слов (`if`, `for`, `while`), после запятых, вокруг
  бинарных операторов. Без пробела внутри скобок `( )`.
- Спецификаторы доступа (`public:`, `private:`) выравниваются по ключевому слову
  `class`/`struct`, члены — на один уровень глубже.
- Объявления членов и присваивания выравниваются по столбцам:

```cpp
struct BinaryExpr : Expr
{
	Op                    Operation = Op::Add;
	std::unique_ptr<Expr> Left;
	std::unique_ptr<Expr> Right;
};
```

```cpp
_header = xenon::ModuleHeader{};
out     = xenon::Module{};
```

- Пустые тела записываются в одну строку: `struct NullExpr : Expr { };`,
  конструктор с инициализацией — `Lexer(std::string_view source) : _source(source) { }`.
- Широко используются спецификаторы `[[nodiscard]]`, `constexpr`, `const`,
  `noexcept` и модификаторы доступа по умолчанию (`= default`).
- В списках перечислителей допускается висячая запятая:

```cpp
enum class Op
{
	Negate,
	Not,
};
```

## Комментарии

- Документация публичного API — Doxygen-комментарии `///` перед объявлением.
- Внутренние пояснения — обычные `//`.
- Разделы внутри файла — баннер `// ---- имя раздела ----`.
- Секции в `CMakeLists.txt` — баннер `# ---- ... ----`.

## Реализация (`.cpp`)

- Методы определяются вне класса с полностью квалифицированным именем;
  типы возвращаемого значения тоже квалифицируются:

```cpp
xenon::Value xenon::CodeGenerator::evaluateConstant(const xenon::Expr* init)
{
	// ...
}
```

- Свободные функции, локальные для файла, объявляются `static` внутри
  `namespace xenon { ... }`, чтобы не попадать в таблицу символов компоновщика
  и не порождать множественные определения:

```cpp
namespace xenon
{
	static void writeU32(std::ostream& os, std::uint32_t value)
	{
		// ...
	}
}
```

## Скрипты `.ixx`

Скрипты используют тот же стиль, что и C++: табуляции, скобки с новой строки,
`PascalCase` для типов и полей, `camelCase` для функций и переменных.

```cpp
// examples/example.ixx
export module Example;

import Math;

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
```

## Файлы `.hint`

Файлы `.hint` — только объявления без тел (сигнатуры функций, структуры,
перечисления). Оформление такое же: табуляции, скобки с новой строки,
висячая запятая в перечислениях.

```cpp
// examples/math.hint
double sqrt(double x);
double pow(double base, double exp);

struct Point
{
	double x;
	double y;
};

enum Color
{
	Red,
	Green,
	Blue,
};
```
