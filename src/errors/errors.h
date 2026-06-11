#pragma once

#include <string>
#include <stdexcept>

namespace qle {
namespace errors {

class QleException : public std::runtime_error {
public:
    QleException(const std::string& message, size_t line = 0, size_t col = 0);
    
    std::string GetFormattedMessage() const;
    size_t GetLine() const { return line_; }
    size_t GetCol() const { return col_; }

private:
    size_t line_;
    size_t col_;
};

// Specialized exceptions
class LexerError : public QleException {
public:
    using QleException::QleException;
};

class ParserError : public QleException {
public:
    using QleException::QleException;
};

class SecurityError : public QleException {
public:
    using QleException::QleException;
};

class RuntimeError : public QleException {
public:
    using QleException::QleException;
};

} // namespace errors
} // namespace qle
