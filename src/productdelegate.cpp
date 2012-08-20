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
**************************************************************************/

#include <QtGui>
#include <QtSql>
#include <QMouseEvent>
#include <QPaintEvent>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kiconloader.h>

#include "productdelegate.h"

#define IDX_CODE 	row,0
#define IDX_NAME 	row,1
#define IDX_PRICE 	row,2
#define IDX_COST 	row,3
#define IDX_STOCKQTY 	row,4
#define IDX_BRABD 	row,5
#define IDX_UNITS 	row,6
#define IDX_TAXMODEL 	row,7
#define IDX_PHOTO 	row,8
#define IDX_CATEGORY 	row,9
#define IDX_POINTS	row,10
#define IDX_ALPHA	row,11
#define IDX_LASTPROV	row,12
#define IDX_SOLDUNITS	row,13
#define IDX_DATELAST	row,14
#define IDX_ISRAW	row,15
#define IDX_ISINDIV	row,16
#define IDX_ISAGRP	row,17


void ProductDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    //painting background when selected...
    //QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
    //    ? QPalette::Normal : QPalette::Disabled;

    //Painting frame
    painter->setRenderHint(QPainter::Antialiasing);
    QString pixName = KStandardDirs::locate("appdata", "images/itemBox.png");
    painter->drawPixmap(option.rect.x()+5,option.rect.y()+5, QPixmap(pixName));

    //get item data
    const QAbstractItemModel *model = index.model();
    int row = index.row();

#ifdef FOOBAR
for(int v=0;v<5;v++) {
	for(int q=0;q<30;q++) {
	    	QModelIndex nI = model->index(v, q);
		DBX(v << q << model->data(nI, Qt::DisplayRole).toInt() << model->data(nI, Qt::DisplayRole).toString());
		}
	}
#endif

    QModelIndex nameIndex = model->index(IDX_NAME);

    QString name = model->data(nameIndex, Qt::DisplayRole).toString();
    QByteArray pixData = model->data(index, Qt::DisplayRole).toByteArray();

    nameIndex = model->index(IDX_STOCKQTY);
    double stockqty = model->data(nameIndex, Qt::DisplayRole).toDouble();

    nameIndex = model->index(IDX_CODE);
    QString strCode = "# " + model->data(nameIndex, Qt::DisplayRole).toString();

    //preparing photo to paint it...
    QPixmap pix;
    if (!pixData.isEmpty() or !pixData.isNull()) {
      pix.loadFromData(pixData);
    }
    else {
      pix = QPixmap(DesktopIcon("lemon-box", 64));
    }
    int max = 128;
    if ((pix.height() > max) || (pix.width() > max) ) {
      if (pix.height() == pix.width()) {
        pix = pix.scaled(QSize(max, max));
      }
      else if (pix.height() > pix.width() ) {
        pix = pix.scaledToHeight(max);
      }
      else  {
        pix = pix.scaledToWidth(max);
      }
    }
    int x = option.rect.x() + (option.rect.width()/2) - (pix.width()/2);
    int y = option.rect.y() + (option.rect.height()/2) - (pix.height()/2) - 10;
    //painting photo
    if (!pix.isNull()) painter->drawPixmap(x,y, pix);

    //Painting name
    QFont font = QFont("Trebuchet MS", 10);
    font.setBold(true);
    //getting name size in pixels...
    QFontMetrics fm(font);
    int strSize = fm.width(name);
    int aproxPerChar = fm.width("A");
    QString nameToDisplay = name;
    int boxSize = option.rect.width()-15; //minus margin and frame-lines
    if (strSize > boxSize) {
      int excess = strSize-boxSize;
      int charEx = (excess/aproxPerChar)+2;
      nameToDisplay = name.left(name.length()-charEx-7) +"...";
      //qDebug()<<"Text does not fit, strSize="<<strSize<<" boxSize:"
      //<<boxSize<<" excess="<<excess<<" charEx="<<charEx<<"nameToDisplay="<<nameToDisplay;
    }
    painter->setFont(font);
    if (option.state & QStyle::State_Selected) {
      painter->setPen(Qt::yellow);
      painter->drawText(option.rect.x()+10,option.rect.y()+145, 150,20,  Qt::AlignCenter, nameToDisplay);
    }
    else {
      painter->setPen(Qt::white);
      painter->drawText(option.rect.x()+10,option.rect.y()+145, 150,20,  Qt::AlignCenter, nameToDisplay);
    }

    //painting stock Availability
    font = QFont("Trebuchet MS", 12);
    font.setBold(true);
    font.setItalic(true);
    painter->setFont(font);
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setPen(Qt::red);
    painter->setBackground(QColor(255,225,0,160));

    QModelIndex nameI = model->index(IDX_CODE);

    nameIndex = model->index(IDX_ISRAW);
    bool isRaw = model->data(nameIndex, Qt::DisplayRole).toBool();

    nameIndex = model->index(IDX_ISINDIV);
    bool isSerial = model->data(nameIndex, Qt::DisplayRole).toBool();

    nameIndex = model->index(IDX_ISAGRP);
    bool isGrp = model->data(nameIndex, Qt::DisplayRole).toBool();

    QString naStr = QString::null;

    if (stockqty <= 0) naStr = i18n(" Out of stock ");

    painter->drawText(option.rect.x()+10,
          option.rect.y()+(option.rect.height()/2)-10,
          150, 20, Qt::AlignCenter, naStr);
    painter->setBackgroundMode(Qt::TransparentMode);

    //painting code number
    font = QFont("Trebuchet MS", 9);
    font.setBold(false);
    font.setItalic(true);
    painter->setFont(font);
    painter->setBackgroundMode(Qt::TransparentMode);
    painter->setPen(Qt::white);
    painter->setBackground(QColor(255,225,0,160));
    painter->drawText(option.rect.x()+10,
                      option.rect.y()+5,
                      150, 20, Qt::AlignCenter, strCode);
    painter->setBackgroundMode(Qt::TransparentMode);
}


QSize ProductDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
  return QSize(170,170);
//   return QItemDelegate::sizeHint(option, index);
}

#include "productdelegate.moc"
