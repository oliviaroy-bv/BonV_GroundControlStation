# BonV GroundStation — Phase 0 baseline audit

Audit completed: 2026-11-21 (Asia/Calcutta). Initial inspection began under the 2026-11-19 environment context; branch, HEAD, working tree and workbook identity were rechecked on 2026-11-21.

Scope: assess the current working tree against the supplied 112-finding register. No resolution phase has been implemented or approved by this report.

## 1. Scope, evidence and preservation

The master register was located at C:/Users/Olivia/Downloads/BonV_GCS_Audit_Issue_Register_FINAL.xlsx, rather than the repository root. Its Issue Register sheet contains exactly GCS-001 through GCS-112. The Read Me and Audit Coverage sheets were also inspected. The workbook describes reported findings, not independently verified defects; architectural recommendations and review gaps must retain that distinction.

Workbook SHA-256, checked again at completion:

    FA914C0753D774CFB3B3DEDFCEC1192DC97A737B32223EEE3F6A66D82BFDE0E7

Every issue below retains its original title, description and reported priority. Priority is transcribed, not reassigned. Proposed sequencing is separate from severity. Original cited locations are retained as historical references; old line numbers are not represented as current locations. Current evidence names the inspected paths and symbols.

Evidence levels:
- **Source finding:** an inspected branch/dataflow supports the concern; this does not mean an aircraft failure was reproduced.
- **Source/test addressed:** the original mechanism has a replacement plus relevant regression assertions. Fresh execution is identified separately; historical logs are not promoted to fresh verification.
- **Partial:** some original behavior is corrected, but a remaining branch or consumer still needs closure.
- **Investigation/review/architecture:** a requirement, performance measurement, platform condition or deep review is needed before calling the finding a confirmed defect.
- **Not reproduced/obsolete mechanism:** the original claim is not established by the current audit; replacement behavior may still need validation.

Only this report was deliberately created in the repository. No application source, QML, tests, CMake, configuration, installer scripts or workbook was edited; nothing was staged, reset, cleaned, stashed, checked out, deleted or untracked.

**Read-only exception:** running CTest with -N unexpectedly rewrote the ignored build/Testing/Temporary/LastTest.log with a 156-byte discovery-only log (timestamp 2026-11-19 18:42 local). The previously inspected historical log was not fully captured, so it was not reconstructed or replaced with guessed content. The independent build/phase6-ctest.log and other named historical logs remain available. This is an audit side effect, not a test run or an application change.

A SHA-256 aggregate over the original 13 modified and eight untracked files matched before report creation and after source inspection/test execution:

    F3-1D-12-38-1A-EF-74-A3-58-D4-6A-4C-B3-65-41-C2-57-54-3A-6B-62-A5-9A-4B-36-9E-E7-B2-99-8A-55-EE

This preservation check covers those existing work files, not a claim that every ignored/generated file was byte-for-byte monitored. Fresh test result files and a GStreamer test registry were written only to uniquely named paths in the Windows temporary directory.

## 2. Repository baseline

| Item | Observed baseline |
|---|---|
| Project | C:/GCS/BonV_GroundControlStation |
| Branch | ParameterConfig |
| HEAD | 667f639b8bdf9836add8f90d2aaecbd32c709e81 |
| HEAD date | 2026-11-12 20:15:15 +0530 |
| HEAD subject | Map Correction, manual planning, link correction |
| Staged changes | None |
| Existing tracked modifications | 13 files |
| Existing untracked files | 8 files |
| Tracked paths | 16,158 |
| Ignored, untracked paths | 28,447 at inventory |
| Git object storage | 3,994 loose objects / 318.29 MiB; 7,389 packed objects in two packs / 343.42 MiB |

Existing modified files, all preserved:

    CMakeLists.txt
    app/main.cpp
    comms/mavlink/Mavlink.cpp
    comms/mavlink/Mavlink.h
    installer/config/config.xml
    installer/packages/com.bonv.gcs/meta/package.xml
    resources.qrc
    tests/CMakeLists.txt
    tests/MavlinkTest.cpp
    tests/qml/tst_CommLinkDiagnostics.qml
    ui/qml/accessories/CommLinkDiagnostics.qml
    ui/qml/panels/TopPanel.qml
    ui/qml/settings/OemSettingsPage.qml

Existing untracked files, all preserved:

    core/parameters/ParameterConfigurationController.cpp
    core/parameters/ParameterConfigurationController.h
    docs/ParameterConfiguration.md
    tests/ParameterConfigurationTest.cpp
    tests/qml/tst_FullParameterList.qml
    ui/qml/accessories/LinkDiagnosticIcon.qml
    ui/qml/settings/FullParameterListPage.qml
    ui/qml/settings/ParameterConfigurationPanel.qml

These are material application/test inputs, not disposable audit output. CMake/resources already reference later work. A checkout of HEAD alone is therefore not the same implementation being inspected.

### GCS-110 and GCS-111

The originally named MissionPlanModel, MissionPlanConverter, MavlinkConsoleController, MapDispatch.js and their inspected tests are now tracked. GCS-110 is partially addressed, not closed: later important files remain untracked.

The integrations tree accounts for 13,860 tracked paths and installer for 2,116. Tracked extensions include 602 .a, 133 .so and 108 .lib files. The .a/.so/.lib/.jar/.rcc inventory totals 851 existing files and 798,965,457 bytes. This is not the full repository footprint or proof every vendored dependency is unnecessary. Android-generated trees and installer payload content are still tracked.

.gitignore excludes build/, common CMake outputs, executable/DLL/object/PDB outputs, autogen files, logs and editor files, but does not retroactively remove tracked payloads. The root installer executable and many generated runtime DLLs are ignored rather than necessarily tracked. Do not infer tracked status merely from a file existing on disk.

Before implementation: agree which work belongs in source control, preserve the current workspace, record dependency provenance/licences and establish a reproducible dependency/artifact strategy. No deletion, untracking or history rewrite is authorized by this report.

### Build and deployment directories

Existing paths include build/Debug, build/Release, build/deploy, build/deploy-backup-20261020-160542, build/deploy-backup-20261113-185249, build/tests, build/Testing, build/console-deploy, build/console-redesign-deploy and multiple phase/communication/link baseline directories. They contain retained binaries, logs, screenshots, INI test data and generated build files.

These folders were not cleaned, renamed or redeployed into. Historical baseline/deployment folders are evidence and must be inventoried before any later cleanup.

## 3. Build, packaging and startup baseline

### Configuration and reproducibility

First-party configuration inspected: root CMakeLists.txt and tests/CMakeLists.txt, including root CTest setup and post-build commands. No project CMakePresets.json was found. The linked source lists were used to distinguish active integrations/SkyRouteX_New planning code from older standalone SkyRouteX desktop files. Qt/GStreamer/generated third-party build internals were not exhaustively audited.

Current configuration:
- C++20, AUTOMOC/AUTORCC; project still MavlinkPOC VERSION 0.1.
- Qt 6.12.0 MSVC 2022 kit at C:/Qt/6.12.0/msvc2022_64.
- Cached generator: Visual Studio 18 2026.
- Cached BONV_BUNDLED_GSTREAMER_ROOT: C:/gstreamer/1.0/msvc_x86_64.
- Cached BONV_DEPLOY_DIR: C:/GCS/BonV_GroundControlStation/build/deploy.
- BONV_REQUIRE_GDAL=OFF and GDAL_DIR=GDAL_DIR-NOTFOUND.
- OpenSSL headers/libraries currently resolve under C:/Program Files/OpenSSL-Win64; current deployment code also hard-codes that runtime prefix.
- Missing tools/RasterImportWorker.cpp is still referenced in the GDAL_FOUND branch. GDAL-disabled workstation builds do not establish that this branch builds.
- The root/common include directory is absent but still listed.
- MSVC runtime discovery has a fatal missing-runtime check; its portability across valid toolchain installations is unverified.

A fresh checkout/build has not been demonstrated. Untracked inputs, dependency discovery, conditional missing sources and shared deployment must be resolved before major work.

### Debug/Release deployment contamination

Both configurations use the same BONV_DEPLOY_DIR. Post-build commands copy the application, GStreamer runtime, media and Qt runtime into it. windeployqt receives --debug or --release based on configuration, but old files are not removed. Alternating configurations can still mix runtimes.

Current read-only observations:
- build/deploy/Qt6Core.dll exists; Qt6Cored.dll does not.
- build/deploy/platforms/qwindowsd.dll does not exist.
- The existence of valid release files such as Qt6Quick3D.dll, dav1d.dll or Qt6VirtualKeyboard.dll must not be mistaken for debug DLLs merely because their names end in d.dll.
- The present folder passes the limited runtime-presence preflight below. This does not close the shared-directory contamination risk.

No Debug or Release build was run in this audit: either can trigger post-build deployment into the preserved shared folder. No reconfiguration, clean-first or alternative deployment override was performed. An incremental no-op build would not establish fresh reproducibility anyway. Use a later isolated build tree and separate per-configuration deployment roots after approval.

### Executable subsystem and terminal logging

Current CMake sets WIN32_EXECUTABLE TRUE under WIN32. Read-only PE header inspection found:

| Existing artifact | Last-write local time | PE subsystem |
|---|---|---|
| build/Debug/BonVGroundStation.exe | 2026-11-13 17:56:28 | 3: Windows console |
| build/Release/BonVGroundStation.exe | 2026-11-13 18:56:41 | 2: Windows GUI |
| build/deploy/BonVGroundStation.exe | 2026-11-13 18:56:45 | 2: Windows GUI |

The Debug binary predates the later GUI-subsystem change. These artifacts must not be described as freshly rebuilt from the current source.

app/main.cpp uses AttachConsole(ATTACH_PARENT_PROCESS), without AllocConsole. It attaches stdout/stderr to CONOUT$ and writes Qt messages to the parent console only when attachment succeeds. Runtime logging appends to runtime.log and flushes synchronously. It also logs a one-second GUI heartbeat.

The original Release console-window mechanism is addressed in source and the existing Release artifact. Explorer double-click, CMD, PowerShell, output redirection and failure to open runtime.log were not interactively tested, because launching the application can create user data, start background subsystems and write logs/settings. In particular, reopening CONOUT$ deserves a redirection smoke test; source intent does not prove redirected terminal logging works.

### Installer inspection and safe preflight

Inspected installer/package.bat, installer/config/config.xml, installer/config/controlscript.qs, installer/packages/com.bonv.gcs/meta/package.xml and installscript.qs.

The package script:
- Reads config/package versions and rejects mismatch.
- Resolves binarycreator and GStreamer inputs.
- Rejects Qt6Cored.dll before deleting the previous package data.
- Deletes/recreates installer/packages/com.bonv.gcs/data, copies deployment and removes symbols.
- Validates required runtime files and compares GStreamer plugin names.
- Invokes binarycreator to write a root installer executable.

The script was **not executed**: payload deletion/copy and installer replacement violate the audit preservation constraints. A read-only equivalent of its key presence checks was run:
- All 12 listed required runtime paths were present.
- 213 source GStreamer DLL names and 213 deployed names were found; none of the source names was missing.
- Debug core and debug Windows platform plugin were absent.
- C:/Qt/Tools/QtInstallerFramework/4.11/bin/binarycreator.exe exists.
- Config and package versions both read STAGED_1.3.

This is a file-presence/name consistency check, not a packaging run, DLL ABI/dependency validation, signature check or successful installation. Other runtime validation inside package.bat was inspected but not all re-executed.

controlscript implements existing-install detection, version comparison and uninstall/reinstall behavior, including nonempty-target options. installscript creates shortcuts and HKCU entries. No disposable-VM upgrade/clean-install/rollback test coverage was located. Updateable=true alone does not establish correct upgrades.

## 4. Current implementation reused by later work

### Parameters and Full Parameter List

Vehicle owns ParameterManager. Mavlink remains the QML-facing transport facade; parameter snapshots, validation, recovery and write transactions remain under ParameterManager.

Inspected ParameterManager.h/.cpp, Mavlink.h/.cpp, Vehicle.h/.cpp, FullParameterListPage.qml, OemSettingsPage.qml, ParameterConfigurationController.h/.cpp, related QML panel, ParameterManagerTest.cpp, MavlinkTest.cpp, ParameterConfigurationTest.cpp and the Full Parameter List/readiness QML tests.

Current source provides:
- Intended-source and generation validation, index/name/count consistency, complete-only publication, revision/provenance, bounded missing-index recovery and explicit failure.
- Retained complete snapshots distinct from current Ready status.
- Source/type/encoding-validated asynchronous writes, deferred submission, queue/deadline/retry handling and terminal per-transaction outcomes.
- OEM Vehicle Settings navigation into the Full Parameter List, ListView virtualization, search/type filters/prefix-derived groups, drafts, Modified/Favorites and favorite persistence.
- Confirmed values separate from proposals; write correlation includes source/ID/generation.
- File import/export/comparison in a presentation/application controller, background parsing/comparison and sequential application through the existing write facade. It does not replace synchronization or transaction ownership.
- ArduPilot pair and QGC/PX4 typed file handling, firmware-family/unknown handling, invalid/type/missing comparison rows, explicit draft conflict/confirmation and per-write/aggregate outcomes in the inspected tests.

The configuration controller and multiple UI/tests remain untracked: functionality existing on disk is not proof it is safely delivered in HEAD. Existing docs/ParameterConfiguration.md describes format limitations. No additional format compatibility research or firmware flight acceptance was claimed during this local audit.

GCS-020 and GCS-030 have direct source and test evidence of correction. GCS-112 has strong core/readiness coverage, but closure requires an exhaustive parameter-dependent consumer map. A blanket startup freeze for unrelated emergency/mission actions is not implied.

### Missions, maps and other subsystems

MissionPlanModel and MissionPlanConverter are real, tracked canonical components with freshly passing tests. Legacy mission parsing/upload still exists and differs in protocol-home handling. Dirty-model protection exists, but download replacement/outcome and upload-vs-disk-save issues remain.

FlyMap/MapDispatch now implement readiness states, document/style identity, bounded dispatch, cancellation, timeout and replay. Renderer termination has failure invalidation and explicit retry. The custom bridge remains an architecture choice with regressions, not automatically a defect. Fresh MapDispatch tests do not prove an actual WebEngine renderer crash recovers.

MAVLink and Vehicle now contain selected-heartbeat lifecycle and telemetry freshness/flight-phase logic. Filtering is not universal: SYS_STATUS, battery arbitration and RC input require separate closure. Video/recording, terrain bounds, generic command acknowledgement and installer workflows still have important unresolved paths.

## 5. Register reconciliation

The issue-by-issue assessment in section 9 is the authoritative mapping. Status labels describe the evidence available today and do not silently replace the workbook's original validation status.


| Current audit status | Count |
|---|---:|
| Addressed in source and tests | 5 |
| Still valid | 63 |
| Partially addressed | 21 |
| Requires investigation | 12 |
| Architecture recommendation | 6 |
| Obsolete original mechanism; replacement needs validation | 1 |
| Not reproduced in this audit | 1 |
| Addressed in source and artifact | 1 |
| Still valid review gap | 1 |
| Review gap | 1 |
| **Total** | **112** |

### Findings already addressed or superseded

Source/test evidence supports addressing GCS-001, GCS-020, GCS-021, GCS-022 and GCS-030. GCS-102 also has current source and existing Release artifact evidence. These are closure candidates, subject to isolated integration/field regressions and the evidence limits listed per issue.

GCS-070's original application-window capture mechanism has been superseded by desktop-screen capture; validate the replacement rather than reproducing an obsolete implementation. GCS-090's tricopter test claim is partly obsolete: the four-motor test now sets up a confirmed quad frame. GCS-110's originally named files are tracked, but new untracked work keeps the finding open. No entire multi-part issue was closed merely because a component with a related name exists.

### Implementation and investigation backlog

The "Still valid" rows require a focused fix or an explicit product-policy resolution. Partial rows must retain remaining branches/consumers in scope. Architecture recommendations are GCS-055 and GCS-104 through GCS-108. GCS-109 is a deep-review gap; GCS-103 is an unclosed installer verification gap. GCS-095 was not reproduced by the current narrow VideoTest.

Requirements or measurement are particularly important for GCS-008, GCS-032, GCS-035, GCS-036, GCS-040, GCS-045, GCS-049, GCS-050, GCS-051, GCS-060, GCS-062, GCS-063, GCS-067, GCS-069, GCS-071 and GCS-098. Some have concrete source behavior but still need a supported-platform, protocol, UX or safety-policy decision. Their original priorities remain unchanged.

