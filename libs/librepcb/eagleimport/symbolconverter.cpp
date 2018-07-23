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
#include "symbolconverter.h"
#include "converterdb.h"
#include <parseagle/symbol/symbol.h>
#include <librepcb/common/graphics/graphicslayer.h>
#include <librepcb/library/sym/symbol.h>

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace eagleimport {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

SymbolConverter::SymbolConverter(const parseagle::Symbol& symbol, ConverterDb& db) noexcept :
    mSymbol(symbol), mDb(db)
{
}

SymbolConverter::~SymbolConverter() noexcept
{
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

std::unique_ptr<library::Symbol> SymbolConverter::generate() const
{
    std::unique_ptr<library::Symbol> symbol(
        new library::Symbol(mDb.getSymbolUuid(mSymbol.getName()), Version("0.1"),
                            "LibrePCB", mSymbol.getName(), createDescription(), ""));

    foreach (const parseagle::Wire& wire, mSymbol.getWires()) {
        QString layerName = convertSchematicLayer(wire.getLayer());
        bool fill = false;
        bool isGrabArea = true;
        UnsignedLength lineWidth = UnsignedLength(Length::fromMm(wire.getWidth()));
        Point startpos = Point::fromMm(wire.getP1().x, wire.getP1().y);
        Point endpos = Point::fromMm(wire.getP2().x, wire.getP2().y);
        Angle angle = Angle::fromDeg(wire.getCurve());
        symbol->getPolygons().append(std::make_shared<Polygon>(Uuid::createRandom(),
            layerName, lineWidth, fill, isGrabArea, Path::line(startpos, endpos, angle)));
    }

    foreach (const parseagle::Rectangle& rect, mSymbol.getRectangles()) {
        QString layerName = convertSchematicLayer(rect.getLayer());
        bool fill = true;
        bool isGrabArea = true;
        UnsignedLength lineWidth = UnsignedLength(0);
        Point p1 = Point::fromMm(rect.getP1().x, rect.getP1().y);
        Point p2 = Point::fromMm(rect.getP2().x, rect.getP2().y);
        symbol->getPolygons().append(std::make_shared<Polygon>(Uuid::createRandom(),
            layerName, lineWidth, fill, isGrabArea, Path::rect(p1, p2)));
    }

    foreach (const parseagle::Circle& circle, mSymbol.getCircles()) {
        QString layerName = convertSchematicLayer(circle.getLayer());
        PositiveLength diameter = PositiveLength(Length::fromMm(circle.getRadius()) * 2);
        Point center = Point::fromMm(circle.getPosition().x, circle.getPosition().y);
        UnsignedLength lineWidth = UnsignedLength(Length::fromMm(circle.getWidth()));
        bool fill = (lineWidth == UnsignedLength(0));
        bool isGrabArea = true;
        symbol->getCircles().append(std::make_shared<Circle>(Uuid::createRandom(),
            layerName, lineWidth, fill, isGrabArea, center, diameter));
    }

    foreach (const parseagle::Polygon& polygon, mSymbol.getPolygons()) {
        QString layerName = convertSchematicLayer(polygon.getLayer());
        bool fill = false;
        bool isGrabArea = true;
        UnsignedLength lineWidth = UnsignedLength(Length::fromMm(polygon.getWidth()));
        Path path;
        for (int i = 0; i < polygon.getVertices().count(); ++i) {
            const parseagle::Vertex vertex = polygon.getVertices().at(i);
            Point pos = Point::fromMm(vertex.getPosition().x, vertex.getPosition().y);
            Angle angle = Angle::fromDeg(vertex.getCurve());
            path.addVertex(pos, angle);
        }
        path.close();
        symbol->getPolygons().append(std::make_shared<Polygon>(Uuid::createRandom(),
            layerName, lineWidth, fill, isGrabArea, path));
    }

    foreach (const parseagle::Text& text, mSymbol.getTexts()) {
        QString layerName = convertSchematicLayer(text.getLayer());
        QString textStr = text.getValue();
        if (textStr.startsWith(">")) {
            textStr = "{{" + textStr.mid(1) + "}}";
        }
        PositiveLength height = PositiveLength(Length::fromMm(text.getSize()) * 2);
        if (textStr == "{{NAME}}") {
            height = Length::fromMm(3.175);
        } else if (textStr == "{{VALUE}}") {
            height = Length::fromMm(2.5);
        }
        Point pos = Point::fromMm(text.getPosition().x, text.getPosition().y);
        Angle rot = Angle::fromDeg(text.getRotation().getAngle());
        Alignment align(HAlign::left(), VAlign::bottom());
        symbol->getTexts().append(std::make_shared<Text>(
            Uuid::createRandom(), layerName, textStr, pos, rot, height, align));
    }

    foreach (const parseagle::Pin& pin, mSymbol.getPins()) {
        Uuid pinUuid = mDb.getSymbolPinUuid(symbol->getUuid(), pin.getName());
        Point pos = Point::fromMm(pin.getPosition().x, pin.getPosition().y);
        UnsignedLength len = UnsignedLength(Length::fromMm(pin.getLengthInMillimeters()));
        Angle rot = Angle::fromDeg(pin.getRotation().getAngle());
        symbol->getPins().append(std::make_shared<library::SymbolPin>(pinUuid, pin.getName(),
                                                                      pos, len, rot));
    }

    return symbol;
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

QString SymbolConverter::createDescription() const noexcept
{
    QString desc = mSymbol.getDescription() + "\n\n";
    desc += "This symbol was automatically imported from Eagle.\n";
    desc += "Library: " % mDb.getCurrentLibraryFilePath().getFilename() % "\n";
    desc += "Symbol: " % mSymbol.getName() % "\n";
    desc += "NOTE: Please remove this text after manual rework!";
    return desc.trimmed();
}

QString SymbolConverter::convertSchematicLayer(int eagleLayerId)
{
    switch (eagleLayerId) {
        case 93: return GraphicsLayer::sSymbolPinNames;
        case 94: return GraphicsLayer::sSymbolOutlines;
        case 95: return GraphicsLayer::sSymbolNames;
        case 96: return GraphicsLayer::sSymbolValues;
        case 99: return GraphicsLayer::sSchematicReferences; // ???
        default: throw Exception(__FILE__, __LINE__, QString("Invalid schematic layer: %1").arg(eagleLayerId));
    }
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace eagleimport
} // namespace librepcb
