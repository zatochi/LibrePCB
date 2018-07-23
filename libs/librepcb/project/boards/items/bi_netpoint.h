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

#ifndef LIBREPCB_PROJECT_BI_NETPOINT_H
#define LIBREPCB_PROJECT_BI_NETPOINT_H

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include "bi_base.h"
#include <librepcb/common/fileio/serializableobject.h>
#include <librepcb/common/uuid.h>
#include "../../erc/if_ercmsgprovider.h"
#include "../graphicsitems/bgi_netpoint.h"

/*****************************************************************************************
 *  Namespace / Forward Declarations
 ****************************************************************************************/
class QGraphicsItem;

namespace librepcb {

class GraphicsLayer;

namespace project {

class NetSignal;
class BI_NetLine;
class BI_Footprint;
class BI_FootprintPad;
class BI_NetSegment;
class BI_Via;
class ErcMsg;

/*****************************************************************************************
 *  Class BI_NetPoint
 ****************************************************************************************/

/**
 * @brief The BI_NetPoint class
 */
class BI_NetPoint final : public BI_Base, public SerializableObject,
                          public IF_ErcMsgProvider
{
        Q_OBJECT
        DECLARE_ERC_MSG_CLASS_NAME(BI_NetPoint)

    public:

        // Constructors / Destructor
        BI_NetPoint() = delete;
        BI_NetPoint(const BI_NetPoint& other) = delete;
        BI_NetPoint(BI_NetSegment& segment, const BI_NetPoint& other, BI_FootprintPad* pad,
                    BI_Via* via);
        BI_NetPoint(BI_NetSegment& segment, const SExpression& node);
        BI_NetPoint(BI_NetSegment& segment, GraphicsLayer& layer, const Point& position);
        BI_NetPoint(BI_NetSegment& segment, GraphicsLayer& layer, BI_FootprintPad& pad);
        BI_NetPoint(BI_NetSegment& segment, GraphicsLayer& layer, BI_Via& via);
        ~BI_NetPoint() noexcept;

        // Getters
        const Uuid& getUuid() const noexcept {return mUuid;}
        GraphicsLayer& getLayer() const noexcept {return *mLayer;}
        bool isAttachedToPad() const noexcept {return (mFootprintPad ? true : false);}
        bool isAttachedToVia() const noexcept {return (mVia ? true : false);}
        bool isAttached() const noexcept {return (isAttachedToPad() || isAttachedToVia());}
        BI_NetSegment& getNetSegment() const noexcept {return mNetSegment;}
        NetSignal& getNetSignalOfNetSegment() const noexcept;
        BI_FootprintPad* getFootprintPad() const noexcept {return mFootprintPad;}
        BI_Via* getVia() const noexcept {return mVia;}
        const QList<BI_NetLine*>& getLines() const noexcept {return mRegisteredLines;}
        bool isUsed() const noexcept {return (mRegisteredLines.count() > 0);}
        UnsignedLength getMaxLineWidth() const noexcept;
        bool isSelectable() const noexcept override;

        // Setters
        void setLayer(GraphicsLayer& layer);
        void setPadToAttach(BI_FootprintPad* pad);
        void setViaToAttach(BI_Via* via);
        void setPosition(const Point& position) noexcept;

        // General Methods
        void addToBoard() override;
        void removeFromBoard() override;
        void registerNetLine(BI_NetLine& netline);
        void unregisterNetLine(BI_NetLine& netline);
        void updateLines() const noexcept;


        /// @copydoc librepcb::SerializableObject::serialize()
        void serialize(SExpression& root) const override;


        // Inherited from SI_Base
        Type_t getType() const noexcept override {return BI_Base::Type_t::NetPoint;}
        const Point& getPosition() const noexcept override {return mPosition;}
        bool getIsMirrored() const noexcept override {return false;}
        QPainterPath getGrabAreaScenePx() const noexcept override;
        void setSelected(bool selected) noexcept override;

        // Operator Overloadings
        BI_NetPoint& operator=(const BI_NetPoint& rhs) = delete;
        bool operator==(const BI_NetPoint& rhs) noexcept {return (this == &rhs);}
        bool operator!=(const BI_NetPoint& rhs) noexcept {return (this != &rhs);}


    private:

        void init();
        bool checkAttributesValidity() const noexcept;


        // General
        QScopedPointer<BGI_NetPoint> mGraphicsItem;
        QMetaObject::Connection mHighlightChangedConnection;

        // Attributes
        BI_NetSegment& mNetSegment;
        Uuid mUuid;
        Point mPosition;
        GraphicsLayer* mLayer;
        BI_FootprintPad* mFootprintPad; ///< only needed if the netpoint is attached to a pad
        BI_Via* mVia;                   ///< only needed if the netpoint is attached to a via

        // Registered Elements
        QList<BI_NetLine*> mRegisteredLines;    ///< all registered netlines

        // ERC Messages
        /// @brief The ERC message for dead netpoints
        QScopedPointer<ErcMsg> mErcMsgDeadNetPoint;
};

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace project
} // namespace librepcb

#endif // LIBREPCB_PROJECT_BI_NETPOINT_H
