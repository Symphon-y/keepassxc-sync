/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_SYNCMERGER_H
#define KEEPASSXC_SYNCMERGER_H

#include "core/Merger.h"
#include "core/SyncCheckpoint.h"

#include <QHash>
#include <QList>
#include <QUuid>

class Database;
class Entry;

// A conflict occurs when the same entry was modified on BOTH the local device
// (sourceDb) and the remote device (targetDb) since the last sync checkpoint.
struct SyncConflict
{
    enum class Resolution
    {
        Unresolved,
        KeepLocal,  // keep sourceDb version
        KeepRemote, // keep targetDb version
    };

    QUuid entryUuid;
    QString entryTitle;
    QString entryGroup;
    QStringList changedFields;
    Resolution resolution = Resolution::Unresolved;
};

struct SyncMergeResult
{
    Merger::ChangeList autoMerged;
    QList<SyncConflict> conflicts;

    bool hasConflicts() const
    {
        return !conflicts.isEmpty();
    }
};

// 3-way conflict classifier that matches the existing Merger(source, target) convention:
//   sourceDb  — the in-memory local database (has the user's unsaved edits).
//               The sync checkpoint is read from this database's CustomData.
//   targetDb  — the freshly loaded remote database (just read from disk).
//
// Usage pattern in DatabaseWidget::reloadDatabaseFile:
//   SyncMerger sm(m_db.get(), db.get());
//   SyncMergeResult result = sm.merge();
//   if (result.hasConflicts()) { /* show dialog */ }
//   Merger(m_db.get(), db.get()).merge();      // existing merger applies auto changes
//   sm.applyResolutions(resolvedConflicts);    // override conflict entries with user choice
class SyncMerger : public QObject
{
    Q_OBJECT
public:
    SyncMerger(const Database* sourceDb, Database* targetDb);

    // Classify entries: auto-merged (one side changed) vs conflicts (both sides changed).
    // Does NOT modify either database.  The dryRun parameter is accepted for API
    // symmetry but has no effect — this method is always non-destructive.
    SyncMergeResult merge(bool dryRun = false);

    // Apply user-chosen conflict resolutions to targetDb.  Call this AFTER the
    // existing Merger has run so that conflict entries get the correct final value.
    void applyResolutions(const QList<SyncConflict>& conflicts);

private:
    static QHash<QUuid, const Entry*> buildConstEntryMap(const Database* db);
    static QHash<QUuid, Entry*> buildEntryMap(Database* db);

    const Database* m_sourceDb;
    Database* m_targetDb;
    SyncCheckpoint m_checkpoint;
};

#endif // KEEPASSXC_SYNCMERGER_H
