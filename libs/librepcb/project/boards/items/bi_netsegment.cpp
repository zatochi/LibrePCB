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
#include "bi_netsegment.h"
#include "bi_netline.h"
#include "bi_netpoint.h"
#include "bi_footprint.h"
#include "bi_footprintpad.h"
#include "bi_via.h"
#include "bi_device.h"
#include "../board.h"
#include "../../project.h"
#include "../../circuit/circuit.h"
#include "../../circuit/netsignal.h"
#include "../../circuit/componentsignalinstance.h"
#include <librepcb/common/scopeguardlist.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace project {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

BI_NetSegment::BI_NetSegment(Board& board, const BI_NetSegment& other,
                             const QHash<const BI_Device*, BI_Device*>& devMap) :
    BI_Base(board), mUuid(Uuid::createRandom()), mNetSignal(&other.getNetSignal())
{
    // copy vias
    QHash<const BI_Via*, BI_Via*> viaMap;
    foreach (const BI_Via* via, other.mVias) {
        BI_Via* copy = new BI_Via(*this, *via);
        Q_ASSERT(!getViaByUuid(copy->getUuid()));
        mVias.append(copy);
        viaMap.insert(via, copy);
    }
    // copy netpoints
    QHash<const BI_NetPoint*, BI_NetPoint*> copiedNetPoints;
    foreach (const BI_NetPoint* netpoint, other.mNetPoints) {
        BI_FootprintPad* pad = nullptr;
        if (netpoint->getFootprintPad()) {
            const BI_Device* oldDevice = &netpoint->getFootprintPad()->getFootprint().getDeviceInstance();
            const BI_Device* newDevice = devMap.value(oldDevice, nullptr); Q_ASSERT(newDevice);
            pad = newDevice->getFootprint().getPad(netpoint->getFootprintPad()->getLibPadUuid()); Q_ASSERT(pad);
        }
        BI_Via* via = viaMap.value(netpoint->getVia(), nullptr);
        BI_NetPoint* copy = new BI_NetPoint(*this, *netpoint, pad, via);
        mNetPoints.append(copy);
        copiedNetPoints.insert(netpoint, copy);
    }
    // copy netlines
    QList<BI_NetLine*> copiedNetLines;
    foreach (const BI_NetLine* netline, other.mNetLines) {
        BI_NetPoint* start = copiedNetPoints.value(&netline->getStartPoint()); Q_ASSERT(start);
        BI_NetPoint* end = copiedNetPoints.value(&netline->getEndPoint()); Q_ASSERT(end);
        BI_NetLine* copy = new BI_NetLine(*netline, *start, *end);
        mNetLines.append(copy);
        copiedNetLines.append(copy);
    }
}

BI_NetSegment::BI_NetSegment(Board& board, const SExpression& node) :
    BI_Base(board), mUuid(node.getChildByIndex(0).getValue<Uuid>()), mNetSignal(nullptr)
{
    try
    {
        Uuid netSignalUuid = node.getValueByPath<Uuid>("net");
        mNetSignal = mBoard.getProject().getCircuit().getNetSignalByUuid(netSignalUuid);
        if(!mNetSignal) {
            throw RuntimeError(__FILE__, __LINE__,
                QString(tr("Invalid net signal UUID: \"%1\"")).arg(netSignalUuid.toStr()));
        }

        // Load all vias
        foreach (const SExpression& node, node.getChildren("via")) {
            BI_Via* via = new BI_Via(*this, node);
            if (getViaByUuid(via->getUuid())) {
                throw RuntimeError(__FILE__, __LINE__,
                    QString(tr("There is already a via with the UUID \"%1\"!"))
                    .arg(via->getUuid().toStr()));
            }
            mVias.append(via);
        }

        // Load all netpoints
        foreach (const SExpression& node, node.getChildren("netpoint")) {
            BI_NetPoint* netpoint = new BI_NetPoint(*this, node);
            if (getNetPointByUuid(netpoint->getUuid())) {
                throw RuntimeError(__FILE__, __LINE__,
                    QString(tr("There is already a netpoint with the UUID \"%1\"!"))
                    .arg(netpoint->getUuid().toStr()));
            }
            mNetPoints.append(netpoint);
        }

        // Load all netlines
        foreach (const SExpression& node, node.getChildren("netline")) {
            BI_NetLine* netline = new BI_NetLine(*this, node);
            if (getNetLineByUuid(netline->getUuid())) {
                throw RuntimeError(__FILE__, __LINE__,
                    QString(tr("There is already a netline with the UUID \"%1\"!"))
                    .arg(netline->getUuid().toStr()));
            }
            mNetLines.append(netline);
        }

        if (!areAllNetPointsConnectedTogether()) {
            throw RuntimeError(__FILE__, __LINE__,
                QString(tr("The netsegment with the UUID \"%1\" is not cohesive!"))
                .arg(mUuid.toStr()));
        }

        if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);
    }
    catch (...)
    {
        // free the allocated memory in the reverse order of their allocation...
        qDeleteAll(mNetLines);          mNetLines.clear();
        qDeleteAll(mNetPoints);         mNetPoints.clear();
        qDeleteAll(mVias);              mVias.clear();
        throw; // ...and rethrow the exception
    }
}

