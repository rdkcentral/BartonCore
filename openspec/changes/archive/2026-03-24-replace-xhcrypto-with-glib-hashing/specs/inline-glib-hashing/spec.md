## ADDED Requirements

### Requirement: deviceServiceHash module for GLib-based file hashing
A `deviceServiceHash` module (`core/src/deviceServiceHash.h` and `core/src/deviceServiceHash.c`) SHALL provide `deviceServiceHashComputeFileMd5HexString(const char *filePath)` which returns a `g_malloc`'d hex-encoded MD5 digest string, or NULL on error. The implementation SHALL use GLib `GChecksum` with `G_CHECKSUM_MD5`, read the file in 4096-byte chunks, check `ferror` after the read loop, and free all resources on every path.

#### Scenario: Successful file hashing
- **WHEN** `deviceServiceHashComputeFileMd5HexString` is called with a valid file path
- **THEN** it SHALL return the lowercase hex-encoded MD5 digest of the file contents

#### Scenario: File open failure
- **WHEN** `deviceServiceHashComputeFileMd5HexString` is called with a nonexistent path
- **THEN** it SHALL return NULL and log an error

#### Scenario: Read error during hashing
- **WHEN** an I/O error occurs during `fread` (detected via `ferror`)
- **THEN** it SHALL return NULL and log an error

### Requirement: All call sites use deviceServiceHashComputeFileMd5HexString
Each of the three `digestFileHex` calls in `deviceDescriptorHandler.c` and the single call in `zigbeeDriverCommon.c` SHALL be replaced with `deviceServiceHashComputeFileMd5HexString`.

#### Scenario: Allowlist MD5 after download
- **WHEN** the allowlist file is downloaded successfully
- **THEN** its MD5 checksum SHALL be computed via `deviceServiceHashComputeFileMd5HexString` and stored as a system property

#### Scenario: Denylist MD5 after download
- **WHEN** the denylist file is downloaded successfully
- **THEN** its MD5 checksum SHALL be computed via `deviceServiceHashComputeFileMd5HexString` and stored as a system property

#### Scenario: File-needs-updating MD5 comparison
- **WHEN** the descriptor handler checks whether a file needs updating
- **THEN** it SHALL compute the local file's MD5 via `deviceServiceHashComputeFileMd5HexString` and compare it to the stored checksum

#### Scenario: Zigbee firmware MD5 validation
- **WHEN** `validateMD5Checksum` is called with a file path and expected MD5
- **THEN** it SHALL compute the file's MD5 via `deviceServiceHashComputeFileMd5HexString` and compare it to the expected value

### Requirement: Unit tests for deviceServiceHash
A cmocka unit test file (`core/test/src/deviceServiceHashTest.c`) SHALL exercise the module using `--wrap=fread,ferror` for fault injection.

#### Scenario: Known empty-file digest
- **WHEN** hashing an empty file
- **THEN** the result SHALL be `d41d8cd98f00b204e9800998ecf8427e`

#### Scenario: Large file spanning multiple read buffers
- **WHEN** hashing a file larger than the 4096-byte internal buffer
- **THEN** the result SHALL match the expected MD5

#### Scenario: Mid-read ferror returns NULL
- **WHEN** `fread` succeeds on the first iteration but returns 0 on the second with `ferror` returning 1
- **THEN** `deviceServiceHashComputeFileMd5HexString` SHALL return NULL

#### Scenario: Partial read then ferror returns NULL
- **WHEN** `fread` returns >0 bytes on the second iteration (partial read) followed by 0 on the third, with `ferror` returning 1
- **THEN** `deviceServiceHashComputeFileMd5HexString` SHALL return NULL

### Requirement: Remove icCrypto includes from all callers
All `#include <icCrypto/digest.h>` directives SHALL be removed from `deviceDescriptorHandler.c`, `zigbeeDriverCommon.c`, and any other files that include it.

#### Scenario: No remaining icCrypto includes
- **WHEN** the codebase is searched for `icCrypto/digest.h`
- **THEN** zero results SHALL be found

## REMOVED Requirements

### Requirement: Delete xhCrypto library
The entire `libs/crypto/` directory SHALL be deleted. This includes `digest.c`, `digest.h`, `verifySignature.c`, `verifySignature.h`, `compat.c`, `util.c`, `cryptoPrivate.h`, and the library's `CMakeLists.txt`.

#### Scenario: libs/crypto/ directory does not exist
- **WHEN** the filesystem is inspected after the change
- **THEN** `libs/crypto/` SHALL not exist

### Requirement: Remove xhCrypto from build system
- `xhCrypto` SHALL be removed from `core/CMakeLists.txt` `target_link_libraries`
- The `add_subdirectory(crypto)` line SHALL be removed from `libs/CMakeLists.txt`

#### Scenario: No CMake reference to xhCrypto
- **WHEN** the CMake files are searched for `xhCrypto` or `crypto` subdirectory
- **THEN** zero references SHALL remain (excluding third-party and unrelated crypto usage)

### Requirement: Remove digestFile mock from unit tests
The `__wrap_digestFile` function in `deviceDescriptorsAvailabilityTest.c` SHALL be removed, and `digestFile` SHALL be removed from the `WRAPPED_FUNCTIONS` list in `core/test/CMakeLists.txt`.

#### Scenario: No digestFile wrapping in test config
- **WHEN** `core/test/CMakeLists.txt` is inspected
- **THEN** `digestFile` SHALL not appear in any `WRAPPED_FUNCTIONS` or `--wrap` list

#### Scenario: No __wrap_digestFile in test source
- **WHEN** `deviceDescriptorsAvailabilityTest.c` is inspected
- **THEN** `__wrap_digestFile` SHALL not appear
