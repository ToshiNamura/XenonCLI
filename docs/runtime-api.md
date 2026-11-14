# Среда выполнения (API)

Среда выполнения построена по образцу JNI (Java C++ API). Класс `Runtime` —
исполнитель: он хранит массив модулей и предоставляет окружение, через которое
хост взаимодействует с виртуальной машиной.

## Обзор

```cpp
#include <xenon/xenon.hpp>

xenon::Runtime runtime;

// модули
runtime.addModule(xenon::environmentModule()); // встроенный модуль xenon.env
runtime.loadModuleFile("example.module");

// куча
xenon::Ref point = runtime.newObject("Point");
runtime.setField(point, "x", 1.5);
xenon::Ref xs = runtime.newArray(3);
runtime.setElement(xs, 0, 10.0);
xenon::Ref s = runtime.newString("hello");

// стек операндов
runtime.push(1.0);
double top = std::get<double>(runtime.pop());

// исполнение
runtime.run("main");
xenon::Value result = runtime.lastValue();

// запуск точки входа с аргументами
int code = runtime.runMain("main", {"a", "b"}, {});
```

## Value

`Value = std::variant<double, bool, Null, Ref>`.

| Тип | Значение |
| --- | --- |
| `double` | число |
| `bool` | логический |
| `Null` | `null` |
| `Ref` | ссылка на объект кучи (`struct { std::uint32_t id; }`) |

## VirtualHeap и HeapObject

`ObjectKind`: `Object` (именованные поля), `Array` (индексированные элементы),
`String` (текст).

```cpp
struct HeapObject
{
    ObjectKind Kind;
    std::string ClassName;                         // для Object
    std::unordered_map<std::string, Value> Fields; // для Object
    std::vector<Value> Elements;                   // для Array
    std::string Text;                              // для String
};
```

## Модель модуля

`xenon::Module` (`xenon/asm/model.hpp`) — единое представление кода и данных:

```cpp
struct Module
{
    std::string Name;
    std::vector<std::string> Imports;
    AttributeList Attributes;     // атрибуты модуля
    std::vector<Function> Functions;
    std::vector<Type> Types;
    std::vector<Global> Globals;
};
```

| Тип | Поля |
| --- | --- |
| `Function` | `Name`, `Signature`, `Exported`, `Defined`, `Arity`, `Locals`, `Body`, `Attributes`, `TemplateParams` |
| `Method` | то же + `AccessModifier` |
| `Type` | `Name`, `Kind`, `Exported`, `Defined`, `Fields`, `Methods`, `Enumerators`, `Parents`, `Attributes`, `TemplateParams` |
| `Field` | `Type`, `Name`, `AccessModifier`, `Exported`, `Attributes`, `TemplateParams` |
| `Global` | `Type`, `Name`, `Exported`, `Constant`, `InitialValue`, `Attributes`, `TemplateParams` |

`Module` — копируемый тип: `Runtime` и `VirtualMachine` хранят собственные копии,
а атрибуты разделяются через `std::shared_ptr`.

## Runtime

| Группа | Методы |
| --- | --- |
| модули | `addModule`, `loadModuleFile`, `moduleCount`, `modules`, `findFunction`, `findMethod`, `findType`, `hasFunction` |
| объекты | `newObject`, `newArray`, `newString`, `objectKind`, `className` |
| поля | `setField`, `getField` |
| массивы | `arrayLength`, `setElement`, `getElement` |
| строки | `stringValue` |
| стек | `push`, `pop`, `peek`, `stackSize` |
| исполнение | `run`, `runMain`, `runChunk`, `lastValue` |
| нативные функции | `registerNative` |
| предикаты | `isNumber`, `isBoolean`, `isNull`, `isRef` |

Ошибки времени выполнения (неверная ссылка, обращение к отсутствующему полю,
выход за границы массива, деление на ноль и т. д.) выбрасываются как
`xenon::RuntimeError`.

## VirtualMachine

| Метод | Назначение |
| --- | --- |
| `load`, `loadAll` | загрузить один или несколько модулей |
| `findFunction`, `findMethod`, `findType`, `hasFunction` | поиск после связывания |
| `registerNative` | связать внешнее объявление с хост-функцией |
| `run` | выполнить функцию/метод по имени |
| `interpret` | выполнить сырой `XenonBinary` (отладка/тесты) |
| `lastValue` | результат последнего `ret` |
| `setHeap` | привязать кучу для `str`/`new` |

## Библиотека `xenon.env`

| Класс | Методы |
| --- | --- |
| `Object` | виртуальные `getType`, `getHash`, `toString` |
| `String` | `value`, `size`, `empty` |
| `Vector<Type>` | `size`, `empty`, `pushBack`, `operator[]`, `at`, `values` |
| `Environment` | `registerClass`, `hasClass`, `create`, `size` |

Встроенный модуль для `Runtime` создаётся функцией `environmentModule()`.

## Формат `.module` и Xenon ASM

- `writeModuleFile` / `readModuleFile` — сериализация `Module` (версия 6).
- `Assembler::assemble` — текст `.asm` → `Module`.
- `disassemble` — `Module` → текст `.asm`.

Подробности — в [bytecode.md](bytecode.md) и [xenon-asm.md](xenon-asm.md).
