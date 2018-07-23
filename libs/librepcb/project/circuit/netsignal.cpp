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
#include "netsignal.h"
#include "netclass.h"
#include <librepcb/common/exceptions.h>
#include "circuit.h"
#include "../erc/ercmsg.h"
#include "componentsignalinstance.h"
#include "../schematics/items/si_netsegment.h"
#include "../boards/items/bi_netsegment.h"
#include "../boards/items/bi_plane.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace project {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

NetSignal::NetSignal(Circuit& circuit, const SExpression& node) :
    QObject(&circuit), mCircuit(circuit), mIsAddedToCircuit(false), mIsHighlighted(false),
    mUuid(node.getChildByIndex(0).getValue<Uuid>()),
    mName(node.getValueByPath<QString>("name", true)),
    mHasAutoName(node.getValueByPath<bool>("auto")),
    mNetClass(nullptr)
{
    Uuid netclassUuid = node.getValueByPath<Uuid>("netclass");
    mNetClass = circuit.getNetClassByUuid(netclassUuid);
    if (!mNetClass) {
        throw RuntimeError(__FILE__, __LINE__,
            QString(tr("Invalid netclass UUID: \"%1\""))
            .arg(netclassUuid.toStr()));
    }

    if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);
}

NetSignal::NetSignal(Circuit& circuit, NetClass& netclass, const QString& name,
                     bool autoName) :
    QObject(&circuit), mCircuit(circuit), mIsAddedToCircuit(false), mIsHighlighted(false),
    mUuid(Uuid::createRandom()), mName(name), mHasAutoName(autoName), mNetClass(&netclass)
{
    if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);
}

NetSignal::~NetSignal() noexcept
{
    Q_ASSERT(!mIsAddedToCircuit);
    Q_ASSERT(!isUsed());
}

/*****************************************************************************************
 *  Getters: General
 ****************************************************************************************/

int NetSignal::getRegisteredElementsCount() const noexcept
{
    int count = 0;
    count += mRegisteredComponentSignals.count();
    count += mRegisteredSchematicNetSegments.count();
    count += mRegisteredBoardNetSegments.count();
    count += mRegisteredBoardPlanes.count();
    return count;
}

bool NetSignal::isUsed() const noexcept
{
    return (getRegisteredElementsCount() > 0);
}

bool NetSignal::isNameForced() const noexcept
{
    foreach (const ComponentSignalInstance* cmp, mRegisteredComponentSignals) {
        if (cmp->isNetSignalNameForced()) {
            return true;
        }
    }
    return false;
}

/*****************************************************************************************
 *  Setters
 ****************************************************************************************/

void NetSignal::setName(const QString& name, bool isAutoName)
{
    if ((name == mName) && (isAutoName == mHasAutoName)) {
        return;
    }
    if(name.isEmpty()) {
        throw RuntimeError(__FILE__, __LINE__,
            tr("The new netsignal name must not be empty!"));
    }
    mName = name;
    mHasAutoName = isAutoName;
    updateErcMessages();
    emit nameChanged(mName);
}

