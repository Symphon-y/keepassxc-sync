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

#include "SyncCheckpoint.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

const QString SyncCheckpoint::CustomDataKey = QStringLiteral("sync.checkpoint");

SyncCheckpoint SyncCheckpoint::fromDatabase(const Database* db)
{
    if (!db || !db->metadata()) {
        return empty();
    }
    const QString json = db->metadata()->customData()->value(CustomDataKey);
    if (json.isEmpty()) {
        return empty();
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return empty();
    }

    const QJsonObject root = doc.object();
    SyncCheckpoint cp;
    cp.m_syncedAt = QDateTime::fromString(root.value(QStringLiteral("syncedAt")).toString(), Qt::ISODateWithMs);

    const QJsonObject entries = root.value(QStringLiteral("entries")).toObject();
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QUuid uuid = QUuid::fromString(it.key());
        if (!uuid.isNull()) {
            cp.m_entryTimes[uuid] = QDateTime::fromString(it.value().toString(), Qt::ISODateWithMs);
        }
    }

    const QJsonObject groups = root.value(QStringLiteral("groups")).toObject();
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const QUuid uuid = QUuid::fromString(it.key());
        if (!uuid.isNull()) {
            cp.m_groupTimes[uuid] = QDateTime::fromString(it.value().toString(), Qt::ISODateWithMs);
        }
    }

    const QJsonArray deleted = root.value(QStringLiteral("deletedUuids")).toArray();
    for (const auto& val : deleted) {
        const QUuid uuid = QUuid::fromString(val.toString());
        if (!uuid.isNull()) {
            cp.m_deletedUuids.insert(uuid);
        }
    }

    return cp;
}

SyncCheckpoint SyncCheckpoint::snapshotDatabase(const Database* db)
{
    if (!db || !db->rootGroup()) {
        return empty();
    }

    SyncCheckpoint cp;
    cp.m_syncedAt = QDateTime::currentDateTimeUtc();

    for (const Entry* entry : db->rootGroup()->entriesRecursive()) {
        cp.m_entryTimes[entry->uuid()] = entry->timeInfo().lastModificationTime();
    }

    for (const Group* group : db->rootGroup()->groupsRecursive(true)) {
        cp.m_groupTimes[group->uuid()] = group->timeInfo().lastModificationTime();
    }

    for (const DeletedObject& del : db->deletedObjects()) {
        cp.m_deletedUuids.insert(del.uuid);
    }

    return cp;
}

SyncCheckpoint SyncCheckpoint::empty()
{
    SyncCheckpoint cp;
    // m_syncedAt is default-constructed (null/invalid)
    return cp;
}

void SyncCheckpoint::saveToDatabase(Database* db) const
{
    if (!db || !db->metadata()) {
        return;
    }

    QJsonObject root;
    root[QStringLiteral("syncedAt")] = m_syncedAt.toString(Qt::ISODateWithMs);

    QJsonObject entries;
    for (auto it = m_entryTimes.constBegin(); it != m_entryTimes.constEnd(); ++it) {
        entries[it.key().toString(QUuid::WithBraces)] = it.value().toString(Qt::ISODateWithMs);
    }
    root[QStringLiteral("entries")] = entries;

    QJsonObject groups;
    for (auto it = m_groupTimes.constBegin(); it != m_groupTimes.constEnd(); ++it) {
        groups[it.key().toString(QUuid::WithBraces)] = it.value().toString(Qt::ISODateWithMs);
    }
    root[QStringLiteral("groups")] = groups;

    QJsonArray deleted;
    for (const QUuid& uuid : m_deletedUuids) {
        deleted.append(uuid.toString(QUuid::WithBraces));
    }
    root[QStringLiteral("deletedUuids")] = deleted;

    const QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    db->metadata()->customData()->set(CustomDataKey, json);
}

QDateTime SyncCheckpoint::entryTime(const QUuid& uuid) const
{
    return m_entryTimes.value(uuid);
}

QDateTime SyncCheckpoint::groupTime(const QUuid& uuid) const
{
    return m_groupTimes.value(uuid);
}

bool SyncCheckpoint::wasDeleted(const QUuid& uuid) const
{
    return m_deletedUuids.contains(uuid);
}

QDateTime SyncCheckpoint::syncedAt() const
{
    return m_syncedAt;
}

bool SyncCheckpoint::isValid() const
{
    return m_syncedAt.isValid();
}