BI_NetSegment::BI_NetSegment(Board& board, NetSignal& signal) :
    BI_Base(board), mUuid(Uuid::createRandom()), mNetSignal(&signal)
{
}

BI_NetSegment::~BI_NetSegment() noexcept
{
    // delete all items
    qDeleteAll(mNetLines);          mNetLines.clear();
    qDeleteAll(mNetPoints);         mNetPoints.clear();
    qDeleteAll(mVias);              mVias.clear();
}

/*****************************************************************************************
 *  Getters
 ****************************************************************************************/

bool BI_NetSegment::isUsed() const noexcept
{
    return ((!mVias.isEmpty()) || (!mNetPoints.isEmpty()) || (!mNetLines.isEmpty()));
}

int BI_NetSegment::getViasAtScenePos(const Point& pos, QList<BI_Via*>& vias) const noexcept
{
    int count = 0;
    foreach (BI_Via* via, mVias) {
        if (via->isSelectable() && via->getGrabAreaScenePx().contains(pos.toPxQPointF()))
        {
            vias.append(via);
            ++count;
        }
    }
    return count;
}

int BI_NetSegment::getNetPointsAtScenePos(const Point& pos, const GraphicsLayer* layer,
                                          QList<BI_NetPoint*>& points) const noexcept
{
    int count = 0;
    foreach (BI_NetPoint* netpoint, mNetPoints) {
        if (netpoint->isSelectable()
            && netpoint->getGrabAreaScenePx().contains(pos.toPxQPointF())
            && ((!layer) || (&netpoint->getLayer() == layer)))
        {
            points.append(netpoint);
            ++count;
        }
    }
    return count;
}

int BI_NetSegment::getNetLinesAtScenePos(const Point& pos, const GraphicsLayer* layer,
                                         QList<BI_NetLine*>& lines) const noexcept
{
    int count = 0;
    foreach (BI_NetLine* netline, mNetLines) {
        if (netline->isSelectable()
            && netline->getGrabAreaScenePx().contains(pos.toPxQPointF())
            && ((!layer) || (&netline->getLayer() == layer)))
        {
            lines.append(netline);
            ++count;
        }
    }
    return count;
}

/*****************************************************************************************
 *  Setters
 ****************************************************************************************/

void BI_NetSegment::setNetSignal(NetSignal& netsignal)
{
    if (&netsignal != mNetSignal) {
        if ((isUsed() && isAddedToBoard()) || (netsignal.getCircuit() != getCircuit())) {
            throw LogicError(__FILE__, __LINE__);
        }
        if (isAddedToBoard()) {
            mNetSignal->unregisterBoardNetSegment(*this); // can throw
            auto sg = scopeGuard([&](){mNetSignal->registerBoardNetSegment(*this);});
            netsignal.registerBoardNetSegment(*this); // can throw
            sg.dismiss();
        }
        mNetSignal = &netsignal;
    }
}

/*****************************************************************************************
 *  Via Methods
 ****************************************************************************************/

BI_Via* BI_NetSegment::getViaByUuid(const Uuid& uuid) const noexcept
{
    foreach (BI_Via* via, mVias) {
        if (via->getUuid() == uuid)
            return via;
    }
    return nullptr;
}

/*****************************************************************************************
 *  NetPoint Methods
 ****************************************************************************************/

BI_NetPoint* BI_NetSegment::getNetPointByUuid(const Uuid& uuid) const noexcept
{
    foreach (BI_NetPoint* netpoint, mNetPoints) {
        if (netpoint->getUuid() == uuid)
            return netpoint;
    }
    return nullptr;
}

/*****************************************************************************************
 *  NetLine Methods
 ****************************************************************************************/

BI_NetLine* BI_NetSegment::getNetLineByUuid(const Uuid& uuid) const noexcept
{
    foreach (BI_NetLine* netline, mNetLines) {
        if (netline->getUuid() == uuid)
            return netline;
    }
    return nullptr;
}

/*****************************************************************************************
 *  NetPoint+NetLine Methods
 ****************************************************************************************/