## 6. Dependencies and proposed execution sequence

The following are proposed resolution workstreams, not automatically started phases. F is a prerequisite gate for implementation; S1-S6 follow the requested safety-first ordering. Read-only safety investigation can proceed alongside F.

| Workstream | Scope and principal issue IDs | Exit evidence |
|---|---|---|
| F — preserve/reproduce/isolate | GCS-110, GCS-111; build GCS-096 to GCS-100, GCS-102; test isolation/discovery GCS-091, GCS-092, GCS-094; installer baseline GCS-103 | Intended source safely preserved; isolated checkout/build and per-config deployments; isolated settings/data/test output; reproducible test/installer baseline |
| S1 — flight safety and vehicle state | GCS-002 to GCS-019; GCS-023, GCS-029; closure regressions GCS-001, GCS-020, GCS-030, GCS-090, GCS-112; narrow GCS-107 capability seam | Correct source identity, home/flight/battery state; deliberate joystick release/control semantics; mode/command confirmation; no destructive unsafe mission upload; verified readiness consumers |
| S2 — communications | GCS-021, GCS-022 regression; GCS-025, GCS-031 to GCS-039, GCS-041, GCS-081 | Valid identity/protocol support, bounded queues/pacing, transport errors and retry idempotence; slow-link and reconnect evidence |
| S3 — mission/map reliability | GCS-024, GCS-026 to GCS-028, GCS-042 to GCS-054, GCS-057, GCS-058, GCS-079 | Canonical/file/wire semantics agree; bounded terrain and valid route domain; map timeout/failure/retry; malformed input protection |
| S4 — stability/video | GCS-059 to GCS-070, GCS-078; investigate GCS-095 | Recording lifetime/thread/shutdown correctness, stale-stream indication, responsive finalization and supported-platform evidence |
| S5 — packaging/security/diagnostics | GCS-056, GCS-071 to GCS-077, GCS-080, GCS-082 to GCS-089; complete GCS-097/GCS-098/GCS-102/GCS-103 matrix | Controlled credentials/session/data/logging/cache policies, diagnostics accuracy, clean standalone install/upgrade/uninstall |
| S6 — architecture and remaining review | GCS-055, GCS-093 residual, GCS-101, GCS-104 to GCS-109; capability decisions GCS-040/GCS-107 | Tested boundaries, documented supported capabilities and bounded module reviews; no speculative whole-application rewrite |

This ordering does not defer urgent subparts until their nominal stream: GCS-065/057/058/085 GUI blocking must be measured/mitigated as dependencies of GCS-014 in S1. GCS-079 malformed ZIP bounds and repository/build reproducibility deserve early containment before using external terrain/configuration inputs. Tests are added with each fix, not postponed wholesale to S6.

Concrete dependency edges (prerequisite → dependent):
- GCS-110/GCS-111 → reproducible work on every feature; especially GCS-096 and later parameter/configuration tests.
- GCS-091 → trustworthy full CTest/direct/QML baselines; GCS-094 → automated QML coverage; GCS-092 → interpretable GCS-095 flakiness analysis.
- GCS-001 selected-source lifecycle + GCS-020 validated source + GCS-030 confirmed transactions → GCS-112 consumer closure and safe parameter UI/configuration operations.
- GCS-005/GCS-107 supported firmware behavior + GCS-029 ACK/command policy → GCS-004 safe mode-to-takeoff sequencing and GCS-002/GCS-003 control semantics.
- GCS-015/GCS-016/GCS-018/GCS-019 → trustworthy vehicle state and UI/calibration; GCS-017/GCS-018 → GCS-009 battery display and GCS-008 warning policy.
- GCS-007/GCS-010 → GCS-011 structured severity and GCS-008 alerts.
- GCS-024/GCS-023/GCS-042 → safe mission serialization; GCS-025/GCS-029/GCS-038 → reliable transfer; all support GCS-006 upload safety.
- GCS-028 → correct unsaved-plan policy in GCS-012/GCS-026/GCS-027.
- GCS-037/GCS-038/GCS-039 → reliable slow-link command/control; GCS-031/GCS-041 → protocol identity/isolation and any future multi-vehicle/signing work.
- GCS-043/GCS-044/GCS-046/GCS-047/GCS-049/GCS-050/GCS-051 → terrain/mission trust; GCS-079 must protect malformed terrain input before broad external-data testing.
- GCS-052/GCS-054 → map readiness/recovery closure GCS-053; preserve GCS-055 dispatch regressions during these changes.
- GCS-064/GCS-065 → recording shutdown GCS-068/GCS-069 and GUI-dependent control GCS-014.
- GCS-078 → reliable application/QML shutdown; coordinate GCS-012 and recorder finalization.
- GCS-082/GCS-083/GCS-084/GCS-085/GCS-086 → bounded, observable diagnostic storage; GCS-087/GCS-088/GCS-089 → accurate, efficient inspector.
- Build/deploy consistency + GCS-096/GCS-097/GCS-098/GCS-102 → meaningful GCS-103 installer VM verification.

Dependencies are planning relationships, not permission to bundle unrelated refactors. Some are mutual policy decisions rather than a strict acyclic code sequence; split them into focused acceptance criteria during planning.

## 7. Test and tool baseline

### Fresh execution

Existing Debug test executables were executed directly after inspecting their side effects. They were not rebuilt. This establishes the behavior of those binaries, not clean-build provenance for the current workspace. Tests generating temporary fixtures used the OS temporary directory.

| Check | Actual method | Result |
|---|---|---|
| ParameterManager | build/tests/Debug/ParameterManagerTest.exe with QtTest -o <unique TEMP file>,txt; Start-Process -Wait | 74 passed, 0 failed, 0 skipped; 252 ms; exit 0 |
| MissionPlanModel | Direct Debug executable, same QtTest output method | 11 passed, 0 failed, 0 skipped; 8 ms; exit 0 |
| MissionPlanConverter | Direct Debug executable, same method; fixtures use QTemporaryDir | 10 passed, 0 failed, 0 skipped; 89 ms; exit 0 |
| VideoTest | Direct Debug executable, same method; GST_REGISTRY redirected to unique TEMP file | 6 passed, 0 failed, 0 skipped; 6 ms; exit 0 |
| MapDispatch QML | qmltestrunner directly, -input tests/qml/tst_MapDispatch.qml -platform offscreen; software backend, disk cache disabled | 25 passed, 0 failed, 0 skipped; 2931 ms; exit 0 |
| Selected readiness QML | qmltestrunner directly, four named functions below, same offscreen/software settings | 6 passed including setup/cleanup, 0 failed, 0 skipped; 32 ms; exit 0 |
| CTest registration inventory | ctest --test-dir build -C Debug -N | 12 C++ tests discovered, no QML; no tests executed; ignored LastTest.log side effect disclosed in section 1 |
| Focused qmllint | Qt 6.12.0 qmllint on FullParameterListPage, ParameterConfigurationPanel, CommLinkDiagnostics and LinkDiagnosticIcon | Exit 0, no diagnostics |
| OEM/top-panel qmllint | qmllint on OemSettingsPage.qml and TopPanel.qml | Exit 0, two existing unqualified-access warnings at OEM line 4669; no TopPanel diagnostics |
| Packaging presence preflight | Read-only PowerShell Test-Path/XML/plugin-name comparisons | 12/12 required paths present; 213/213 source plugins represented; no debug core/platform plugin |

Qt binaries used C:/Qt/6.12.0/msvc2022_64/bin; native tests had that directory and C:/gstreamer/1.0/msvc_x86_64/bin prepended to process PATH. Start-Process was hidden and waited for completion. The primary captured ParameterManager result is the 74-pass run; preliminary stdout probes produced no captured QtTest output and are not additional evidence of reported pass counts.

Selected readiness functions, excluding screenshot-producing cases:
- ParameterReadiness::test_progressAndRetainedSnapshot
- ParameterReadiness::test_retry
- ParameterReadiness::test_oldStatusSourceCannotRestoreReady
- ParameterReadiness::test_recreateWhileLoadingAndRapidStateChanges

Representative direct QML command (output path unique per run):

    C:/Qt/6.12.0/msvc2022_64/bin/qmltestrunner.exe -input C:/GCS/BonV_GroundControlStation/tests/qml/tst_MapDispatch.qml -platform offscreen -o <TEMP-result>,txt

The readiness command used its QML file plus the four fully qualified function names. Neither QML run was through CTest.

Fresh result files retained:
- C:/Users/Olivia/AppData/Local/Temp/BonV-phase0-parameters-28180fb6-59bb-4551-9964-136a9bd8491b.txt
- C:/Users/Olivia/AppData/Local/Temp/BonV-phase0-MissionPlanModelTest-843c1a8d-0cf2-4838-a084-b5c329791639.txt
- C:/Users/Olivia/AppData/Local/Temp/BonV-phase0-MissionPlanConverterTest-8b28e547-edc7-4d13-990e-3056449e6c37.txt
- C:/Users/Olivia/AppData/Local/Temp/BonV-phase0-video-450d50c4-23a9-429e-9753-0881e6ac70b2.txt
- C:/Users/Olivia/AppData/Local/Temp/BonV-phase0-mapdispatch-9c001be7-85cf-4cba-b4c5-4f09a2c4094c.txt
- C:/Users/Olivia/AppData/Local/Temp/BonV-phase0-readiness-selected-2acbd7ad-535c-4981-890f-f6253b94b456.txt

### Safely deferred execution and historical evidence

| Requested baseline | Why not freshly run in full | Available evidence and limits |
|---|---|---|
| Debug build | Post-build overwrites preserved shared deployment and may mix runtimes | Existing Debug artifact and build/top-dropdowns-build.log dated Nov 13; not a fresh build result |
| Release build | Same shared deployment; no clean/reconfigure/override authorized in this audit | Existing Nov 13 Release/deploy GUI-subsystem artifacts; older build/comm-quality-final-release.log and related release logs are historical, not current-source proof |
| Full CTest | Existing registration writes build test logs; test isolation from HKCU/Documents is incomplete | build/phase6-ctest.log records 12/12 passing, 60.82 s, Nov 13; this is not a fresh full-suite pass |
| Full MAVLink C++ | Logging setters use default QSettings and some tests enable logging/save mission data | build/phase6-mavlink-tests.txt: 105 passed, 0 failed, 20793 ms, historical |
| ParameterConfiguration C++ | Harness changes logging settings; full suite not proven isolated despite QTemporaryDir file fixtures | build/phase6-configuration-tests.txt: 38 passed, 0 failed, 8140 ms, historical |
| Vehicle/console/other C++ full suites | Some construct logging/transport subsystems or use real data roots; safe scope not established for every test | Source and historical CTest reviewed; no new full-suite result claimed |
| Full FullParameterList QML | Writes fixed build/phase5-favorites-tests.ini and multiple existing screenshot paths | build/phase6-full-parameter-tests-final.txt: 108 passed, 0 failed, 5950 ms, historical |
| Full readiness QML | test_status and loading motor case overwrite existing build screenshots | build/phase6-readiness-tests.txt: 21 passed, 0 failed, 542 ms, historical; selected four functions freshly passed separately |
| Installer packaging/install/upgrade | package.bat deletes/repopulates payload and replaces installer; VM/installation changes exceed read-only scope | Limited fresh preflight passed; no new binarycreator, clean-install or upgrade result |

Historical phase6 logs reflect the later configuration work and show **12**, not the earlier phase's 11, CTest targets. The retained LastTestsFailed.log mentions an older failure and is stale relative to the later full passing phase6 log; it is not evidence of a fresh failure. The overwritten LastTest.log must no longer be used as the historical pass artifact.

Fresh checks had no test failures. The two OEM lint warnings remain. Important unexecuted paths include physical serial unplug, joystick hardware, slow-radio load, actual renderer crash, streaming/recording races, installer workflows and all live/SITL vehicle behavior.

## 8. Risks and gates before flight-related SITL validation

A disarmed, contained protocol simulator may be used later to reproduce defects; this audit does not approve flight, propeller-on testing or broad aircraft control.

Before flight-related SITL acceptance:
1. Preserve intended source and establish an isolated reproducible build/deployment/test baseline (GCS-110/111/091/094/096-103).
2. Resolve joystick release/protocol conflict/calibration/control cadence (GCS-002/003/013/014), including UI-blocking dependencies.
3. Verify firmware-specific modes and mode-to-command acknowledgement rather than delay-based takeoff/mission start (GCS-004/005/029/107).
4. Remove or explicitly contain destructive mission clear-before-upload and validate home/first-item/count/retry semantics (GCS-006/023/024/025/042).
5. Prove selected-source home/flight/battery/RC state and safe reconnect reset behavior (GCS-015-019); retain heartbeat-expiry regressions (GCS-001).
6. Complete the parameter-dependent consumer readiness audit, retaining source/generation/confirmed-write integrity (GCS-020/030/112).
7. Preserve autopilot severity and provide agreed failsafe/battery/close policies (GCS-007-012).
8. Establish slow-link budgets, transport failure/backpressure and identity/protocol correctness (GCS-031-039/041).
9. Constrain generator inputs, terrain/obstacle claims and geospatial domain; reject missing/void/malformed data safely (GCS-043-051/079).
10. Validate map failure visibility and operator recovery; imagery must not appear ready after renderer/resource failure (GCS-052-058).
11. Eliminate recording lifetime races and blocking shutdown/control interactions; stale video must be identifiable (GCS-059-070/078).
12. Make logging failures/space/resource growth observable without blocking control; preserve standalone runtime/installer consistency (GCS-082-086/103).

These are acceptance gates and dependencies, not additional issue IDs or silently revised severities. Some findings describe capability limits requiring explicit operational restrictions instead of immediate feature expansion.

## 9. All 112 issue assessments

For every entry: the original description is quoted verbatim from the FINAL workbook; current validity, inspected evidence, dependencies, proposed workstream and verification are the audit assessment. No original issue priority has been changed.

### GCS-001 — Vehicle link loss is not detected

**Original category/subcategory:** Flight safety & vehicle state / Connectivity / heartbeat  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:1735, 3184; Vehicle::heartbeatFresh; FlyView.qml:26

**Original description:**

> Connected state becomes true on first heartbeat and only clears when the transport drops. UDP and a still-plugged serial radio can remain connected after the air link is lost; UI can continue showing Connected and leave ARM/RTL/LAND enabled. heartbeatFresh exists but is not bound in QML.

**Current status/validity:** Addressed in source and tests. A selected-autopilot heartbeat deadline now establishes/revokes the application connection, cancels parameter/mission work and permits generation-safe heartbeat reconnect. This closes the original absence of link-loss detection; physical-link and flight validation remain outstanding.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: handleHeartbeat, processConnectionTimeout, endConnection; tests/MavlinkTest.cpp: connectionRequiresValidHeartbeat, heartbeatExpiryRevokesSnapshotAndReconnects, heartbeatExpiryCancelsActiveAndQueuedWrites; core/vehicle/Vehicle.cpp: expireTelemetryState.

**Dependencies:** GCS-091, GCS-093.  
**Recommended resolution workstream:** S1 regression.  
**Recommended verification:** Isolated MAVLink lifecycle suite, then dropped-heartbeat SITL with stale-command rejection.

### GCS-002 — Joystick release sends zero throttle

**Original category/subcategory:** Flight safety & vehicle state / Joystick control  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** JoystickController.cpp:499, 519

**Original description:**

> On disable or unplug, sendManualControl(0,0,0,0) is sent; the report states MANUAL_CONTROL z=0 means minimum throttle, not neutral. The RC-release packet sent immediately afterwards is currently relied upon to avoid a throttle cut.

**Current status/validity:** Still valid. Both release paths still send MANUAL_CONTROL with z=0 before releasing RC overrides. Source confirms the reported command; its aircraft-specific throttle effect has not been reproduced on a vehicle.

**Current source/symbols and evidence inspected:** app/JoystickController.cpp: release paths near lines 499 and 519; sendManualControl(0,0,0,0,0).

**Dependencies:** GCS-003, GCS-005, GCS-029.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Capture joystick enable/disable/device-loss wire traffic and verify firmware-specific release semantics in disarmed SITL.

### GCS-003 — Conflicting joystick control message types are sent simultaneously

**Original category/subcategory:** Flight safety & vehicle state / Joystick control  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** JoystickController.cpp:1016-1020

**Original description:**

> While enabled, both MANUAL_CONTROL and RC_CHANNELS_OVERRIDE are sent at 25 Hz. The report flags two potentially conflicting control sources and possible overriding of the pilot's physical transmitter.

