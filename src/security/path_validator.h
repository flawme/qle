#pragma once

#include <string>

namespace qle {
namespace security {

// Validates source file paths against traversal attacks.
// Blocks ".." path components while allowing subdirectories and absolute paths.
void ValidateSourcePath(const std::string& path);

} // namespace security
} // namespace qle