void BI_NetSegment::addElements(const QList<BI_Via*>& vias,
                                const QList<BI_NetPoint*>& netpoints,
                                const QList<BI_NetLine*>& netlines)
{
    if (!isAddedToBoard()) {
        throw LogicError(__FILE__, __LINE__);
    }

    ScopeGuardList sgl(netpoints.count() + netlines.count());
    foreach (BI_Via* via, vias) {
        if ((mVias.contains(via)) || (&via->getNetSegment() != this)) {
            throw LogicError(__FILE__, __LINE__);
        }
        // check if there is no via with the same uuid in the list
        if (getViaByUuid(via->getUuid())) {
            throw RuntimeError(__FILE__, __LINE__,
                QString(tr("There is already a via with the UUID \"%1\"!"))
                .arg(via->getUuid().toStr()));
        }
        // add to board
        via->addToBoard(); // can throw
        mVias.append(via);
        sgl.add([this, via](){via->removeFromBoard(); mVias.removeOne(via);});
    }
    foreach (BI_NetPoint* netpoint, netpoints) {
        if ((mNetPoints.contains(netpoint)) || (&netpoint->getNetSegment() != this)) {
            throw LogicError(__FILE__, __LINE__);
        }
        // check if there is no netpoint with the same uuid in the list
        if (getNetPointByUuid(netpoint->getUuid())) {
            throw RuntimeError(__FILE__, __LINE__,
                QString(tr("There is already a netpoint with the UUID \"%1\"!"))
                .arg(netpoint->getUuid().toStr()));
        }
        // add to board
        netpoint->addToBoard(); // can throw
        mNetPoints.append(netpoint);
        sgl.add([this, netpoint](){netpoint->removeFromBoard(); mNetPoints.removeOne(netpoint);});
    }
    foreach (BI_NetLine* netline, netlines) {
        if ((mNetLines.contains(netline)) || (&netline->getNetSegment() != this)) {
            throw LogicError(__FILE__, __LINE__);
        }
        // check if there is no netline with the same uuid in the list
        if (getNetLineByUuid(netline->getUuid())) {
            throw RuntimeError(__FILE__, __LINE__,
                QString(tr("There is already a netline with the UUID \"%1\"!"))
                .arg(netline->getUuid().toStr()));
        }
        // add to board
        netline->addToBoard(); // can throw
        mNetLines.append(netline);
        sgl.add([this, netline](){netline->removeFromBoard(); mNetLines.removeOne(netline);});
    }

    if (!areAllNetPointsConnectedTogether()) {
        throw LogicError(__FILE__, __LINE__,
            QString(tr("The netsegment with the UUID \"%1\" is not cohesive!"))
            .arg(mUuid.toStr()));
    }

    sgl.dismiss();
}