**Current status/validity:** Still valid. The joystick send loop still sends MANUAL_CONTROL and RC_CHANNELS_OVERRIDE for the same input. There is no inspected negotiated choice of control protocol.

**Current source/symbols and evidence inspected:** app/JoystickController.cpp: periodic control transmission near lines 1016-1050; comms/mavlink/Mavlink.cpp: sendManualControl, sendRcChannelsOverride.

**Dependencies:** GCS-002, GCS-005, GCS-039.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Wire-level assertions for exactly one approved control path per supported firmware.

### GCS-004 — Takeoff proceeds without confirming mode change

**Original category/subcategory:** Flight safety & vehicle state / Command sequencing  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:618-626

**Original description:**

> takeoff() sends mode 4, waits 1200 ms, then sends TAKEOFF regardless of whether the mode change was accepted.

**Current status/validity:** Still valid. takeoff requests custom mode 4 and schedules NAV_TAKEOFF after 1200 ms. Connection/generation guards are new protection, but mode acknowledgement or observed-mode confirmation is still absent.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: takeoff.

**Dependencies:** GCS-005, GCS-029, GCS-001.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Reject/delay/lose SET_MODE in SITL; verify no takeoff command until approved mode confirmation.

### GCS-005 — Flight mode commands and names assume ArduCopter

**Original category/subcategory:** Flight safety & vehicle state / Firmware compatibility  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp; Vehicle.cpp:386; BottomPanel.qml:1274

**Original description:**

> startMission() sends mode 3, which is not AUTO for ArduPlane; mode-name table and mode buttons are Copter-only, so Plane/Rover/PX4 may show incorrect mode names or receive wrong mode commands.

**Current status/validity:** Still valid. Takeoff/startMission still use Copter custom modes 4/3 and Vehicle's names remain firmware-specific. Parameter encoding and console-family detection do not implement flight-mode abstraction.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: takeoff, startMission, sendSetMode; core/vehicle/Vehicle.cpp: heartbeat mode decoding.

**Dependencies:** GCS-107, GCS-029.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Table-driven family/vehicle-type mode tests and ArduCopter/Plane/Rover/PX4 SITL.

### GCS-006 — Mission clear-before-upload can leave vehicle without a mission

**Original category/subcategory:** Flight safety & vehicle state / Mission operation safety  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp mission clear/upload path

**Original description:**

> If mission upload fails partway after clearing the existing mission, the vehicle is left with no mission. Nothing prevents clear/upload while the vehicle is flying in AUTO. The audit recommends avoiding destructive clear-before-upload behaviour.

**Current status/validity:** Still valid. Both legacy and canonical upload paths enter ClearingBeforeUpload and send MISSION_CLEAR_ALL. missionTransferAvailable checks connection and transfer occupancy, without an armed/AUTO prohibition or recoverable mission replacement guarantee.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: uploadMissionFile, uploadMissionPlan, writeLoadedMissionToVehicle, missionTransferAvailable.

**Dependencies:** GCS-023, GCS-024, GCS-025, GCS-029.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Interrupt after clear ACK; test armed/AUTO gating, failure recovery and preservation of vehicle mission.

### GCS-007 — Autopilot STATUSTEXT severity is discarded

**Original category/subcategory:** Flight safety & vehicle state / Operator awareness  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Vehicle.cpp:632; FlyView.qml:998

**Original description:**

> Vehicle::statusTextReceived emits only text; QML guesses severity from keywords. Actual MAVLink severity is lost, so failsafe and other important messages can be displayed as informational.

**Current status/validity:** Partially addressed. Flight-controller logging has severity-to-text handling, but Vehicle still emits only the STATUSTEXT string to the message UI. The original UI severity loss remains.

**Current source/symbols and evidence inspected:** core/vehicle/Vehicle.cpp: STATUSTEXT case/statusTextReceived; comms/mavlink/Mavlink.cpp: mavSeverityText, logFlightControllerStatusText; ui/qml/core/FlyView.qml: classifyVehicleMessage.

**Dependencies:** GCS-010, GCS-011, GCS-008.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Inject equal text at different MAV_SEVERITY levels; assert preserved structured severity through UI/logs.

### GCS-008 — No audio alerts or dedicated failsafe / low-battery warnings

**Original category/subcategory:** Flight safety & vehicle state / Operator awareness  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** 

**Original description:**

> The report says there are no audio alerts and no dedicated low-battery or failsafe warning.

**Current status/validity:** Requires investigation. No dedicated audio/failsafe/low-battery alert path was located in the inspected message and battery UI. This is a safety capability/requirements gap; absence of an alert specification is not proof of a reproduced runtime defect.

**Current source/symbols and evidence inspected:** ui/qml/core/FlyView.qml: vehicle-message handling/classifyVehicleMessage; ui/qml/panels/TopPanel.qml: battery presentation; core/vehicle/Vehicle.cpp: battery/status messages.

**Dependencies:** GCS-007, GCS-009, GCS-017.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Agree alert requirements, then inject critical/failsafe/battery events with acknowledgement and suppression tests.

### GCS-009 — Battery indicator has no voltage fallback when percentage is unknown

**Original category/subcategory:** Flight safety & vehicle state / Battery telemetry  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** TopPanel.qml:55

**Original description:**

> When no battery monitor is configured, percentage may be unknown and the indicator displays '--' without falling back to voltage.

**Current status/validity:** Partially addressed. The battery popup now shows voltage/current independently when connected. The compact charge indicator still displays unknown percentage without substituting voltage. Voltage cannot be assumed available when no monitor is configured.

**Current source/symbols and evidence inspected:** ui/qml/panels/TopPanel.qml: batteryKnown, BatteryTile, batteryDetailsPopup; core/vehicle/Vehicle.cpp: battery fields.

**Dependencies:** GCS-017, GCS-018.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** QML tests for unknown percentage with valid/unknown voltage and disconnected telemetry.

### GCS-010 — STATUSTEXT chunks are not reassembled

**Original category/subcategory:** Flight safety & vehicle state / Operator awareness  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** 

**Original description:**

> Long STATUSTEXT messages split across multiple packets are never reconstructed into the original message.

**Current status/validity:** Still valid. Vehicle decodes one fixed text field per STATUSTEXT and emits it immediately; no id/chunk_seq reassembly is present in that UI path.

**Current source/symbols and evidence inspected:** core/vehicle/Vehicle.cpp: STATUSTEXT handling near line 569.

**Dependencies:** GCS-007, GCS-011.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Interleaved/truncated/missing/duplicate STATUSTEXT chunks with bounded reassembly tests.

### GCS-011 — Keyword-based severity classification can misclassify messages

**Original category/subcategory:** Flight safety & vehicle state / Operator awareness  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** FlyView.qml:998; classifyVehicleMessage

**Original description:**

> Messages such as Radio Failsafe, Battery Failsafe or Crash: Disarming can be shown as info; conversely benign text such as 'Fail-safe disabled' can be classified as an error.

**Current status/validity:** Still valid. FlyView still classifies vehicle text through keywords rather than the MAVLink severity carried on the wire.

**Current source/symbols and evidence inspected:** ui/qml/core/FlyView.qml: classifyVehicleMessage; core/vehicle/Vehicle.cpp: statusTextReceived.

**Dependencies:** GCS-007, GCS-010.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Messages with misleading keywords and actual severity; verify structured severity takes precedence.

### GCS-012 — Closing application while armed or controlling vehicle has no warning

**Original category/subcategory:** Flight safety & vehicle state / Shutdown safety  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Window close handling; MissionPlanModel.dirty

**Original description:**

> No close handler warns about an armed vehicle, active joystick override, in-progress mission upload or unsaved plan; operations can be dropped on exit.

**Current status/validity:** Still valid. The inspected application/FlyView closing paths do not provide an armed/control/transfer/dirty-plan close confirmation. The PiP onClosing handler only cleans PiP state.

**Current source/symbols and evidence inspected:** ui/qml/core/App.qml; ui/qml/core/FlyView.qml: PiP onClosing, OEM and mission popup close handlers; app/main.cpp: app.exec lifecycle.

**Dependencies:** GCS-001, GCS-016, GCS-028, GCS-068.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Close in armed, joystick-active, upload-active, unsaved-plan and recording states; verify cancel/continue behavior.

### GCS-013 — Joystick calibration centre is captured from current stick position

**Original category/subcategory:** Flight safety & vehicle state / Joystick calibration  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** JoystickController.cpp:357

**Original description:**

> Centre is taken from wherever the sticks are when Finish is pressed; off-centre sticks can create a permanent bias.

**Current status/validity:** Still valid. finishCalibration bounds the axis centre using its current value at completion. No explicit neutral-position stage was observed.

**Current source/symbols and evidence inspected:** app/JoystickController.cpp: finishCalibration, state.center assignment near line 357.

**Dependencies:** GCS-002, GCS-003.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Finish while sticks are displaced; validate deliberate centre capture and persisted calibration.

### GCS-014 — Joystick control loop depends on GUI thread responsiveness

**Original category/subcategory:** Flight safety & vehicle state / Joystick timing  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** JoystickController; Video.cpp stopRecording

**Original description:**

> The joystick send loop is affected by GUI-thread blocking; the report specifically links this to video stop/recording shutdown blocking the GUI for up to five seconds.

**Current status/validity:** Still valid. Joystick polling/transmission remains QTimer-driven on the application/GUI-owned controller. Video recording finalization can still block that thread; no control deadline isolation is demonstrated.

**Current source/symbols and evidence inspected:** app/JoystickController.cpp: pollTimer construction/timeout connection; core/video/Video.cpp: clearRecordingPipeline; app/main.cpp: AppBackend construction.

**Dependencies:** GCS-065, GCS-057, GCS-058, GCS-085.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Measure wire cadence during induced GUI stalls and recording stop; define fail-safe deadline behavior before threading changes.

### GCS-015 — Home position is taken from first GPS fix instead of HOME_POSITION

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Vehicle telemetry handling

**Original description:**

> Connecting mid-flight can therefore assign an incorrect home position.

**Current status/validity:** Still valid. First nonzero GPS fix still sets homeSet/home coordinates. Uploaded-home reset exists, but no HOME_POSITION receive handler was found in Vehicle.

**Current source/symbols and evidence inspected:** core/vehicle/Vehicle.cpp: GLOBAL_POSITION_INT first-fix home branch, setHomeLocation/reset-home integration.

**Dependencies:** GCS-016, GCS-018.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Different GPS-first-fix and HOME_POSITION inputs, home updates, equator/prime-meridian cases.

### GCS-016 — Home, flight time and flight distance do not reset

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Vehicle telemetry handling

**Original description:**

> These telemetry values reportedly persist across reconnect or disarm instead of resetting appropriately.

**Current status/validity:** Partially addressed. New freshness/flight-phase handling prevents treating stale telemetry as airborne and starts flight time on a confirmed phase. resetTelemetryState does not reset homeSet, flightStarted/flightTimer or accumulated travel data, so session reuse remains unresolved.

**Current source/symbols and evidence inspected:** core/vehicle/Vehicle.cpp: resetTelemetryState, updateTelemetryState, flightTime, first-fix home logic; tests/VehicleTest.cpp.

**Dependencies:** GCS-001, GCS-015.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Reconnect/new-flight/disarm scenarios with explicit expected home/time/distance policy.

### GCS-017 — Battery message sources overwrite one another

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Vehicle telemetry handling

**Original description:**

> SYS_STATUS, BATTERY_STATUS and multi-battery messages overwrite each other rather than being maintained coherently.

**Current status/validity:** Still valid. SYS_STATUS and BATTERY_STATUS both overwrite the same battery fields, with no inspected battery-ID arbitration or source ownership.

**Current source/symbols and evidence inspected:** core/vehicle/Vehicle.cpp: SYS_STATUS and BATTERY_STATUS cases.

**Dependencies:** GCS-018, GCS-009.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Interleave sources, battery IDs and unknown sentinel values; assert deterministic primary-battery policy.

### GCS-018 — SYS_STATUS from non-autopilot components is not filtered

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Vehicle telemetry handling

**Original description:**

> SYS_STATUS messages from other components can affect the vehicle telemetry state.

**Current status/validity:** Still valid. Vehicle's selected-source guard covers heartbeat/motion messages only. SYS_STATUS is outside that condition and can update battery state from another source.

**Current source/symbols and evidence inspected:** core/vehicle/Vehicle.cpp: motionMessage guard near line 285 and SYS_STATUS case.

**Dependencies:** GCS-017, GCS-040.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Foreign sysid/compid SYS_STATUS injection while a selected autopilot is active.

### GCS-019 — RC_CHANNELS from other components can corrupt RC display/calibration

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Mavlink.cpp RC_CHANNELS handling

**Original description:**

> RC_CHANNELS is not filtered by source system/component, so RC readings may come from a camera, gimbal or companion component.

**Current status/validity:** Still valid. The receive loop calls handleRcChannels for parsed messages; that helper decodes RC_CHANNELS/RAW without a selected-source filter. Parameter source filtering does not protect RC calibration inputs.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: readData receive dispatch, handleRcChannels.

**Dependencies:** GCS-018, GCS-040, GCS-112.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Interleave selected and foreign RC frames; assert live channels/calibration remain selected-source only.

### GCS-020 — Parameter values from other components can corrupt autopilot parameter cache

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Mavlink.cpp:1997

**Original description:**

> PARAM_VALUE sender is not checked; a gimbal/camera/companion response can be mixed into autopilot parameters and change expected param_count.

**Current status/validity:** Addressed in source and tests. ParameterManager receives system/component/generation and validates source before staging or updating the confirmed snapshot. Foreign-component records are covered by manager and routing tests.

**Current source/symbols and evidence inspected:** core/parameters/ParameterManager.cpp: receive/source checks; comms/mavlink/Mavlink.cpp: handleParameterValue; tests/ParameterManagerTest.cpp: invalidSourcesAndSentinelsDoNotAffectRetries; tests/MavlinkTest.cpp: parameterRoutingAndWritesPreserveSnapshots. Fresh manager suite: 74 passed.

**Dependencies:** GCS-001, GCS-112.  
**Recommended resolution workstream:** S1 regression.  
**Recommended verification:** Keep foreign-source, stale-generation and index/count sentinel regressions; rerun facade suite after isolation.

### GCS-021 — RADIO_STATUS packet-loss calculation is incorrect

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** RADIO_STATUS handling

**Original description:**

> Reported formula is rxerrors/(rxerrors+fixed), which does not correctly represent packet loss.

**Current status/validity:** Addressed in source and tests. RADIO_STATUS errors/fixed are now recent counter deltas explicitly labelled 'not packet loss'. A selected-autopilot sequence continuity estimate is separate, with reset/wrap/freshness handling.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: updateMavlinkQuality, linkQualityDiagnostics; tests/MavlinkTest.cpp: qualityFreshnessAndRadioCounters, qualitySequenceWindow; ui/qml/accessories/CommLinkDiagnostics.qml.

**Dependencies:** GCS-022, GCS-093.  
**Recommended resolution workstream:** S2 regression.  
**Recommended verification:** Run quality counter/wrap/reordering/freshness tests and constrained-radio trace comparison.

### GCS-022 — Link latency measurement stops above one second

**Original category/subcategory:** Flight safety & vehicle state / Vehicle telemetry  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Ping/latency matching

**Original description:**

> Only the most recent ping is matched, so latency is no longer measured once it exceeds one second.

**Current status/validity:** Addressed in source and tests. PING tracking now retains a bounded map of outstanding requests, matches seq/time/source and computes recent median latency, instead of accepting only the most recent one-second ping.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: sendLinkPing, handleLinkPing, pendingQualityPings; tests/MavlinkTest.cpp: qualityPingValidationAndMedian, qualityExpiryAndPingContinuity.

**Dependencies:** GCS-021.  
**Recommended resolution workstream:** S2 regression.  
**Recommended verification:** Replies above one second, out-of-order replies, stale/source-mismatched replies and bounded request eviction.

### GCS-023 — Mission file upload can drop the first real waypoint

**Original category/subcategory:** MAVLink & communications / Mission transfer  
**Original reported priority (unchanged):** Critical  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:3309-3320; MissionPlanConverter; MissionFileWriter

**Original description:**

> The 'write from file' button calls uploadMissionFile. Its parser removes the home row and renumbers from zero; ArduPilot treats item 0 as home, so the first real mission item (often TAKEOFF) can be swallowed. The newer MissionPlanConverter path keeps the home row. A hand-drawn plan without a home row can hit the same problem.

