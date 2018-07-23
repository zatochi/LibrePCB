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
#include "primitivepathgraphicsitem.h"
#include "../toolbox.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

PrimitivePathGraphicsItem::PrimitivePathGraphicsItem(QGraphicsItem* parent) noexcept :
    QGraphicsItem(parent), mLineLayer(nullptr), mFillLayer(nullptr)
{
    mPen.setCapStyle(Qt::RoundCap);
    mPenHighlighted.setCapStyle(Qt::RoundCap);
    mPen.setJoinStyle(Qt::RoundJoin);
    mPenHighlighted.setJoinStyle(Qt::RoundJoin);
    mPen.setWidthF(0);
    mPenHighlighted.setWidthF(0);
    updateColors();
    updateBoundingRectAndShape();
    updateVisibility();
}

PrimitivePathGraphicsItem::~PrimitivePathGraphicsItem() noexcept
{
    // unregister from graphics layers
    setLineLayer(nullptr);
    setFillLayer(nullptr);
}

/*****************************************************************************************
 *  Setters
 ****************************************************************************************/

void PrimitivePathGraphicsItem::setPosition(const Point& pos) noexcept
{
    QGraphicsItem::setPos(pos.toPxQPointF());
}

void PrimitivePathGraphicsItem::setRotation(const Angle& rot) noexcept
{
    QGraphicsItem::setRotation(-rot.toDeg());
}

void PrimitivePathGraphicsItem::setPath(const QPainterPath& path) noexcept
{
    mPainterPath = path;
    updateBoundingRectAndShape();
}

void PrimitivePathGraphicsItem::setLineWidth(const UnsignedLength& width) noexcept
{
    mPen.setWidthF(width->toPx());
    mPenHighlighted.setWidthF(width->toPx());
    updateBoundingRectAndShape();
}

void PrimitivePathGraphicsItem::setLineLayer(const GraphicsLayer* layer) noexcept
{
    if (mLineLayer) {
        mLineLayer->unregisterObserver(*this);
    }
    mLineLayer = layer;
    if (mLineLayer) {
        mLineLayer->registerObserver(*this);
    }
    updateColors();
    updateVisibility();
    updateBoundingRectAndShape(); // grab area may have changed
}

void PrimitivePathGraphicsItem::setFillLayer(const GraphicsLayer* layer) noexcept
{
    if (mFillLayer) {
        mFillLayer->unregisterObserver(*this);
    }
    mFillLayer = layer;
    if (mFillLayer) {
        mFillLayer->registerObserver(*this);
    }
    updateColors();
    updateVisibility();
    updateBoundingRectAndShape(); // grab area may have changed
}

/*****************************************************************************************
 *  Inherited from IF_LayerObserver
 ****************************************************************************************/

void PrimitivePathGraphicsItem::layerColorChanged(const GraphicsLayer& layer, const QColor& newColor) noexcept
{
    Q_UNUSED(layer);
    Q_UNUSED(newColor);
    updateColors();
    updateVisibility();
}

void PrimitivePathGraphicsItem::layerHighlightColorChanged(const GraphicsLayer& layer, const QColor& newColor) noexcept
{
    Q_UNUSED(layer);
    Q_UNUSED(newColor);
    updateColors();
    updateVisibility();
}

void PrimitivePathGraphicsItem::layerVisibleChanged(const GraphicsLayer& layer, bool newVisible) noexcept
{
    Q_UNUSED(layer);
    Q_UNUSED(newVisible);
    updateColors();
    updateVisibility();
}

void PrimitivePathGraphicsItem::layerEnabledChanged(const GraphicsLayer& layer, bool newEnabled) noexcept
{
    Q_UNUSED(layer);
    Q_UNUSED(newEnabled);
    updateColors();
    updateVisibility();
}

void PrimitivePathGraphicsItem::layerDestroyed(const GraphicsLayer& layer) noexcept
{
    if (&layer == mLineLayer) {
        setLineLayer(nullptr);
    } else if (&layer == mFillLayer) {
        setFillLayer(nullptr);
    } else {
        Q_ASSERT(false);
    }
}

/*****************************************************************************************
 *  Inherited from QGraphicsItem
 ****************************************************************************************/

void PrimitivePathGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) noexcept
{
    Q_UNUSED(widget);
    if (option->state.testFlag(QStyle::State_Selected)) {
        painter->setPen(mPenHighlighted);
        painter->setBrush(mBrushHighlighted);
    } else {
        painter->setPen(mPen);
        painter->setBrush(mBrush);
    }
    painter->drawPath(mPainterPath);
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

void PrimitivePathGraphicsItem::updateColors() noexcept
{
    if (mLineLayer && mLineLayer->isVisible()) {
        mPen.setStyle(Qt::SolidLine);
        mPenHighlighted.setStyle(Qt::SolidLine);
        mPen.setColor(mLineLayer->getColor(false));
        mPenHighlighted.setColor(mLineLayer->getColor(true));
    } else {
        mPen.setStyle(Qt::NoPen);
        mPenHighlighted.setStyle(Qt::NoPen);
    }

    if (mFillLayer && mFillLayer->isVisible()) {
        mBrush.setStyle(Qt::SolidPattern);
        mBrushHighlighted.setStyle(Qt::SolidPattern);
        mBrush.setColor(mFillLayer->getColor(false));
        mBrushHighlighted.setColor(mFillLayer->getColor(true));
    } else {
        mBrush.setStyle(Qt::NoBrush);
        mBrushHighlighted.setStyle(Qt::NoBrush);
    }
    update();
}

void PrimitivePathGraphicsItem::updateBoundingRectAndShape() noexcept
{
    prepareGeometryChange();
    mShape = Toolbox::shapeFromPath(mPainterPath, mPen, mBrush, Length(200000));
    mBoundingRect = mShape.controlPointRect();
    update();
}

void PrimitivePathGraphicsItem::updateVisibility() noexcept
{
    setVisible((mPen.style() != Qt::NoPen) || (mBrush.style() != Qt::NoBrush));
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb
