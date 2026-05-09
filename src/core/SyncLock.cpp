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

#include "SyncLock.h"

#include "core/Config.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QUuid>

SyncLock::SyncLock(const QString& dbPath)
    : m_dbPath(dbPath)
    , m_lockFilePath(lockFilePath(dbPath))
{
}

SyncLock::~SyncLock()
{
    release();
}

void SyncLock::acquire()
{
    QJsonObject obj;
    obj[QStringLiteral("deviceName")] = QSysInfo::machineHostName();
    obj[QStringLiteral("deviceId")] = deviceId();
    obj[QStringLiteral("openedAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QFile file(m_lockFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        m_acquired = true;
    }
}

void SyncLock::release()
{
    if (m_acquired) {
        QFile::remove(m_lockFilePath);
        m_acquired = false;
    }
}

bool SyncLock::isLockedByOther(LockInfo* info) const
{
    QFile file(m_lockFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject obj = doc.object();
    const QString lockDeviceId = obj.value(QStringLiteral("deviceId")).toString();

    // Owned by us
    if (lockDeviceId == deviceId()) {
        return false;
    }

    const QDateTime openedAt =
        QDateTime::fromString(obj.value(QStringLiteral("openedAt")).toString(), Qt::ISODateWithMs);

    // Stale lock
    if (!openedAt.isValid() || openedAt.secsTo(QDateTime::currentDateTimeUtc()) > StaleHours * 3600) {
        return false;
    }

    if (info) {
        info->deviceName = obj.value(QStringLiteral("deviceName")).toString();
        info->deviceId = lockDeviceId;
        info->openedAt = openedAt;
    }
    return true;
}

QString SyncLock::deviceId()
{
    const QString key = QStringLiteral("SyncDeviceId");
    QString id = config()->get(Config::SyncDeviceId).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        config()->set(Config::SyncDeviceId, id);
    }
    return id;
}

QString SyncLock::lockFilePath(const QString& dbPath)
{
    return dbPath + QStringLiteral(".sync-lock");
}
