/**************************************************************************
*   Copyright (C) 2007-2009 by Miguel Chavez Gamboa                       *
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
#ifndef PRINT_CUPS_H
#define PRINT_CUPS_H

#include "../src/structs.h"

#include <QFont>
#include <QPainter>

/**
* This class is for printing on printers with cups driver installed
* Accessing them trough cups.
*
* @author Miguel Chavez Gamboa <miguel@lemonpos.org>
* @version 0.1
*/

class QString;

class PrintCUPS {
  public:
    static void start(QPrinter *printer,int Mar,QFont hFont);
    static int end();
    static void pageend();
    static bool autoformfeed(bool now=false);
    static void formfeed();
    static bool printSmallBalance(const PrintBalanceInfo &pbInfo, QPrinter &printer);
    static bool printSmallTicket(const PrintTicketInfo &ptInfo, QPrinter &printer,const QString );
    static bool printBigTicket(const PrintTicketInfo &ptInfo, QPrinter &printer,const QString );
    static bool printSmallEndOfDay(const PrintEndOfDayInfo &pdInfo, QPrinter &printer);
    static bool printBigEndOfDay(const PrintEndOfDayInfo &pdInfo, QPrinter &printer);

    static bool printSmallLowStockReport(const PrintLowStockInfo &plInfo, QPrinter &printer);
    static bool printBigLowStockReport(const PrintLowStockInfo &plInfo, QPrinter &printer);

    static bool printSmallSOTicket(const PrintTicketInfo &ptInfo, QPrinter &printer);
    static bool printStocktakeCount(const PrintStocktakeInfo &pInfo, QPrinter &printer);
    static bool printStocktakeReport(const PrintStocktakeInfo &pInfo, QPrinter &printer);
 private:
    static QFont header;
    static QFontMetrics fm;
    static QPrinter *printer;
    static QPainter painter;
    static int yPos;
    static int pagenum;
    static int Margin;
    static QPen normalPen;
    static QSize textWidth;
    static void prLine(int xpos,int ypos,QString text);
    static void prLineAlignCenter(QString text);
    static void prLineAlignRight(QString text);
    static void prTop(int xpos,QString text);
    static void prAsTitle(QString text);
    static void printFirstHead(const PrintShopData *sd, int fsize);
    static void printPageHead(QString title1, QString title2, QString title3,int size);
};

#endif
