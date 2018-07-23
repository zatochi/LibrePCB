/*
 * LibrePCB - Professional EDA for everyone!
 * Copyright (C) 2013 LibrePCB Developers, see AUTHORS.md for contributors.
 * http://librepcb.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include <QtWidgets>
#include "circlepropertiesdialog.h"
#include "ui_circlepropertiesdialog.h"
#include "../geometry/circle.h"
#include "../geometry/cmd/cmdcircleedit.h"
#include "../graphics/graphicslayer.h"
#include "../undostack.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

CirclePropertiesDialog::CirclePropertiesDialog(Circle& circle, UndoStack& undoStack,
        QList<GraphicsLayer*> layers, QWidget* parent) noexcept :
    QDialog(parent), mCircle(circle), mUndoStack(undoStack),
    mUi(new Ui::CirclePropertiesDialog)
{
    mUi->setupUi(this);

    foreach (const GraphicsLayer* layer, layers) {
        mUi->cbxLayer->addItem(layer->getNameTr(), layer->getName());
    }

    connect(mUi->buttonBox, &QDialogButtonBox::clicked,
            this, &CirclePropertiesDialog::buttonBoxClicked);

    // load circle attributes
    selectLayerNameInCombobox(mCircle.getLayerName());
    mUi->spbLineWidth->setValue(mCircle.getLineWidth()->toMm());
    mUi->cbxFillArea->setChecked(mCircle.isFilled());
    mUi->cbxIsGrabArea->setChecked(mCircle.isGrabArea());
    mUi->spbDiameter->setValue(mCircle.getDiameter()->toMm());
    mUi->spbPosX->setValue(mCircle.getCenter().getX().toMm());
    mUi->spbPosY->setValue(mCircle.getCenter().getY().toMm());
}

CirclePropertiesDialog::~CirclePropertiesDialog() noexcept
{
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void CirclePropertiesDialog::buttonBoxClicked(QAbstractButton* button) noexcept
{
    switch (mUi->buttonBox->buttonRole(button)) {
        case QDialogButtonBox::ApplyRole:
            applyChanges();
            break;
        case QDialogButtonBox::AcceptRole:
            if (applyChanges()) {
                accept();
            }
            break;
        case QDialogButtonBox::RejectRole:
            reject();
            break;
        default: Q_ASSERT(false); break;
    }
}

bool CirclePropertiesDialog::applyChanges() noexcept
{
    try {
        PositiveLength diameter = PositiveLength(Length::fromMm(mUi->spbDiameter->value())); // can throw

        QScopedPointer<CmdCircleEdit> cmd(new CmdCircleEdit(mCircle));
        if (mUi->cbxLayer->currentIndex() >= 0 && mUi->cbxLayer->currentData().isValid()) {
            cmd->setLayerName(mUi->cbxLayer->currentData().toString(), false);
        }
        cmd->setIsFilled(mUi->cbxFillArea->isChecked(), false);
        cmd->setIsGrabArea(mUi->cbxIsGrabArea->isChecked(), false);
        cmd->setLineWidth(UnsignedLength(Length::fromMm(mUi->spbLineWidth->value())), false); // can throw
        cmd->setDiameter(diameter, false);
        cmd->setCenter(Point::fromMm(mUi->spbPosX->value(), mUi->spbPosY->value()), false);
        mUndoStack.execCmd(cmd.take());
        return true;
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.getMsg());
        return false;
    }
}

void CirclePropertiesDialog::selectLayerNameInCombobox(const QString& name) noexcept
{
    mUi->cbxLayer->setCurrentIndex(mUi->cbxLayer->findData(name));
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
