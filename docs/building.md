# Сборка и запуск

## Требования

- CMake ≥ 3.24
- Компилятор с поддержкой C++23 (`std::format`):
  - MSVC ≥ 19.34 (VS 2022 17.4+)
  - GCC ≥ 13
  - Clang ≥ 17

## Сборка

Через пресеты:

```sh
cmake --preset default
cmake --build --preset default
```

Либо вручную:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Тесты

```sh
ctest --test-dir build --output-on-failure
```

Наборы тестов:

| Тест | Что проверяет |
| --- | --- |
| `xenon_tests_vm` | байт-код и VM |
| `xenon_tests_module` | сериализация `.module` |
| `xenon_tests_env` | среда выполнения (`Runtime`, куча, стек) |
| `xenon_tests_lang` | компилятор (функции, управляющий поток, ошибки) |
| `xenon_tests_hint` | внешние объявления (`.hint`) и связывание на этапе выполнения |

## Командная строка

```sh
xenon                                    # версия и справка
xenon help                               # справка
xenon version                            # версия

# скомпилировать .ixx + .hint в .module
xenon compile examples/sqrt_demo.ixx examples/math.hint
xenon compile examples/sqrt_demo.ixx examples/math.hint custom_name

# запустить функцию из .module
xenon run examples/sqrt_demo.module main
xenon run examples/sqrt_demo.module main arg1 arg2

# упаковать модули в .xenon и запустить его
xenon pack examples/sqrt_demo.module demo.xenon
xenon run demo.xenon arg1 arg2
```

При запуске `.xenon` точка входа берётся из манифеста; если она не указана —
ошибка.

## Цели сборки

| Цель | Назначение |
| --- | --- |
| `xenon_core` | библиотека `xenon.core` (базовые классы + компилятор + исполнитель) |
| `xenon_env` | библиотека `xenon.env` (Object, String, Vector, Environment) |
| `xenon` | исполняемый файл CLI |
| `xenon_tests_*` | тестовые исполняемые файлы |
