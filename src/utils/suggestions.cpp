#include "utils/suggestions.h"
#include <algorithm>

namespace qle {
namespace utils {

size_t LevenshteinDistance(const std::string& s1, const std::string& s2) {
    size_t len1 = s1.size();
    size_t len2 = s2.size();
    std::vector<std::vector<size_t>> d(len1 + 1, std::vector<size_t>(len2 + 1));

    for (size_t i = 0; i <= len1; ++i) d[i][0] = i;
    for (size_t j = 0; j <= len2; ++j) d[0][j] = j;

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });
        }
    }
    return d[len1][len2];
}

std::string SuggestField(const std::string& unknown, const std::vector<std::string>& available) {
    std::string best_match;
    size_t min_dist = 3; 
    for (const auto& field : available) {
        size_t dist = LevenshteinDistance(unknown, field);
        if (dist < min_dist) {
            min_dist = dist;
            best_match = field;
        }
    }
    if (!best_match.empty()) {
        return " (Did you mean '" + best_match + "'?)";
    }
    return "";
}

} // namespace utils
} // namespace qle
