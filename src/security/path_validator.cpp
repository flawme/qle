#include "security/path_validator.h"
#include "errors/errors.h"
#include <sstream>

namespace qle {
namespace security {

void ValidateSourcePath(const std::string& path) {
    if (path.empty()) {
        throw errors::SecurityError("Source path cannot be empty.");
    }

    // Block ".." path components to prevent directory traversal
    std::istringstream stream(path);
    std::string segment;
    while (std::getline(stream, segment, '/')) {
        if (segment == "..") {
            throw errors::SecurityError("Path traversal ('..') is not allowed.");
        }
    }

    // Also check backslash-based traversal on Windows-style paths
    std::istringstream stream2(path);
    while (std::getline(stream2, segment, '\\')) {
        if (segment == "..") {
            throw errors::SecurityError("Path traversal ('..') is not allowed.");
        }
    }
}

} // namespace security
} // namespace qle