void NetSignal::setHighlighted(bool hl) noexcept
{
    if (hl != mIsHighlighted) {
        mIsHighlighted = hl;
        emit highlightedChanged(mIsHighlighted);
    }
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void NetSignal::addToCircuit()
{
    if (mIsAddedToCircuit || isUsed()) {
        throw LogicError(__FILE__, __LINE__);
    }
    mNetClass->registerNetSignal(*this); // can throw
    mIsAddedToCircuit = true;
    updateErcMessages();
}

void NetSignal::removeFromCircuit()
{
    if (!mIsAddedToCircuit) {
        throw LogicError(__FILE__, __LINE__);
    }
    if (isUsed()) {
        throw RuntimeError(__FILE__, __LINE__,
            QString(tr("The net signal \"%1\" cannot be removed because it is still in use!"))
            .arg(mName));
    }
    mNetClass->unregisterNetSignal(*this); // can throw
    mIsAddedToCircuit = false;
    updateErcMessages();
}

void NetSignal::registerComponentSignal(ComponentSignalInstance& signal)
{
    if ((!mIsAddedToCircuit) || (mRegisteredComponentSignals.contains(&signal))
        || (signal.getCircuit() != mCircuit))
    {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredComponentSignals.append(&signal);
    updateErcMessages();
}

void NetSignal::unregisterComponentSignal(ComponentSignalInstance& signal)
{
    if ((!mIsAddedToCircuit) || (!mRegisteredComponentSignals.contains(&signal))) {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredComponentSignals.removeOne(&signal);
    updateErcMessages();
}

void NetSignal::registerSchematicNetSegment(SI_NetSegment& netsegment)
{
    if ((!mIsAddedToCircuit) || (mRegisteredSchematicNetSegments.contains(&netsegment))
        || (netsegment.getCircuit() != mCircuit))
    {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredSchematicNetSegments.append(&netsegment);
    updateErcMessages();
}

void NetSignal::unregisterSchematicNetSegment(SI_NetSegment& netsegment)
{
    if ((!mIsAddedToCircuit) || (!mRegisteredSchematicNetSegments.contains(&netsegment))) {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredSchematicNetSegments.removeOne(&netsegment);
    updateErcMessages();
}

void NetSignal::registerBoardNetSegment(BI_NetSegment& netsegment)
{
    if ((!mIsAddedToCircuit) || (mRegisteredBoardNetSegments.contains(&netsegment))
        || (netsegment.getCircuit() != mCircuit))
    {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredBoardNetSegments.append(&netsegment);
    updateErcMessages();
}

void NetSignal::unregisterBoardNetSegment(BI_NetSegment& netsegment)
{
    if ((!mIsAddedToCircuit) || (!mRegisteredBoardNetSegments.contains(&netsegment))) {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredBoardNetSegments.removeOne(&netsegment);
    updateErcMessages();
}

void NetSignal::registerBoardPlane(BI_Plane& plane)
{
    if ((!mIsAddedToCircuit) || (mRegisteredBoardPlanes.contains(&plane))
        || (plane.getCircuit() != mCircuit))
    {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredBoardPlanes.append(&plane);
    updateErcMessages();
}

void NetSignal::unregisterBoardPlane(BI_Plane& plane)
{
    if ((!mIsAddedToCircuit) || (!mRegisteredBoardPlanes.contains(&plane))) {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredBoardPlanes.removeOne(&plane);
    updateErcMessages();
}

void NetSignal::serialize(SExpression& root) const
{
    if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);

    root.appendChild(mUuid);
    root.appendChild("auto", mHasAutoName, false);
    root.appendChild("name", mName, false);
    root.appendChild("netclass", mNetClass->getUuid(), true);
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

bool NetSignal::checkAttributesValidity() const noexcept
{
    if (mName.isEmpty())        return false;
    if (mNetClass == nullptr)   return false;
    return true;
}

void NetSignal::updateErcMessages() noexcept
{
    if (mIsAddedToCircuit && (!isUsed())) {
        if (!mErcMsgUnusedNetSignal) {
            mErcMsgUnusedNetSignal.reset(new ErcMsg(mCircuit.getProject(), *this,
                mUuid.toStr(), "Unused", ErcMsg::ErcMsgType_t::CircuitError, QString()));
        }
        mErcMsgUnusedNetSignal->setMsg(QString(tr("Unused net signal: \"%1\"")).arg(mName));
        mErcMsgUnusedNetSignal->setVisible(true);
    }
    else if (mErcMsgUnusedNetSignal) {
        mErcMsgUnusedNetSignal.reset();
    }

    if (mIsAddedToCircuit && (mRegisteredComponentSignals.count() < 2)) {
        if (!mErcMsgConnectedToLessThanTwoPins) {
            mErcMsgConnectedToLessThanTwoPins.reset(new ErcMsg(mCircuit.getProject(), *this,
                mUuid.toStr(), "ConnectedToLessThanTwoPins", ErcMsg::ErcMsgType_t::CircuitWarning));
        }
        mErcMsgConnectedToLessThanTwoPins->setMsg(
            QString(tr("Net signal connected to less than two pins: \"%1\"")).arg(mName));
        mErcMsgConnectedToLessThanTwoPins->setVisible(true);
    }
    else if (mErcMsgConnectedToLessThanTwoPins) {
        mErcMsgConnectedToLessThanTwoPins.reset();
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project
} // namespace librepcb
