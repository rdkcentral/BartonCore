### Requirement: Script execution timeout
The mquickjs runtime SHALL enforce a maximum execution time for SBMD mapper scripts. The timeout SHALL be implemented using the mquickjs `JS_SetInterruptHandler` mechanism. When a script exceeds the configured timeout, the interrupt handler SHALL cause the engine to throw an exception, terminating the script.

#### Scenario: Script completes within timeout
- **WHEN** a mapper script executes and completes within the configured timeout period
- **THEN** the script SHALL return its result normally and the interrupt handler SHALL not interfere

#### Scenario: Infinite loop terminated by timeout
- **WHEN** a mapper script contains an infinite loop (e.g., `while(true){}`)
- **THEN** the interrupt handler SHALL terminate the script after the configured timeout and `ExecuteScript` SHALL return `false`

#### Scenario: Long-running computation terminated
- **WHEN** a mapper script performs a computation that exceeds the configured timeout
- **THEN** the interrupt handler SHALL terminate the script and the operation SHALL fail gracefully without crashing

#### Scenario: Timeout produces diagnostic logging
- **WHEN** a script is terminated due to timeout
- **THEN** the system SHALL log an error message indicating the script was interrupted due to exceeding the execution time limit

#### Scenario: Context remains usable after timeout
- **WHEN** a script is terminated due to timeout
- **THEN** subsequent script executions on other devices or resources SHALL succeed normally

### Requirement: Script execution timeout configuration
The script execution timeout SHALL be configurable via the `BCORE_SBMD_SCRIPT_TIMEOUT_MS` CMake integer option with a default value of 5000 (5 seconds). The value SHALL be compiled into the binary as `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS`.

#### Scenario: Default timeout value
- **WHEN** `BCORE_SBMD_SCRIPT_TIMEOUT_MS` is not explicitly set
- **THEN** the default timeout SHALL be 5000 milliseconds

#### Scenario: Custom timeout value
- **WHEN** `BCORE_SBMD_SCRIPT_TIMEOUT_MS=10000` is set at CMake configuration time
- **THEN** scripts SHALL be allowed up to 10 seconds of execution time

### Requirement: Interrupt handler lifecycle
The interrupt handler SHALL be installed once during `MQuickJsRuntime::Initialize()` and remain installed for the lifetime of the context. The handler SHALL use a static deadline variable to determine whether a timeout is active. `ExecuteScript` SHALL arm the deadline via `SetDeadline()` before `JS_Call` and disarm it via `ClearDeadline()` after. When no deadline is active (cleared), the handler SHALL return 0 (do not interrupt).

#### Scenario: Handler installed at initialization
- **WHEN** `MQuickJsRuntime::Initialize()` is called
- **THEN** `JS_SetInterruptHandler` SHALL be called on the shared context with the timeout handler

#### Scenario: Handler inactive outside script execution
- **WHEN** the interrupt handler is called outside of `ExecuteScript` (e.g., during `SbmdUtilsLoader::LoadBundle`)
- **THEN** the handler SHALL return 0, allowing execution to continue uninterrupted
