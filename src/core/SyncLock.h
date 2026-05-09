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

#ifndef KEEPASSXC_SYNCLOCK_H
#define KEEPASSXC_SYNCLOCK_H

#include <QDateTime>
#include <QString>

// Writes a small JSON sidecar file (<db>.sync-lock) when a database is opened.
// On another device, if the cloud client syncs the lock file down, KeePassXC
// will warn the user that the database appears to be open elsewhere.
//
// The lock is advisory (soft lock): it warns but does not prevent editing.
// Lock files older than 24 hours are treated as stale and silently ignored.
class SyncLock
{
public:
    struct LockInfo
    {
        QString deviceName;
        QString deviceId;
        QDateTime openedAt;
    };

    explicit SyncLock(const QString& dbPath);
    ~SyncLock();

    // Write this device's lock file.  Safe to call multiple times.
    void acquire();

    // Delete this device's lock file.  Safe to call when not acquired.
    void release();

    // Returns true if a non-stale lock file exists for a DIFFERENT device.
    // Populates `info` when returning true.
    bool isLockedByOther(LockInfo* info = nullptr) const;

    // Returns a persistent device ID stored in application config.
    static QString deviceId();

private:
    static QString lockFilePath(const QString& dbPath);
    static constexpr int StaleHours = 24;

    QString m_dbPath;
    QString m_lockFilePath;
    bool m_acquired = false;
};

#endif // KEEPASSXC_SYNCLOCK_H
