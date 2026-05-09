# Cloud Sync Feature — Design & Verification

This document describes the multi-device sync feature added to this fork of KeePassXC.

## Problem

KeePassXC databases stored in cloud-synced folders (Google Drive, Dropbox, OneDrive, etc.) suffer from two failure modes when multiple devices have the database open simultaneously:

1. **Last-write-wins overwrites** — the device that saves last silently discards the other device's changes.
2. **Conflict copy proliferation** — cloud clients create `file.conflicted.kdbx` copies that are hard to track and reconcile.

The upstream KeePassXC has a `Merger` class and `MergeDialog`, but they are user-initiated and use a 2-way (timestamp-only) strategy with no concept of a common ancestor. This means a rapid edit on both devices in the same sync window can produce incorrect results even when timestamps are compared.

## Solution Overview

This fork adds automatic, safe multi-device sync triggered by the existing `FileWatcher` infrastructure (which already detects when the cloud client writes a new version of the database to disk). No cloud API integration is required — it works with any cloud provider that syncs files to a local folder.

### Key design decisions

| Decision | Choice | Rationale |
|---|---|---|
| Conflict UX | Auto-merge silently; dialog only for true conflicts | Minimizes friction; users only see a dialog when the same entry was edited on both devices |
| Merge depth | 3-way merge with checkpoint as common ancestor | Prevents false conflicts caused by clock skew or rapid edits in the same sync window |
| Soft locking | Warn (don't block) when another device has the DB open | Preserves usability; the merge handles conflicts anyway |
| Cloud scope | Filesystem-agnostic | Works with any cloud provider, no API keys or OAuth required |

---

## Architecture

### New classes

#### `src/core/SyncCheckpoint` — common ancestor tracking

Stores a snapshot of every entry's modification timestamp at the moment the database was last saved. Persisted as a JSON string inside `Database::metadata()->customData()` under the key `"sync.checkpoint"`, so it travels with the `.kdbx` file to every device.

```
{
  "syncedAt": "2026-05-09T14:30:00.000Z",
  "entries": { "{uuid}": "ISO8601 timestamp", ... },
  "groups":  { "{uuid}": "ISO8601 timestamp", ... },
  "deletedUuids": ["{uuid}", ...]
}
```

**Key methods:**
- `SyncCheckpoint::fromDatabase(db)` — deserializes the stored checkpoint from a database
- `SyncCheckpoint::snapshotDatabase(db)` — captures the current in-memory state as a new checkpoint
- `saveToDatabase(db)` — writes the checkpoint back into `Metadata::customData()`

#### `src/core/SyncMerger` — conflict classifier

Reads the checkpoint from `sourceDb` (the in-memory local copy) and classifies every entry as one of:

| Classification | Condition | Action |
|---|---|---|
| No-op | Neither side changed since checkpoint | Skip |
| Auto-merged (local) | Only local changed | Existing `Merger` applies it |
| Auto-merged (remote) | Only remote changed | Already correct in remote db |
| **Conflict** | **Both sides changed since checkpoint** | **Show `SyncConflictDialog`** |

`SyncMerger::merge()` is always non-destructive (classification only). The existing `Merger` class does the actual data merging. `SyncMerger::applyResolutions()` is called afterward to override conflict entries with the user's explicit choice.

#### `src/core/SyncLock` — soft lock

Writes `<database>.kdbx.sync-lock` (JSON) when a database is opened. If the cloud client syncs this file to another device and that device opens the same database, it will display a non-blocking warning banner.

Lock file contents:
```json
{ "deviceName": "Travis-Laptop", "deviceId": "<persistent-uuid>", "openedAt": "ISO8601" }
```

Locks older than **24 hours** are treated as stale and silently ignored. The lock is automatically released when the database is locked or closed.

#### `src/gui/SyncConflictDialog` — conflict resolution UI

Displays a table of conflicting entries. Each row shows the entry title, group path, and a list of fields that differ between local and remote. A per-row combo box lets the user pick **Keep Local** or **Keep Remote**. Bulk **Keep All Local** / **Keep All Remote** buttons are provided for convenience.

The losing version is preserved in the entry's history — no edits are silently discarded.

### Modified files

| File | Change |
|---|---|
| `src/gui/DatabaseWidget.cpp` | `reloadDatabaseFile()` — runs `SyncMerger` to classify conflicts before running `Merger`, shows `SyncConflictDialog` when needed, applies resolutions, writes new checkpoint. `performSave()` — snapshots checkpoint into `CustomData` before each write. `replaceDatabase()` — acquires soft lock. `lock()` — releases soft lock. |
| `src/gui/DatabaseWidget.h` | Added `m_syncLock` member. |
| `src/core/Config.h/cpp` | Added `Config::SyncDeviceId` (machine-local persistent UUID for the soft lock). |
| `src/CMakeLists.txt` | Registered new `.cpp` source files. |

---

## Merge flow (step by step)

```
FileWatcher detects cloud client wrote new version of database to disk
    │
    ▼
DatabaseWidget::reloadDatabaseFile()
    │
    ├─ Load remote db from disk
    ├─ Check SyncLock → show warning banner if another device's lock detected
    │
    ├─ [if local db is modified]
    │       SyncMerger(localDb, remoteDb).merge()
    │           → classifies each entry: auto-merged or conflict
    │
    │       [if conflicts]
    │           SyncConflictDialog → user picks Keep Local / Keep Remote per entry
    │
    │       Merger(localDb, remoteDb).merge()
    │           → applies all changes using timestamp logic
    │
    │       SyncMerger.applyResolutions(userChoices)
    │           → overrides conflict entries with user's explicit choice
    │           → losing version pushed into entry history
    │
    │       SyncCheckpoint::snapshotDatabase(mergedDb).saveToDatabase(mergedDb)
    │           → new checkpoint written into CustomData
    │
    ▼
replaceDatabase(mergedDb)
    → UI updated, status bar shows "Sync successful"
```

---

## Build

```bash
# From repo root (Linux/macOS/WSL)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Run all tests
cd build && ctest --output-on-failure

# Run a specific test
cd build && ctest -R TestSyncMerger --output-on-failure
```

**Dependencies:** Qt 6.2.4+, Botan 2.19.1+, zlib, CMake 3.16+. See the upstream [Build from Source](https://github.com/keepassxreboot/keepassxc/wiki/Building-KeePassXC) wiki for platform-specific setup.

---

## Verification plan

### Unit tests — `tests/core/TestSyncMerger.cpp` (to be written)

| Test case | Expected result |
|---|---|
| Device A edits entry X, Device B edits entry Y | Both changes auto-merged, no dialog |
| Device A and B both edit entry X | Conflict produced; dialog shown |
| Device A deletes entry X, Device B has not changed it | Entry auto-deleted on B |
| Device A deletes entry X, Device B edited it | Conflict produced |
| Empty checkpoint (first sync, no prior checkpoint) | Falls back to timestamp-only via existing `Merger` |
| Clock skew (remote timestamp slightly ahead) | Checkpoint comparison still classifies correctly |

### Manual smoke test (two running KeePassXC instances, shared folder)

1. Open the same `.kdbx` from a cloud-synced folder on Instance A and Instance B.
2. **Verify soft lock warning:** Instance B should show a warning banner that the database appears open on Instance A's device.
3. **Non-conflicting edits:**
   - Edit `Gmail` password on Instance A → save.
   - Edit `GitHub` username on Instance B (keep open).
   - Wait for Instance B's FileWatcher to detect Instance A's save.
   - Verify both changes are present in Instance B with no dialog shown.
4. **Conflicting edit:**
   - Edit `Gmail` password on Instance A → save.
   - Edit `Gmail` notes on Instance B (keep open).
   - Wait for FileWatcher trigger.
   - Verify conflict dialog appears listing the `Gmail` entry.
   - Pick Keep Local → verify Instance A's password update is in entry history.
5. **Lock release:** Close Instance A → verify `.kdbx.sync-lock` is deleted → reopen on Instance B → verify no warning shown.
6. **Stale lock:** Manually write a `.kdbx.sync-lock` with `openedAt` more than 24 hours ago → open database → verify no warning shown.

### Checkpoint persistence test

1. Open database, make an edit, save.
2. Open the saved `.kdbx` in a hex editor or KeePassXC's XML export.
3. Verify `sync.checkpoint` key is present in `Meta/CustomData` with correct entry timestamps.

---

## Pulling upstream KeePassXC updates

```bash
git fetch upstream
git checkout develop
git rebase upstream/develop
git checkout feature/cloud-sync
git rebase develop
```

Conflicts during rebase will most likely occur in `src/gui/DatabaseWidget.cpp` (the `reloadDatabaseFile` and `performSave` functions). Resolve by keeping the sync additions while incorporating the upstream change.
