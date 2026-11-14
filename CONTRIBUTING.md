# Как участвовать в разработке

Проект использует trunk-based flow: короткоживущие ветки, обязательный CI и
squash-merge в `main`.

## Ветки

| Префикс | Назначение | Пример |
| --- | --- | --- |
| `feat/` | новая возможность | `feat/long-jumps` |
| `fix/` | исправление | `fix/relative-jump-range` |
| `refactor/` | переработка без изменения поведения | `refactor/unified-xenon-asm` |
| `docs/` | документация | `docs/language-spec` |
| `chore/` | сборка, CI, зависимости | `chore/bump-cmake` |

Ветка создаётся от `main` и называется по номеру задачи, если она есть:
`feat/13-long-jumps`.

## Цикл работы

```sh
git switch -c feat/<issue>-<slug> main

# изменения
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug

git add -A
git commit -m "feat: <краткое описание>"
git push -u origin HEAD

gh pr create --fill --label <компонент> --milestone "<milestone>"
gh pr checks --watch
```

Требования к PR:

- зелёный CI (MSVC, GCC, Clang) — это обязательные проверки `main`;
- история линейная: PR вливается только через squash;
- заголовок PR в стиле Conventional Commits (`feat:`, `fix:`, `refactor:`,
  `docs:`, `build:`, `test:`);
- в описании — «что» и «почему», а также как проверялось;
- для изменений поведения — тест, который падает без изменения.

## Коммиты

```
<type>: <краткое описание в императиве>

<почему это нужно, если не очевидно из первой строки>
```

Типы: `feat`, `fix`, `refactor`, `perf`, `docs`, `test`, `build`, `ci`, `chore`.

## Метки и milestones

| Метка | Область |
| --- | --- |
| `architecture` | слои, крупные переработки |
| `compiler` | лексер, парсер, AST, кодогенерация, язык `.ixx` |
| `vm` | система инструкций, байт-код, виртуальная машина |
| `runtime` | `Runtime`, куча, стек, `xenon.env` |
| `tooling` | CLI, CMake, CI, форматирование |
| `packaging` | `.xenon`, манифесты, поставка |

Milestones соответствуют этапам `ROADMAP.md`:

| Milestone | Содержание |
| --- | --- |
| `v0.2.0 — Unified Xenon ASM` | завершён: единая модель, `.module` v5, `asm`/`disasm` |
| `v0.3.0 — Language completeness` | операторы, типы, импорты, перегрузки |
| `v0.4.0 — VM and runtime` | дальние переходы, диспетчеризация, диагностика, GC |
| `v0.5.0 — Tooling and packaging` | сжатие, зависимости, бенчмарки, спека |

## Стиль

- [docs/style-guide.md](docs/style-guide.md) — C++, `.ixx`, `.hint`, `.asm`;
- `.clang-format`, `.clang-tidy`, `.editorconfig` — автоматическая проверка;
- документация обновляется в том же PR, что и код.

## Локальные проверки перед PR

```sh
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Дополнительно, для строгой сборки:

```sh
cmake --preset x64-debug -DXENON_WARNINGS_AS_ERRORS=ON
cmake --build --preset x64-debug
```

## Работа с GitHub

Все операции (issues, PR, Actions, releases, search) выполняются через `gh` —
см. [.github/skills/github-cli/SKILL.md](.github/skills/github-cli/SKILL.md).
