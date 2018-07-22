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

#ifndef LIBREPCB_UUID_H
#define LIBREPCB_UUID_H

/*****************************************************************************************
 *  Includes
 ****************************************************************************************/
#include <QtCore>
#include "fileio/sexpression.h"

/*****************************************************************************************
 *  Namespace / Forward Declarations
 ****************************************************************************************/
namespace librepcb {

/*****************************************************************************************
 *  Class Uuid
 ****************************************************************************************/

/**
 * @brief The Uuid class is a replacement for QUuid to get UUID strings without {} braces
 *
 * This class implements an RFC4122 compliant UUID of type "DCE" in Version 4 (random
 * UUID). Other types and/or versions of UUIDs are considered as invalid. The characters
 * in a UUID are always lowercase.
 *
 * A valid UUID looks like this: "d79d354b-62bd-4866-996a-78941c575e78"
 *
 * @see https://de.wikipedia.org/wiki/Universally_Unique_Identifier
 * @see https://tools.ietf.org/html/rfc4122
 *
 * @author ubruhin
 * @date 2015-09-29
 */
class Uuid final
{
        Q_DECLARE_TR_FUNCTIONS(Uuid)

    public:

        // Constructors / Destructor

        /**
         * @brief Default constructor (creates a NULL #Uuid object)
         */
        Uuid() noexcept : mUuid() {}

        /**
         * @brief Constructor which creates a #Uuid object from a string
         *
         * @param uuid      The uuid as a string (without braces)
         */
        explicit Uuid(const QString& uuid) noexcept : mUuid() {setUuid(uuid);}

        /**
         * @brief Copy constructor
         *
         * @param other     Another #Uuid object
         */
        Uuid(const Uuid& other) noexcept : mUuid(other.mUuid) {}

        /**
         * @brief Destructor
         */
        ~Uuid() noexcept = default;


        // Getters

        /**
         * @brief Check whether this object represents a NULL UUID or a valid UUID
         *
         * @return true if NULL/invalid UUID, false if valid UUID
         */
        bool isNull() const noexcept {return mUuid.isEmpty();}

        /**
         * @brief Get the UUID as a string (without braces)
         *
         * @return The UUID as a string
         */
        QString toStr() const noexcept {return mUuid;}


        // Setters

        /**
         * @brief Set a new UUID
         *
         * @param uuid  The uuid as a string (without braces)
         *
         * @return true if uuid was valid, false if not (=> NULL UUID)
         *
         * @note If this method returns false, the UUID becames invalid (#isNull() = true)
         */
        bool setUuid(const QString& uuid) noexcept;


        //@{
        /**
         * @brief Operator overloadings
         *
         * @param rhs   The other object to compare
         *
         * @return Result of comparing the UUIDs as strings
         */
        Uuid& operator=(const Uuid& rhs) noexcept {mUuid = rhs.mUuid; return *this;}
        bool operator==(const Uuid& rhs) const noexcept {return mUuid == rhs.mUuid;}
        bool operator!=(const Uuid& rhs) const noexcept {return mUuid != rhs.mUuid;}
        bool operator<(const Uuid& rhs) const noexcept {return mUuid < rhs.mUuid;}
        bool operator>(const Uuid& rhs) const noexcept {return mUuid > rhs.mUuid;}
        bool operator<=(const Uuid& rhs) const noexcept {return mUuid <= rhs.mUuid;}
        bool operator>=(const Uuid& rhs) const noexcept {return mUuid >= rhs.mUuid;}
        //@}


        // Static Methods

        /**
         * @brief Create a new random UUID
         *
         * @return The new UUID
         */
        static Uuid createRandom() noexcept;


    private:

        // Private Attributes
        QString mUuid;
};

/*****************************************************************************************
 *  Non-Member Functions
 ****************************************************************************************/

template <>
inline SExpression serializeToSExpression(const Uuid& obj) {
    return SExpression::createToken(obj.isNull() ? "null" : obj.toStr());
}

template <>
inline Uuid deserializeFromSExpression(const SExpression& sexpr, bool throwIfEmpty) {
    QString str = sexpr.getStringOrToken(throwIfEmpty);
    Uuid uuid(str);
    if (uuid.isNull() && (str != "null")) {
        throw RuntimeError(__FILE__, __LINE__,
                           QString(Uuid::tr("Invalid UUID: \"%1\"")).arg(str));
    }
    return uuid;
}

inline QDataStream& operator<<(QDataStream& stream, const Uuid& uuid) noexcept {
    stream << uuid.toStr();
    return stream;
}

inline QDebug operator<<(QDebug stream, const Uuid& uuid) noexcept {
    stream << QString("Uuid(%1)").arg(uuid.toStr());
    return stream;
}

inline uint qHash(const Uuid& key, uint seed) noexcept {
    return ::qHash(key.toStr(), seed);
}

/*****************************************************************************************
 *  End of File
 ****************************************************************************************/

} // namespace librepcb

#endif // LIBREPCB_UUID_H