**Current status/validity:** Still valid. Legacy parseMissionFile still consumes the home row with continue and renumbers remaining items from zero. Canonical conversion preserves protocol rows, but legacy upload remains callable; aircraft behavior requires wire/SITL reproduction.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: parseMissionFile, uploadMissionFile; core/mission/MissionPlanConverter.cpp: exportVehicleMission; tests/MavlinkTest.cpp: uploadedMissionStoresHomeLocation.

**Dependencies:** GCS-024, GCS-006, GCS-090.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Compare exact uploaded sequences for files with/without home and canonical plans on each supported autopilot.

### GCS-024 — Multiple mission parsers implement inconsistent home-row handling

**Original category/subcategory:** MAVLink & communications / Mission transfer  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp; core/mission/MissionFileWriter.cpp; MissionPlanConverter

**Original description:**

> There are multiple mission parsers (including MissionFileWriter) with inconsistent home-row handling. The report describes two parsers with opposite behaviour and recommends unifying around one converter.

**Current status/validity:** Partially addressed. A canonical MissionPlanModel/Converter and conversion tests now exist and pass, but legacy parsing and separate generator/writer paths remain. Source still contains different home-row semantics.

**Current source/symbols and evidence inspected:** core/mission/MissionPlanModel.cpp; core/mission/MissionPlanConverter.cpp; core/mission/MissionFileWriter.cpp; comms/mavlink/Mavlink.cpp: parseMissionFile. Fresh model/converter: 11/10 passed.

**Dependencies:** GCS-023, GCS-028.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Round-trip a common fixture corpus through every reachable file/import/upload path and compare wire semantics.

### GCS-025 — Mission download aborts on duplicate sequence after retry

**Original category/subcategory:** MAVLink & communications / Mission transfer  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:2900

**Original description:**

> A late reply after timeout retry can be received twice; the download aborts as duplicate-sequence instead of ignoring a duplicate.

**Current status/validity:** Still valid. Mission download still terminates Failed with duplicate-sequence when an already received item is encountered; retry idempotence is not implemented in this branch.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: handleMissionMessage duplicate item branch near line 3294.

**Dependencies:** GCS-006, GCS-029.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Delay a response past retry, then deliver duplicate and out-of-order mission items; retain valid progress.

### GCS-026 — Automatic mission download can overwrite local plan

**Original category/subcategory:** MAVLink & communications / Mission transfer  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:1743

**Original description:**

> Auto-download on every connect can silently replace a local plan, even where the user has not explicitly requested replacement.

**Current status/validity:** Partially addressed. Connection still schedules automatic download. Converter dirty-plan protection prevents an unconditional dirty overwrite, but clean local plans can still be replaced and downloaded legacy/display state is updated independently.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: handleHeartbeat auto-download timer, downloadMissionFromVehicle, handleMissionMessage; core/mission/MissionPlanConverter.cpp: importVehicleMission dirty guard.

**Dependencies:** GCS-027, GCS-028, GCS-024.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Connect with clean local plan, dirty plan and empty plan; inspect both canonical and displayed state.

### GCS-027 — Auto-download reports failure when local plan has unsaved changes

**Original category/subcategory:** MAVLink & communications / Mission transfer  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:1743

**Original description:**

> When the local plan is dirty, the report says the download can succeed but then report model-import failure.

**Current status/validity:** Still valid. On download completion, failed import into a dirty canonical model still calls finishMissionTransfer(Failed,'model-import',...). Successful wire download is therefore conflated with local replacement refusal.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: handleMissionMessage completion/importVehicleMission branch.

**Dependencies:** GCS-026, GCS-028.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Complete a vehicle download while local edits exist; separate transfer success, staged result and replacement decision.

### GCS-028 — Successful upload incorrectly marks plan as saved to disk

**Original category/subcategory:** MAVLink & communications / Mission transfer  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:3151

**Original description:**

> Uploading to vehicle and saving a local file are treated as the same state, weakening unsaved-change protection.

**Current status/validity:** Still valid. finishMissionTransfer still calls markCurrentRevisionSaved after a successful canonical upload with matching revision. That operation is not a filesystem save.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: finishMissionTransfer near line 3540; core/mission/MissionPlanModel.cpp: markCurrentRevisionSaved/dirty state.

**Dependencies:** GCS-024, GCS-026, GCS-012.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Upload unsaved edits, close/reopen and verify disk-save and vehicle-upload states remain distinct.

### GCS-029 — COMMAND_LONG has no robust ACK/retry handling

**Original category/subcategory:** MAVLink & communications / Command reliability  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:2985-3009

**Original description:**

> COMMAND_LONG always uses confirmation=0 and is never retried. ACKs are matched by command ID or FIFO order rather than by sender system/component, so an ACK may be associated with the wrong sender. No general command reliability layer is present.

**Current status/validity:** Partially addressed. Connection/generation gating and bounded parameter transactions now exist, but generic sendCommandLong remains fire-and-forget. Motor/servo ACK association remains command/FIFO-based. Parameter transactions do not close the generic command reliability finding.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: sendCommandLong, handleMissionMessage COMMAND_ACK, pendingMotorTestAcks/pendingServoAcks; tests/MavlinkTest.cpp.

**Dependencies:** GCS-001, GCS-031, GCS-005.  
**Recommended resolution workstream:** S1.  
**Recommended verification:** Wrong-source/late/duplicate ACKs, IN_PROGRESS, timeout/retry and same-command concurrency tests.

### GCS-030 — Parameter cache updates before vehicle confirms write

**Original category/subcategory:** MAVLink & communications / Parameter protocol  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Mavlink.cpp:2247-2253

**Original description:**

> The UI can show a parameter value that the vehicle has rejected because the local cache is updated optimistically.

**Current status/validity:** Addressed in source and tests. Writes now route through ParameterManager::requestWrite; acceptance does not mutate the confirmed snapshot. Confirmed/rejected terminal outcomes follow matching authoritative replies, with retries, deadlines and generation cancellation.

**Current source/symbols and evidence inspected:** core/parameters/ParameterManager.cpp: requestWrite, receive, finishWrite; comms/mavlink/Mavlink.cpp: requestParameterWrite; tests/ParameterManagerTest.cpp: managedWriteEncoding, writeSourcesIdentityAndAuthoritativeRejection, writeRetryTimeoutAndTransportError. Fresh suite: 74 passed.

**Dependencies:** GCS-020, GCS-112.  
**Recommended resolution workstream:** S1 regression.  
**Recommended verification:** Retain confirmed-value integrity, refusal, mismatched replies, encoding and cancellation tests; facade/UI rerun after isolation.

### GCS-031 — Outgoing messages use component ID zero

**Original category/subcategory:** MAVLink & communications / MAVLink identity  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Outgoing MAVLink message construction

**Original description:**

> Most outgoing packets reportedly use component 0 (MAV_COMP_ID_ALL) rather than a valid GCS sender component; ping uses MISSIONPLANNER, creating inconsistency.

**Current status/validity:** Partially addressed. New PING/VPS use MAV_COMP_ID_MISSIONPLANNER, but parameter requests/sets, generic commands and stream requests still pack sender component 0. The identity inconsistency remains.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: requestList/requestRead/writeRequested connections, sendCommandLong, requestDataStreams, sendLinkPing.

**Dependencies:** GCS-029, GCS-036, GCS-041.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Wire capture every outbound message class and assert one valid configured GCS identity.

### GCS-032 — UDP link uses random local port rather than listening on configured port

**Original category/subcategory:** MAVLink & communications / UDP transport  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** comms/transport/udp/UdpLink.cpp

**Original description:**

> connectToHost makes the GCS a UDP client on a random local port; standard listener configurations (e.g. SITL, MAVProxy or companion computer sending to port 14550) may not work.

**Current status/validity:** Still valid. UdpLink connects a UDP socket to the peer; it does not bind the requested listener port. This is a client mode, not a general configured-port MAVLink listener. Whether both modes are required is a product decision.

**Current source/symbols and evidence inspected:** comms/transport/udp/UdpLink.cpp: connectResolvedAddress/connectLink; comms/mavlink/Mavlink.cpp: protocol connection.

**Dependencies:** GCS-033, GCS-040.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Send first datagram to configured port without prior outbound traffic; separately test client and listener modes.

### GCS-033 — UDP reports connected immediately

**Original category/subcategory:** MAVLink & communications / UDP transport  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** UdpLink.cpp

**Original description:**

> UDP transport reports connected before receiving a valid vehicle heartbeat or confirming a peer.

**Current status/validity:** Partially addressed. UdpLink's transport-connected signal is still immediate, but Mavlink's application connected state now requires a valid selected heartbeat and has an initial timeout. The original false vehicle-ready implication is mitigated; transport/UI wording should remain distinct.

**Current source/symbols and evidence inspected:** comms/transport/udp/UdpLink.cpp: connectResolvedAddress; comms/mavlink/Mavlink.cpp: beginConnection, handleHeartbeat; tests/MavlinkTest.cpp: connectionRequiresValidHeartbeat.

**Dependencies:** GCS-001, GCS-032.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** UDP transport without heartbeat, malformed/GCS heartbeat, valid heartbeat and initial timeout UI tests.

### GCS-034 — Serial port may remain open after USB unplug

**Original category/subcategory:** MAVLink & communications / Serial transport  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Serial transport; background port-list refresh

**Original description:**

> USB unplug raises disconnected but reportedly leaves the port open. Because the port list refreshes in the background, connecting to a freshly plugged port can fail until the next scan.

**Current status/validity:** Still valid. SerialLink's ResourceError handler emits disconnected without closing QSerialPort. Mavlink revokes readiness on that signal, but the error branch itself does not release the handle. Physical unplug/replug timing remains unverified.

**Current source/symbols and evidence inspected:** comms/transport/serial/SerialLink.cpp: errorOccurred, disconnectLink; comms/mavlink/Mavlink.cpp: replaceLink disconnected callback.

**Dependencies:** GCS-001, GCS-038.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** USB unplug/replug with handle-open assertions and immediate reconnect/port-list refresh.

### GCS-035 — MAVLink 1-only vehicles are not supported by outgoing path

**Original category/subcategory:** MAVLink & communications / Protocol compatibility  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** MAVLink send path

**Original description:**

> GCS always sends MAVLink 2 and does not adapt to MAVLink-1-only vehicles; telemetry may arrive while commands are silently dropped.

**Current status/validity:** Requires investigation. Outgoing pack paths remain MAVLink-2-oriented; no explicit per-peer MAVLink-1 negotiation was found. This is a compatibility gap conditional on supporting MAVLink-1-only firmware, not a reproduced failure in this audit.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: sendMavlinkMessage, message pack calls, shared channel state; comms/mavlink/Mavlink.h.

**Dependencies:** GCS-031, GCS-041, GCS-107.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** MAVLink-1-only peer and capability/protocol negotiation capture; agree supported protocol versions.

### GCS-036 — MAVLink signing is unsupported

**Original category/subcategory:** MAVLink & communications / Protocol compatibility  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** MAVLink protocol implementation

**Original description:**

> Vehicles configured to require MAVLink signing may reject all GCS commands because signing is not implemented.

**Current status/validity:** Requires investigation. No signing/key/replay-policy API was located in the inspected Mavlink facade. Excluding SETUP_SIGNING from telemetry logs is not signing support. Treat as a security capability gap until threat model/support requirements are defined.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: shouldLogTelemetryMessage; comms/mavlink/Mavlink.h; transport interfaces.

**Dependencies:** GCS-031, GCS-035, GCS-041.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Signed/unsigned/replayed/invalid-signature peers against an explicit accepted-message policy.

### GCS-037 — Outbound write queues can grow without bound on slow links

**Original category/subcategory:** MAVLink & communications / Backpressure  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** QSerialPort/QTcpSocket write paths

**Original description:**

> Serial/TCP writes have no backpressure; joystick traffic, pings and parameter bursts can accumulate, delaying later RTL/LAND commands.

**Current status/validity:** Partially addressed. Parameter writes now have a bounded queue and paced retries; configuration application submits one pending transaction at a time. Generic socket/serial write queues still lack a unified high-water/backpressure policy.

**Current source/symbols and evidence inspected:** core/parameters/ParameterManager.h: WriteConfig/maxPending; tests/ParameterManagerTest.cpp: boundedWriteQueueAndQueueDeadline; comms/transport/tcp/TcpLink.cpp and serial/SerialLink.cpp: writeData; ParameterConfigurationController.cpp: submitNext.

**Dependencies:** GCS-039, GCS-086, GCS-038.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Blocked/slow transport soak, queue-size limits, priority and drop/error reporting.

### GCS-038 — Transport write errors are ignored

**Original category/subcategory:** MAVLink & communications / Backpressure  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Transport write paths

**Original description:**

> Write errors are reportedly ignored across the outbound communication paths.

**Current status/validity:** Partially addressed. sendMavlinkMessage now returns whether the entire frame was accepted and parameter writes propagate transport rejection. Generic command/mission callers still discard this return and there is no inspected asynchronous write-failure handling across all paths.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: sendMavlinkMessage, writeRequested connection, sendCommandLong, sendMissionClearAll; tests/ParameterManagerTest.cpp: writeRetryTimeoutAndTransportError.

**Dependencies:** GCS-029, GCS-037.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Short write, -1 write and later transport error for commands, missions and parameter transactions.

### GCS-039 — Telemetry stream requests can saturate 57600-baud radios

**Original category/subcategory:** MAVLink & communications / Telemetry bandwidth  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Telemetry stream-rate configuration

**Original description:**

> Every stream is requested at 10 Hz plus RC channels at 50 Hz, which the report says can saturate a 57600-baud link.

**Current status/validity:** Still valid. requestDataStreams still requests all streams at 10 Hz, RC at 50 Hz and a 20,000-us RC interval, without adapting to configured serial bandwidth. Actual saturation depends on firmware message mix.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: requestDataStreams; comms/transport/serial/SerialLink.cpp: baud configuration.

**Dependencies:** GCS-037, GCS-014, GCS-021.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** 57600-baud bandwidth budget and packet/cadence tests under representative telemetry and mission loads.

### GCS-040 — Vehicle manager is effectively single-vehicle

**Original category/subcategory:** MAVLink & communications / Multi-vehicle support  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** VehicleManager; MAVLINK_COMM_0

**Original description:**

> VehicleManager has a vehicle map but reportedly always uses one instance; parsing uses global MAVLink channel MAVLINK_COMM_0.

**Current status/validity:** Requires investigation. VehicleManager constructs exactly one active Vehicle. This is an explicit current capability limit; whether it violates a multi-vehicle requirement needs confirmation. Source selection must still protect the single vehicle from foreign telemetry.

**Current source/symbols and evidence inspected:** core/vehicle/VehicleManager.cpp: constructor/activeVehicle; tests/VehicleManagerTest.cpp.

**Dependencies:** GCS-018, GCS-019, GCS-041, GCS-107.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** Clarify supported vehicle count; inject two systems on one transport and test active-source boundaries.

### GCS-041 — Global MAVLink channel is shared by parsing and tests

**Original category/subcategory:** MAVLink & communications / Multi-vehicle support  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** MAVLink parsing/test infrastructure

**Original description:**

> Use of global MAVLINK_COMM_0 creates shared state between parsing paths and tests.

**Current status/validity:** Still valid. Production parsing and several test packet helpers still use MAVLINK_COMM_0. Per-instance mavlinkStatus does not prove isolation of the generated library's channel-local parse/transmit state.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: readData/replaceLink; tests/MavlinkTest.cpp: readCommands/readMavlinkMessages.

**Dependencies:** GCS-040, GCS-035, GCS-036.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Interleave fragmented frames across two instances/channels and run parallel protocol fixtures.

### GCS-042 — Mission model permits 65,536 items although MAVLink count is 16-bit

**Original category/subcategory:** MAVLink & communications / Mission protocol limits  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** MissionPlanModel.cpp:156; Mavlink.cpp:2613, 2622

**Original description:**

> Casting 65,536 to uint16_t becomes zero, causing MISSION_COUNT and item requests to fail.

**Current status/validity:** Still valid. MissionPlanModel permits max(quint16)+1 items (65,536), while sendMissionCount casts size to uint16_t and sendMissionItem compares against that cast. This is a source-demonstrable boundary mismatch.

**Current source/symbols and evidence inspected:** core/mission/MissionPlanModel.cpp: candidate size validation near line 156; core/mission/MissionPlanConverter.cpp: exportVehicleMission; comms/mavlink/Mavlink.cpp: sendMissionCount/sendMissionItem.

**Dependencies:** GCS-024, GCS-006.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Boundary cases 65,534/65,535/65,536 with exact serialized count and transactional model validation.

### GCS-043 — Mission generator accepts unsafe or implausible values

