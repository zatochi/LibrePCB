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
#include "ses_addnetlabel.h"
#include "../schematiceditor.h"
#include "ui_schematiceditor.h"
#include <librepcb/common/undostack.h>
#include <librepcb/project/circuit/circuit.h>
#include <librepcb/project/schematics/items/si_netlabel.h>
#include <librepcb/project/schematics/items/si_netsegment.h>
#include <librepcb/project/schematics/cmd/cmdschematicnetlabeladd.h>
#include <librepcb/project/schematics/cmd/cmdschematicnetlabeledit.h>
#include <librepcb/project/schematics/schematic.h>
#include <librepcb/project/schematics/items/si_netline.h>
#include <librepcb/common/gridproperties.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

SES_AddNetLabel::SES_AddNetLabel(SchematicEditor& editor, Ui::SchematicEditor& editorUi,
                                 GraphicsView& editorGraphicsView, UndoStack& undoStack) :
    SES_Base(editor, editorUi, editorGraphicsView, undoStack),
    mUndoCmdActive(false), mCurrentNetLabel(nullptr), mEditCmd(nullptr)
{
}

SES_AddNetLabel::~SES_AddNetLabel()
{
    Q_ASSERT(mUndoCmdActive == false);
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

SES_Base::ProcRetVal SES_AddNetLabel::process(SEE_Base* event) noexcept
{
    switch (event->getType())
    {
        case SEE_Base::GraphicsViewEvent:
            return processSceneEvent(event);
        default:
            return PassToParentState;
    }
}

bool SES_AddNetLabel::entry(SEE_Base* event) noexcept
{
    Q_UNUSED(event);
    Schematic* schematic = mEditor.getActiveSchematic();
    if (!schematic) return false;

    // change the cursor
    mEditorGraphicsView.setCursor(Qt::CrossCursor);

    return true;
}

bool SES_AddNetLabel::exit(SEE_Base* event) noexcept
{
    Q_UNUSED(event);

    if (mUndoCmdActive)
    {
        try
        {
            mUndoStack.abortCmdGroup();
            mUndoCmdActive = false;
        }
        catch (Exception& e)
        {
            QMessageBox::critical(&mEditor, tr("Error"), e.getMsg());
            return false;
        }
    }

    // change the cursor
    mEditorGraphicsView.setCursor(Qt::ArrowCursor);

    return true;
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

SES_Base::ProcRetVal SES_AddNetLabel::processSceneEvent(SEE_Base* event) noexcept
{
    QEvent* qevent = SEE_RedirectedQEvent::getQEventFromSEE(event);
    Q_ASSERT(qevent); if (!qevent) return PassToParentState;
    Schematic* schematic = mEditor.getActiveSchematic();
    Q_ASSERT(schematic); if (!schematic) return PassToParentState;

    switch (qevent->type())
    {
        case QEvent::GraphicsSceneMouseDoubleClick:
        case QEvent::GraphicsSceneMousePress:
        {
            QGraphicsSceneMouseEvent* sceneEvent = dynamic_cast<QGraphicsSceneMouseEvent*>(qevent);
            Point pos = Point::fromPx(sceneEvent->scenePos(), *mEditor.getGridProperties().getInterval());
            switch (sceneEvent->button())
            {
                case Qt::LeftButton:
                {
                    if (mUndoCmdActive) {
                        fixLabel(pos);
                    } else {
                        addLabel(*schematic, pos);
                    }
                    return ForceStayInState;
                }
                case Qt::RightButton:
                    return ForceStayInState;
                default:
                    break;
            }
            break;
        }

        case QEvent::GraphicsSceneMouseRelease:
        {
            QGraphicsSceneMouseEvent* sceneEvent = dynamic_cast<QGraphicsSceneMouseEvent*>(qevent);
            Point pos = Point::fromPx(sceneEvent->scenePos(), *mEditor.getGridProperties().getInterval());
            switch (sceneEvent->button())
            {
                case Qt::RightButton:
                {
                    if (mUndoCmdActive && (sceneEvent->screenPos() == sceneEvent->buttonDownScreenPos(Qt::RightButton))) {
                        mEditCmd->rotate(Angle::deg90(), pos, true);
                        return ForceStayInState;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }

        case QEvent::GraphicsSceneMouseMove:
        {
            QGraphicsSceneMouseEvent* sceneEvent = dynamic_cast<QGraphicsSceneMouseEvent*>(qevent);
            Q_ASSERT(sceneEvent);
            Point pos = Point::fromPx(sceneEvent->scenePos(), *mEditor.getGridProperties().getInterval());
            updateLabel(pos);
            return ForceStayInState;
        }

        default:
            break;
    }
    return PassToParentState;
}

bool SES_AddNetLabel::addLabel(Schematic& schematic, const Point& pos) noexcept
{
    Q_ASSERT(mUndoCmdActive == false);

    try
    {
        QList<SI_NetLine*> netlinesUnderCursor = schematic.getNetLinesAtScenePos(pos);
        if (netlinesUnderCursor.isEmpty()) return false;
        SI_NetSegment& netsegment = netlinesUnderCursor.first()->getNetSegment();

        mUndoStack.beginCmdGroup(tr("Add net label to schematic"));
        mUndoCmdActive = true;
        CmdSchematicNetLabelAdd* cmdAdd = new CmdSchematicNetLabelAdd(netsegment, pos);
        mUndoStack.appendToCmdGroup(cmdAdd);
        mCurrentNetLabel = cmdAdd->getNetLabel();
        mEditCmd = new CmdSchematicNetLabelEdit(*mCurrentNetLabel);
        return true;
    }
    catch (Exception& e)
    {
        if (mUndoCmdActive)
        {
            try {mUndoStack.abortCmdGroup();} catch (...) {}
            mUndoCmdActive = false;
        }
        QMessageBox::critical(&mEditor, tr("Error"), e.getMsg());
        return false;
    }
}

bool SES_AddNetLabel::updateLabel(const Point& pos) noexcept
{
    if (mUndoCmdActive) {
        mEditCmd->setPosition(pos, true);
        return true;
    } else {
        return false;
    }
}

bool SES_AddNetLabel::fixLabel(const Point& pos) noexcept
{
    if (!mUndoCmdActive) return false;

    try
    {
        mEditCmd->setPosition(pos, false);
        mUndoStack.appendToCmdGroup(mEditCmd);
        mUndoStack.commitCmdGroup();
        mUndoCmdActive = false;
        return true;
    }
    catch (Exception& e)
    {
        if (mUndoCmdActive)
        {
            try {mUndoStack.abortCmdGroup();} catch (...) {}
            mUndoCmdActive = false;
        }
        QMessageBox::critical(&mEditor, tr("Error"), e.getMsg());
        return false;
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace editor
} // namespace project
} // namespace librepcb