void BI_NetSegment::removeElements(const QList<BI_Via*>& vias,
                                   const QList<BI_NetPoint*>& netpoints,
                                   const QList<BI_NetLine*>& netlines)
{
    if (!isAddedToBoard()) {
        throw LogicError(__FILE__, __LINE__);
    }

    ScopeGuardList sgl(netpoints.count() + netlines.count());
    foreach (BI_NetLine* netline, netlines) {
        if (!mNetLines.contains(netline)) {
            throw LogicError(__FILE__, __LINE__);
        }
        // remove from board
        netline->removeFromBoard(); // can throw
        mNetLines.removeOne(netline);
        sgl.add([this, netline](){netline->addToBoard(); mNetLines.append(netline);});
    }
    foreach (BI_NetPoint* netpoint, netpoints) {
        if (!mNetPoints.contains(netpoint)) {
            throw LogicError(__FILE__, __LINE__);
        }
        // remove from board
        netpoint->removeFromBoard(); // can throw
        mNetPoints.removeOne(netpoint);
        sgl.add([this, netpoint](){netpoint->addToBoard(); mNetPoints.append(netpoint);});
    }
    foreach (BI_Via* via, vias) {
        if (!mVias.contains(via)) {
            throw LogicError(__FILE__, __LINE__);
        }
        // remove from board
        via->removeFromBoard(); // can throw
        mVias.removeOne(via);
        sgl.add([this, via](){via->addToBoard(); mVias.append(via);});
    }

    if (!areAllNetPointsConnectedTogether()) {
        throw LogicError(__FILE__, __LINE__,
            QString(tr("The netsegment with the UUID \"%1\" is not cohesive!"))
            .arg(mUuid.toStr()));
    }

    sgl.dismiss();
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void BI_NetSegment::addToBoard()
{
    if (isAddedToBoard()) {
        throw LogicError(__FILE__, __LINE__);
    }

    ScopeGuardList sgl(mNetPoints.count() + mNetLines.count() + 1);
    mNetSignal->registerBoardNetSegment(*this); // can throw
    sgl.add([&](){mNetSignal->unregisterBoardNetSegment(*this);});
    foreach (BI_Via* via, mVias) {
        via->addToBoard(); // can throw
        sgl.add([via](){via->removeFromBoard();});
    }
    foreach (BI_NetPoint* netpoint, mNetPoints) {
        netpoint->addToBoard(); // can throw
        sgl.add([netpoint](){netpoint->removeFromBoard();});
    }
    foreach (BI_NetLine* netline, mNetLines) {
        netline->addToBoard(); // can throw
        sgl.add([netline](){netline->removeFromBoard();});
    }

    BI_Base::addToBoard(nullptr);
    sgl.dismiss();
}

void BI_NetSegment::removeFromBoard()
{
    if ((!isAddedToBoard())) {
        throw LogicError(__FILE__, __LINE__);
    }

    ScopeGuardList sgl(mNetPoints.count() + mNetLines.count() + 1);
    foreach (BI_NetLine* netline, mNetLines) {
        netline->removeFromBoard(); // can throw
        sgl.add([netline](){netline->addToBoard();});
    }
    foreach (BI_NetPoint* netpoint, mNetPoints) {
        netpoint->removeFromBoard(); // can throw
        sgl.add([netpoint](){netpoint->addToBoard();});
    }
    foreach (BI_Via* via, mVias) {
        via->removeFromBoard(); // can throw
        sgl.add([via](){via->addToBoard();});
    }
    mNetSignal->unregisterBoardNetSegment(*this); // can throw
    sgl.add([&](){mNetSignal->registerBoardNetSegment(*this);});

    BI_Base::removeFromBoard(nullptr);
    sgl.dismiss();
}

void BI_NetSegment::setSelectionRect(const QRectF rectPx) noexcept
{
    foreach (BI_Via* via, mVias)
        via->setSelected(via->isSelectable() && via->getGrabAreaScenePx().intersects(rectPx));
    foreach (BI_NetPoint* netpoint, mNetPoints)
        netpoint->setSelected(netpoint->isSelectable() && netpoint->getGrabAreaScenePx().intersects(rectPx));
    foreach (BI_NetLine* netline, mNetLines)
        netline->setSelected(netline->isSelectable() && netline->getGrabAreaScenePx().intersects(rectPx));
}

void BI_NetSegment::clearSelection() const noexcept
{
    foreach (BI_Via* via, mVias)
        via->setSelected(false);
    foreach (BI_NetPoint* netpoint, mNetPoints)
        netpoint->setSelected(false);
    foreach (BI_NetLine* netline, mNetLines)
        netline->setSelected(false);
}

void BI_NetSegment::serialize(SExpression& root) const
{
    if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);

    root.appendChild(mUuid);
    root.appendChild("net", mNetSignal->getUuid(), true);
    serializePointerContainerUuidSorted(root, mVias, "via");
    serializePointerContainerUuidSorted(root, mNetPoints, "netpoint");
    serializePointerContainerUuidSorted(root, mNetLines, "netline");
}

/*****************************************************************************************
 *  Inherited from SI_Base
 ****************************************************************************************/

QPainterPath BI_NetSegment::getGrabAreaScenePx() const noexcept
{
    return QPainterPath();
}

bool BI_NetSegment::isSelected() const noexcept
{
    if (mNetLines.isEmpty()) return false;
    foreach (const BI_NetLine* netline, mNetLines) {
        if (!netline->isSelected()) return false;
    }
    return true;
}

void BI_NetSegment::setSelected(bool selected) noexcept
{
    BI_Base::setSelected(selected);
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

bool BI_NetSegment::checkAttributesValidity() const noexcept
{
    if (mNetSignal == nullptr)                  return false;
    if (!areAllNetPointsConnectedTogether())    return false;
    return true;
}

bool BI_NetSegment::areAllNetPointsConnectedTogether() const noexcept
{
    if (mNetPoints.count() > 1) {
        const BI_NetPoint* firstPoint = mNetPoints.first();
        QList<const BI_NetPoint*> points({firstPoint});
        findAllConnectedNetPoints(*firstPoint, points);
        return (points.count() == mNetPoints.count());
    } else {
        return true; // there is only 0 or 1 netpoint => must be "connected together" :)
    }
}

void BI_NetSegment::findAllConnectedNetPoints(const BI_NetPoint& p, QList<const BI_NetPoint*>& points) const noexcept
{
    if (p.isAttachedToVia()) { Q_ASSERT(p.getVia());
        foreach (const BI_NetPoint* np, mNetPoints) {
            if ((np->getVia() == p.getVia()) && (!points.contains(np))) {
                points.append(np);
                findAllConnectedNetPoints(*np, points);
            }
        }
    }
    foreach (const BI_NetLine* line, mNetLines) {
        const BI_NetPoint* p2 = line->getOtherPoint(p);
        if ((p2) && (!points.contains(p2))) {
            points.append(p2);
            findAllConnectedNetPoints(*p2, points);
        }
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project
} // namespace librepcb
