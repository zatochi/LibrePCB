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
#include "packagepad.h"

/*****************************************************************************************
 *  Namespace
 ****************************************************************************************/
namespace librepcb {
namespace library {

/*****************************************************************************************
 *  Constructors / Destructor
 ****************************************************************************************/

PackagePad::PackagePad(const PackagePad& other) noexcept :
    mUuid(other.mUuid), mName(other.mName)
{
}

PackagePad::PackagePad(const Uuid& uuid, const QString& name) noexcept :
    mUuid(uuid), mName(name)
{
}

PackagePad::PackagePad(const SExpression& node) :
    mUuid(node.getChildByIndex(0).getValue<Uuid>()),
    mName(node.getValueByPath<QString>("name", true))
{
    if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);
}

PackagePad::~PackagePad() noexcept
{
}

/*****************************************************************************************
 *  Setters
 ****************************************************************************************/

void PackagePad::setName(const QString& name) noexcept
{
    mName = name;
}

/*****************************************************************************************
 *  General Methods
 ****************************************************************************************/

void PackagePad::serialize(SExpression& root) const
{
    if (!checkAttributesValidity()) throw LogicError(__FILE__, __LINE__);

    root.appendChild(mUuid);
    root.appendChild("name", mName, false);
}

/*****************************************************************************************
 *  Operator Overloadings
 ****************************************************************************************/

bool PackagePad::operator==(const PackagePad& rhs) const noexcept
{
    if (mUuid != rhs.mUuid) return false;
    if (mName != rhs.mName) return false;
    return true;
}

PackagePad& PackagePad::operator=(const PackagePad& rhs) noexcept
{
    mUuid = rhs.mUuid;
    mName = rhs.mName;
    return *this;
}

/*****************************************************************************************
 *  Private Methods
 ****************************************************************************************/

bool PackagePad::checkAttributesValidity() const noexcept
{
    if (mName.isEmpty())    return false;
    return true;
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace library
} // namespace librepcb
