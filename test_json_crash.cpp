#include "adapters/json/json_adapter.h"
#include <iostream>

int main() {
    qle::adapters::json::JsonAdapter adapter;
    try {
        adapter.Open("tests/malformed.json");
        while (adapter.HasNext()) {
            adapter.Next();
        }
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << std::endl;
    }
    return 0;
}
