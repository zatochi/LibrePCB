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
#include "boarddesignrulesdialog.h"
#include "ui_boarddesignrulesdialog.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

BoardDesignRulesDialog::BoardDesignRulesDialog(const BoardDesignRules& rules, QWidget* parent) :
    QDialog(parent), mUi(new Ui::BoardDesignRulesDialog), mDesignRules(rules)
{
    mUi->setupUi(this);

    updateWidgets();
}

BoardDesignRulesDialog::~BoardDesignRulesDialog()
{
    delete mUi;     mUi = nullptr;
}

/*****************************************************************************************
 *  Private Slots
 ****************************************************************************************/

void BoardDesignRulesDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    switch (mUi->buttonBox->buttonRole(button))
    {
        case QDialogButtonBox::ApplyRole:
        case QDialogButtonBox::AcceptRole:
            applyRules();
            emit rulesChanged(mDesignRules);
            break;

        case QDialogButtonBox::ResetRole:
            mDesignRules.restoreDefaults();
            updateWidgets();
            emit rulesChanged(mDesignRules);
            break;

        default:
            break;
    }
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void BoardDesignRulesDialog::updateWidgets() noexcept
{
    // general attributes
    mUi->edtName->setText(mDesignRules.getName());
    mUi->txtDescription->setPlainText(mDesignRules.getDescription());
    // stop mask
    mUi->spbxStopMaskClrRatio->setValue(mDesignRules.getStopMaskClearanceRatio().toPercent());
    mUi->spbxStopMaskClrMin->setValue(mDesignRules.getStopMaskClearanceMin()->toMm());
    mUi->spbxStopMaskClrMax->setValue(mDesignRules.getStopMaskClearanceMax()->toMm());
    mUi->spbxStopMaskMaxViaDia->setValue(mDesignRules.getStopMaskMaxViaDiameter()->toMm());
    // cream mask
    mUi->spbxCreamMaskClrRatio->setValue(mDesignRules.getCreamMaskClearanceRatio().toPercent());
    mUi->spbxCreamMaskClrMin->setValue(mDesignRules.getCreamMaskClearanceMin()->toMm());
    mUi->spbxCreamMaskClrMax->setValue(mDesignRules.getCreamMaskClearanceMax()->toMm());
    // restring
    mUi->spbxRestringPadsRatio->setValue(mDesignRules.getRestringPadRatio().toPercent());
    mUi->spbxRestringPadsMin->setValue(mDesignRules.getRestringPadMin()->toMm());
    mUi->spbxRestringPadsMax->setValue(mDesignRules.getRestringPadMax()->toMm());
    mUi->spbxRestringViasRatio->setValue(mDesignRules.getRestringViaRatio().toPercent());
    mUi->spbxRestringViasMin->setValue(mDesignRules.getRestringViaMin()->toMm());
    mUi->spbxRestringViasMax->setValue(mDesignRules.getRestringViaMax()->toMm());
}

void BoardDesignRulesDialog::applyRules() noexcept
{
    try {
        // general attributes
        mDesignRules.setName(mUi->edtName->text());
        mDesignRules.setDescription(mUi->txtDescription->toPlainText());
        // stop mask
        mDesignRules.setStopMaskClearanceRatio(Ratio::fromPercent(mUi->spbxStopMaskClrRatio->value()));
        mDesignRules.setStopMaskClearanceMin(UnsignedLength(Length::fromMm(mUi->spbxStopMaskClrMin->value()))); // can throw
        mDesignRules.setStopMaskClearanceMax(UnsignedLength(Length::fromMm(mUi->spbxStopMaskClrMax->value()))); // can throw
        mDesignRules.setStopMaskMaxViaDiameter(UnsignedLength(Length::fromMm(mUi->spbxStopMaskMaxViaDia->value()))); // can throw
        // cream mask
        mDesignRules.setCreamMaskClearanceRatio(Ratio::fromPercent(mUi->spbxCreamMaskClrRatio->value()));
        mDesignRules.setCreamMaskClearanceMin(UnsignedLength(Length::fromMm(mUi->spbxCreamMaskClrMin->value()))); // can throw
        mDesignRules.setCreamMaskClearanceMax(UnsignedLength(Length::fromMm(mUi->spbxCreamMaskClrMax->value()))); // can throw
        // restring
        mDesignRules.setRestringPadRatio(Ratio::fromPercent(mUi->spbxRestringPadsRatio->value()));
        mDesignRules.setRestringPadMin(UnsignedLength(Length::fromMm(mUi->spbxRestringPadsMin->value()))); // can throw
        mDesignRules.setRestringPadMax(UnsignedLength(Length::fromMm(mUi->spbxRestringPadsMax->value()))); // can throw
        mDesignRules.setRestringViaRatio(Ratio::fromPercent(mUi->spbxRestringViasRatio->value()));
        mDesignRules.setRestringViaMin(UnsignedLength(Length::fromMm(mUi->spbxRestringViasMin->value()))); // can throw
        mDesignRules.setRestringViaMax(UnsignedLength(Length::fromMm(mUi->spbxRestringViasMax->value()))); // can throw
    } catch (const Exception& e) {
        QMessageBox::warning(this, tr("Could not apply settings"), e.getMsg());
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
