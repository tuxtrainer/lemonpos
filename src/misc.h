/**************************************************************************
*   Copyright © 2007-2010 by Miguel Chavez Gamboa                         *
*   miguel@lemonpos.org                                                   *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
***************************************************************************/
#ifndef MISC_H
#define MISC_H


class QByteArray;
class QPixmap;


/**
 * This class is for Misc code.
 * Actual Stuff:  pixmap2ByteArray in diferent versions.
 *
 * @author Miguel Chavez Gamboa <miguel@lemonpos.org>
 * @version 0.1
 */
class Misc {
  public:
    static QByteArray pixmap2ByteArray(QPixmap *pix);
    static QByteArray pixmap2ByteArray(QPixmap *pix, int maxW, int maxH);
};
#endif
