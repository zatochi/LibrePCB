/*
 * EDA4U - Professional EDA for everyone!
 * Copyright (C) 2013 Urban Bruhin
 * http://eda4u.ubruhin.ch/
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
#include "cmdsymbolinstancemove.h"
#include "../symbolinstance.h"

namespace project {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

CmdSymbolInstanceMove::CmdSymbolInstanceMove(SymbolInstance& symbol, UndoCommand* parent) throw (Exception) :
    UndoCommand(QCoreApplication::translate("CmdSymbolInstanceMove", "Move symbol"), parent),
    mSymbolInstance(symbol), mStartPos(symbol.getPosition()), mDeltaPos(0, 0),
    mEndPos(symbol.getPosition()), mStartAngle(symbol.getAngle()), mDeltaAngle(0),
    mEndAngle(symbol.getAngle()), mRedoOrUndoCalled(false)
{
}

CmdSymbolInstanceMove::~CmdSymbolInstanceMove() noexcept
{
    if ((!mRedoOrUndoCalled) && (!mDeltaPos.isOrigin()))
    {
        mSymbolInstance.setPosition(mStartPos);
        mSymbolInstance.setAngle(mStartAngle);
    }
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void CmdSymbolInstanceMove::setAbsolutePosTemporary(Point& absPos) noexcept
{
    Q_ASSERT(mRedoOrUndoCalled == false);
    mDeltaPos = absPos - mStartPos;
    mSymbolInstance.setPosition(absPos);
}

void CmdSymbolInstanceMove::setDeltaToStartPosTemporary(Point& deltaPos) noexcept
{
    Q_ASSERT(mRedoOrUndoCalled == false);
    mDeltaPos = deltaPos;
    mSymbolInstance.setPosition(mStartPos + mDeltaPos);
}

void CmdSymbolInstanceMove::rotate(const Angle& angle, const Point& center) noexcept
{
    Q_ASSERT(mRedoOrUndoCalled == false);
    mDeltaPos = Point(mStartPos + mDeltaPos).rotated(angle, center) - mStartPos;
    mDeltaAngle += angle;
    mSymbolInstance.setPosition(mStartPos + mDeltaPos);
    mSymbolInstance.setAngle(mStartAngle + mDeltaAngle);
}

/*****************************************************************************************
 *  Inherited from UndoCommand
 ****************************************************************************************/

void CmdSymbolInstanceMove::redo() throw (Exception)
{
    mRedoOrUndoCalled = true;
    UndoCommand::redo(); // throws an exception on error
    mEndPos = mStartPos + mDeltaPos;
    mEndAngle = mStartAngle + mDeltaAngle;
    mSymbolInstance.setPosition(mEndPos);
    mSymbolInstance.setAngle(mEndAngle);
}

void CmdSymbolInstanceMove::undo() throw (Exception)
{
    mRedoOrUndoCalled = true;
    UndoCommand::undo();
    mSymbolInstance.setPosition(mStartPos);
    mSymbolInstance.setAngle(mStartAngle);
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project