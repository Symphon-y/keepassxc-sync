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

#ifndef KEEPASSXC_SYNCCHECKPOINT_H
#define KEEPASSXC_SYNCCHECKPOINT_H

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QUuid>

class Database;

// Stores a snapshot of per-entry modification timestamps at the moment the
// database was last saved.  Used as the common ancestor for 3-way merging:
// changes on each device are measured relative to this baseline so that
// non-conflicting edits can be merged automatically.
//
// The checkpoint is persisted inside the database's own Metadata CustomData
// (key "sync.checkpoint") so it travels with the file across devices.
class SyncCheckpoint
{
public:
    // Read the stored checkpoint from the database's Metadata CustomData.
    // Returns empty() if none exists yet (first sync).
    static SyncCheckpoint fromDatabase(const Database* db);

    // Snapshot the current in-memory state of all entries/groups.
    // Call this after a successful save to record the new baseline.
    static SyncCheckpoint snapshotDatabase(const Database* db);

    // Return an empty (epoch) checkpoint — used when no prior sync exists.
    static SyncCheckpoint empty();

    // Persist into db->metadata()->customData().  Does NOT save the database.
    void saveToDatabase(Database* db) const;

    QDateTime entryTime(const QUuid& uuid) const;
    QDateTime groupTime(const QUuid& uuid) const;
    bool wasDeleted(const QUuid& uuid) const;
    QDateTime syncedAt() const;

    bool isValid() const;

private:
    QDateTime m_syncedAt;
    QHash<QUuid, QDateTime> m_entryTimes;
    QHash<QUuid, QDateTime> m_groupTimes;
    QSet<QUuid> m_deletedUuids;

    static const QString CustomDataKey;
};

#endif // KEEPASSXC_SYNCCHECKPOINT_H
