# Parameter configuration compatibility

## Format contract (Phase 6, established before implementation)

- ArduPilot-style `.param`: two fields, name and decimal value, separated by whitespace or a comma. No per-entry type is implied. Export uses the confirmed vehicle type to validate values, but this format cannot preserve that type in each row.
- QGC/PX4-style `.params`: five whitespace/tab-separated fields: system ID, component ID, name, decimal value, MAVLink parameter type. QGC also uses this format with ArduPilot. Syntax alone never proves firmware family.
- Both extensions accept either detected text layout. Mixed layouts, malformed entries, duplicates, unsupported classic-protocol types and nonfinite values are reported, never silently dropped. Decimal notation only; binary BSON/packed parameter files, hexadecimal values and firmware reset operations are outside this implementation.
- Comments beginning with `#` are supported. BonV exports `# BonV firmware: ardupilot|px4|unknown`; QGC `# Stack: ArduPilot|PX4` headers are recognized. Conflicting or unrecognized family declarations are reported as invalid.
- Known cross-family files are blocked. Missing file identity or unknown vehicle identity requires explicit acknowledgement; acknowledgement does not bypass name, type, range, current-snapshot or encoding validation. File system/component IDs describe the saved source, not a command target. Mixed components are reported as unsupported because the current manager owns one selected component.
- Export chooses two-field `.param` for known ArduPilot and typed `.params` for known PX4. Unknown family requires acknowledgement and exports explicitly unidentified typed `.params`. Stale exports require a separate warning acknowledgement and are marked as retained snapshots. Numeric output uses 17 significant decimal digits to preserve the confirmed double representation, including decoded REAL32 values.

Sources: [ArduPilot pymavlink reader/writer](https://raw.githubusercontent.com/ArduPilot/pymavlink/master/mavparm.py), [QGC parameter writer](https://github.com/mavlink/qgroundcontrol/blob/master/src/FactSystem/ParameterManager.cc), [QGC format documentation](https://github.com/mavlink/qgroundcontrol/blob/master/docs/en/qgc-dev-guide/file_formats/parameters.md).

## Ownership and lifecycle

`ParameterConfigurationController` is a file/comparison/application adapter owned by Mavlink. It does not synchronize parameters, retain an authoritative parameter cache, encode MAVLink messages, retry writes, or assign transaction outcomes. Parsing, export and comparison run through QtConcurrent using value copies. A comparison is explicitly tied to a snapshot revision and source generation.

Application submits one selected difference at a time through `Mavlink::requestParameterWrite`, waiting for its real terminal result before scheduling another. Existing manager pacing and queue limits still apply. No atomicity is promised. Existing active writes block starting an operation; each submission rechecks readiness, source, parameter identity, type, encoding and the compared current value. Draft conflicts are excluded by default; the user must explicitly choose to apply file values over those proposals. Drafts themselves are never overwritten by import.

Closing the comparison panel does not cancel an in-flight backend write. Stop prevents further submissions and waits for the pending result. Disconnect/refresh/source change prevents continuation, preserves completed results and requires a fresh comparison; reconnect never resumes automatically. Controller destruction discards UI bookkeeping without claiming cancellation; transport/manager shutdown owns backend cancellation. Reset to defaults is deferred.

Files are limited to 16 MiB and 100,000 non-comment entries. Exports use QSaveFile and report success only after commit. Extension and declared format are validated; malformed lines remain visible in the report. Favorites continue to use QSettings independently; imported values and results are session-local.

Snapshot validity on export refers to the immutable snapshot captured when export starts. Comparison statuses describe the compared revision; successful/rejected result rows refresh their current value from the authoritative snapshot and retain the actual transaction result. A new comparison is required after application or an interrupted operation.
