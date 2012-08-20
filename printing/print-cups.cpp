/**************************************************************************
*   Copyright (C) 2009 by Miguel Chavez Gamboa                            *
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

#include "print-cups.h"

#include <QString>
#include <QFont>
#include <QtGui/QPrinter>
#include <QPainter>
#include <QLocale>
#include <QDebug>
#include <klocale.h>


//#define THE_LINE (Margin + yPos +textWidth.height())
#define THE_LINE (Margin + yPos +fm.lineSpacing())
#define THE2_LINE (Margin + yPos +fm.lineSpacing()*2)
#define LINEFEED {yPos = yPos + fm.lineSpacing();DBX(fm.lineSpacing());}
#define DLINEFEED {yPos = yPos + 2*fm.lineSpacing();}
#define NLINEFEED(N) {yPos = yPos + N*fm.lineSpacing();}

QFont PrintCUPS::header;
QFontMetrics PrintCUPS::fm(QFont("Impact", 38));
QPrinter *PrintCUPS::printer;
QPainter PrintCUPS::painter;
int PrintCUPS::yPos;
int PrintCUPS::pagenum;
int PrintCUPS::Margin;
QPen PrintCUPS::normalPen;
QSize PrintCUPS::textWidth;

void PrintCUPS::prLineAlignCenter(QString text)
{
    fm = painter.fontMetrics();
    textWidth= fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
    painter.drawText((printer->width()/2)-(textWidth.width()/2),Margin + yPos + fm.lineSpacing(), text);
}
    
void PrintCUPS::prLine(int xpos,int theypos,QString text)
{
  fm = painter.fontMetrics();
  painter.drawText(xpos, theypos, text);
}

void PrintCUPS::prLineAlignRight(QString text)
{
   fm = painter.fontMetrics();
   textWidth= fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
   painter.drawText(printer->width()-Margin-textWidth.width(), Margin + yPos + fm.lineSpacing(), text);
}

void PrintCUPS::prTop(int xpos,QString text)
{
   fm = painter.fontMetrics();
   textWidth= fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
   painter.drawText(xpos,Margin+textWidth.height(), text);
}

void PrintCUPS::prAsTitle(QString text)
{
   fm = painter.fontMetrics();
   textWidth= fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
   painter.drawText((printer->width()/2)-(textWidth.width()/2), THE_LINE + fm.lineSpacing(), text);
} 

void PrintCUPS::start(QPrinter *pr,int mar,QFont hFont)
{
yPos=0;
pagenum=1;
printer=pr;
Margin=mar;
header=hFont;

painter.begin( printer );
fm = painter.fontMetrics();
normalPen = painter.pen();
}

int PrintCUPS::end()
{
  return painter.end();// this makes the print job start
}

bool PrintCUPS::autoformfeed(bool now)
    {
    if (now ||  ((Margin + yPos +5*fm.lineSpacing()) > printer->height() - Margin )) {
	formfeed();
	return true;
        }
    return false;
    }

void PrintCUPS::pageend()
{
	QFont tmpFont = QFont("Bitstream Vera Sans", 12);
	tmpFont.setItalic(true);
	painter.setFont(tmpFont);
	yPos+=5;
	painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	painter.drawLine(Margin, Margin + yPos , printer->width()-Margin, Margin + yPos );
	painter.setPen(normalPen);
	yPos+=5;
	prLine(Margin, THE_LINE,QString(I18N_NOOP("Page %1")).arg(pagenum++));
}

void PrintCUPS::formfeed()
{
TRACE;
pageend();
printer->newPage();
yPos=Margin;
}

bool PrintCUPS::printSmallBalance(const PrintBalanceInfo &pbInfo, QPrinter &pr)
{
  bool result = false;
  QFont tmpFont = QFont("Bitstream Vera Sans", 19);
  QString text;

  start(&pr,20,QFont("Impact", 38));

  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font

  printFirstHead(&pbInfo.sd,18);

  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  yPos+=15;
  prLineAlignCenter(pbInfo.thTitle);
  DLINEFEED;

  //change font
  tmpFont = QFont("Bitstream Vera Sans", 18);
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  painter.setPen(normalPen);
  prLineAlignCenter(pbInfo.thBalanceId);
  LINEFEED;

  //Date
  tmpFont = QFont("Bitstream Vera Sans", 16);
  painter.setFont(tmpFont);
  LINEFEED;

  prLineAlignCenter(pbInfo.startDate);
  LINEFEED;

  prLineAlignCenter(pbInfo.endDate);
  LINEFEED;
  
  // drawer balance 
  painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  tmpFont.setBold(true);
  painter.setFont(tmpFont);
  prLine(Margin+30, THE_LINE,pbInfo.thDeposit);
  prLine(Margin+220, THE_LINE,pbInfo.thIn);
  prLine(Margin+390, THE_LINE,pbInfo.thOut);
  prLine(Margin+555, THE_LINE,pbInfo.thInDrawer);
  DLINEFEED;

  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);

  //The quantities
  tmpFont.setBold(false);
  painter.setFont(tmpFont);

  prLine(Margin, THE_LINE,"");
  prLine(Margin, THE_LINE,pbInfo.initAmount);
  prLine(Margin+205, THE_LINE,pbInfo.inAmount);
  prLine(Margin+390, THE_LINE,pbInfo.outAmount);
  prLine(Margin+555, THE_LINE,pbInfo.cashAvailable);

  //TRANSACTION DETAILS
  
  NLINEFEED(3);
  //header
  tmpFont = QFont("Bitstream Vera Sans", 19);
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  prLineAlignCenter(pbInfo.thTitleDetails);
  DLINEFEED;

  tmpFont = QFont("Bitstream Vera Sans", 15);
  tmpFont.setItalic(false);
  tmpFont.setBold(true);
  painter.setFont(tmpFont);
  painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));

  prLine(Margin+30, THE_LINE,pbInfo.thTrId);
  prLine(Margin+130, THE_LINE,pbInfo.thTrTime);
  prLine(Margin+220, THE_LINE,pbInfo.thTrAmount);
  prLine(Margin+450, THE_LINE,pbInfo.thTrPaidW);
  prLineAlignRight(pbInfo.thTrPayMethod);
  LINEFEED;
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos + 5, printer->width()-Margin, Margin + yPos +5);
  
  tmpFont.setBold(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  painter.setPen(QPen(normalPen));
  LINEFEED;
  //Iterating each transaction
  foreach(QString trStr, pbInfo.trList) {
    QStringList data = trStr.split("|");
    //NOTE: ticket printers with autocut on each page it maybe cut the paper!
    if ( Margin + yPos > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
    //we have 5 fields in the string [ORDER] ID, HOUR, AMOUNT, PAIDWITH, PAYMETHOD
    prLine(Margin, THE_LINE,data.at(0)); //ID
    prLine(Margin+130, THE_LINE,data.at(1));  // HOUR
    prLine(Margin+220, THE_LINE,data.at(2)); // AMOUNT
    prLine(Margin+450, THE_LINE,data.at(3)); // PAID WITH
    prLineAlignRight(data.at(4)); // PAYMENT METHOD
    LINEFEED;
TRACE ;
  }
  
  LINEFEED;
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);

  // CASH FLOW DETAILS
  if (!pbInfo.cfList.isEmpty()) {
    DLINEFEED;

    tmpFont = QFont("Bitstream Vera Sans", 19);
    tmpFont.setItalic(true);
    painter.setPen(QPen(normalPen));
    painter.setFont(tmpFont);

    prLineAlignCenter(pbInfo.thTitleCFDetails);
    DLINEFEED;

    tmpFont = QFont("Bitstream Vera Sans", 15);
    tmpFont.setItalic(false);
    tmpFont.setBold(true);
    painter.setFont(tmpFont);

    //titles
    painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));

    prLine(Margin+30, THE_LINE,pbInfo.thTrId);
    prLine(Margin+130, THE_LINE,pbInfo.thCFDate);
    prLine(Margin+220, THE_LINE,pbInfo.thCFReason);
    prLine(Margin+450, THE_LINE,pbInfo.thTrAmount);
    prLineAlignRight(pbInfo.thCFType);
    LINEFEED;

    painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    painter.drawLine(Margin, Margin + yPos + 5, printer->width()-Margin, Margin + yPos +5);

    tmpFont = QFont("Bitstream Vera Sans", 15);
    tmpFont.setItalic(false);
    tmpFont.setBold(false);
    painter.setFont(tmpFont);
    painter.setPen(QPen(normalPen));
    fm = painter.fontMetrics();
    LINEFEED;
    //Iterating each cashflow
    foreach(QString cfStr, pbInfo.cfList) {
      QStringList data = cfStr.split("|");
      //we have 5 fields in the string [ORDER] ID, TYPESTR, REASON, AMOUNT, DATE
      prLine(Margin, THE_LINE,data.at(0)); // ID
      prLine(Margin+130, THE_LINE,data.at(4)); // HOUR
      prLine(Margin+220, THE_LINE,data.at(2)); // REASON
      prLine(Margin+450, THE_LINE,data.at(3)); // AMOUNT

      text = data.at(1); // REASON
      fm = painter.fontMetrics();
      while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width() >= (printer->width()-fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width())) { text.chop(2); }

      prLineAlignRight(text);
      
      LINEFEED;
TRACE ;
    }
TRACE ;
  }
  result = end();
  return result;
}


#include "printSmallTicket.txt.cpp"

#include "printBigTicket.txt.cpp"

bool PrintCUPS::printBigEndOfDay(const PrintEndOfDayInfo &pdInfo, QPrinter &pr)
{
  bool result = false;
  
  start(&pr,20,QFont("Impact", 38));
  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  QFont tmpFont = QFont("Bitstream Vera Sans", 18);
  QString text;
  
  printFirstHead(&pdInfo.sd,18);

  printPageHead(pdInfo.thTitle,pdInfo.thDate,pdInfo.salesPerson +" "+ pdInfo.terminal,16);
  
  // Transaction Headers
  tmpFont = QFont("Bitstream Vera Sans", 10);
  painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  tmpFont.setBold(true);
  painter.setFont(tmpFont);

  prLine(Margin+30, THE_LINE,pdInfo.thTicket);
  prLine(Margin+130, THE_LINE,pdInfo.thTime);
  prLine(Margin+220, THE_LINE,pdInfo.thAmount);
  prLine(Margin+450, THE_LINE,pdInfo.thProfit);
  prLineAlignRight(pdInfo.thPayMethod);
  DLINEFEED;

  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);
  
  //TRANSACTION DETAILS
  
  tmpFont.setBold(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  painter.setPen(QPen(normalPen));
  LINEFEED;
  //Iterating each transaction
  foreach(QString trStr, pdInfo.trLines) {
    QStringList data = trStr.split("|");
    if ( (Margin + yPos +fm.lineSpacing()) > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
    //we have 5 fields in the string [ORDER] ID, HOUR, AMOUNT, PROFIT, PAYMETHOD
    prLine(Margin+10, THE_LINE,data.at(0)); // ID
    prLine(Margin+130, THE_LINE,data.at(1)); // HOUR
    prLine(Margin+220, THE_LINE,data.at(2)); // AMOUNT
    prLine(Margin+450, THE_LINE,data.at(3)); // PROFIT
    prLineAlignRight(data.at(4)); // PAYMENT METHOD
    LINEFEED;
TRACE ;
  }
  
  LINEFEED;
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);

  if ( (Margin + yPos +fm.lineSpacing()) > printer->height() - Margin ) {
    formfeed();
TRACE ;
  }
  
  //TOTALS
  tmpFont.setBold(true);
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  painter.setPen(QPen(Qt::black, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  LINEFEED;
  prLine(Margin+10 , Margin + yPos,QString::number(pdInfo.trLines.count())); //transaction count
DBX(pdInfo.thTotalSales);
  prLine(Margin+210, Margin + yPos,pdInfo.thTotalSales); // sales
DBX(pdInfo.thTotalProfit);
  prLine(Margin+440, Margin + yPos,pdInfo.thTotalProfit); // profit

  result = end();
  return result;
}

bool PrintCUPS::printSmallEndOfDay(const PrintEndOfDayInfo &pdInfo, QPrinter &pr)
{
  bool result = false;
  
  start(&pr,20,QFont("Impact", 38));
  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  QFont tmpFont = QFont("Bitstream Vera Sans", 22);
  QString text;
  printFirstHead(&pdInfo.sd,18);

  tmpFont.setItalic(true);
  painter.setFont(tmpFont);

  printPageHead(pdInfo.thTitle,pdInfo.thDate,pdInfo.salesPerson +" "+ pdInfo.terminal,18);
  
  // Transaction Headers
  tmpFont = QFont("Bitstream Vera Sans", 16);
  painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  tmpFont.setBold(true);
  painter.setFont(tmpFont);
  prLine(Margin+30, THE_LINE,pdInfo.thTicket);
  prLine(Margin+130, THE_LINE,pdInfo.thTime);
  prLine(Margin+220, THE_LINE,pdInfo.thAmount);
  prLine(Margin+450, THE_LINE,pdInfo.thProfit);
  prLineAlignRight( pdInfo.thPayMethod);
  DLINEFEED;

  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);
  
  //TRANSACTION DETAILS
  
  tmpFont.setBold(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  painter.setPen(QPen(normalPen));
  LINEFEED;
  //Iterating each transaction
  foreach(QString trStr, pdInfo.trLines) {
    QStringList data = trStr.split("|");
    //we have 5 fields in the string [ORDER] ID, HOUR, AMOUNT, PROFIT, PAYMETHOD
    prLine(Margin+10, THE_LINE,data.at(0)); // ID
    prLine(Margin+130, THE_LINE,data.at(1)); // HOUR
    prLine(Margin+220, THE_LINE,data.at(2)); // AMOUNT
    prLine(Margin+450, THE_LINE,data.at(3)); // PROFIT
    prLineAlignRight(data.at(4)); // PAYMENT METHOD
    LINEFEED;
  }
  
  LINEFEED;
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);


  //TOTALS
  tmpFont.setBold(true);
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  painter.setPen(QPen(Qt::black, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  LINEFEED;

  prLine(Margin+10, Margin + yPos ,QString::number(pdInfo.trLines.count())); //transaction count
  prLine(Margin+210, Margin + yPos ,pdInfo.thTotalSales);
  prLine(Margin+440, Margin + yPos ,pdInfo.thTotalProfit);

  result = end();
  return result;
}

bool PrintCUPS::printSmallLowStockReport(const PrintLowStockInfo &plInfo, QPrinter &pr)
{
  bool result = false;
  
  start(&pr,20,QFont("Impact", 38));
  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  QFont tmpFont = QFont("Bitstream Vera Sans", 22);
  QString text;
  printFirstHead(&plInfo.sd,18);

  tmpFont.setItalic(true);
  painter.setFont(tmpFont);


  printPageHead(plInfo.hTitle,plInfo.hDate,"",22);
  
  // Content Headers
  tmpFont = QFont("Bitstream Vera Sans", 16);
  painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  tmpFont.setBold(true);
  painter.setFont(tmpFont);
  prLine(Margin+30, THE_LINE,plInfo.hCode);
  prLine(Margin+160, THE_LINE,plInfo.hDesc);
  prLine(Margin+600, THE_LINE,plInfo.hQty);
  //prLineAlignRight(plInfo.hUnitStr);
  prLineAlignRight(plInfo.hSoldU);
  DLINEFEED;

  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);
  
  //PRODUCTS DETAILS
  
  tmpFont.setBold(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  painter.setPen(QPen(normalPen));
  LINEFEED;
  //Iterating each product
  foreach(QString trStr, plInfo.pLines) {
DBX( trStr);
    QStringList data = trStr.split("|");
    //we have 5 fields in the string  CODE, DESC, STOCK QTY, UNITSTR, SOLDUNITS
    prLine(Margin+10, THE_LINE,data.at(0)); // CODE
    text = data.at(1); // DESCRIPTION
    while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width() >= 270) { text.chop(2); }
    prLine(Margin+160, THE_LINE, text);

    text = data.at(2); // STOCK QTY
    // We must be aware of the locale. europe uses ',' instead of '.' as the decimals separator.
    if (text.endsWith(".00") || text.endsWith(",00")) { text.chop(3); text += "   ";}//we chop the trailing zeroes...
    text = text +" "+ data.at(3); // 10 pieces
    prLine(Margin+600, THE_LINE, text);

//  prLine(Margin+600, THE_LINE,data.at(3)); // UNITSTR

    text = data.at(4); // SOLD UNITS
    // We must be aware of the locale. europe uses ',' instead of '.' as the decimals separator.
    if (text.endsWith(".00") || text.endsWith(",00")) { text.chop(3); text += "   ";}//we chop the trailing
    prLineAlignRight( text);
    LINEFEED;
TRACE ;
  }
  
  LINEFEED;
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);

  result = end();
  return result;
}

bool PrintCUPS::printBigLowStockReport(const PrintLowStockInfo &plInfo, QPrinter &pr)
{
  bool result = false;
  start(&pr,20,QFont("Impact", 25));
  
  printFirstHead(&plInfo.sd,14);

  printPageHead(plInfo.hTitle,plInfo.hDate,"",18);
  
  //Iterating each product
  bool firstpage=true;
  foreach(QString trStr, plInfo.pLines) {
TRACE ;
    if(firstpage || autoformfeed(false)) {
	firstpage=false;
// 	Content Headers
	QFont tmpFont = QFont("Bitstream Vera Sans", 10);
	painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	tmpFont.setBold(true);
	painter.setFont(tmpFont);
	prLine(Margin, THE_LINE,"");
	prLine(Margin+30, THE_LINE,plInfo.hCode);
	prLine(Margin+160, THE_LINE,plInfo.hDesc);
	prLine(Margin+600, THE_LINE,plInfo.hQty);
	prLineAlignRight(plInfo.hSoldU);
	DLINEFEED;
	painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
	painter.setPen(normalPen);
	tmpFont.setBold(false);
	painter.setFont(tmpFont);
	fm = painter.fontMetrics();
	painter.setPen(QPen(normalPen));
	}

//  PRODUCTS DETAILS

    QString text;

DBX( trStr);
    QStringList data = trStr.split("|");
    //we have 5 fields in the string  CODE, DESC, STOCK QTY, UNITSTR, SOLDUNITS
    prLine(Margin+30, THE_LINE,data.at(0)); // CODE
    prLine(Margin+160, THE_LINE,data.at(1)); // DESCRIPTION

    text = data.at(2); // STOCK QTY
    // We must be aware of the locale. europe uses ',' instead of '.' as the decimals separator.
    if (text.endsWith(".00") || text.endsWith(",00")) { text.chop(3); text += "   ";}//we chop the trailing zeroes...
    text = text +" "+ data.at(3); // 10 pieces
    prLine(Margin+600,THE_LINE, text);

    text = data.at(4); // SOLD UNITS
    // We must be aware of the locale. europe uses ',' instead of '.' as the decimals separator.
    if (text.endsWith(".00") || text.endsWith(",00")) { text.chop(3); text += "   ";}//we chop the trailing
    prLineAlignRight(text);
    LINEFEED;
TRACE ;
  }
        
  pageend();
  
  result = end();
  return result;
}

bool PrintCUPS::printSmallSOTicket(const PrintTicketInfo &ptInfo, QPrinter &pr)
{
  bool result = false;
  start(&pr,20,QFont("Impact", 38));
  
  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  // Header
  QFont tmpFont = QFont("Bitstream Vera Sans", 18);
  QString text;
  // Store Name
  painter.setFont(header);
  prLineAlignCenter(ptInfo.sd.storeName);
  LINEFEED;

  tmpFont = QFont("Bitstream Vera Sans", 16);
  tmpFont.setItalic(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  // Header line
  painter.setPen(QPen(Qt::gray, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos+10, printer->width()-Margin, Margin+yPos+10);
  LINEFEED;
  //Date, Ticket number
  prLine(Margin, THE_LINE,ptInfo.thDate);
  //change font for ticket number... bigger. Check for really big numbers to fit the page.
  tmpFont = QFont("Bitstream Vera Sans", 22);
  tmpFont.setItalic(false);
  tmpFont.setBold(true);
  painter.setFont(tmpFont);
  prLineAlignRight(ptInfo.thTicket);
  //change font again
  tmpFont = QFont("Bitstream Vera Sans", 17);
  tmpFont.setItalic(false);
  tmpFont.setBold(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  LINEFEED;
  //Vendor, terminal number
  prLine(Margin, THE_LINE,ptInfo.salesPerson + ", " + ptInfo.terminal);
  yPos = yPos + 3*fm.lineSpacing();
  // Products Subheader

  painter.setPen(Qt::darkBlue);
  tmpFont = QFont("Bitstream Vera Sans", 17 );
  tmpFont.setWeight(QFont::Bold);
  painter.setFont(tmpFont);
  prLine(Margin,Margin+yPos, ptInfo.thProduct);
  prLineAlignRight(ptInfo.thQty);
  LINEFEED;
  
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);
  tmpFont = QFont("Bitstream Vera Sans", 17 );
  painter.setFont(tmpFont);
  LINEFEED;
  // End of Header Information.

  // Content : Each product
  QLocale localeForPrinting;
  for (int i = 0; i < ptInfo.ticketInfo.lines.size(); ++i)
  {
    TicketLineInfo tLine = ptInfo.ticketInfo.lines.at(i);
    QString  idesc =  tLine.desc;
    QString iqty   =  localeForPrinting.toString(tLine.qty, 'f', 2);
    bool isGroup = ptInfo.ticketInfo.lines.at(i).isGroup;
    QString sp;
    QDateTime deliveryDT = ptInfo.ticketInfo.lines.at(i).deliveryDateTime;
    // We must be aware of the locale. europe uses ',' instead of '.' as the decimals separator.
    // note: QLocale has a method that does this locale aware! if the locale is set correctly
    if (iqty.endsWith(".00") || iqty.endsWith(",00")) { iqty.chop(3); iqty += "   ";}//we chop the trailing zeroes...
    iqty = iqty;
    tmpFont.setBold(true);
    painter.setFont(tmpFont);
    while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, idesc).width() >= ((printer->width()-Margin-180)-Margin)) { idesc.chop(2); }
    prLine(Margin, Margin+yPos, idesc); //first product description...

    tmpFont.setBold(false);
    prLineAlignRight(iqty);
    LINEFEED;

      //check if there is a Special Order, to print contents
      if ( !tLine.geForPrint.isEmpty() ) {
        QStringList strList = tLine.geForPrint.split("|");
        //strList.append("\n"); //extra line for separate between special orders
        strList.removeFirst();
        foreach(QString strTmp, strList) {
          if ( Margin + yPos > printer->height() - Margin ) {
            formfeed();
TRACE ;
          }
          prLine(Margin, Margin+yPos, strTmp);
          LINEFEED;
TRACE ;
        }
        ///Check for delivery date and if its a SO
        if (!isGroup && tLine.payment>0 ) {
          tmpFont = QFont("Bitstream Vera Sans", 16 );
          tmpFont.setWeight(QFont::Bold);
          painter.setFont(tmpFont);
          prLine(Margin, Margin+yPos,ptInfo.deliveryDateStr + deliveryDT.toString());
          LINEFEED;
          tmpFont = QFont("Bitstream Vera Sans", 16 );
          tmpFont.setWeight(QFont::Normal);
          painter.setFont(tmpFont);
          fm = painter.fontMetrics();
          LINEFEED;
TRACE ;
        }
        LINEFEED;
TRACE ;
      }
      //Check if space for the next text line
      if ( Margin + yPos > printer->height() - Margin ) {
        formfeed();
TRACE ;
      }
TRACE ;
  } //for each item
  
  //now the totals...
  //Check if space for the next text 3 lines
  if ( (Margin + yPos +fm.lineSpacing()*3) > printer->height() - Margin ) {
    formfeed();
TRACE ;
  }
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  
  result = end();
  
  return result;
}


#define PSLPOS_COST  PSLPOS_MES
#define PSLPOS_CODE  Margin+tabs[0][0]
#define PSLPOS_ALPHA Margin+tabs[1][1]
#define PSLPOS_CAT   Margin+tabs[1][3]
#define PSLPOS_NAME  Margin+tabs[0][2]
#define PSLPOS_MES   Margin+tabs[0][6]
#define PSLPOS_PRICE Margin+tabs[0][4]
#define PSLPOS_QTY   Margin+tabs[0][5]
#define PSLPOS_CNT   Margin+tabs[1][7]

#define LINE_CODE  THE_LINE
#define LINE_ALPHA THE2_LINE
#define LINE_CAT   THE2_LINE
#define LINE_NAME  THE_LINE
#define LINE_MES   THE_LINE
#define LINE_PRICE THE_LINE
#define LINE_QTY   THE2_LINE
#define LINE_CNT   THE_LINE


bool PrintCUPS::printStocktakeCount(const PrintStocktakeInfo &pInfo, QPrinter &pr)
{
  int tabs[2][8]=  {
	{ 10,50,150,200,430,500,550,600} 
       ,{ 10,50,150,200,430,500,550,600} 
	} ;

  bool firstpage=true
	,result = false;
  
  start(&pr,50,QFont("Impact", 38));
  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  QFont tmpFont = QFont("Bitstream Vera Sans", 18);
  QString text
	,TheCat
	,LastCat;

  printFirstHead(&pInfo.sd,14);
  
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);

  printPageHead(pInfo.hTitle,pInfo.hDate,"",18);

  //Iterating each product
 foreach(QString trStr, pInfo.pLines) {
   QStringList data = trStr.split("|");
// CAT
   TheCat = data.at(2); 
DBX(TheCat);
   
//	New Page at new Category
	if(LastCat != TheCat) {
DBX(LastCat << TheCat);
		if(!LastCat.isEmpty())  {
DBX(LastCat << TheCat);
			firstpage=true;
			formfeed();
		}

	LastCat = TheCat;
	}

   if(firstpage || autoformfeed()) {
DBX(LastCat << TheCat);
	firstpage=false;
//	Transaction Headers
	tmpFont = QFont("Bitstream Vera Sans", 10);
	painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	tmpFont.setBold(true);
	painter.setFont(tmpFont);

  	prLineAlignCenter(TheCat); 
	DLINEFEED;

	prLine(PSLPOS_CODE, LINE_CODE,pInfo.hCode);
	prLine(PSLPOS_ALPHA, LINE_ALPHA,pInfo.hAlphacode);
	prLine(PSLPOS_NAME, LINE_NAME,pInfo.hName);
	prLine(PSLPOS_MES, LINE_MAX,pInfo.hMeasure);
	prLine(PSLPOS_PRICE, LINE_PRICE,pInfo.hPrice);
	prLine(PSLPOS_QTY, LINE_QTY,pInfo.hQty);
	prLine(PSLPOS_CNT, LINE_CNT,pInfo.hCounted);
	DLINEFEED;

	yPos+=5;
	painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	painter.drawLine(Margin, Margin + yPos , printer->width()-Margin, Margin + yPos );
	yPos+=5;

  	tmpFont.setBold(false);
	painter.setFont(tmpFont);
	fm = painter.fontMetrics();
	painter.setPen(QPen(normalPen));
	}

DBX(data);

DBX(LastCat);
//  we have 7 fields in the string Code , Alphacode , Category , Name , Measure , Price , Qty
  prLine(PSLPOS_CODE ,LINE_CODE ,data.at(0)); // CODE
  prLine(PSLPOS_ALPHA,LINE_ALPHA,data.at(1)); //      ALPHA
  prLine(PSLPOS_NAME ,LINE_NAME  ,data.at(3)); //      NAME
  prLine(PSLPOS_MES  ,LINE_MES  , data.at(4));//      MES  
  prLine(PSLPOS_PRICE,LINE_PRICE, data.at(5));//      PRICE
  prLine(PSLPOS_QTY  ,LINE_QTY  , data.at(6));//      QTY
//prLine(PSLPOS_CNT,LINE_CNT, data.at(7);     //      CNT
  prLine(PSLPOS_CNT  ,LINE_QTY  , "_______________");//      CNT
  
  NLINEFEED(3);


TRACE ;
  }

  pageend();
  
  result = end();
  return result;
}

#define PSRPOS_CODE  Margin+tabs[0]
#define PSRPOS_NAME  Margin+tabs[1]
#define PSRPOS_COST  Margin+tabs[2]
#define PSRPOS_DIFF  Margin+tabs[3]
#define PSRPOS_LOST  Margin+tabs[4]


bool PrintCUPS::printStocktakeReport(const PrintStocktakeInfo &pInfo, QPrinter &pr)
{
  int tabs[2][8]=  {
	{ 10,50,150,200,430,500,550,600} 
       ,{ 10,50,150,200,430,500,550,600} 
	} ;

  bool firstpage=true
	,result = false;
  
  start(&pr,50,QFont("Impact", 38));
  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  QFont tmpFont = QFont("Bitstream Vera Sans", 18);
  QString text;

  printFirstHead(&pInfo.sd,14);
  
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);

  printPageHead(pInfo.hTitle,pInfo.hDate,"",18);

  //Iterating each product
 foreach(QString trStr, pInfo.pLines) {
   QStringList data = trStr.split("|");
   
   if(firstpage || autoformfeed()) {
	firstpage=false;
//	Transaction Headers
	tmpFont = QFont("Bitstream Vera Sans", 10);
	painter.setPen(QPen(Qt::blue, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	tmpFont.setBold(true);
	painter.setFont(tmpFont);

	prLine(PSLPOS_CODE, LINE_CODE,pInfo.hCode);
	prLine(PSLPOS_ALPHA, LINE_ALPHA,pInfo.hAlphacode);
	prLine(PSLPOS_NAME, LINE_NAME,pInfo.hName);
	prLine(PSLPOS_PRICE, LINE_PRICE,pInfo.hCost);
	prLine(PSLPOS_QTY, LINE_QTY,pInfo.hQty);
	prLine(PSLPOS_CNT, LINE_CNT,pInfo.hCounted);
	DLINEFEED;

	yPos+=5;
	painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	painter.drawLine(Margin, Margin + yPos , printer->width()-Margin, Margin + yPos );
	yPos+=5;

  	tmpFont.setBold(false);
	painter.setFont(tmpFont);
	fm = painter.fontMetrics();
	painter.setPen(QPen(normalPen));
	}

DBX(data);

//  we have 8 fields in the string Code , Alphacode , Category , Name , Measure , Price , Qty, Count
  prLine(PSLPOS_CODE ,LINE_CODE , data.at(0)); // CODE
  prLine(PSLPOS_ALPHA,LINE_ALPHA, data.at(1)); //      ALPHA
  prLine(PSLPOS_NAME ,LINE_NAME , data.at(3)); //      NAME
  prLine(PSLPOS_PRICE,LINE_PRICE, data.at(5));//      PRICE
  prLine(PSLPOS_QTY  ,LINE_QTY  , data.at(6));//      QTY
  prLine(PSLPOS_CNT  ,LINE_CNT  , data.at(7)); //      CNT
  
  NLINEFEED(3);


TRACE ;
  }

//  yPos+=5;
//  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
//  painter.drawLine(Margin, Margin + yPos , printer->width()-Margin, Margin + yPos );
//  yPos+=5;

  tmpFont = QFont("Bitstream Vera Sans", 18);
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  painter.setPen(QPen(normalPen));

  prLineAlignRight(pInfo.hTotal);

  pageend();
  
  result = end();
  return result;
}


void PrintCUPS::printPageHead(QString title1, QString title2, QString title3,int size)
	{
	QFont tmpFont = QFont("Bitstream Vera Sans", size);
	tmpFont.setItalic(false);
	painter.setFont(tmpFont);

//	Title1
	prAsTitle(title1);
	DLINEFEED;

//	Title2
	tmpFont = QFont("Bitstream Vera Sans", size-2);
	tmpFont.setItalic(true);
	painter.setFont(tmpFont);
	prLineAlignCenter(title2);
	DLINEFEED;

//	Title3
	tmpFont = QFont("Bitstream Vera Sans", size-4);
	tmpFont.setItalic(true);
	painter.setFont(tmpFont);
	prLineAlignCenter(title3);
	DLINEFEED;
	}

void PrintCUPS::printFirstHead(const PrintShopData *sd, int fsize)
	{
	QFont tmpFont = QFont("Bitstream Vera Sans", fsize);

DBX(sd->storeName);
DBX(sd->storeAddr);

	tmpFont.setItalic(true);

	painter.setFont(tmpFont);
	painter.setPen(Qt::darkGray);

	if (sd->logoOnTop) {
		if(sd->storeLogo.width() && sd->storeLogo.height()) {
			painter.drawPixmap(printer->width()/2 - (sd->storeLogo.width()/2), Margin,sd->storeLogo);
			yPos += sd->storeLogo.height();
			}
//		Store Name
		painter.setFont(header);
		prLineAlignCenter(sd->storeName);
		LINEFEED;

//		Store Addr
		painter.setFont(tmpFont);
		painter.setPen(Qt::darkGray);
		prLineAlignCenter(sd->storeAddr);
		LINEFEED;
	} else {
		if(sd->storeLogo.width() && sd->storeLogo.height()) {
			painter.drawPixmap(printer->width() - sd->storeLogo.width() - Margin, Margin+20, sd->storeLogo);
			yPos += sd->storeLogo.height();
			}
		
//		Store Name
		painter.setFont(header);
		prLine(Margin,yPos,sd->storeName);
		LINEFEED;

//		Store Addr
		painter.setFont(tmpFont);
		painter.setPen(Qt::darkGray);
		prLine(Margin,yPos,sd->storeAddr);
		LINEFEED;
	} 
//	Header line
	LINEFEED;
	painter.setPen(QPen(Qt::gray, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
	painter.drawLine(Margin, Margin+yPos, printer->width()-Margin, Margin+yPos);
	yPos+=5;
}
