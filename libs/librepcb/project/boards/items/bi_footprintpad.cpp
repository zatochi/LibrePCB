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
#include "bi_footprintpad.h"
#include "bi_footprint.h"
#include <librepcb/library/pkg/footprint.h>
#include <librepcb/library/pkg/footprintpad.h>
#include <librepcb/library/pkg/packagepad.h>
#include "../board.h"
#include "../../project.h"
#include "../../circuit/circuit.h"
#include "../../settings/projectsettings.h"
#include <librepcb/common/graphics/graphicsscene.h>
#include "bi_device.h"
#include <librepcb/library/dev/device.h>
#include "../../circuit/componentinstance.h"
#include <librepcb/library/cmp/component.h>
#include "bi_netpoint.h"
#include "../../circuit/componentsignalinstance.h"
#include <librepcb/common/graphics/graphicslayer.h>
#include "../../circuit/netsignal.h"
#include <librepcb/library/pkg/package.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace project {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

BI_FootprintPad::BI_FootprintPad(BI_Footprint& footprint, const Uuid& padUuid) :
    BI_Base(footprint.getBoard()), mFootprint(footprint), mFootprintPad(nullptr),
    mPackagePad(nullptr), mComponentSignalInstance(nullptr)
{
    mFootprintPad = mFootprint.getLibFootprint().getPads().get(padUuid).get(); // can throw
    mPackagePad = mFootprint.getDeviceInstance().getLibPackage().getPads().get(padUuid).get(); // can throw
    tl::optional<Uuid> cmpSignalUuid = mFootprint.getDeviceInstance().getLibDevice().getPadSignalMap().get(padUuid)->getSignalUuid(); // can throw
    if (cmpSignalUuid) {
        mComponentSignalInstance = mFootprint.getDeviceInstance().getComponentInstance().getSignalInstance(*cmpSignalUuid);
        connect(mComponentSignalInstance, &ComponentSignalInstance::netSignalChanged,
                this, &BI_FootprintPad::componentSignalInstanceNetSignalChanged);
    }

    mGraphicsItem.reset(new BGI_FootprintPad(*this));
    updatePosition();

    // connect to the "attributes changed" signal of the footprint
    connect(&mFootprint, &BI_Footprint::attributesChanged,
            this, &BI_FootprintPad::footprintAttributesChanged);
}

BI_FootprintPad::~BI_FootprintPad()
{
    Q_ASSERT(!isUsed());
    mGraphicsItem.reset();
}

/*****************************************************************************************
 *  Getters
 ****************************************************************************************/

const Uuid& BI_FootprintPad::getLibPadUuid() const noexcept
{
    return mFootprintPad->getPackagePadUuid();
}

QString BI_FootprintPad::getDisplayText() const noexcept
{
    NetSignal* signal = getCompSigInstNetSignal();
    if (signal) {
        return QString("%1:\n%2").arg(mPackagePad->getName(), signal->getName());
    } else {
        return mPackagePad->getName();
    }
}

QString BI_FootprintPad::getLayerName() const noexcept
{
    if (getIsMirrored())
        return GraphicsLayer::getMirroredLayerName(mFootprintPad->getLayerName());
    else
        return mFootprintPad->getLayerName();
}

bool BI_FootprintPad::isOnLayer(const QString& layerName) const noexcept
{
    if (getIsMirrored()) {
        return mFootprintPad->isOnLayer(GraphicsLayer::getMirroredLayerName(layerName));
    } else {
        return mFootprintPad->isOnLayer(layerName);
    }
}