**Original category/subcategory:** Mission planning & terrain / Input validation  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** core/mission/MissionFileWriter.cpp:152

**Original description:**

> Terrain clearance 0 and takeoff altitude 0 are accepted; cruise speed 1000 m/s is accepted; delay and hover times are not validated, so negative values can become negative LOITER_TIME. Values are not checked against the selected drone model's limits.

**Current status/validity:** Still valid. Generator still allows zero takeoff/clearance, any positive cruise speed, and lacks comprehensive finite/vehicle-limit/delay validation in its shown validation branch. Existing model constants are not enforcement of an aircraft envelope.

**Current source/symbols and evidence inspected:** core/mission/MissionFileWriter.cpp: generateMission validation near line 152, MissionParams assignment.

**Dependencies:** GCS-044, GCS-045, GCS-107.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** NaN/infinity, zero/negative delays/altitudes and per-aircraft speed/altitude boundaries; no automatic flight approval.

### GCS-044 — Terrain is checked/prefetched only for takeoff/landing bounding box

**Original category/subcategory:** Mission planning & terrain / Planner terrain coverage  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** integrations/SkyRouteX_New/src/path_planner.cpp:539; core/mission/MissionFileWriter.cpp:189

**Original description:**

> The corridor search can move up to 3.5 km sideways from the straight line, but terrain availability is checked/prefetched only for the takeoff/landing bounding box. If a detour crosses into an SRTM tile not on disk, planning returns no path and does not trigger a download; the user sees a generic 'Could not plan outbound path'.

**Current status/validity:** Still valid. Prefetch/availability use only min/max endpoint bounds, whereas the linked SkyRouteX_New planner searches a lateral corridor up to 3.5 km. The inspected paths have different coverage envelopes.

**Current source/symbols and evidence inspected:** core/mission/MissionFileWriter.cpp: bounds/prefetch near line 185; integrations/SkyRouteX_New/src/path_planner.cpp: corridorHalfWidthM near line 539; CMakeLists.txt: linked planner source.

**Dependencies:** GCS-043, GCS-047, GCS-049.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Route near tile boundary where a lateral candidate leaves the endpoint box; require all sampled terrain or fail clearly.

### GCS-045 — Planner models bare terrain only

**Original category/subcategory:** Mission planning & terrain / Airspace safety  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** SkyRouteX path planner; SRTM terrain model

**Original description:**

> The planner models bare terrain only: approximately 20 m clearance radius over 30 m SRTM terrain data, without buildings, power lines, no-fly zones or geofences. Detours can therefore enter airspace not represented to the operator.

**Current status/validity:** Requires investigation. The planner uses SRTM elevation and clearance, not an inspected building/vegetation/wire/geofence dataset. This is a flight-planning capability limitation, not proof that a specific generated route collides.

**Current source/symbols and evidence inspected:** integrations/SkyRouteX_New/src/path_planner.cpp; core/mission/MissionFileWriter.cpp: elevationFn; core/terrain/TerrainProvider.cpp.

**Dependencies:** GCS-043, GCS-044, GCS-050.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Define planning safety scope; obstacle/geofence datasets and adversarial routes before relying on generated missions.

### GCS-046 — Missing SRTM elevation values are converted to sea level in 3D view

**Original category/subcategory:** Mission planning & terrain / Terrain data integrity  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** integrations/Offline-3DMaps/src/elevationprovider.cpp:256

**Original description:**

> elevationprovider converts missing-data values to 0.0. SRTM NoData gaps, particularly in steep mountainous areas, can therefore render as holes down at sea level.

**Current status/validity:** Still valid. Offline-3DMaps ElevationProvider still maps the SRTM void sentinel to 0.0 before interpolation. The mission generator now uses NaN for unavailable TerrainProvider data, but that does not fix the separate 3D path.

**Current source/symbols and evidence inspected:** integrations/Offline-3DMaps/src/elevationprovider.cpp: sample lambda near line 254; core/mission/MissionFileWriter.cpp: elevationFn.

**Dependencies:** GCS-047, GCS-057.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Void-containing HGT fixture, interpolation and 3D unknown-height presentation; distinguish rendering from mission elevations.

### GCS-047 — Missing terrain data can be retried forever by MAVLink terrain server

**Original category/subcategory:** Mission planning & terrain / Terrain protocol  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** MAVLink terrain server; elevationprovider.cpp:256

**Original description:**

> In the MAVLink terrain server, a missing-data gap causes the terrain request to be queued and retried indefinitely, so ArduPilot may never complete terrain data for that area.

**Current status/validity:** Partially addressed. Missing terrain blocks remain queued and retried without an inspected attempt/deadline budget while connected. New lifecycle teardown clears the queue on disconnect, so retries no longer survive that boundary indefinitely.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: handleTerrainRequest, queuePendingTerrainBlock, retryPendingTerrainRequests, clearPendingTerrainRequests/endConnection.

**Dependencies:** GCS-044, GCS-051, GCS-086.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Permanently absent/void terrain with fake clock, bounded retries, terminal diagnostics and disconnect cancellation.

### GCS-048 — Same takeoff and landing point produces generic planning failure

**Original category/subcategory:** Mission planning & terrain / Planner edge cases  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Mission planner

**Original description:**

> The same takeoff and landing point falls through to the generic 'Could not plan' error rather than a specific validation message or supported zero-distance handling.

**Current status/validity:** Still valid. Planner returns no path for negligible endpoint separation and generator reports generic outbound/return planning failure. No distinct same-coordinate validation was found.

**Current source/symbols and evidence inspected:** integrations/SkyRouteX_New/src/path_planner.cpp: baseLen guard; core/mission/MissionFileWriter.cpp: generic plan failure.

**Dependencies:** GCS-043.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Identical and near-identical endpoints with a precise, actionable validation result.

### GCS-049 — Routes crossing the antimeridian fail

**Original category/subcategory:** Mission planning & terrain / Geospatial edge cases  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Mission builder; terrain sampling

**Original description:**

> Mission builder routes crossing the 180° meridian fall through to the generic planning error; terrain sampling across the antimeridian also creates invalid longitudes/tile names (tracked separately in GCS-054).

**Current status/validity:** Requires investigation. Endpoint min/max longitude and local east projection use raw longitude differences. These support the reported antimeridian concern, but no current antimeridian fixture was executed to establish the exact failure or supported route limits.

**Current source/symbols and evidence inspected:** core/mission/MissionFileWriter.cpp: min/max bounds; integrations/SkyRouteX_New/src/path_planner.cpp: local projection; core/terrain/TerrainProvider.cpp: bounds handling.

**Dependencies:** GCS-044, GCS-050, GCS-051.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** 179.9 to -179.9 routes in both directions with bounded tile requests and geodesic reference checks.

### GCS-050 — Flat-earth local projection loses accuracy on long routes

**Original category/subcategory:** Mission planning & terrain / Geospatial accuracy  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** SkyRouteX path planner

**Original description:**

> The planner uses a flat-earth local projection, so positional error increases on long routes.

**Current status/validity:** Requires investigation. The active planner still uses an origin-based local projection with a cosine floor. Accuracy impact depends on latitude and route length; the original risk has no quantified current error evidence.

**Current source/symbols and evidence inspected:** integrations/SkyRouteX_New/src/path_planner.cpp: localToGeo/geoToLocal near lines 319-331.

**Dependencies:** GCS-043, GCS-049.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Numerical error budget against geodesic reference over supported distances/latitudes, then enforce domain limits.

### GCS-051 — Longitude ±180° creates invalid or unavailable terrain tile requests

**Original category/subcategory:** Mission planning & terrain / Geospatial edge cases  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Terrain tile naming and request queue

**Original description:**

> Terrain sampling can produce longitudes above +180°; the tile name becomes invalid and the request can remain queued. Longitude 180 exactly also looks for a tile that does not exist.

**Current status/validity:** Requires investigation. Elevation tile naming floors longitude, and terrain block offsets can move beyond the dateline. No inspected explicit normalization closes the +180 edge, but the complete request/failure path requires a targeted fixture.

**Current source/symbols and evidence inspected:** integrations/Offline-3DMaps/src/elevationprovider.cpp: tileName; comms/mavlink/Mavlink.cpp: buildTerrainDataBlock; core/terrain/TerrainProvider.cpp: tile lookup.

**Dependencies:** GCS-047, GCS-049.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Test -180/+180 and offset samples beyond each bound; assert canonical naming and bounded failure.

### GCS-052 — No offline fallback map style on first run

**Original category/subcategory:** Mission planning & terrain / Map availability  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** integrations/map_provider/map/map.html

**Original description:**

> A field laptop that has never loaded the MapTiler style can show a blank map without internet. A revoked or over-quota API key can produce the same result, leaving map-based planning unavailable; no offline fallback style is reported.

**Current status/validity:** Still valid. The initial MapLibre styles remain remote MapTiler hybrid/streets styles. Disk caching/offline mode do not provide a bundled first-run fallback style when the cache is empty.

**Current source/symbols and evidence inspected:** integrations/map_provider/map/map.html: mapStyles/new map; core/map/TileCacheManager.cpp: offlineMode fetch branch.

**Dependencies:** GCS-053, GCS-054, GCS-076.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Empty-cache first launch without network, error UI and deliberate retry; verify usable fallback requirements.

### GCS-053 — Map renderer crash is not recovered

**Original category/subcategory:** Mission planning & terrain / Map reliability  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Qt WebEngine renderProcessTerminated handling; map startup readiness

**Original description:**

> There is no renderProcessTerminated recovery. If Chromium's renderer exits (e.g. GPU driver reset or out-of-memory), the map remains blank for the rest of the session. Readiness checks reportedly run only during startup.

**Current status/validity:** Partially addressed. FlyMap now invalidates document/bridge state on renderProcessTerminated and exposes operator-driven retryMap/reload. MapDispatch has generations, deadlines and replay tests (25 freshly passed). An actual renderer kill/recovery was not executed.

**Current source/symbols and evidence inspected:** ui/qml/map/FlyMap.qml: onRenderProcessTerminated, failMapReadiness, retryMap; ui/qml/map/MapDispatch.js; tests/qml/tst_MapDispatch.qml.

**Dependencies:** GCS-052, GCS-054, GCS-055.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Terminate WebEngine renderer, verify failed/disabled state, explicit retry and replay without stale command acceptance.

### GCS-054 — Map tile requests have no transfer timeout

**Original category/subcategory:** Mission planning & terrain / Map networking  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** integrations/map_provider/map/map.html

**Original description:**

> Map tile requests have no transfer timeout, so weak or captive-portal internet can leave requests hanging.

**Current status/validity:** Still valid. TileCacheManager fetch uses QNetworkAccessManager::get and finished handling without an inspected transfer timeout. Bridge dispatch deadlines protect UI command readiness, not stalled resource transfers.

**Current source/symbols and evidence inspected:** core/map/TileCacheManager.cpp: fetch; core/map/TileCacheSchemeHandler.cpp; ui/qml/map/MapDispatch.js: dispatch deadlines.

**Dependencies:** GCS-052, GCS-053, GCS-058.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Hung HTTP server and slow transfers; bounded completion, cancellation and accurate map readiness.

### GCS-055 — Map bridge uses hand-built runJavaScript command/ack protocol

**Original category/subcategory:** Mission planning & terrain / Map integration  
**Original reported priority (unchanged):** Architecture risk  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** ui/qml/map/MapDispatch.js

**Original description:**

> MapDispatch.js uses a hand-built command/ack protocol over runJavaScript; the report suggests QWebChannel to reduce custom bridge complexity.

**Current status/validity:** Architecture recommendation. The custom runJavaScript command/ack bridge remains, but now has explicit document/style generations, bounded dispatch and timeout/replay tests. Its existence is not a confirmed defect or sufficient reason to replace it.

**Current source/symbols and evidence inspected:** ui/qml/map/MapDispatch.js; ui/qml/map/FlyMap.qml; tests/qml/tst_MapDispatch.qml: fresh 25 passed.

**Dependencies:** GCS-053, GCS-093.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** Retain protocol invariants and measure complexity/reliability before deciding whether QWebChannel migration is warranted.

### GCS-056 — HTTP(S) request interceptor caches all default-profile web requests as tiles

**Original category/subcategory:** Mission planning & terrain / Map cache  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** TileCacheRequestInterceptor.cpp

**Original description:**

> The interceptor redirects every HTTP(S) request from the default web profile through the tile cache, including non-tile web content, with no expiry.

**Current status/validity:** Still valid. The interceptor redirects every HTTP(S) request on the default profile to the bonv cache scheme, with no tile-host/resource allowlist. Cache size limiting exists, but no inspected expiry/revalidation policy addresses non-tile resources.

**Current source/symbols and evidence inspected:** core/map/TileCacheRequestInterceptor.cpp: interceptRequest; app/main.cpp: defaultProfile interceptor; core/map/TileCacheManager.cpp: readCached/fetch/storeCached.

**Dependencies:** GCS-054, GCS-076, GCS-086.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Load unrelated/default-profile resources and changing authenticated/style content; verify scope, expiry and privacy policy.

### GCS-057 — SRTM terrain tiles load synchronously on GUI thread and cache has no size limit

**Original category/subcategory:** Mission planning & terrain / Terrain resource handling  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** core/terrain/TerrainProvider.cpp:437

**Original description:**

> SRTM terrain tiles are reported to be about 25 MB each, load synchronously on the GUI thread and are cached without a size limit.

**Current status/validity:** Still valid. TerrainProvider still reads/decodes an entire HGT tile synchronously and inserts it into an in-memory tile cache without an inspected memory limit. A 3601-square int16 tile alone is approximately 24.7 MiB; GUI latency/memory growth need profiling.

**Current source/symbols and evidence inspected:** core/terrain/TerrainProvider.cpp: load tile branch near line 437, tileCache; comms/mavlink/Mavlink.cpp: synchronous terrain block sampling.

**Dependencies:** GCS-014, GCS-047, GCS-086.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Cold-cache large-tile profiling, multi-tile memory budget and control-loop cadence under sampling load.

### GCS-058 — Every stored tile triggers full directory scan on GUI thread

**Original category/subcategory:** Mission planning & terrain / Tile cache performance  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** TileCacheManager.cpp:165

**Original description:**

> TileCacheManager reportedly scans the full directory after each stored tile, slowing as cache grows.

**Current status/validity:** Still valid. storeCached still calls refreshCacheSize after each successful tile write; refreshCacheSize walks cached tile files. No inspected incremental accounting/worker boundary removes that scan cost.

**Current source/symbols and evidence inspected:** core/map/TileCacheManager.cpp: storeCached, refreshCacheSize, enforceCacheLimit.

**Dependencies:** GCS-014, GCS-054, GCS-086.  
**Recommended resolution workstream:** S3.  
**Recommended verification:** Large existing cache plus prefetch; measure per-tile cost, UI latency and eviction correctness.

### GCS-059 — Frozen video frame can appear live

**Original category/subcategory:** Video & recording / Stream health  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** core/video/Video.cpp; default RTSP timeout

**Original description:**

> The video remains marked 'streaming' until GStreamer reports an error; default RTSP timeout is 60 seconds. If packets simply stop (common with UDP/radio dropout), the last frame can look live for up to a minute without warning.

**Current status/validity:** Still valid. The default stream timeout is still 60,000 ms and no independent last-frame freshness watchdog was found. A retained frame/streaming flag can therefore remain visually stale until transport/pipeline failure detection.

**Current source/symbols and evidence inspected:** core/video/Video.h: connectionTimeoutMsValue; core/video/Video.cpp: frame delivery/setStreaming; ui/qml/video/Video.qml.

**Dependencies:** GCS-060, GCS-061.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Stop frame delivery while retaining RTSP connection; verify stale age/indicator and recovery policy.

### GCS-060 — Video feed has no automatic reconnect

**Original category/subcategory:** Video & recording / Reconnect  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** core/video/Video.cpp

**Original description:**

> There is no automatic reconnect. A short dropout (even around one second) can stop the feed and recording until an operator presses Start again.

**Current status/validity:** Still valid. The inspected bus error/EOS path stops the video pipeline and does not schedule reconnect/backoff. This is a capability gap whose desired reconnect policy must be agreed.

**Current source/symbols and evidence inspected:** core/video/Video.cpp: pollBus, stop, start; ui/qml/video/Video.qml.

**Dependencies:** GCS-059, GCS-061.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** RTSP restart, intermittent loss and repeated failure with bounded backoff and explicit manual stop.

### GCS-061 — Video stop on stream error also ends recording

**Original category/subcategory:** Video & recording / Recording continuity  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** core/video/Video.cpp:420-428

