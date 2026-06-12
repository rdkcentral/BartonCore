## 1. Build System Configuration

- [x] 1.1 Add `BCORE_SBMD_SCRIPT_TIMEOUT_MS` integer option (default 5000) to `config/cmake/options.cmake` with definition `BARTON_CONFIG_SBMD_SCRIPT_TIMEOUT_MS`

## 2. Script Execution Timeout

- [x] 2.1 Implement interrupt handler in `MQuickJsRuntime`: add static deadline variable, implement `ScriptInterruptHandler` callback that checks `steady_clock::now()` against deadline, wire `JS_SetInterruptHandler` into `Initialize()`
- [x] 2.2 Integrate timeout into `SbmdScriptImpl::ExecuteScript`: set deadline before `JS_Call`, clear after, log error on timeout
- [x] 2.3 Add unit tests for timeout: infinite loop script terminates, normal script completes, context usable after timeout

## 3. Verification

- [x] 3.1 Build and run full unit test suite to confirm no regressions
