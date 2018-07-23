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

#ifndef LIBREPCB_PROJECT_BI_NETLINE_H
#define LIBREPCB_PROJECT_BI_NETLINE_H

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include "bi_base.h"
#include <librepcb/common/fileio/serializableobject.h>
#include <librepcb/common/geometry/path.h>
#include <librepcb/common/uuid.h>
#include "../graphicsitems/bgi_netline.h"

/*****************************************************************************************
 *  Namespace / Forward Declarations
 ****************************************************************************************/
namespace librepcb {

class GraphicsLayer;

namespace project {

class NetSignal;
class BI_NetPoint;
class BI_NetSegment;

/*****************************************************************************************
 *  Class BI_NetLine
 ****************************************************************************************/

/**
 * @brief The BI_NetLine class
 */
class BI_NetLine final : public BI_Base, public SerializableObject
{
        Q_OBJECT

    public:

        // Constructors / Destructor
        BI_NetLine() = delete;
        BI_NetLine(const BI_NetLine& other) = delete;
        BI_NetLine(const BI_NetLine& other, BI_NetPoint& startPoint, BI_NetPoint& endPoint);
        BI_NetLine(BI_NetSegment& segment, const SExpression& node);
        BI_NetLine(BI_NetPoint& startPoint, BI_NetPoint& endPoint, const PositiveLength& width);
        ~BI_NetLine() noexcept;

        // Getters
        BI_NetSegment& getNetSegment() const noexcept;
        const Uuid& getUuid() const noexcept {return mUuid;}
        const PositiveLength& getWidth() const noexcept {return mWidth;}
        BI_NetPoint& getStartPoint() const noexcept {return *mStartPoint;}
        BI_NetPoint& getEndPoint() const noexcept {return *mEndPoint;}
        BI_NetPoint* getOtherPoint(const BI_NetPoint& firstPoint) const noexcept;
        NetSignal& getNetSignalOfNetSegment() const noexcept;
        GraphicsLayer& getLayer() const noexcept;
        bool isAttached() const noexcept;
        bool isAttachedToFootprint() const noexcept;
        bool isAttachedToVia() const noexcept;
        bool isSelectable() const noexcept override;
        Path getSceneOutline(const Length& expansion = Length(0)) const noexcept;

        // Setters
        void setWidth(const PositiveLength& width) noexcept;

        // General Methods
        void addToBoard() override;
        void removeFromBoard() override;
        void updateLine() noexcept;

        /// @copydoc librepcb::SerializableObject::serialize()
        void serialize(SExpression& root) const override;


        // Inherited from SI_Base
        Type_t getType() const noexcept override {return BI_Base::Type_t::NetLine;}
        const Point& getPosition() const noexcept override {return mPosition;}
        bool getIsMirrored() const noexcept override {return false;}
        QPainterPath getGrabAreaScenePx() const noexcept override;
        void setSelected(bool selected) noexcept override;

        // Operator Overloadings
        BI_NetLine& operator=(const BI_NetLine& rhs) = delete;


    private:

        void init();
        bool checkAttributesValidity() const noexcept;


        // General
        QScopedPointer<BGI_NetLine> mGraphicsItem;
        Point mPosition; ///< the center of startpoint and endpoint
        QMetaObject::Connection mHighlightChangedConnection;

        // Attributes
        Uuid mUuid;
        BI_NetPoint* mStartPoint;
        BI_NetPoint* mEndPoint;
        PositiveLength mWidth;
};

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project
} // namespace librepcb

#endif // LIBREPCB_PROJECT_BI_NETLINE_H
