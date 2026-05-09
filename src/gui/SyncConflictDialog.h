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

#ifndef KEEPASSXC_SYNCCONFLICTDIALOG_H
#define KEEPASSXC_SYNCCONFLICTDIALOG_H

#include "core/SyncMerger.h"

#include <QComboBox>
#include <QDialog>
#include <QScopedPointer>

namespace Ui
{
    class SyncConflictDialog;
}

// Presents a list of sync conflicts to the user.  For each conflicting entry
// the user picks "Keep Local" or "Keep Remote" via a per-row combo box.
// Call resolvedConflicts() after exec() == QDialog::Accepted to retrieve
// the updated SyncConflict list with resolutions filled in.
class SyncConflictDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SyncConflictDialog(QList<SyncConflict> conflicts, QWidget* parent = nullptr);
    ~SyncConflictDialog() override;

    // Returns conflicts with resolution set by the user.
    // Only valid after exec() == Accepted.
    QList<SyncConflict> resolvedConflicts() const;

private slots:
    void keepAllLocal();
    void keepAllRemote();

private:
    static constexpr int ResolutionColumn = 3;

    void populateTable();
    void setAllResolutions(SyncConflict::Resolution resolution);

    QScopedPointer<Ui::SyncConflictDialog> m_ui;
    QList<SyncConflict> m_conflicts;
    QList<QComboBox*> m_combos;
};

#endif // KEEPASSXC_SYNCCONFLICTDIALOG_H
