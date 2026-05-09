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

#include "SyncConflictDialog.h"

#include "ui_SyncConflictDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QTableWidgetItem>

SyncConflictDialog::SyncConflictDialog(QList<SyncConflict> conflicts, QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::SyncConflictDialog)
    , m_conflicts(std::move(conflicts))
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    populateTable();

    connect(m_ui->keepAllLocalButton, &QPushButton::clicked, this, &SyncConflictDialog::keepAllLocal);
    connect(m_ui->keepAllRemoteButton, &QPushButton::clicked, this, &SyncConflictDialog::keepAllRemote);
}

SyncConflictDialog::~SyncConflictDialog() = default;

void SyncConflictDialog::populateTable()
{
    auto* table = m_ui->conflictTable;
    table->setRowCount(m_conflicts.size());
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide();

    for (int row = 0; row < m_conflicts.size(); ++row) {
        const SyncConflict& conflict = m_conflicts.at(row);

        table->setItem(row, 0, new QTableWidgetItem(conflict.entryTitle));
        table->setItem(row, 1, new QTableWidgetItem(conflict.entryGroup));
        table->setItem(row, 2, new QTableWidgetItem(conflict.changedFields.join(QStringLiteral(", "))));

        auto* combo = new QComboBox(this);
        combo->addItem(tr("Keep Local"), static_cast<int>(SyncConflict::Resolution::KeepLocal));
        combo->addItem(tr("Keep Remote"), static_cast<int>(SyncConflict::Resolution::KeepRemote));
        table->setCellWidget(row, ResolutionColumn, combo);
        m_combos.append(combo);
    }
}

QList<SyncConflict> SyncConflictDialog::resolvedConflicts() const
{
    QList<SyncConflict> resolved = m_conflicts;
    for (int i = 0; i < m_combos.size() && i < resolved.size(); ++i) {
        const int data = m_combos.at(i)->currentData().toInt();
        resolved[i].resolution = static_cast<SyncConflict::Resolution>(data);
    }
    return resolved;
}

void SyncConflictDialog::keepAllLocal()
{
    setAllResolutions(SyncConflict::Resolution::KeepLocal);
}

void SyncConflictDialog::keepAllRemote()
{
    setAllResolutions(SyncConflict::Resolution::KeepRemote);
}

void SyncConflictDialog::setAllResolutions(SyncConflict::Resolution resolution)
{
    const int index = (resolution == SyncConflict::Resolution::KeepLocal) ? 0 : 1;
    for (QComboBox* combo : m_combos) {
        combo->setCurrentIndex(index);
    }
}
