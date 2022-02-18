#pragma once

using System::String;
using namespace System::IO;

namespace pe {
enum class Type { Invalid, Pe32, Pe32Plus };

// clang-format off
Type GetType(String^ path);
// clang-format on
} // namespace pe