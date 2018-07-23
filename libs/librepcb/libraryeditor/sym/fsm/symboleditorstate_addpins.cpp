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
#include "symboleditorstate_addpins.h"
#include <librepcb/common/graphics/graphicsview.h>
#include <librepcb/common/graphics/graphicsscene.h>
#include <librepcb/library/sym/symbol.h>
#include <librepcb/library/sym/symbolgraphicsitem.h>
#include <librepcb/library/sym/symbolpin.h>
#include <librepcb/library/sym/symbolpingraphicsitem.h>
#include <librepcb/library/sym/cmd/cmdsymbolpinedit.h>
#include "../symboleditorwidget.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace library {
namespace editor {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

SymbolEditorState_AddPins::SymbolEditorState_AddPins(const Context& context) noexcept :
    SymbolEditorState(context), mCurrentPin(nullptr), mCurrentGraphicsItem(nullptr),
    mNameLineEdit(nullptr), mLastLength(2540000)
{
}

SymbolEditorState_AddPins::~SymbolEditorState_AddPins() noexcept
{
    Q_ASSERT(mEditCmd.isNull());
    Q_ASSERT(mCurrentPin == nullptr);
    Q_ASSERT(mCurrentGraphicsItem == nullptr);
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

bool SymbolEditorState_AddPins::entry() noexcept
{
    mContext.graphicsScene.setSelectionArea(QPainterPath()); // clear selection
    mContext.graphicsView.setCursor(Qt::CrossCursor);

    // populate command toolbar
    mContext.commandToolBar.addLabel(tr("Name:"));
    mNameLineEdit = new QLineEdit();
    std::unique_ptr<QLineEdit> nameLineEdit(mNameLineEdit);
    nameLineEdit->setMaxLength(20);
    nameLineEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(nameLineEdit.get(), &QLineEdit::textEdited,
            this, &SymbolEditorState_AddPins::nameLineEditTextChanged);
    mContext.commandToolBar.addWidget(std::move(nameLineEdit));

    mContext.commandToolBar.addLabel(tr("Length:"), 10);
    std::unique_ptr<QDoubleSpinBox> lengthSpinBox(new QDoubleSpinBox());
    lengthSpinBox->setMinimum(0);
    lengthSpinBox->setMaximum(100);
    lengthSpinBox->setSingleStep(1.27);
    lengthSpinBox->setDecimals(6);
    lengthSpinBox->setValue(mLastLength->toMm());
    connect(lengthSpinBox.get(),
            static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this, &SymbolEditorState_AddPins::lengthSpinBoxValueChanged);
    mContext.commandToolBar.addWidget(std::move(lengthSpinBox));

    Point pos = mContext.graphicsView.mapGlobalPosToScenePos(QCursor::pos(), true, true);
    return addNextPin(pos, Angle::deg0());
}

bool SymbolEditorState_AddPins::exit() noexcept
{
    // abort command
    try {
        mCurrentGraphicsItem = nullptr;
        mCurrentPin = nullptr;
        mEditCmd.reset();
        mContext.undoStack.abortCmdGroup();
    } catch (const Exception& e) {
        QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
        return false;
    }

    // cleanup command toolbar
    mNameLineEdit = nullptr;
    mContext.commandToolBar.clear();

    mContext.graphicsView.setCursor(Qt::ArrowCursor);
    return true;
}

/*****************************************************************************************
 *  Event Handlers
 ****************************************************************************************/

bool SymbolEditorState_AddPins::processGraphicsSceneMouseMoved(QGraphicsSceneMouseEvent& e) noexcept
{
    Point currentPos = Point::fromPx(e.scenePos(), *getGridInterval());
    mEditCmd->setPosition(currentPos, true);
    return true;
}

bool SymbolEditorState_AddPins::processGraphicsSceneLeftMouseButtonPressed(QGraphicsSceneMouseEvent& e) noexcept
{
    Point currentPos = Point::fromPx(e.scenePos(), *getGridInterval());
    mEditCmd->setPosition(currentPos, true);
    Angle currentRot = mCurrentPin->getRotation();
    try {
        mCurrentGraphicsItem->setSelected(false);
        mCurrentGraphicsItem = nullptr;
        mCurrentPin = nullptr;
        mContext.undoStack.appendToCmdGroup(mEditCmd.take());
        mContext.undoStack.commitCmdGroup();
        return addNextPin(currentPos, currentRot);
    } catch (const Exception& e) {
        QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
        return false;
    }
}

bool SymbolEditorState_AddPins::processRotateCw() noexcept
{
    mEditCmd->rotate(-Angle::deg90(), mCurrentPin->getPosition(), true);
    return true;
}

bool SymbolEditorState_AddPins::processRotateCcw() noexcept
{
    mEditCmd->rotate(Angle::deg90(), mCurrentPin->getPosition(), true);
    return true;
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

bool SymbolEditorState_AddPins::addNextPin(const Point& pos, const Angle& rot) noexcept
{
    try {
        mNameLineEdit->setText(determineNextPinName());
        mContext.undoStack.beginCmdGroup(tr("Add symbol pin"));
        mCurrentPin = new SymbolPin(Uuid::createRandom(), mNameLineEdit->text(),
                                    pos, mLastLength, rot);

        mContext.undoStack.appendToCmdGroup(new CmdSymbolPinInsert(
            mContext.symbol.getPins(), std::shared_ptr<SymbolPin>(mCurrentPin)));
        mEditCmd.reset(new CmdSymbolPinEdit(*mCurrentPin));
        mCurrentGraphicsItem = mContext.symbolGraphicsItem.getPinGraphicsItem(mCurrentPin->getUuid());
        Q_ASSERT(mCurrentGraphicsItem);
        mCurrentGraphicsItem->setSelected(true);
        return true;
    } catch (const Exception& e) {
        QMessageBox::critical(&mContext.editorWidget, tr("Error"), e.getMsg());
        mCurrentGraphicsItem = nullptr;
        mCurrentPin = nullptr;
        mEditCmd.reset();
        return false;
    }
}

void SymbolEditorState_AddPins::nameLineEditTextChanged(const QString& text) noexcept
{
    if (mEditCmd && (!text.trimmed().isEmpty())) {
        mEditCmd->setName(text.trimmed(), true);
    }
}

void SymbolEditorState_AddPins::lengthSpinBoxValueChanged(double value) noexcept
{
    mLastLength = Length::fromMm(value);
    if (mEditCmd) {
        mEditCmd->setLength(mLastLength, true);
    }
}

QString SymbolEditorState_AddPins::determineNextPinName() const noexcept
{
    int i = 1;
    while (hasPin(QString::number(i))) ++i;
    return QString::number(i);
}

bool SymbolEditorState_AddPins::hasPin(const QString& name) const noexcept
{
    return mContext.symbol.getPins().contains(name);
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace editor
} // namespace library
} // namespace librepcb

