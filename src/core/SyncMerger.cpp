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

#include "SyncMerger.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/SyncCheckpoint.h"

SyncMerger::SyncMerger(const Database* sourceDb, Database* targetDb)
    : m_sourceDb(sourceDb)
    , m_targetDb(targetDb)
    , m_checkpoint(SyncCheckpoint::fromDatabase(sourceDb))
{
}

// Classify entries into auto-merged and conflicted sets.
// This method never modifies either database — it is always effectively a dry-run.
// Actual merging is done by the existing Merger class in DatabaseWidget.
// applyResolutions() handles the conflict overrides after the user decides.
SyncMergeResult SyncMerger::merge(bool /*dryRun*/)
{
    SyncMergeResult result;

    const QHash<QUuid, const Entry*> sourceEntries = buildConstEntryMap(m_sourceDb);
    const QHash<QUuid, const Entry*> targetEntries = buildConstEntryMap(m_targetDb);

    QSet<QUuid> allUuids;
    for (const QUuid& uuid : sourceEntries.keys()) {
        allUuids.insert(uuid);
    }
    for (const QUuid& uuid : targetEntries.keys()) {
        allUuids.insert(uuid);
    }

    for (const QUuid& uuid : allUuids) {
        const Entry* sourceEntry = sourceEntries.value(uuid, nullptr);
        const Entry* targetEntry = targetEntries.value(uuid, nullptr);

        const QDateTime checkpointTime = m_checkpoint.entryTime(uuid);
        const bool inCheckpoint = checkpointTime.isValid();

        const bool localChanged = sourceEntry
            && (!inCheckpoint
                || sourceEntry->timeInfo().lastModificationTime() > checkpointTime);
        const bool remoteChanged = targetEntry
            && (!inCheckpoint
                || targetEntry->timeInfo().lastModificationTime() > checkpointTime);

        if (!sourceEntry || !targetEntry) {
            // Additions and deletions are handled correctly by the existing Merger;
            // no special conflict classification needed here.
            continue;
        }

        if (!localChanged || !remoteChanged) {
            // At most one side changed — existing Merger handles this correctly.
            if (localChanged) {
                result.autoMerged << Merger::Change(Merger::Change::Type::Modified,
                                                    *sourceEntry,
                                                    tr("local change"));
            } else if (remoteChanged) {
                result.autoMerged << Merger::Change(Merger::Change::Type::Modified,
                                                    *targetEntry,
                                                    tr("remote change"));
            }
            continue;
        }

        // Both sides changed since checkpoint — genuine conflict
        SyncConflict conflict;
        conflict.entryUuid = uuid;
        conflict.entryTitle = sourceEntry->title();
        if (sourceEntry->group()) {
            conflict.entryGroup = sourceEntry->group()->fullPath();
        }
        conflict.changedFields = sourceEntry->calculateDifference(targetEntry);
        result.conflicts.append(conflict);
    }

    return result;
}

// Apply user-chosen conflict resolutions to targetDb.
// Called AFTER the existing Merger has already run (which picked the newer
// timestamp winner).  This overrides those results with the explicit user choice.
void SyncMerger::applyResolutions(const QList<SyncConflict>& conflicts)
{
    const QHash<QUuid, const Entry*> sourceEntries = buildConstEntryMap(m_sourceDb);
    QHash<QUuid, Entry*> targetEntries = buildEntryMap(m_targetDb);

    for (const SyncConflict& conflict : conflicts) {
        if (conflict.resolution == SyncConflict::Resolution::Unresolved) {
            continue;
        }

        const Entry* sourceEntry = sourceEntries.value(conflict.entryUuid, nullptr);
        Entry* targetEntry = targetEntries.value(conflict.entryUuid, nullptr);

        if (!sourceEntry || !targetEntry) {
            continue;
        }

        if (conflict.resolution == SyncConflict::Resolution::KeepLocal) {
            // Replace targetEntry with a clone of sourceEntry.
            // Absorb targetEntry's history so remote edits aren't silently discarded.
            Group* targetGroup = targetEntry->group();
            if (!targetGroup) {
                continue;
            }
            Entry* cloned = sourceEntry->clone(Entry::CloneIncludeHistory);

            Entry* remoteSnapshot = targetEntry->clone(Entry::CloneNoFlags);
            cloned->addHistoryItem(remoteSnapshot);
            for (Entry* histItem : targetEntry->historyItems()) {
                cloned->addHistoryItem(histItem->clone(Entry::CloneNoFlags));
            }
            cloned->truncateHistory(m_targetDb->metadata()->historyMaxItems());

            targetEntry->setParent(nullptr);
            delete targetEntry;
            cloned->setParent(targetGroup);

        } else if (conflict.resolution == SyncConflict::Resolution::KeepRemote) {
            // targetEntry already has the remote version (Merger used it as the winner
            // or we left it in place).  Absorb source's history so local edits aren't lost.
            Entry* localSnapshot = sourceEntry->clone(Entry::CloneNoFlags);
            targetEntry->addHistoryItem(localSnapshot);
            for (Entry* histItem : sourceEntry->historyItems()) {
                targetEntry->addHistoryItem(histItem->clone(Entry::CloneNoFlags));
            }
            targetEntry->truncateHistory(m_targetDb->metadata()->historyMaxItems());
        }
    }
}

QHash<QUuid, const Entry*> SyncMerger::buildConstEntryMap(const Database* db)
{
    QHash<QUuid, const Entry*> map;
    if (!db || !db->rootGroup()) {
        return map;
    }
    for (const Entry* entry : db->rootGroup()->entriesRecursive()) {
        map[entry->uuid()] = entry;
    }
    return map;
}

QHash<QUuid, Entry*> SyncMerger::buildEntryMap(Database* db)
{
    QHash<QUuid, Entry*> map;
    if (!db || !db->rootGroup()) {
        return map;
    }
    for (Entry* entry : db->rootGroup()->entriesRecursive()) {
        map[entry->uuid()] = entry;
    }
    return map;
}
