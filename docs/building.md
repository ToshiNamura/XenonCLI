# Сборка и запуск

## Требования

- CMake ≥ 3.24
- Компилятор с поддержкой C++23 (`std::string::ends_with`, `.contains`,
  `std::to_chars` для `double`):
  - MSVC ≥ 19.34 (VS 2022 17.4+)
  - GCC ≥ 12
  - Clang ≥ 15
- Ninja (для пресетов)

На Windows запускайте команды из **Developer PowerShell** (или `x64 Native Tools
Command Prompt`), чтобы `cl.exe` и `ninja` были в `PATH`; VS Code CMake Tools
делает это автоматически.

## Сборка через пресеты

```sh
cmake --preset x64-debug        # Windows
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Доступные пресеты:

| Платформа | Debug | Release |
| --- | --- | --- |
| Windows x64 | `x64-debug` | `x64-release` |
| Linux | `linux-debug` | `linux-release` |
| macOS | `macos-debug` | `macos-release` |

Каталог сборки — `out/build/<пресет>`, установки — `out/install/<пресет>`.
`CMakePresets.json` содержит configure-, build- и test-пресеты с одинаковыми
именами, поэтому команды `--build --preset` и `ctest --preset` работают сразу.

### Полезные опции

| Опция | По умолчанию | Назначение |
| --- | --- | --- |
| `XENON_BUILD_TESTS` | `ON` | собирать юнит-тесты |
| `XENON_WARNINGS_AS_ERRORS` | `OFF` | `-Werror` / `/WX` |

```sh
cmake --preset x64-debug -DXENON_WARNINGS_AS_ERRORS=ON
```

### Сборка без пресетов

```sh
cmake -S . -B out/build/manual -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/manual
ctest --test-dir out/build/manual --output-on-failure
```

## Тесты

| Тест | Что проверяет |
| --- | --- |
| `xenon_tests_vm` | инструкции VM, 16-битные операнды, переходы |
| `xenon_tests_module` | сериализация `.module`, round-trip, загрузка в VM |
| `xenon_tests_env` | `Runtime`, куча, стек, вызовы |
| `xenon_tests_lang` | компилятор: функции, управляющий поток, шаблоны, глобалы, межмодульные вызовы |
| `xenon_tests_hint` | `.hint`, нативные функции и ошибки связывания |
| `xenon_tests_asm` | модель, ассемблер, дизассемблер, исполнение собранного ASM |

## Командная строка

```sh
xenon                                    # версия и справка
xenon help                               # справка
xenon version                            # версия

# скомпилировать .ixx + .hint в .module
xenon compile examples/sqrt_demo.ixx examples/math.hint
xenon compile examples/sqrt_demo.ixx examples/math.hint -o out/sqrt_demo.module
xenon compile examples/sqrt_demo.ixx examples/math.hint out/sqrt_demo.module

# текстовый Xenon ASM <-> .module
xenon asm examples/hello.asm -o out/hello.module
xenon disasm out/hello.module
xenon disasm out/hello.module -o out/hello.asm

# запустить .module (по умолчанию main)
xenon run examples/sqrt_demo.module
xenon run examples/sqrt_demo.module main 9
xenon run examples/sqrt_demo.module sqrt 9

# упаковать модули в .xenon и запустить
xenon pack examples/sqrt_demo.module demo.xenon
xenon run demo.xenon
```

Второй аргумент `run` считается именем функции/метода, только если такой символ
есть в модуле; иначе аргументы передаются в `main`.

## Цели сборки

| Цель | Назначение |
| --- | --- |
| `xenon_core` | Xenon ASM, компилятор, формат `.module`, VM |
| `xenon_env` | `Object`, `String`, `Vector`, `Environment` |
| `xenon_packer` | `Packer`, `Manifest` |
| `xenon` | исполняемый файл CLI |
| `xenon_tests_*` | тестовые исполняемые файлы |

## Установка

```sh
cmake --install out/build/x64-release --prefix out/install/x64-release
```

Устанавливаются исполняемый файл `bin/xenon` и заголовки `include/xenon`.
