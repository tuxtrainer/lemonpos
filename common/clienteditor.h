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
#ifndef CLIENTEDITOR_H
#define CLIENTEDITOR_H

#include <KDialog>
#include <QtGui>
#include "ui_editclient_widget.h"

#define DBX(X) qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << #X << X

class ClientEditorUI : public QFrame, public Ui::clientEditor
{
  Q_OBJECT
  public:
    ClientEditorUI( QWidget *parent=0 );
};

class ClientEditor : public KDialog
{
  Q_OBJECT
  public:
    ClientEditor( QWidget *parent=0,bool admin=false );
    ~ClientEditor();
    void setName(QString name) { ui->editClientName->setText(name); };
    void setFName(QString fname) { DBX(fname); ui->editClientFName->setText(fname); };
    void setAddress(QString address) { ui->editClientAddress->setText(address); } ;
    void setPhone(QString phone) { ui->editClientPhone->setText(phone); };
    void setCell(QString cell) { ui->editClientCell->setText(cell); };
    void setPoints(qulonglong p) { ui->editClientPoints->setText(QString::number(p)); };
    void setDiscount(double d) {ui->editClientDiscount->setText(QString::number(d)); };
    void setPhoto(QPixmap photo) { ui->labelClientPhoto->setPixmap(photo); pix = photo; };
    void setId(long int id) { clientId = id; };
    void setSinceDate(QDate date) { ui->sinceDatePicker->setDate(date); }

    QString getFName(){ return ui->editClientFName->text();};
    QString getName(){ return ui->editClientName->text();};
    QString getAddress(){ return ui->editClientAddress->toPlainText();};
    QString getPhone(){ return ui->editClientPhone->text();};
    QString getCell(){ return ui->editClientCell->text();};
    qulonglong getPoints() { return ui->editClientPoints->text().toULongLong(); };
    double  getDiscount() {return ui->editClientDiscount->text().toDouble(); }
    QPixmap getPhoto(){ return pix;};
    QDate   getSinceDate() { return ui->sinceDatePicker->date(); }


  private slots:
    void changePhoto();
    void checkName();


  private:
    ClientEditorUI *ui;
    long int clientId;
    QPixmap pix;
};

#endif