**Original description:**

> When an error or end-of-stream arrives, pollBus calls stop(); stop() also ends recording, so a brief stream dropout can terminate the recording.

**Current status/validity:** Still valid. Stream error/EOS calls Video::stop, which calls stopRecording. Stopping recording may be the correct policy, but the original coupling remains and requires explicit UI/outcome semantics.

**Current source/symbols and evidence inspected:** core/video/Video.cpp: pollBus, stop, stopRecording.

**Dependencies:** GCS-059, GCS-060, GCS-065.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Stream loss while recording, inspect final file and user-visible recording outcome; distinguish interruption from requested stop.

### GCS-062 — Video view crops image edges

**Original category/subcategory:** Video & recording / Rendering / performance  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** core/video/VideoItem.cpp:60

**Original description:**

> VideoItem scales with KeepAspectRatioByExpanding, cropping the image edges and potentially cutting off targets or camera overlays near the frame boundary.

**Current status/validity:** Still valid. VideoItem paint still uses KeepAspectRatioByExpanding, which deliberately crops edges for mismatched aspect ratios. Whether this is a defect depends on the required flight-camera viewing policy.

**Current source/symbols and evidence inspected:** core/video/VideoItem.cpp: paint.

**Dependencies:** GCS-063.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Aspect-ratio pattern with marked edges; verify fit/crop choice and explicit user visibility.

### GCS-063 — Video frames are software-scaled on GUI thread

**Original category/subcategory:** Video & recording / Rendering / performance  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** core/video/VideoItem.cpp:60

**Original description:**

> Every frame is software-scaled with smooth filtering on the GUI thread, which can consume substantial CPU at 1080p.

**Current status/validity:** Requires investigation. VideoItem still uses QPainter smooth image scaling for each paint. This is a performance risk supported by the source, but current frame-rate/CPU/latency were not benchmarked; the rendering thread should be measured rather than assumed.

**Current source/symbols and evidence inspected:** core/video/VideoItem.cpp: paint/SmoothPixmapTransform/drawImage; core/video/Video.cpp: image copy/frame delivery.

**Dependencies:** GCS-014, GCS-064.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** 1080p/high-rate profiling on target hardware including Qt render-thread attribution, dropped frames and control cadence.

### GCS-064 — Video recording has a thread race and possible use-after-free

**Original category/subcategory:** Video & recording / Thread safety  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** core/video/Video.cpp:650, 670

**Original description:**

> The GStreamer thread reads recordingAppSrc and recordingState without a lock while stopRecording on the GUI thread frees them, creating a possible use-after-free/data race.

**Current status/validity:** Still valid. The appsink callback calls pushRecordingFrame, which reads recordingState/recordingAppSrc. clearRecordingPipeline unrefs/nulls the same appsrc from the owner path without an inspected shared recording lock/reference handoff. The race remains visible; a UAF crash was not reproduced.

**Current source/symbols and evidence inspected:** core/video/Video.cpp: appsink callback near line 882, pushRecordingFrame, clearRecordingPipeline; core/video/Video.h: frame mutex covers frameValue only.

**Dependencies:** GCS-065, GCS-069, GCS-093.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Repeated start/stop/error/destruction under incoming frames with sanitizer/stress instrumentation.

### GCS-065 — Stopping recording can block GUI for up to five seconds

**Original category/subcategory:** Video & recording / Responsiveness  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** core/video/Video.cpp stopRecording

**Original description:**

> Stopping recording can wait up to five seconds for the recording pipeline to close, blocking the GUI exactly when the stream/link may already be struggling.

**Current status/validity:** Still valid. clearRecordingPipeline still waits synchronously on gst_bus_timed_pop_filtered for up to 5*GST_SECOND. The owner/UI stop path can therefore block control/UI work.

**Current source/symbols and evidence inspected:** core/video/Video.cpp: stopRecording, clearRecordingPipeline near line 638.

**Dependencies:** GCS-014, GCS-064.  
**Recommended resolution workstream:** S4 (S1 mitigation first).  
**Recommended verification:** Withhold EOS/error, measure GUI/control latency and verify asynchronous bounded finalization.

### GCS-066 — Recording timestamps assume fixed 30 fps

**Original category/subcategory:** Video & recording / Recording correctness  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Video recording pipeline

**Original description:**

> Frames are timestamped at fixed 30 fps regardless of actual frame rate.

**Current status/validity:** Still valid. Recording caps and buffer PTS/DTS still assume 30/1 and frameIndex*(GST_SECOND/30), rather than observed capture timing.

**Current source/symbols and evidence inspected:** core/video/Video.cpp: recordingPipelineDescription, pushRecordingFrame.

**Dependencies:** GCS-059, GCS-061, GCS-069.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Variable/dropped input frame rates; compare recording duration/timestamps with monotonic elapsed time.

### GCS-067 — Video pipeline is Windows-specific

**Original category/subcategory:** Video & recording / Platform compatibility  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** GStreamer video pipeline

**Original description:**

> The video pipeline hard-codes the d3d11download element, making this implementation Windows-specific and unsuitable for other platform paths without adaptation.

**Current status/validity:** Requires investigation. The linked video pipeline still contains d3d11download and Windows-specific decoder ranking. This is confirmed platform coupling, but becomes a runtime defect only for a declared supported non-Windows build.

**Current source/symbols and evidence inspected:** core/video/Video.cpp: pipelineDescription, GStreamer initialization ranks; CMakeLists.txt: platform dependencies.

**Dependencies:** GCS-099, GCS-063.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Document supported platforms; configure and exercise an approved non-Windows pipeline if required.

### GCS-068 — ScreenRecorder teardown may leave AVI without index

**Original category/subcategory:** Video & recording / Crash recovery  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** app/ScreenRecorder.cpp:97

**Original description:**

> ScreenRecorder destructor tears down the pipeline without finishing the file. Closing during screen recording can leave an AVI without an index and unable to play.

**Current status/validity:** Partially addressed. Normal ScreenRecorder stop has asynchronous EOS/finalization and a timeout, but the destructor still stops timers and clears the pipeline without EOS. Shutdown finalization remains unproven; this is no longer accurately described as a purely hand-written AVI-index implementation.

**Current source/symbols and evidence inspected:** app/ScreenRecorder.cpp: destructor, stop/requestPipelineFinalize, forcePipelineFinalize, clearPipeline; app/AppBackend.cpp: stopScreenRecording.

**Dependencies:** GCS-012, GCS-069, GCS-070.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Exit while recording and during finalization; validate AVI index/playability and bounded shutdown.

### GCS-069 — AVI recordings may be unrecoverable after crash or power loss

**Original category/subcategory:** Video & recording / Crash recovery  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** ScreenRecorder and Video recorder

**Original description:**

> Both recorders use AVI. A crash or power loss mid-recording may make the whole file unrecoverable; the report suggests a crash-resilient container such as Matroska or fragmented MP4.

**Current status/validity:** Requires investigation. Both recording paths still use avimux. Crash/power-loss recovery depends on mux/file behavior and cannot be established by the container name alone. No destructive crash/file-recovery experiment was performed.

**Current source/symbols and evidence inspected:** app/ScreenRecorder.cpp: pipeline description/clearPipeline; core/video/Video.cpp: recordingPipelineDescription.

**Dependencies:** GCS-064, GCS-065, GCS-068.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Kill isolated recorder processes at multiple write stages and validate recoverability with media tools.

### GCS-070 — Minimised-window screen recording can capture black frames

**Original category/subcategory:** Video & recording / Screen capture  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** ScreenRecorder grabWindow path

**Original description:**

> When the application window is minimised, grabWindow returns black frames and those frames are recorded.

**Current status/validity:** Obsolete original mechanism; replacement needs validation. ScreenRecorder now grabs the full desktop screen (window id 0 and screen geometry), rather than the application-window surface. The original minimized-window-black explanation does not directly apply. Minimize/lock/remote-session capture and privacy scope remain unverified.

**Current source/symbols and evidence inspected:** app/ScreenRecorder.cpp: captureWindowFrame near line 311; captureFrame.

**Dependencies:** GCS-068, GCS-069.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Manual minimize/lock/RDP/multi-monitor recording; verify advertised capture scope and frame quality.

### GCS-071 — Login uses one shared password without username

**Original category/subcategory:** Security, login & licensing / Authentication  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** LoginWindow.qml:197

**Original description:**

> Login is a shared password with no username, attempt limit, lockout or record of who logged in.

**Current status/validity:** Still valid. Login still delegates a single password check without an inspected username/account, rate-limit or lockout mechanism. Desired account/security guarantees require a product decision.

**Current source/symbols and evidence inspected:** ui/qml/activation/LoginWindow.qml; app/AppBackend.cpp: verifyLoginPassword.

**Dependencies:** GCS-072, GCS-073, GCS-077.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Authentication threat model, repeated failures, session expiry and account separation tests.

### GCS-072 — Login password is hard-coded

**Original category/subcategory:** Security, login & licensing / Authentication  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** app/AppBackend.cpp:11

**Original description:**

> The login password '098765' is hard-coded in AppBackend.cpp.

**Current status/validity:** Still valid. verifyLoginPassword still compares against an application-embedded LOGIN_PASSWORD constant. No secrets were extracted or changed during the audit.

**Current source/symbols and evidence inspected:** app/AppBackend.cpp: LOGIN_PASSWORD and verifyLoginPassword.

**Dependencies:** GCS-071, GCS-073.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Credential provisioning/rotation design and release-binary secret scan; do not treat obfuscation as credential protection.

### GCS-073 — Licence key generation can be reproduced from shipped binary

**Original category/subcategory:** Security, login & licensing / Licensing  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** app/KeyManager.cpp:19

**Original description:**

> The reported licence scheme is a 48-bit truncated SHA-256 of machine ID + expiry + a salt shipped in the binary. Someone with the binary can derive/generate keys.

**Current status/validity:** Still valid. Key validation still derives an expected truncated SHA-256-based signature using data/salt shipped with the client. This is a source-visible licence authenticity risk, not an independently executed key forgery.

**Current source/symbols and evidence inspected:** app/KeyManager.cpp: generateSignature, validateKeyDetailed.

**Dependencies:** GCS-071, GCS-072, GCS-075.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Review public-key signature/provisioning design; negative tamper tests without publishing secret material.

### GCS-074 — Expired licence key is deleted

**Original category/subcategory:** Security, login & licensing / Licensing  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** KeyManager.cpp:428-431

**Original description:**

> KeyManager::isActivated clears stored key when expired, so user loses the key and cannot inspect which key expired.

**Current status/validity:** Still valid. isActivated still calls clearLocalActivation for Expired; that removes the license QSettings subtree. Expiry is not merely a read-only validation result.

**Current source/symbols and evidence inspected:** app/KeyManager.cpp: isActivated, clearLocalActivation.

**Dependencies:** GCS-075, GCS-091.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Isolated settings with expired licence; verify retention/renewal policy and no destructive check side effect.

### GCS-075 — Licence expiry depends on local system clock

**Original category/subcategory:** Security, login & licensing / Licensing  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** KeyManager expiry validation

**Original description:**

> Expiry uses the local system date. Setting the PC clock backwards can extend a licence; a wrong clock (e.g. dead CMOS battery) can make a valid key appear expired. The report also notes expired keys are deleted (GCS-077).

**Current status/validity:** Still valid. validateKeyDetailed still compares expiry with QDate::currentDate. Clock rollback/tamper policy remains unresolved. The original description's cross-reference to GCS-077 appears inconsistent; the key-deletion finding is GCS-074, retained separately.

**Current source/symbols and evidence inspected:** app/KeyManager.cpp: validateKeyDetailed/isActivated.

**Dependencies:** GCS-073, GCS-074.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Isolated clock-provider tests for rollback, time zones and offline renewal; clarify threat model.

### GCS-076 — MapTiler API key is hard-coded

**Original category/subcategory:** Security, login & licensing / API secrets  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** map.html:149; SettingsWindow.qml:150

**Original description:**

> API key is present in map.html and SettingsWindow.qml.

**Current status/validity:** Still valid. Remote map styles still contain a hard-coded MapTiler credential. Current tile errors strip query strings before warning, which reduces incidental log exposure but does not remove the embedded key.

**Current source/symbols and evidence inspected:** integrations/map_provider/map/map.html: mapStyles; core/map/TileCacheManager.cpp: sanitized failure URL; related settings UI.

**Dependencies:** GCS-052, GCS-056.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Provider-approved token restrictions/provisioning, release asset scan and redacted logs; no key rotation in Phase 0.

### GCS-077 — Vehicle and joystick objects run before login

**Original category/subcategory:** Security, login & licensing / Startup / access control  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** app/main.cpp; Login/Activation startup flow

**Original description:**

> Serial-port polling, MAVLink object and joystick polling start from main() before login. Logout is not possible and there is no idle lock.

**Current status/validity:** Still valid. main constructs VehicleManager and AppBackend before loading the login UI; AppBackend constructs the polling JoystickController. Authentication currently gates presentation rather than construction of these subsystems.

**Current source/symbols and evidence inspected:** app/main.cpp: object construction/engine.load; app/AppBackend.cpp: constructor; app/JoystickController.cpp: pollTimer; ui/qml/core/App.qml: login flow.

**Dependencies:** GCS-071, GCS-012, GCS-078.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Pre-login and post-logout command/input/network activity tests; define permitted background discovery.

### GCS-078 — QML engine and exposed objects have unsafe destruction order

**Original category/subcategory:** Application stability & lifecycle / Startup / shutdown  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** main.cpp:166-177

**Original description:**

> Objects exposed to QML are declared after the engine and may be destroyed first while QML still references them during teardown.

**Current status/validity:** Still valid. QQmlApplicationEngine is declared before multiple stack objects exposed to QML, so those objects are destroyed before the engine on normal scope exit. Vehicle's explicit Mavlink teardown improves its internal lifetime but does not establish safe application/QML teardown.

**Current source/symbols and evidence inspected:** app/main.cpp: engine, manager, mission/raster/backend declarations and setContextProperty; core/vehicle/Vehicle.cpp: destructor.

**Dependencies:** GCS-012, GCS-068, GCS-108.  
**Recommended resolution workstream:** S4.  
**Recommended verification:** Quit with live QML bindings/connections and async operations under memory/lifetime instrumentation.

### GCS-079 — Malformed stored ZIP can cause out-of-bounds read

**Original category/subcategory:** Application stability & lifecycle / Input validation / memory safety  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** core/terrain/TerrainProvider.cpp:653

**Original description:**

> For a stored (uncompressed) ZIP entry, code copies uncompressedSize bytes but checks compressedSize against the buffer. A malformed/truncated ZIP whose header claims a larger uncompressed size can cause an out-of-bounds read.

**Current status/validity:** Still valid. Stored ZIP validation bounds dataOffset+compressedSize, but the compression==0 branch copies uncompressedSize bytes without proving that many input bytes exist or that sizes match. This is a concrete bounds gap; no malformed archive was executed.

**Current source/symbols and evidence inspected:** core/terrain/TerrainProvider.cpp: extract/download ZIP branch near line 653.

**Dependencies:** GCS-046, GCS-047, GCS-093.  
**Recommended resolution workstream:** F then S3.  
**Recommended verification:** Malformed stored/deflated ZIP corpus with sanitizer, size caps, equality/bounds checks and atomic output tests.

### GCS-080 — No single-instance guard

**Original category/subcategory:** Application stability & lifecycle / Process management  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Application startup; WebEngine profile/settings/cache paths

**Original description:**

> Two app copies share the default WebEngine profile folder, settings, tile cache and serial ports. The second map can fail to load due to Chromium profile locking; both processes can contend over shared caches/resources.

**Current status/validity:** Still valid. No startup single-instance lock was found before shared settings/profile/cache/log access. Concurrent instances remain possible; consequences require a controlled isolated two-instance test.

**Current source/symbols and evidence inspected:** app/main.cpp: startup; core/storage/AppDataPaths.cpp; default WebEngine profile setup.

**Dependencies:** GCS-084, GCS-085, GCS-091.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Two isolated instances, lock recovery after crash and explicit multi-instance policy.

### GCS-081 — Malformed link settings entry can throw QML error on Connect

**Original category/subcategory:** Application stability & lifecycle / Settings validation  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** ui/qml/panels/TopPanel.qml:220

**Original description:**

> Saved JSON is parsed safely, but individual link entries are not validated. An entry missing address can throw a JavaScript error when Connect is clicked.

**Current status/validity:** Still valid. JSON parsing is caught, but individual saved entries are not structurally validated before connectCommLink accesses commLink.address.split(':'). A syntactically valid array with a missing/non-string address can still throw.

