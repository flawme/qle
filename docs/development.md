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
make qle_tests
ctest --output-on-failure
```

*Note: The project is compiled with AddressSanitizer (ASAN) by default to strictly guarantee no memory leaks occur during testing.*

## Creating New Adapters

QLE is designed to easily expand. To add a new data source format (e.g., Parquet, Protobuf):

1. **Implement `IAdapter`**: Create a new class inheriting from `qle::adapters::IAdapter`.
2. **Implement Methods**: 
   - `Open(const std::string& source)`
   - `HasNext() -> bool`
   - `Next() -> Row`
   - `Close()`
3. **Register Adapter**: Open `src/runtime/runtime.cpp (or the relevant decoupled component like evaluator/executor)` and update `Runtime::GetAdapterForSource()` to return your new adapter when a matching file extension or protocol is detected.

Maintain file size limits within your adapter to ensure system stability. All files should adhere to the single-responsibility principle and remain under 500 lines.

## Modular Architecture

The execution engine has been cleanly decoupled from a monolithic structure into 5 distinct component files (`evaluator.cpp`, `executor_streaming.cpp`, `executor_groupby.cpp`, `executor_orderby.cpp`, `runtime.cpp`). This separation of concerns allows developers to isolate scaling changes (like Map-Reduce threading logic in `executor_groupby.cpp`) without impacting the overall `Runtime` state machine.
