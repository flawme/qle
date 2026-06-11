# Development & Adapters

## Building the Project

QLE uses CMake. A C++17 compliant compiler is required.

```bash
mkdir build
cd build
cmake ..
make
```

## Running Tests

The project includes an integrated test suite spanning lexer logic, parser mechanics, and strict security constraint validation.

To run the tests:
```bash
cd build
make
ctest --output-on-failure
```

## Creating New Adapters

QLE is designed to easily expand. To add a new data source format (e.g., SQLite, YAML):

1. **Implement `IAdapter`**: Create a new class inheriting from `qle::adapters::IAdapter`.
2. **Implement Methods**: 
   - `Open(const std::string& source)`
   - `HasNext() -> bool`
   - `Next() -> Row`
   - `Close()`
3. **Register Adapter**: Open `src/runtime/runtime.cpp` and update `Runtime::GetAdapterForSource()` to return your new adapter when a matching file extension or protocol is detected.

Maintain file size limits within your adapter to ensure system stability. All files should adhere to the single-responsibility principle and remain under 500 lines.
