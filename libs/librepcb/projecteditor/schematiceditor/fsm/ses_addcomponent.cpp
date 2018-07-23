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
#include "ses_addcomponent.h"
#include "../schematiceditor.h"
#include "ui_schematiceditor.h"
#include <librepcb/project/project.h>
#include <librepcb/library/cmp/component.h>
#include <librepcb/common/undostack.h>
#include <librepcb/project/circuit/componentinstance.h>
#include <librepcb/project/schematics/cmd/cmdsymbolinstanceedit.h>
#include <librepcb/project/schematics/items/si_symbol.h>
#include <librepcb/project/schematics/schematic.h>
#include "../../dialogs/addcomponentdialog.h"
#include <librepcb/common/gridproperties.h>
#include <librepcb/workspace/workspace.h>
#include <librepcb/library/sym/symbol.h>
#include "../../cmd/cmdaddcomponenttocircuit.h"
#include "../../cmd/cmdaddsymboltoschematic.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace project {
namespace editor {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

SES_AddComponent::SES_AddComponent(SchematicEditor& editor, Ui::SchematicEditor& editorUi,
                                   GraphicsView& editorGraphicsView, UndoStack& undoStack) :
    SES_Base(editor, editorUi, editorGraphicsView, undoStack),
    mIsUndoCmdActive(false), mAddComponentDialog(nullptr), mLastAngle(0),
    mCurrentComponent(nullptr),
    mCurrentSymbVarItemIndex(-1), mCurrentSymbolToPlace(nullptr),
    mCurrentSymbolEditCommand(nullptr)
{
}

SES_AddComponent::~SES_AddComponent()
{
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

SES_Base::ProcRetVal SES_AddComponent::process(SEE_Base* event) noexcept
{
    switch (event->getType())
    {
        case SEE_Base::AbortCommand:
        {
            if (mAddComponentDialog)
            {
                try
                {
                    if (!abortCommand(true)) return PassToParentState;
                    mLastAngle.setAngleMicroDeg(0); // reset the angle
                    startAddingComponent();
                    return ForceStayInState;
                }
                catch (UserCanceled& exc)
                {
                }
                catch (Exception& exc)
                {
                    QMessageBox::critical(&mEditor, tr("Error"), exc.getMsg());
                }
            }
            return PassToParentState;
        }
        case SEE_Base::StartAddComponent:
        {
            try
            {
                // start adding (another) component
                SEE_StartAddComponent* e = dynamic_cast<SEE_StartAddComponent*>(event);
                Q_ASSERT(e);
                if (!abortCommand(true)) return PassToParentState;
                mLastAngle.setAngleMicroDeg(0); // reset the angle
                startAddingComponent(e->getComponentUuid(), e->getSymbVarUuid());
                return ForceStayInState;
            }
            catch (UserCanceled& exc)
            {
            }
            catch (Exception& exc)
            {
                QMessageBox::critical(&mEditor, tr("Error"), exc.getMsg());
            }
            return PassToParentState;
        }
        case SEE_Base::Edit_RotateCW:
            mCurrentSymbolEditCommand->rotate(-Angle::deg90(), mCurrentSymbolToPlace->getPosition(), true);
            return ForceStayInState;
        case SEE_Base::Edit_RotateCCW:
            mCurrentSymbolEditCommand->rotate(Angle::deg90(), mCurrentSymbolToPlace->getPosition(), true);
            return ForceStayInState;
        case SEE_Base::GraphicsViewEvent:
            return processSceneEvent(event);
        default:
            return PassToParentState;
    }
}

bool SES_AddComponent::entry(SEE_Base* event) noexcept
{
    // only accept events of type SEE_StartAddComponent
    if (!event) return false;
    if (event->getType() != SEE_Base::StartAddComponent) return false;
    SEE_StartAddComponent* e = dynamic_cast<SEE_StartAddComponent*>(event);
    Q_ASSERT(e); if (!e) return false;
    Q_ASSERT(mIsUndoCmdActive == false);
    mLastAngle.setAngleMicroDeg(0);

    // start adding the specified component
    try
    {
        startAddingComponent(e->getComponentUuid(), e->getSymbVarUuid());
    }
    catch (UserCanceled& exc)
    {
        if (mIsUndoCmdActive) abortCommand(false);
        delete mAddComponentDialog; mAddComponentDialog = nullptr;
        return false;
    }
    catch (Exception& exc)
    {
        QMessageBox::critical(&mEditor, tr("Error"), QString(tr("Could not add component:\n\n%1")).arg(exc.getMsg()));
        if (mIsUndoCmdActive) abortCommand(false);
        delete mAddComponentDialog; mAddComponentDialog = nullptr;
        return false;
    }
    return true;
}

bool SES_AddComponent::exit(SEE_Base* event) noexcept
{
    Q_UNUSED(event);
    if (!abortCommand(true)) return false;
    Q_ASSERT(mIsUndoCmdActive == false);
    delete mAddComponentDialog;   mAddComponentDialog = nullptr;
    return true;
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

SES_Base::ProcRetVal SES_AddComponent::processSceneEvent(SEE_Base* event) noexcept
{
    QEvent* qevent = SEE_RedirectedQEvent::getQEventFromSEE(event);
    Q_ASSERT(qevent); if (!qevent) return PassToParentState;
    Schematic* schematic = mEditor.getActiveSchematic();
    Q_ASSERT(schematic); if (!schematic) return PassToParentState;
    if (!mIsUndoCmdActive) return PassToParentState; // temporary

    switch (qevent->type())
    {
        case QEvent::GraphicsSceneMouseMove:
        {
            QGraphicsSceneMouseEvent* sceneEvent = dynamic_cast<QGraphicsSceneMouseEvent*>(qevent);
            Q_ASSERT(sceneEvent);
            Point pos = Point::fromPx(sceneEvent->scenePos(), mEditor.getGridProperties().getInterval());
            // set temporary position of the current symbol
            Q_ASSERT(mCurrentSymbolEditCommand);
            mCurrentSymbolEditCommand->setPosition(pos, true);
            break;
        }

        case QEvent::GraphicsSceneMouseDoubleClick:
        case QEvent::GraphicsSceneMousePress:
        {
            QGraphicsSceneMouseEvent* sceneEvent = dynamic_cast<QGraphicsSceneMouseEvent*>(qevent);
            Q_ASSERT(sceneEvent);
            Point pos = Point::fromPx(sceneEvent->scenePos(), mEditor.getGridProperties().getInterval());
            switch (sceneEvent->button())
            {
                case Qt::LeftButton:
                {
                    try
                    {
                        // place the current symbol finally
                        mCurrentSymbolEditCommand->setPosition(pos, false);
                        mUndoStack.appendToCmdGroup(mCurrentSymbolEditCommand);
                        mCurrentSymbolEditCommand = nullptr;
                        mUndoStack.commitCmdGroup();
                        mIsUndoCmdActive = false;
                        mUndoStack.beginCmdGroup(tr("Add Symbol to Schematic"));
                        mIsUndoCmdActive = true;

                        // check if there is a next symbol to add
                        mCurrentSymbVarItemIndex++;
                        const library::ComponentSymbolVariantItem* currentSymbVarItem =
                                mCurrentComponent->getSymbolVariant().getSymbolItems()
                                .value(mCurrentSymbVarItemIndex).get();

                        if (currentSymbVarItem)
                        {
                            // create the next symbol instance and add it to the schematic
                            CmdAddSymbolToSchematic* cmd = new CmdAddSymbolToSchematic(
                                mWorkspace, *schematic,
                                mCurrentSymbolToPlace->getComponentInstance(),
                                currentSymbVarItem->getUuid(), pos);
                            mUndoStack.appendToCmdGroup(cmd);
                            mCurrentSymbolToPlace = cmd->getSymbolInstance();
                            Q_ASSERT(mCurrentSymbolToPlace);

                            // add command to move the current symbol
                            Q_ASSERT(mCurrentSymbolEditCommand == nullptr);
                            mCurrentSymbolEditCommand = new CmdSymbolInstanceEdit(*mCurrentSymbolToPlace);
                            mCurrentSymbolEditCommand->setRotation(mLastAngle, true);
                            return ForceStayInState;
                        }
                        else
                        {
                            // all symbols placed, start adding the next component
                            Uuid componentUuid = mCurrentComponent->getLibComponent().getUuid();
                            Uuid symbVarUuid = mCurrentComponent->getSymbolVariant().getUuid();
                            mUndoStack.commitCmdGroup();
                            mIsUndoCmdActive = false;
                            abortCommand(false); // reset attributes
                            startAddingComponent(componentUuid, symbVarUuid);
                            return ForceStayInState;
                        }
                    }
                    catch (Exception& e)
                    {
                        QMessageBox::critical(&mEditor, tr("Error"), e.getMsg());
                        abortCommand(false);
                        return ForceLeaveState;
                    }
                    break;
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
            Q_ASSERT(sceneEvent);
            switch (sceneEvent->button())
            {
                case Qt::RightButton:
                    if (sceneEvent->screenPos() == sceneEvent->buttonDownScreenPos(Qt::RightButton)) {
                        // rotate symbol
                        mLastAngle += Angle::deg90();
                        mCurrentSymbolEditCommand->setRotation(mLastAngle, true);
                        return ForceStayInState;
                    }
                    break;

                default:
                    break;
            }
            break;
        }

        default:
        {
            // Always accept graphics scene events, even if we do not react on some of the events!
            // This will give us the full control over the graphics scene. Otherwise, the graphics
            // scene can react on some events and disturb our state machine. Only the wheel event
            // is ignored because otherwise the view will not allow to zoom with the mouse wheel.
            if (qevent->type() != QEvent::GraphicsSceneWheel)
                return ForceStayInState;
            else
                return PassToParentState;
        }
    }
    return PassToParentState;
}

void SES_AddComponent::startAddingComponent(const tl::optional<Uuid>& cmp,
                                            const tl::optional<Uuid>& symbVar)
{
    Schematic* schematic = mEditor.getActiveSchematic();
    Q_ASSERT(schematic); if (!schematic) throw LogicError(__FILE__, __LINE__);

    try
    {
        // start a new command
        Q_ASSERT(!mIsUndoCmdActive);
        mUndoStack.beginCmdGroup(tr("Add Component to Schematic"));
        mIsUndoCmdActive = true;

        if (cmp && symbVar) {
            // add selected component to circuit
            auto* cmd = new CmdAddComponentToCircuit(mWorkspace, mProject, *cmp, *symbVar);
            mUndoStack.appendToCmdGroup(cmd);
            mCurrentComponent = cmd->getComponentInstance();
        } else {
            // show component chooser dialog
            if (!mAddComponentDialog)
                mAddComponentDialog = new AddComponentDialog(mWorkspace, mProject, &mEditor);
            if (mAddComponentDialog->exec() != QDialog::Accepted)
                throw UserCanceled(__FILE__, __LINE__); // abort
            if (!mAddComponentDialog->getSelectedComponentUuid())
                throw LogicError(__FILE__, __LINE__);
            if (!mAddComponentDialog->getSelectedSymbVarUuid())
                throw LogicError(__FILE__, __LINE__);

            // add selected component to circuit
            auto cmd = new CmdAddComponentToCircuit(mWorkspace, mProject,
                *mAddComponentDialog->getSelectedComponentUuid(),
                *mAddComponentDialog->getSelectedSymbVarUuid(),
                mAddComponentDialog->getSelectedDeviceUuid());
            mUndoStack.appendToCmdGroup(cmd);
            mCurrentComponent = cmd->getComponentInstance();
        }

        // create the first symbol instance and add it to the schematic
        mCurrentSymbVarItemIndex = 0;
        const library::ComponentSymbolVariantItem* currentSymbVarItem =
            mCurrentComponent->getSymbolVariant().getSymbolItems().value(mCurrentSymbVarItemIndex).get();
        if (!currentSymbVarItem) {
            throw RuntimeError(__FILE__, __LINE__,
                QString(tr("The component with the UUID \"%1\" does not have any symbol."))
                .arg(mCurrentComponent->getUuid().toStr()));
        }
        Point pos = mEditorGraphicsView.mapGlobalPosToScenePos(QCursor::pos(), true, true);
        CmdAddSymbolToSchematic* cmd2 = new CmdAddSymbolToSchematic(mWorkspace, *schematic,
            *mCurrentComponent, currentSymbVarItem->getUuid(), pos);
        mUndoStack.appendToCmdGroup(cmd2);
        mCurrentSymbolToPlace = cmd2->getSymbolInstance();
        Q_ASSERT(mCurrentSymbolToPlace);

        // add command to move the current symbol
        Q_ASSERT(mCurrentSymbolEditCommand == nullptr);
        mCurrentSymbolEditCommand = new CmdSymbolInstanceEdit(*mCurrentSymbolToPlace);
        mCurrentSymbolEditCommand->setRotation(mLastAngle, true);
    }
    catch (Exception& e)
    {
        if (mIsUndoCmdActive) {try {mUndoStack.abortCmdGroup(); mIsUndoCmdActive = false;} catch (...) {}}
        throw;
    }
}

bool SES_AddComponent::abortCommand(bool showErrMsgBox) noexcept
{
    try
    {
        // delete the current move command
        delete mCurrentSymbolEditCommand;
        mCurrentSymbolEditCommand = nullptr;

        // abort the undo command
        if (mIsUndoCmdActive)
        {
            mUndoStack.abortCmdGroup();
            mIsUndoCmdActive = false;
        }

        // reset attributes, go back to idle state
        mCurrentComponent = nullptr;
        mCurrentSymbVarItemIndex = -1;
        mCurrentSymbolToPlace = nullptr;
        return true;
    }
    catch (Exception& e)
    {
        if (showErrMsgBox) QMessageBox::critical(&mEditor, tr("Error"), e.getMsg());
        return false;
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace editor
} // namespace project
} // namespace librepcb