**Current source/symbols and evidence inspected:** ui/qml/panels/TopPanel.qml: loadCommLinks, resetConnectionStates, connectCommLink.

**Dependencies:** GCS-032, GCS-033.  
**Recommended resolution workstream:** S2.  
**Recommended verification:** Persist null/missing/non-string address and malformed serial records; Connect must fail safely with feedback.

### GCS-082 — Telemetry logging ignores file.write result

**Original category/subcategory:** Application stability & lifecycle / Disk / logging  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** comms/mavlink/Mavlink.cpp:252

**Original description:**

> Telemetry log writer ignores file.write result. When disk fills, logging may stop while UI still says logging is active; its in-memory buffer has no limit and can keep growing, especially on slow/network/OneDrive storage.

**Current status/validity:** Still valid. BufferedLogWriter::drain still calls file.write(pending) without checking the returned byte count or communicating failure. Moving writes to a worker does not guarantee storage success.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: BufferedLogWriter::drain/start/stop.

**Dependencies:** GCS-083, GCS-086.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Short writes, disk-full, revoked permissions and flush failure with accurate observable log status.

### GCS-083 — No free-disk monitoring or logging failure indication

**Original category/subcategory:** Application stability & lifecycle / Disk / logging  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Telemetry log writer

**Original description:**

> No free-space monitoring is reported. Full disk can silently stop logging, while slow/network storage can increase buffering; the UI does not clearly indicate the logging failure.

**Current status/validity:** Still valid. Start/open errors exist, but no inspected free-space monitor or continuous write/flush failure indication closes the runtime logging failure gap.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: BufferedLogWriter, startTelemetryLog/startFlightControllerLog; app/main.cpp: runtime log handler.

**Dependencies:** GCS-082, GCS-085, GCS-086.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Low-space and mid-session write failures; warning policy and bounded logging degradation without blocking control.

### GCS-084 — Application data folder is under Documents and may be OneDrive-synced

**Original category/subcategory:** Application stability & lifecycle / Disk / storage  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** Application data folder configuration

**Original description:**

> The data folder is in Documents, commonly redirected to OneDrive. Thousands of small tile files plus TLogs and video may be synced, causing locks, slow writes and large sync traffic.

**Current status/validity:** Still valid. AppDataPaths still uses Documents/BonV GCS for logs, caches, terrain and missions, with fallback locations. Documents may be redirected/synchronized; the actual machine's redirection was not changed or tested.

**Current source/symbols and evidence inspected:** core/storage/AppDataPaths.cpp: documents/base paths and ensureAllDirectories.

**Dependencies:** GCS-080, GCS-085, GCS-091.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Redirected/OneDrive/offline/read-only Documents and explicit user-data/cache policy.

### GCS-085 — runtime.log grows indefinitely and flushes synchronously

**Original category/subcategory:** Application stability & lifecycle / Logging  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** runtime.log implementation

**Original description:**

> runtime.log grows indefinitely, writes a 'GUI thread alive' line every second and synchronously flushes every debug line.

**Current status/validity:** Still valid. runtime.log is opened Append; the installed message handler writes/flushes synchronously under a mutex and a one-second 'GUI thread alive' timer continually adds entries. No rotation/bounded-size policy was found.

**Current source/symbols and evidence inspected:** app/main.cpp: RuntimeLogger/runtimeMessageHandler and final QTimer.

**Dependencies:** GCS-014, GCS-082, GCS-083, GCS-084.  
**Recommended resolution workstream:** S5 (S1 latency assessment).  
**Recommended verification:** Long-run size/latency test and slow-storage injection; verify logging cannot delay flight/control deadlines.

### GCS-086 — No bounded resource policy for terrain cache / logs / queues

**Original category/subcategory:** Application stability & lifecycle / Long-running stability  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Fourth-pass crash/disruption review (user-shared in chat)  
**Original cited locations (historical):** TerrainProvider.cpp; Mavlink.cpp; transport write queues

**Original description:**

> Reports identify unbounded terrain cache, logging buffer and outbound queues, creating long-session memory/resource risks.

**Current status/validity:** Partially addressed. Parameter pending writes, quality windows, inspector streams/raw packets and disk tile cache have bounds. Terrain memory cache, general transport buffers and worker log buffers still lack a coordinated resource policy. The original blanket 'no bounds anywhere' interpretation is obsolete.

**Current source/symbols and evidence inspected:** core/parameters/ParameterManager.h: WriteConfig; core/map/TileCacheManager.cpp: enforceCacheLimit; core/mavlink/MavlinkInspectorController.cpp: maximumStreams; comms/mavlink/Mavlink.cpp: BufferedLogWriter/terrain queue; core/terrain/TerrainProvider.cpp.

**Dependencies:** GCS-037, GCS-047, GCS-057, GCS-082, GCS-085.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Resource inventory with individual budgets, overflow outcomes and prolonged constrained-storage/network soak.

### GCS-087 — Inspector rate is all-time average rather than recent rate

**Original category/subcategory:** Diagnostics & MAVLink inspector / Rate measurement  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** MavlinkInspectorController.cpp:173-178

**Original description:**

> Inspector rate is an all-time average from the first packet. It does not fall to zero when a stream stops and reacts slowly to rate changes.

**Current status/validity:** Still valid. Inspector computes rate from total count/bytes since firstTimestamp, not a sliding recent interval. A silent stream can therefore retain a nonzero all-time rate.

**Current source/symbols and evidence inspected:** core/mavlink/MavlinkInspectorController.cpp: observeFrame elapsedMs/rateHz/bytesPerSecond.

**Dependencies:** GCS-088, GCS-089.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Burst, silence, rate-change and reconnect fixtures with a monotonic clock/recent-window policy.

### GCS-088 — Inspector rows accumulate across reconnects

**Original category/subcategory:** Diagnostics & MAVLink inspector / Reconnect handling  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** MavlinkInspectorController

**Original description:**

> Rows are keyed by an identifier built from the link object's memory address and are only cleared manually. Reconnects add duplicate rows until the 4,096-row cap is reached, after which new message types stop appearing.

**Current status/validity:** Still valid. Inspector keys include pointer-derived link identity; streams are capped but not evidently retired on reconnect. Changed link identities can accumulate old rows until the cap. Exact accumulation frequency depends on allocator reuse.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: readData linkIdentity; core/mavlink/MavlinkInspectorController.cpp: observeFrame/streams/maximumStreams.

**Dependencies:** GCS-087, GCS-089, GCS-040.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Repeated reconnect/new-link identity tests and stale-row retention/eviction policy.

### GCS-089 — Inspector bookkeeping continues while page is closed

**Original category/subcategory:** Diagnostics & MAVLink inspector / Performance  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** MavlinkInspectorController

**Original description:**

> Per-packet bookkeeping runs in receive path even when inspector page is not open.

**Current status/validity:** Partially addressed. observeFrame still runs whenever inspection is not paused, independent of panel visibility, but generic field decoding is now deferred to the selected details/graph UI cadence instead of every frame. Remaining bookkeeping cost is bounded but unprofiled.

**Current source/symbols and evidence inspected:** core/mavlink/MavlinkInspectorController.cpp: observeFrame latestMessage/fieldsDirty, UI update timer; comms/mavlink/Mavlink.cpp: frameObserved connection.

**Dependencies:** GCS-087, GCS-088, GCS-014.  
**Recommended resolution workstream:** S5.  
**Recommended verification:** Closed/open/paused inspector CPU and latency profiling under high-rate streams; verify diagnostic fidelity.

### GCS-090 — Two tests encode known buggy behaviour

**Original category/subcategory:** Testing & quality assurance / Incorrect test expectations  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Motor test and mission test names

**Original description:**

> runAllMotorTestSendsFourDelayedMotorCommands asserts the qMax(motorCount, 4) behaviour, which sends a motor-4 command even for a tricopter. uploadedMissionStoresHomeLocation asserts the legacy path that drops the home row. Both tests should change when the underlying bugs are fixed.

**Current status/validity:** Partially addressed. The four-motor test now prepares a confirmed quad frame and requests four motors; the old tricopter qMax claim no longer describes that test. Legacy home extraction still has a test and the unsafe legacy upload semantics remain. Do not delete either test merely from its name.

**Current source/symbols and evidence inspected:** tests/MavlinkTest.cpp: runAllMotorTestSendsFourDelayedMotorCommands, commandPrerequisites tri-frame cases, uploadedMissionStoresHomeLocation; comms/mavlink/Mavlink.cpp: motorConfigurationError/parseMissionFile.

**Dependencies:** GCS-023, GCS-024, GCS-112.  
**Recommended resolution workstream:** S1/S3.  
**Recommended verification:** Assert exactly three commands for confirmed tricopter and exact home/TAKEOFF wire sequence for legacy/canonical upload.

### GCS-091 — Tests write to real Documents folder and HKCU settings

**Original category/subcategory:** Testing & quality assurance / Test isolation  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Qt test setup; MavlinkConsoleTest

**Original description:**

> Tests do not enable QStandardPaths::setTestModeEnabled or set their own settings scope. Tests that connect Mavlink (e.g. MavlinkConsoleTest) can write real Documents/BonV GCS/Logs/TLogs and read/write real HKCU settings.

**Current status/validity:** Partially addressed. Manager tests use injected clocks; converter/configuration tests use temporary files and favorites probes have explicit INI paths. However logging setters use default QSettings, several network tests start logging, and QML suites write fixed files under build. Whole-suite isolation is not established.

**Current source/symbols and evidence inspected:** tests/ParameterManagerTest.cpp; tests/ParameterConfigurationTest.cpp: Harness; tests/MavlinkTest.cpp; tests/MavlinkConsoleTest.cpp; tests/qml/tst_FullParameterList.qml and tst_ParameterReadiness.qml; comms/mavlink/Mavlink.cpp: setters/start logs.

**Dependencies:** GCS-084, GCS-094, GCS-095.  
**Recommended resolution workstream:** F.  
**Recommended verification:** Run all tests with temporary settings/data/cache/output roots; assert real Documents/HKCU and preexisting artifacts unchanged.

### GCS-092 — Tests rely on fixed qWait delays and real local sockets

**Original category/subcategory:** Testing & quality assurance / Test determinism  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Test suite; VideoTest

**Original description:**

> Tests use fixed waits such as qWait(30), qWait(100), plus real local sockets. This makes outcomes timing-dependent and likely contributes to VideoTest being flaky.

**Current status/validity:** Partially addressed. New recovery/quality tests use fake clocks, but real localhost sockets and fixed qWait calls remain, including manager real-timer tests and MAVLink lifecycle paths. Real integration tests are useful; replace only timing assumptions that cause nondeterminism.

**Current source/symbols and evidence inspected:** tests/ParameterManagerTest.cpp: RecoveryHarness/realTimerPacingAndDestruction; tests/MavlinkTest.cpp: local sockets and qWait; tests/VideoTest.cpp.

**Dependencies:** GCS-091, GCS-095.  
**Recommended resolution workstream:** F/S6.  
**Recommended verification:** Repeated/load-varied runs, observable synchronization and separate deterministic/unit versus real-I/O integration coverage.

### GCS-093 — High-risk behaviours lack tests

**Original category/subcategory:** Testing & quality assurance / Coverage gaps  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** tests/

**Original description:**

> No tests were identified for heartbeat loss, duplicate mission items after retry, joystick neutral/release, gimbal stop, RC calibration validation, planner range/limits or installer upgrade.

**Current status/validity:** Partially addressed. Heartbeat expiry, selected-source parameter routing, parameter recovery/readiness/write transactions and motor prerequisites now have substantial regressions. Duplicate mission retry, joystick release, broad foreign telemetry, planner edge cases and installer workflows remain coverage gaps.

**Current source/symbols and evidence inspected:** tests/MavlinkTest.cpp: lifecycle/quality/parameter/motor cases; tests/ParameterManagerTest.cpp; tests/VehicleTest.cpp; tests/qml/tst_MapDispatch.qml; tests/CMakeLists.txt.

**Dependencies:** GCS-001 through GCS-006; GCS-019, GCS-025, GCS-103.  
**Recommended resolution workstream:** F then each S phase.  
**Recommended verification:** Build a requirement-to-test map; add a failing regression for each confirmed defect before the corresponding fix.

### GCS-094 — QML tests are not wired into CTest

**Original category/subcategory:** Testing & quality assurance / QML test execution  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** tests/qml; CTest

**Original description:**

> Tests under tests/qml/ are not wired into CTest and therefore do not run in the reported test workflow.

**Current status/validity:** Still valid. tests/CMakeLists registers 12 C++ targets through addGcsTest. No qmltestrunner test registration was found. Fresh ctest -N lists no QML suites; direct MapDispatch QML execution is reported separately.

**Current source/symbols and evidence inspected:** tests/CMakeLists.txt; build/tests/CTestTestfile.cmake; tests/qml. Fresh CTest inventory: 12 tests.

**Dependencies:** GCS-091, GCS-092.  
**Recommended resolution workstream:** F.  
**Recommended verification:** Register isolated QML suites or maintain explicit separate CI execution; verify discovery and actual runner output.

### GCS-095 — VideoTest is timing-sensitive / has previously failed

**Original category/subcategory:** Testing & quality assurance / Coverage gaps  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** build/Testing; VideoTest

**Original description:**

> The earlier run reported C++ tests passing, but VideoTest had failed previously and later passed. Combined with fixed qWait delays and real sockets, this is a timing-sensitive test concern.

**Current status/validity:** Not reproduced in this audit. The current VideoTest contains four property/empty-input cases, with no qWait/socket use in that file, and freshly passed 6/6 including setup/cleanup. Historical Nov 13 CTest also passed. A single narrow pass does not disprove historical flakiness or exercise real streaming/recording.

**Current source/symbols and evidence inspected:** tests/VideoTest.cpp; fresh direct Debug VideoTest: 6 passed, 0 failed, 6 ms, exit 0; build/phase6-ctest.log (historical). GStreamer registry redirected to a unique temporary path.

**Dependencies:** GCS-091, GCS-092, GCS-064.  
**Recommended resolution workstream:** F/S4.  
**Recommended verification:** Repeat isolated video tests under load and add real pipeline/recording fault coverage; preserve any failure artifacts and distinguish historical tests from current cases.

### GCS-096 — CMake references missing RasterImportWorker.cpp

**Original category/subcategory:** Build, packaging & platform support / Build configuration  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** CMakeLists.txt; tools/RasterImportWorker.cpp

**Original description:**

> CMakeLists builds tools/RasterImportWorker.cpp, which does not exist. Configure reportedly succeeds only because GDAL is not found; installing GDAL may activate this broken target.

**Current status/validity:** Still valid. Root CMake still conditionally adds tools/RasterImportWorker.cpp when GDAL_FOUND, but that file is absent. Cached GDAL_DIR-NOTFOUND/BONV_REQUIRE_GDAL=OFF hides the branch in this workstation build, not in a GDAL-enabled clean build.

**Current source/symbols and evidence inspected:** CMakeLists.txt: if(GDAL_FOUND)/add_executable(RasterImportWorker); Test-Path tools/RasterImportWorker.cpp=false; build/CMakeCache.txt.

**Dependencies:** GCS-110, GCS-111.  
**Recommended resolution workstream:** F.  
**Recommended verification:** Isolated configure matrix with GDAL present/absent and required/optional modes, without modifying current build/deploy.

### GCS-097 — OpenSSL path is hard-coded

**Original category/subcategory:** Build, packaging & platform support / Dependency configuration  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** CMake/OpenSSL configuration

**Original description:**

> C:/Program Files/OpenSSL-Win64 is hard-coded and can break builds on machines without that exact installation path.

**Current status/validity:** Still valid. OpenSSL runtime deployment still assigns C:/Program Files/OpenSSL-Win64 directly and fails if DLLs are not there. A cache entry named BONV_OPENSSL_RUNTIME_DIR exists but is not used by the inspected current hard-coded branch.

**Current source/symbols and evidence inspected:** CMakeLists.txt: OPENSSL_INSTALL_DIR/runtime DLL glob; build/CMakeCache.txt: OpenSSL paths.

**Dependencies:** GCS-098, GCS-111.  
**Recommended resolution workstream:** F.  
**Recommended verification:** Fresh machine/custom prefix OpenSSL deployment and standalone TLS smoke tests using the selected build kit.

### GCS-098 — MSVC redistributable check can fail build outright

**Original category/subcategory:** Build, packaging & platform support / Dependency configuration  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** CMake build configuration

**Original description:**

