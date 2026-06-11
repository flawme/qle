#pragma once

#include <cstddef>
#include <cstdint>

namespace qle {
namespace security {

struct Limits {
    // Limits
    size_t max_file_size = 100 * 1024 * 1024; // 100 MB
    size_t max_query_size = 64 * 1024;        // 64 KB
    size_t max_tokens = 100000;               // 100000
    size_t max_ast_nodes = 50000;             // 50000
    size_t max_rows_processed = 1000000;      // 1000000
    size_t max_string_length = 8192;          // 8192
    size_t max_recursion_depth = 128;         // 128
    size_t max_execution_time_ms = 30000;     // 30 seconds
    size_t max_memory_usage = 512 * 1024 * 1024; // 512 MB

    // Global instance
    static Limits& Get() {
        static Limits instance;
        return instance;
    }
};

} // namespace security
} // namespace qle
