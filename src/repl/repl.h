#pragma once

#include "utils/formatter.h"

namespace qle {
namespace repl {

class Repl {
public:
    static void Start(utils::OutputFormat format, bool show_time);
};

} // namespace repl
} // namespace qle
