#pragma once

#include "xenon/module/module.hpp"

namespace xenon
{

/// Builds the built-in `xenon.env` module describing the environment classes
/// (`Object`, `String`, `Vector`, `Environment`). It can be added to a `Runtime`
/// so that scripts can refer to those types.
[[nodiscard]] Module environmentModule();

} // namespace xenon