NetSignal* BI_FootprintPad::getCompSigInstNetSignal() const noexcept
{
    if (mComponentSignalInstance) {
        return mComponentSignalInstance->getNetSignal();
    } else {
        return nullptr;
    }
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void BI_FootprintPad::addToBoard()
{
    if (isAddedToBoard() || isUsed()) {
        throw LogicError(__FILE__, __LINE__);
    }
    if (mComponentSignalInstance) {
        mComponentSignalInstance->registerFootprintPad(*this); // can throw
    }
    componentSignalInstanceNetSignalChanged(nullptr, getCompSigInstNetSignal());
    BI_Base::addToBoard(mGraphicsItem.data());
}

void BI_FootprintPad::removeFromBoard()
{
    if ((!isAddedToBoard()) || isUsed()) {
        throw LogicError(__FILE__, __LINE__);
    }
    if (mComponentSignalInstance) {
        mComponentSignalInstance->unregisterFootprintPad(*this); // can throw
    }
    componentSignalInstanceNetSignalChanged(getCompSigInstNetSignal(), nullptr);
    BI_Base::removeFromBoard(mGraphicsItem.data());
}

void BI_FootprintPad::registerNetPoint(BI_NetPoint& netpoint)
{
    if ((!isAddedToBoard()) || (!mComponentSignalInstance)
        || (netpoint.getBoard() != mBoard)
        || (mRegisteredNetPoints.contains(netpoint.getLayer().getName()))
        || (&netpoint.getNetSignalOfNetSegment() != mComponentSignalInstance->getNetSignal())
        || (!netpoint.getLayer().isCopperLayer())
        || (!isOnLayer(netpoint.getLayer().getName())))
    {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredNetPoints.insert(netpoint.getLayer().getName(), &netpoint);
    netpoint.updateLines();
}

void BI_FootprintPad::unregisterNetPoint(BI_NetPoint& netpoint)
{
    if ((!isAddedToBoard()) || (!mComponentSignalInstance)
        || (getNetPointOfLayer(netpoint.getLayer().getName()) != &netpoint)
        || (&netpoint.getNetSignalOfNetSegment() != mComponentSignalInstance->getNetSignal()))
    {
        throw LogicError(__FILE__, __LINE__);
    }
    mRegisteredNetPoints.remove(netpoint.getLayer().getName());
    netpoint.updateLines();
}

void BI_FootprintPad::updatePosition() noexcept
{
    mPosition = mFootprint.mapToScene(mFootprintPad->getPosition());
    mRotation = mFootprint.getRotation() + mFootprintPad->getRotation();
    mGraphicsItem->setPos(mPosition.toPxQPointF());
    updateGraphicsItemTransform();
    mGraphicsItem->updateCacheAndRepaint();
    foreach (BI_NetPoint* netpoint, mRegisteredNetPoints) {
        netpoint->setPosition(mPosition);
    }
}

/*****************************************************************************************
 *  Inherited from BI_Base
 ****************************************************************************************/

bool BI_FootprintPad::getIsMirrored() const noexcept
{
    return mFootprint.getIsMirrored();
}

QPainterPath BI_FootprintPad::getGrabAreaScenePx() const noexcept
{
    return mGraphicsItem->sceneTransform().map(mGraphicsItem->shape());
}

bool BI_FootprintPad::isSelectable() const noexcept
{
    return mFootprint.isSelectable() && mGraphicsItem->isSelectable();
}

void BI_FootprintPad::setSelected(bool selected) noexcept
{
    BI_Base::setSelected(selected);
    mGraphicsItem->update();
}

Path BI_FootprintPad::getOutline(const Length& expansion) const noexcept
{
    return mFootprintPad->getOutline(expansion);
}

Path BI_FootprintPad::getSceneOutline(const Length& expansion) const noexcept
{
    return getOutline(expansion).rotated(mRotation).translated(mPosition);
}

/*****************************************************************************************
 *  Private Slots
 ****************************************************************************************/

void BI_FootprintPad::footprintAttributesChanged()
{
    mGraphicsItem->updateCacheAndRepaint();
}

void BI_FootprintPad::componentSignalInstanceNetSignalChanged(NetSignal* from, NetSignal* to)
{
    if (mHighlightChangedConnection) {
        disconnect(mHighlightChangedConnection);
    }
    if (to) {
        mHighlightChangedConnection = connect(to, &NetSignal::highlightedChanged,
                                              [this](){mGraphicsItem->update();});
    }
    mBoard.scheduleAirWiresRebuild(from);
    mBoard.scheduleAirWiresRebuild(to);
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void BI_FootprintPad::updateGraphicsItemTransform() noexcept
{
    QTransform t;
    if (mFootprint.getIsMirrored()) t.scale(qreal(-1), qreal(1));
    t.rotate(-mRotation.toDeg());
    mGraphicsItem->setTransform(t);
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project
} // namespace librepcb
