#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "xenon/bytecode/chunk.hpp"
#include "xenon/module/module.hpp"
#include "xenon/runtime/object.hpp"
#include "xenon/runtime/value.hpp"
#include "xenon/runtime/vm.hpp"

namespace xenon
{

/// The Xenon runtime environment: the bridge between the host and the VM.
///
/// It owns the managed heap, the operand stack and the execution engine, and
/// exposes a JNI-like API to create objects, access fields, manipulate the
/// stack and run functions.
class Env
{
  public:
    // ---- heap / objects ----
    Ref new_object(std::string_view class_name);
    Ref new_array(std::size_t length);
    Ref new_string(std::string_view text);

    ObjectKind object_kind(Ref ref) const;
    std::string_view class_name(Ref ref) const;

    // ---- fields ----
    void set_field(Ref ref, std::string_view name, Value value);
    Value get_field(Ref ref, std::string_view name) const;

    // ---- arrays ----
    std::size_t array_length(Ref ref) const;
    void set_element(Ref ref, std::size_t index, Value value);
    Value get_element(Ref ref, std::size_t index) const;

    // ---- strings ----
    std::string_view string_value(Ref ref) const;

    // ---- operand stack ----
    void push(Value value);
    Value pop();
    const Value& peek(std::size_t depth = 0) const;
    std::size_t stack_size() const noexcept;

    // ---- execution ----
    void load(const Module& module);
    [[nodiscard]] const Symbol* find(std::string_view name) const noexcept;
    [[nodiscard]] InterpretResult run(std::string_view name);
    [[nodiscard]] InterpretResult run_chunk(const Chunk& chunk);
    [[nodiscard]] const Value& last_value() const noexcept;

    // ---- accessors ----
    Heap& heap() noexcept
    {
        return heap_;
    }
    const Heap& heap() const noexcept
    {
        return heap_;
    }
    std::vector<Value>& stack() noexcept
    {
        return stack_;
    }

    // ---- type predicates ----
    [[nodiscard]] static bool is_number(const Value& value);
    [[nodiscard]] static bool is_boolean(const Value& value);
    [[nodiscard]] static bool is_null(const Value& value);
    [[nodiscard]] static bool is_ref(const Value& value);

  private:
    Heap heap_;
    std::vector<Value> stack_;
    VM vm_;
};

} // namespace xenon
