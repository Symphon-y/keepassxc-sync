# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is a fork of [KeePassXC](https://github.com/keepassxreboot/keepassxc) focused on adding robust multi-device sync with conflict resolution for cloud-stored KDBX databases. The upstream codebase is C++20 with Qt6, built via CMake.

## Build Commands

```bash
# Configure (from repo root, out-of-source build recommended)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel

# Run
./build/src/keepassxc          # Linux/macOS
.\build\src\keepassxc.exe      # Windows

# Run tests
cmake --build build --target test
# or directly:
cd build && ctest --output-on-failure

# Run a single test
cd build && ctest -R <TestName> --output-on-failure

# Lint / format (uses clang-format)
find src -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror

# With specific features enabled
cmake -B build -S . \
  -DWITH_XC_BROWSER=ON \
  -DWITH_XC_SSHAGENT=ON \
  -DWITH_XC_KEESHARE=ON \
  -DWITH_XC_NETWORKING=ON
```

## Key Dependencies

- **Qt 6.2.4+** — GUI, file watching (`QFileSystemWatcher`), networking
- **Botan 2.19.1+** — all cryptography (AES-256-CBC/GCM, ChaCha20, Argon2, AES-KDF, HMAC)
- **zlib** — payload compression inside KDBX

## Architecture

### Database Format (`src/format/`)

KDBX files are structured as: encrypted header → compressed+encrypted XML payload. The XML payload contains the full entry/group tree plus metadata and deleted-object tombstones.

- `KdbxReader` / `KdbxWriter` — abstract base; version-specific subclasses are `Kdbx3Reader/Writer` and `Kdbx4Reader/Writer`
- `KdbxXmlReader` / `KdbxXmlWriter` — convert between XML and the in-memory object model
- Atomic saves write to a temp file then rename; this is the pattern to follow for any sync writes

KDBX4 (the current default) adds a binary pool for deduplicating attachments and uses inner headers for extended metadata.

### Core Object Model (`src/core/`)

- `Database` — root object; owns the entry/group tree, metadata, and recyclebin group
- `Group` — tree node; has a `mergeMode` field controlling how the `Merger` handles it
- `Entry` — leaf; every entry carries a full `History` (list of prior `Entry` snapshots) and `TimeInfo` (created/modified/accessed/expires timestamps with microsecond precision)
- `Merger` — **the most important class for this project**. Merges two `Database` objects by UUID matching and timestamp comparison. Conflict strategy: newer entry becomes primary, older snapshot is pushed onto history. Emits a `ChangeList` of typed `Change` records (Added, Modified, Moved, Deleted, Metadata). Supports dry-run mode.
- `FileWatcher` — wraps `QFileSystemWatcher` with SHA-256 content diffing and polling fallback for NFS. Emits `fileChanged(path)` with a configurable delay to debounce rapid saves.

### GUI Layer (`src/gui/`)

- `MainWindow` — `QMainWindow`; hosts `DatabaseTabWidget` for multi-database tabs
- `DatabaseWidget` — per-database controller; owns the `FileWatcher`, handles `reloadDatabaseFile()`, and emits signals like `databaseSyncInProgress` / `databaseSyncCompleted` / `databaseSyncFailed` that are hooks for sync UI
- `MergeDialog` — shows a `ChangeList` table before applying a merge; already exists and can be extended for sync conflict review

### Existing Sync Infrastructure

KeePassXC has a command-based remote sync (`Database > Database Settings > Remote`) that shells out to rsync/scp with a `{TEMP_DATABASE}` placeholder. It is not atomic and has no conflict handling — this is the gap this fork fills.

KeeShare (`src/keeshare/`) is a separate peer-to-peer group-sharing feature; it is not related to full-database sync.

## Sync Feature — Design Notes

The goal is automatic, safe merging when a cloud-synced KDBX file changes on disk (i.e., another device saved). The existing pieces that compose the solution:

| Existing piece | Role in sync |
|---|---|
| `FileWatcher` | Detects when the cloud provider writes a new version to disk |
| `Merger` | Merges remote version into local working copy |
| `MergeDialog` | Shows changes; extend to surface true conflicts requiring user attention |
| `Entry::History` | Stores the common ancestor and all intermediate states |
| `TimeInfo` | Basis for timestamp comparison during merge |
| `databaseSyncInProgress/Completed/Failed` signals | Ready-made UI signal hooks |

The merge strategy already in `Merger` is timestamp-based (2-way). A 3-way merge (local changes vs. remote changes vs. last-known common ancestor) would require storing the ancestor's content hash or a lightweight snapshot — the entry history is the natural place for this.

## File Locations for Sync Work

- `src/core/Merger.h/cpp` — extend for 3-way merge, ancestor tracking
- `src/core/FileWatcher.h/cpp` — hook for triggering sync on cloud-write events
- `src/gui/DatabaseWidget.h/cpp` — orchestrate auto-merge flow, emit sync signals
- `src/gui/MergeDialog.h/cpp` — conflict review UI
- `src/format/KdbxXmlWriter.h/cpp` — may need to write ancestor snapshots into custom data
