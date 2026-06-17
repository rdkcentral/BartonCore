## MODIFIED Requirements

### Requirement: Sbmd built-in library
The system SHALL provide a built-in JavaScript library `Sbmd` (loaded into every QuickJS context) with: `Sbmd.Tlv.decode(base64)` for Matter TLV decoding, `Sbmd.Tlv.encode(value, type)` for TLV encoding, `Sbmd.Tlv.encodeStruct(obj, schema)` for struct encoding, `Sbmd.Tlv.emptyStruct()` for empty struct TLV, `Sbmd.Base64` for base64 encode/decode, `Sbmd.Tlv.TYPE` with TLV type constants, and `Sbmd.result()` for building handler result chains. The library SHALL NOT provide `Sbmd.getPersistentData()` or `Sbmd.getTransientData()` functions — all storage reads go through supplements.

#### Scenario: Decode boolean TLV
- **WHEN** `Sbmd.Tlv.decode(base64)` is called with a TLV-encoded boolean `true`
- **THEN** it SHALL return JavaScript `true`

#### Scenario: Encode uint8 TLV
- **WHEN** `Sbmd.Tlv.encode(128, 'uint8')` is called
- **THEN** it SHALL return a base64 string containing the TLV-encoded uint8 value 128

#### Scenario: Decode invalid Base64 input
- **WHEN** `Sbmd.Tlv.decode(base64)` or `Sbmd.Base64.decode(base64)` is called with a string containing characters outside the Base64 alphabet (not A–Z, a–z, 0–9, `+`, `/`, or `=`)
- **THEN** it SHALL throw a JavaScript `Error` describing the invalid input

#### Scenario: No standalone storage getter functions
- **WHEN** a handler attempts to call `Sbmd.getPersistentData()` or `Sbmd.getTransientData()`
- **THEN** a JavaScript TypeError SHALL occur because these functions do not exist on the Sbmd namespace
