#include "errors/errors.h"
#include <sstream>

namespace qle {
namespace errors {

QleException::QleException(const std::string& message, size_t line, size_t col)
    : std::runtime_error(message), line_(line), col_(col) {}

std::string QleException::GetFormattedMessage() const {
    std::stringstream ss;
    ss << "Error: " << what() << "\n";
    if (line_ > 0) {
        ss << "Location:\nLine " << line_ << "\nColumn " << col_ << "\n";
    }
    return ss.str();
}

} // namespace errors
} // namespace qle