> The MSVC redistributable check reportedly fails the build outright.

**Current status/validity:** Requires investigation. CMake still makes missing redistributable discovery a fatal configure condition. Failing before producing an incomplete installer can be intentional; whether valid SDK/toolchain layouts are wrongly rejected needs an isolated matrix.

**Current source/symbols and evidence inspected:** CMakeLists.txt: BONV_MSVC_REDIST_DIR, BONV_MSVC_RUNTIME_DLLS fatal branch near lines 309-330.

**Dependencies:** GCS-097, GCS-103.  
**Recommended resolution workstream:** F.  
**Recommended verification:** VS editions/architectures and VCToolsRedistDir absent/custom cases; retain required runtime validation.

### GCS-099 — Unconditional windows.h include blocks Android build paths

**Original category/subcategory:** Build, packaging & platform support / Platform compatibility  
**Original reported priority (unchanged):** High  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** app/main.cpp

**Original description:**

> main.cpp includes windows.h unconditionally, so Android code paths cannot build.

**Current status/validity:** Still valid. app/main.cpp still includes windows.h/io.h/fcntl.h unconditionally. Windows-only use guards do not make the include portable. Android viability also depends on Qt WebEngine/video dependencies, beyond this one include.

**Current source/symbols and evidence inspected:** app/main.cpp: first includes/attachConsole; CMakeLists.txt: Qt WebEngine/GStreamer requirements.

**Dependencies:** GCS-067, GCS-107.  
**Recommended resolution workstream:** F if portability required.  
**Recommended verification:** Document supported targets; isolated non-Windows configure/compile before claiming Android support.

### GCS-100 — CMake include path references non-existent common directory

**Original category/subcategory:** Build, packaging & platform support / Build hygiene  
**Original reported priority (unchanged):** Medium  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** CMakeLists.txt

**Original description:**

> CMake include path lists a non-existent common/ directory.

**Current status/validity:** Still valid. Root target include directories still contain ${CMAKE_SOURCE_DIR}/common, which does not exist. This stale path is not necessarily the path resolving common/mavlink.h and need not break the current build.

**Current source/symbols and evidence inspected:** CMakeLists.txt: target_include_directories near line 236; Test-Path common=false.

**Dependencies:** GCS-096, GCS-111.  
**Recommended resolution workstream:** F.  
**Recommended verification:** Inspect actual compile include resolution and clean configure/build on an isolated checkout.

### GCS-101 — Project is still named MavlinkPOC

**Original category/subcategory:** Build, packaging & platform support / Product identity  
**Original reported priority (unchanged):** Low  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** CMake/project metadata

**Original description:**

> The project is still named MavlinkPOC.

**Current status/validity:** Still valid. The project and core library retain MavlinkPOC naming. This is a low-priority naming/maintenance finding, not demonstrated application failure.

**Current source/symbols and evidence inspected:** CMakeLists.txt: project(MavlinkPOC VERSION 0.1), MavlinkPOCCore target; generated cache/solution names.

**Dependencies:** GCS-096, GCS-100.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** If renamed later, verify consumers, target names, test registration and packaging remain consistent.

### GCS-102 — Console window appears in release build

**Original category/subcategory:** Build, packaging & platform support / Release packaging  
**Original reported priority (unchanged):** Medium  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** CMakeLists.txt

**Original description:**

> WIN32_EXECUTABLE FALSE means the console window appears in the release build.

**Current status/validity:** Addressed in source and artifact. Current CMake sets WIN32_EXECUTABLE TRUE. Existing Release and deployed executables have PE subsystem 2 (Windows GUI), while the older Debug executable has subsystem 3 (console). main attaches only to an existing parent console; it does not allocate a new one. Interactive CMD/PowerShell and redirection were not smoke-tested.

**Current source/symbols and evidence inspected:** CMakeLists.txt: WIN32_EXECUTABLE; app/main.cpp: attachConsole/runtimeMessageHandler; PE optional header inspected for build/Release, build/deploy and build/Debug/BonVGroundStation.exe.

**Dependencies:** GCS-085, GCS-099, GCS-103.  
**Recommended resolution workstream:** F regression.  
**Recommended verification:** Fresh isolated Debug/Release launch from Explorer/CMD/PowerShell, with and without redirected stdout/stderr; check no extra Explorer console.

### GCS-103 — Installer upgrade behaviour is not tested

**Original category/subcategory:** Build, packaging & platform support / Deployment / upgrade  
**Original reported priority (unchanged):** Not assigned  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** Installer/test suite

**Original description:**

> Installer upgrade behaviour is listed among high-risk areas without automated tests.

**Current status/validity:** Still valid review gap. Installer scripts implement existing-install lookup, version comparison and uninstall/reinstall flows, but no automated clean-install/upgrade/rollback tests were found in CTest or the inspected tests. Presence of scripts is not workflow verification.

**Current source/symbols and evidence inspected:** installer/config/controlscript.qs; installer/packages/com.bonv.gcs/meta/installscript.qs; installer/package.bat; tests/CMakeLists.txt.

**Dependencies:** GCS-096, GCS-097, GCS-098, GCS-102, GCS-110, GCS-111.  
**Recommended resolution workstream:** F/S5.  
**Recommended verification:** Disposable Windows VM matrix: clean install, same/newer/older version, cancellation/failure, running app, non-admin and uninstall/data preservation.

### GCS-104 — Mavlink.cpp has excessive responsibilities

**Original category/subcategory:** Architecture & maintainability / God objects  
**Original reported priority (unchanged):** Architecture risk  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** comms/mavlink/Mavlink.cpp

**Original description:**

> Mavlink.cpp is reported at about 3,500 lines and handles transport, mission, parameters, terrain, VPS, logging, motor tests and servos; it should be decomposed into protocol/service managers.

**Current status/validity:** Architecture recommendation. Mavlink still owns many responsibilities (transport/lifecycle, command, mission, terrain, quality, logging, facade). ParameterManager, configuration controller, console/inspector/forwarder extraction reduce some coupling. No automatic broad refactor is justified by line count.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp/.h; core/parameters/ParameterManager.cpp; core/parameters/ParameterConfigurationController.cpp; core/mavlink controllers.

**Dependencies:** GCS-029, GCS-041, GCS-093.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** Map responsibilities and dependency/lifetime boundaries; extract only behind proven regression coverage.

### GCS-105 — FlyView.qml is excessively large

**Original category/subcategory:** Architecture & maintainability / God objects  
**Original reported priority (unchanged):** Architecture risk  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** ui/qml/core/FlyView.qml

**Original description:**

> FlyView.qml is reported at about 5,200 lines and was only spot-checked for logic/layout in the review; it should be decomposed into smaller QML components.

**Current status/validity:** Architecture recommendation. FlyView remains a large coordinator with mission/map/flight/UI logic. Recent map dispatch separation and canonical mission model are improvements, not proof all coupling risks are removed.

**Current source/symbols and evidence inspected:** ui/qml/core/FlyView.qml; ui/qml/map/FlyMap.qml; ui/qml/map/MapDispatch.js.

**Dependencies:** GCS-024, GCS-053, GCS-055, GCS-094.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** Identify tested seams and ownership before incremental extraction; protect mission/map behavior.

### GCS-106 — OemSettingsPage.qml is excessively large

**Original category/subcategory:** Architecture & maintainability / God objects  
**Original reported priority (unchanged):** Architecture risk  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** ui/qml/settings/OemSettingsPage.qml

**Original description:**

> OemSettingsPage.qml is reported at about 4,500 lines and was only spot-checked for logic/layout in the review; it should be decomposed into smaller components.

**Current status/validity:** Architecture recommendation. OEM Settings remains large. FullParameterListPage and ParameterConfigurationPanel are separate reusable pages with explicit vehicle integration, but other settings domains remain concentrated.

**Current source/symbols and evidence inspected:** ui/qml/settings/OemSettingsPage.qml; FullParameterListPage.qml; ParameterConfigurationPanel.qml.

**Dependencies:** GCS-094, GCS-112.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** Navigation/SAVE/viewport regressions and component ownership review before further decomposition.

### GCS-107 — No per-autopilot firmware abstraction

**Original category/subcategory:** Architecture & maintainability / Firmware abstraction  
**Original reported priority (unchanged):** Architecture risk  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Vehicle.cpp; BottomPanel.qml; Mavlink.cpp

**Original description:**

> ArduCopter assumptions are spread through C++ and QML. A per-autopilot abstraction is needed for modes, home semantics and takeoff behaviour.

**Current status/validity:** Architecture recommendation. Firmware identity now chooses parameter encoding and configuration format, but general flight modes/control behavior still lack a per-autopilot abstraction. Preserve distinction between this architectural recommendation and the concrete mode defect GCS-005.

**Current source/symbols and evidence inspected:** comms/mavlink/Mavlink.cpp: handleHeartbeat, takeoff/startMission; core/parameters/ParameterConfigurationController.cpp: liveFamily; core/vehicle/Vehicle.cpp.

**Dependencies:** GCS-005, GCS-035, GCS-036, GCS-067.  
**Recommended resolution workstream:** S1 narrow seam then S6.  
**Recommended verification:** Define supported firmware/vehicle capability matrix and test behavior at the existing facade boundary.

### GCS-108 — Context properties used instead of registered QML types

**Original category/subcategory:** Architecture & maintainability / QML integration  
**Original reported priority (unchanged):** Architecture risk  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** QML context property registration

**Original description:**

> Context properties lead to typeof missionPlanModel !== undefined guards and reduce QML type safety and qmllint/type-checking support.

**Current status/validity:** Architecture recommendation. main still injects manager/backend/model instances with setContextProperty, alongside some registered QML types. This is a tooling/coupling recommendation, not inherently a defect; lifetime finding GCS-078 is concrete and separate.

**Current source/symbols and evidence inspected:** app/main.cpp: qmlRegisterType/qmlRegisterUncreatableType/setContextProperty.

**Dependencies:** GCS-078, GCS-094.  
**Recommended resolution workstream:** S6.  
**Recommended verification:** Assess ownership, typed tooling and testability; migrate only with QML API/lifecycle regressions.

### GCS-109 — Several large modules were only spot-checked or not reviewed deeply

**Original category/subcategory:** Architecture & maintainability / Scope / review coverage  
**Original reported priority (unchanged):** Review gap  
**Original source:** Third-pass findings (user-shared in chat)  
**Original cited locations (historical):** FlyView.qml; OemSettingsPage.qml; VerticalTelemetryPanel.qml; SettingsWindow.qml; Offline-3DMaps; SkyRouteX; ScreenRecorder; GimbalController; MotorTest; inspector models

**Original description:**

> The review says only spot-checks were performed on FlyView.qml, OemSettingsPage.qml and VerticalTelemetryPanel.qml styling/layout; the fourth pass says OemSettingsPage.qml, most of FlyView.qml, SettingsWindow.qml, Offline-3DMaps, SkyRouteX, ScreenRecorder, GimbalController, MotorTest and inspector models still needed deeper review. The third pass also flags VerticalTelemetryPanel as only spot-checked.

**Current status/validity:** Review gap. This Phase 0 audit spot-checks active linked paths and relevant tests; it does not establish a full deep review of Offline-3DMaps, SkyRouteX planning, recording, gimbal, motor and inspector code. The workbook explicitly says these areas were incompletely audited.

**Current source/symbols and evidence inspected:** Workbook Audit Coverage sheet; CMakeLists.txt: actual linked sources; integrations/Offline-3DMaps, integrations/SkyRouteX_New; app/ScreenRecorder.cpp; core/mavlink/MavlinkInspectorController.cpp.

**Dependencies:** GCS-043 through GCS-070; GCS-093.  
**Recommended resolution workstream:** Each S phase, tracked to S6.  
**Recommended verification:** Bounded module reviews with dataflow, boundary/fault tests, platform matrix and explicit reviewed/not-reviewed inventory.

### GCS-110 — Important source files and tests are untracked

**Original category/subcategory:** Repository hygiene & source control / Untracked source files  
**Original reported priority (unchanged):** Urgent hygiene  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** git status; listed untracked paths

**Original description:**

> Untracked files explicitly named: MissionPlanModel, MissionPlanConverter, MavlinkConsoleController, MapDispatch.js and their tests. The current work exists only on this machine and could be lost.

**Current status/validity:** Partially addressed. Originally named MissionPlanModel/Converter, MavlinkConsoleController, MapDispatch and their tests are now tracked. However eight important later source/test/document files are untracked, including configuration controller and Full Parameter List. A checkout of HEAD does not reproduce the current application.

**Current source/symbols and evidence inspected:** git status --short; git ls-files for original names; eight-file untracked inventory in repository baseline; CMakeLists.txt/resources.qrc reference later files.

**Dependencies:** GCS-111, GCS-096, GCS-094.  
**Recommended resolution workstream:** F before implementation.  
**Recommended verification:** Review and deliberately preserve/commit intended work with owner approval; verify fresh checkout contains every referenced source/resource/test. No staging done in audit.

### GCS-111 — Repository tracks many binaries and generated files

**Original category/subcategory:** Repository hygiene & source control / Repository bloat  
**Original reported priority (unchanged):** Urgent hygiene  
**Original source:** Earlier audit report (uploaded report; duplicate upload counted once)  
**Original cited locations (historical):** Git repository inventory

**Original description:**

> .git is reported as 671 MB. The repository tracks 848 binaries including GStreamer libraries, .obj files, Android .so files, ZIPs, a root .exe and home/user/mapcache data.

**Current status/validity:** Still valid. Tracked vendored/generated payloads remain substantial: integrations dominates tracked paths, installer payload is tracked, and hundreds of static/shared libraries are in Git. Ignore rules do not remove already tracked content. This is a reproducibility/repository policy task, not authorization to delete anything.

**Current source/symbols and evidence inspected:** git ls-files counts/extensions; git count-objects -vH; .gitignore; installer/packages/com.bonv.gcs/data; integrations Android/generated trees.

**Dependencies:** GCS-110, GCS-096, GCS-097, GCS-103.  
**Recommended resolution workstream:** F before implementation.  
**Recommended verification:** Classify source/vendor/runtime/generated artifacts, preserve backups and licensing/provenance, then validate an approved artifact/dependency strategy in a fresh checkout.

### GCS-112 — Application proceeds before the full parameter list has finished loading

**Original category/subcategory:** MAVLink & communications / Parameter protocol / startup readiness  
**Original reported priority (unchanged):** Not assigned — safety impact requires code review and reproduction  
**Original source:** User-discovered follow-up after the earlier, third-pass and fourth-pass audits  
**Original cited locations (historical):** Not yet traced; inspect PARAM_REQUEST_LIST initiation, PARAM_VALUE handling, expected-count/index tracking, and all startup/vehicle-ready gates

**Original description:**

> The application reportedly proceeds to subsequent operations before the complete parameter list has been received. This can leave the parameter cache incomplete and allow UI/configuration or other workflows to use missing, stale, default or uninitialized parameter values. Completion must be based on validated receipt of all expected parameter indices from the intended autopilot component, with explicit timeout, missing-index and disconnect handling.

**Current status/validity:** Partially addressed. Validated complete snapshots, count/index/name/source checks, missing-index recovery, timeout/failure and retained-stale semantics are implemented. Parameter-dependent write/calibration/motor paths use readiness/provenance gates and QML readiness exists. Fresh manager tests pass; all possible startup/configuration consumers have not been exhaustively mapped, so the broad user-reported issue is not closed globally.

**Current source/symbols and evidence inspected:** core/parameters/ParameterManager.cpp: receive, parametersReady, recovery, snapshotProvenance; comms/mavlink/Mavlink.cpp: parameterPrerequisiteError/motorConfigurationError/write facade; ui/qml/accessories/ParameterSyncStatus.qml; tests/ParameterManagerTest.cpp; tests/qml/tst_ParameterReadiness.qml; tests/MavlinkTest.cpp: commandPrerequisites.

**Dependencies:** GCS-001, GCS-019, GCS-020, GCS-030, GCS-093, GCS-094.  
**Recommended resolution workstream:** S1 closure review.  
**Recommended verification:** Trace every parameter-dependent consumer; run loading/missing/duplicate/foreign/stale/disconnect/refresh cases. Do not require unrelated emergency/mission actions to wait for parameters without a safety requirement.

## 10. Review boundary

Phase 0 ends with this report. No Phase 1 fixes, source-control changes, build/deployment replacement or installer execution have been performed. Review the foundation plan, issue statuses, verification deferrals and disclosed CTest discovery-log side effect before authorizing implementation. Closure candidates require evidence review; all other original findings remain tracked with their original priorities.

