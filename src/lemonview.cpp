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
#include "lemonview.h"
#include "settings.h"
#include "inputdialog.h"
#include "productdelegate.h"
#include "pricechecker.h"
#include "../common/clienteditor.h"
#include "../dataAccess/azahar.h"
#include "../printing/print-dev.h"
#include "../printing/print-cups.h"
#include "ticketpopup.h"

//StarMicronics printers
// #include "printers/sp500.h"

#include <QtGui/QPrinter>
#include <QtGui/QPrintDialog>

#include <QWidget>
#include <QStringList>
#include <QTimer>
#include <QColor>
#include <QPixmap>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCodec>
#include <QRegExp>
#include <QRegExpValidator>
#include <QValidator>
#include <QGridLayout>
#include <QDesktopWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QTextDocument>
#include <QTextEdit>
#include <QPushButton>

#include <klocale.h>
#include <kiconloader.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kpassivepopup.h>
#include <KNotification>


#define TMP_CODE_ADD 9999990000000000LL
#define MAX_SPECIAL_CODE 9LL

/* Widgets zone                                                                                                         */
/*======================================================================================================================*/

class BalanceDialog : public QDialog
{
  private:
    QGridLayout *gridLayout;
    QTextEdit *editText;
    QPushButton *buttonClose;

  public:
    BalanceDialog(QWidget *parent=0, QString str="")
    {
      setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
      setWindowModality(Qt::ApplicationModal);

      gridLayout = new QGridLayout(this);
      editText = new QTextEdit(str);
      editText->setReadOnly(true);
      editText->setMinimumSize(QSize(320,450));
      gridLayout->addWidget(editText, 0, 0);
      buttonClose = new QPushButton(this);
      buttonClose->setText(i18n("Continue"));
      buttonClose->setDefault(true);
      buttonClose->setShortcut(Qt::Key_Enter);
      gridLayout->addWidget(buttonClose, 1,0);

      connect(buttonClose, SIGNAL(clicked()), this, SLOT(close()));
      //connect(buttonClose, SIGNAL(clicked()), parent, SLOT(slotDoStartOperation()));
TRACE;
    }
    virtual void paint(QPainter *) {}
  protected:
    void paintEvent(QPaintEvent *e)
    {
      QPainter painter;
      painter.begin(this);
      painter.setClipRect(e->rect());
      painter.setRenderHint(QPainter::Antialiasing);

      paint(&painter);
      painter.restore();
      painter.save();
      int level = 180;
      painter.setPen(QPen(QColor(level, level, level), 6));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(rect());
TRACE;
    }

};


void lemonView::cancelByExit()
{
  clearUsedWidgets();
  refreshTotalLabel();
  preCancelCurrentTransaction();
  Azahar * myDb = new Azahar;
  myDb->setDatabase(db);
  myDb->deleteEmptyTransactions();
  if (db.isOpen()) db.close();
TRACE;
}

lemonView::lemonView(QWidget *parent) //: QWidget(parent)
{
  drawerCreated=false;
  modelsCreated=false;
  QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
  db = QSqlDatabase::addDatabase("QMYSQL"); //moved here because calling multiple times cause a crash on certain installations (Not kubuntu 8.10).
  ui_mainview.setupUi(this);
  currentTransaction.id=0;
  currentBalance.id=0;
  currentBalance.done=false;

  dlgLogin = new LoginWindow(this,
                             i18n("Welcome to Lemon"),
                             i18n("Enter username and password to start using the system."),
                             LoginWindow::FullScreen);
  dlgPassword = new LoginWindow(this,
                             i18n("Authorisation Required"),
                             i18n("Enter administrator password please."),
                             LoginWindow::PasswordOnly);

  refreshTotalLabel();
  QTimer::singleShot(1000, this, SLOT(setupDB()));
  setAutoFillBackground(true);
  QTimer::singleShot(1100, this, SLOT(login()));
  QTimer *timerClock = new QTimer(this);

  loggedUserRole = roleBasic;
  
  //Signals
  connect(timerClock, SIGNAL(timeout()), SLOT(timerTimeout()) );
  //connect(ui_mainview.editItemDescSearch, SIGNAL(returnPressed()), this, SLOT(doSearchItemDesc()));
  connect(ui_mainview.editItemDescSearch, SIGNAL(textEdited(const QString&)), this, SLOT(doSearchItemDesc()));
  connect(ui_mainview.editItemCode, SIGNAL(returnPressed()), this, SLOT(doEmitSignalQueryDb()));
  connect(this, SIGNAL(signalQueryDb(QString)), this, SLOT(insertItem(QString)) );
  connect(ui_mainview.tableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), SLOT(itemDoubleClicked(QTableWidgetItem*)) );
  connect(ui_mainview.tableSearch, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), SLOT(itemSearchDoubleClicked(QTableWidgetItem*)) );
  connect(ui_mainview.tableWidget, SIGNAL(itemClicked(QTableWidgetItem*)), SLOT(displayItemInfo(QTableWidgetItem*)));
  //connect(ui_mainview.listView, SIGNAL(activated(const QModelIndex &)), SLOT(listViewOnClick(const QModelIndex &)));
  connect(ui_mainview.listView, SIGNAL(clicked(const QModelIndex &)), SLOT(listViewOnClick(const QModelIndex &)));
  connect(ui_mainview.listView, SIGNAL(entered(const QModelIndex &)), SLOT(listViewOnMouseMove(const QModelIndex &)));
  connect(ui_mainview.buttonSearchDone, SIGNAL(clicked()), SLOT(buttonDone()) );
  connect(ui_mainview.checkCard, SIGNAL(toggled(bool)), SLOT(checksChanged())  );
  connect(ui_mainview.checkCash, SIGNAL(toggled(bool)), SLOT(checksChanged())  );
  connect(ui_mainview.editAmount,SIGNAL(returnPressed()), SLOT(finishCurrentTransaction()) );
  connect(ui_mainview.editAmount, SIGNAL(textChanged(const QString &)), SLOT(refreshTotalLabel()));
  connect(ui_mainview.editCardNumber, SIGNAL(returnPressed()), SLOT(goSelectCardAuthNumber()) );
  connect(ui_mainview.editCardAuthNumber, SIGNAL(returnPressed()), SLOT(finishCurrentTransaction()) );
  connect(ui_mainview.splitter, SIGNAL(splitterMoved(int, int)), SLOT(setUpTable()));
  connect(ui_mainview.comboClients, SIGNAL(currentIndexChanged(int)), SLOT(comboClientsOnChange()));
  connect(ui_mainview.btnChangeSaleDate, SIGNAL(clicked()), SLOT(showChangeDate()));
  connect(ui_mainview.editClient, SIGNAL(clicked()), SLOT(editClient()));

  ui_mainview.editTicketDatePicker->setDate(QDate::currentDate());
  connect(ui_mainview.editTicketDatePicker, SIGNAL(dateChanged(const QDate &)), SLOT(setHistoryFilter()) );
  connect(ui_mainview.btnTicketDone, SIGNAL(clicked()), SLOT(btnTicketsDone()) );
  connect(ui_mainview.btnTicketPrint, SIGNAL(clicked()), SLOT(printSelTicket()) );
  connect(ui_mainview.ticketView, SIGNAL(doubleClicked(const QModelIndex &)), SLOT(itemHIDoubleClicked(const QModelIndex &)) );

  connect(ui_mainview.editItemCode, SIGNAL(plusKeyPressed()), this, SLOT(plusPressed()));

  timerClock->start(1000);

  drawer = new Gaveta();
  drawer->setPrinterDevice(Settings::printerDevice());
  //NOTE: setPrinterDevice: what about CUPS printers recently added support for?
  drawerCreated = true;
  
  operationStarted = false;
  productsHash.clear();
  clientsHash.clear();
  //ui_mainview.lblClientPhoto->hide();
  ui_mainview.labelInsertCodeMsg->hide();
  transDateTime = QDateTime::currentDateTime();
  ui_mainview.editTransactionDate->setDateTime(transDateTime);
  ui_mainview.groupSaleDate->hide();


  ui_mainview.editItemCode->setEmptyMessage(i18n("Enter code or qty*code. <Enter> or <+> Keys to go pay"));

  clearUsedWidgets();
  loadIcons();
  setUpInputs();
  QTimer::singleShot(500, this, SLOT(setUpTable()));
  ui_mainview.groupWidgets->setCurrentIndex(pageMain);
  ui_mainview.mainPanel->setCurrentIndex(pageMain);

  QTimer::singleShot(1000, this, SLOT(setupGridView()));
TRACE;
}

void lemonView::editClient()
{
    qulonglong nid=clientInfo.id;
    QString oldclientname=""
	,clientname="";
    QAbstractItemModel *model;

DBX(nid);
DBX(clientInfo.fname << clientInfo.name );

    //Launch Edit dialog
    ClientEditor *clientEditorDlg = new ClientEditor(this);

DBX(clientInfo.address);
DBX(clientInfo.phone);

    if(nid>1) {
DBX(nid);
	clientEditorDlg->setId(clientInfo.id);
	clientEditorDlg->setFName(clientInfo.fname);
	clientEditorDlg->setName(clientInfo.name);
	clientEditorDlg->setAddress(clientInfo.address);
	clientEditorDlg->setPhone(clientInfo.phone);
	clientEditorDlg->setCell(clientInfo.cell);
	clientEditorDlg->setPoints(clientInfo.points);
	clientEditorDlg->setDiscount(clientInfo.discount);

        if(!clientInfo.fname.isEmpty() && !clientInfo.name.isEmpty()) oldclientname = QString("%1,%2").arg(clientInfo.name).arg(clientInfo.fname);
        else if(!clientInfo.name.isEmpty()) oldclientname = clientInfo.name;
        else if(!clientInfo.fname.isEmpty()) oldclientname = clientInfo.fname;
	}

    if (clientEditorDlg->exec() ) {
TRACE;
      clientInfo.fname    = clientEditorDlg->getFName();
      clientInfo.name     = clientEditorDlg->getName();
      clientInfo.address  = clientEditorDlg->getAddress();
      clientInfo.phone    = clientEditorDlg->getPhone();
      clientInfo.cell     = clientEditorDlg->getCell();
      clientInfo.points   = clientEditorDlg->getPoints();
      clientInfo.discount = clientEditorDlg->getDiscount();
      clientInfo.since    = clientEditorDlg->getSinceDate();

      if(!clientInfo.fname.isEmpty() && !clientInfo.name.isEmpty()) clientname = QString("%1,%2").arg(clientInfo.name).arg(clientInfo.fname);
      else if(!clientInfo.name.isEmpty()) clientname = clientInfo.name;
      else if(!clientInfo.fname.isEmpty()) clientname = clientInfo.fname;

      Azahar *myDb = new Azahar;
      myDb->setDatabase(db);

//	UPDATE
      if(nid>1) {
		int oldidx=ui_mainview.comboClients->findText(oldclientname);

		clientInfo.id       = nid;

		myDb->updateClient(clientInfo);

 	   	clientsHash.remove(QString::number(clientInfo.id));
		clientsHash.insert(QString::number(clientInfo.id), clientInfo);
		if(clientname != oldclientname && oldidx>=0) {
			comboClients->setItemText(oldidx,clientname);
			model =ui_mainview.comboClients->model();
			model->sort(0);
			
			oldidx=ui_mainview.comboClients->findText(clientname);
			if(oldidx>=0) ui_mainview.comboClients->setCurrentIndex(oldidx);
			}
//	INSERT
      } else {
       	    int idx;
            nid=myDb->insertClient(clientInfo);
DBX(nid);
	    if(nid>0) {
DBX(nid);
		clientInfo.id=nid;
		clientsHash.insert(QString::number(clientInfo.id), clientInfo);
		ui_mainview.comboClients->addItem(clientname);
		model =ui_mainview.comboClients->model();
		model->sort(0);

		idx=ui_mainview.comboClients->findText(clientname);
		if(idx>=0) ui_mainview.comboClients->setCurrentIndex(idx);
		}
            }

TRACE;
     }
     delete clientEditorDlg;
DBX(clientInfo.id);
DBX(clientInfo.name);
}

void lemonView::showChangeDate()
{
  ui_mainview.groupSaleDate->show();
TRACE;
}

void lemonView::setupGridView()
{
  if (Settings::showGrid()) emit signalShowProdGrid();
  else showProductsGrid(false);
  
TRACE;
}

void lemonView::loadIcons()
{
  ui_mainview.labelImageSearch->setPixmap(DesktopIcon("edit-find", 64));
  QString logoBottomFile = KStandardDirs::locate("appdata", "images/logo_bottom.png");
  ui_mainview.labelBanner->setPixmap(QPixmap(logoBottomFile));
  ui_mainview.labelBanner->setAlignment(Qt::AlignCenter);
TRACE;
}

void lemonView::setUpTable()
{
  QSize tableSize = ui_mainview.tableWidget->size();
  int portion = tableSize.width()/10;
  ui_mainview.tableWidget->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colCode, portion); //BAR CODE
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colDesc, (portion*4)+9); //DESCRIPTION
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colPrice, portion); //PRICE
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colQty, portion-20);  //QTY
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colUnits, portion-15);//UNITS
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colDisc, portion); //Discount
  ui_mainview.tableWidget->horizontalHeader()->resizeSection(colDue, portion+10); //DUE
  //search table
  tableSize = ui_mainview.tableSearch->size();
  ui_mainview.tableSearch->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
  ui_mainview.tableSearch->horizontalHeader()->resizeSection(0, 2*(tableSize.width()/4));
  ui_mainview.tableSearch->horizontalHeader()->resizeSection(1, tableSize.width()/4);
  ui_mainview.tableSearch->horizontalHeader()->resizeSection(2, tableSize.width()/4);
TRACE;
}

void lemonView::setUpInputs()
{
  //TODO: Tratar de poner un filtro con lugares llenos de ceros, e ir insertando los numeros.
  //For amount received.
//kh  QRegExp regexpA("[0-9]*[//.]{0,1}[0-9][0-9]*"); //Cualquier numero flotante (0.1, 100, 0, .10, 100.0, 12.23)
  QRegExp regexpA("[0-9]*[//.,]{0,1}[0-9][0-9]*"); //Cualquier numero flotante (0.1, 100, 0, .10, 100.0, 12.23)
  QRegExpValidator * validatorFloat = new QRegExpValidator(regexpA,this);
  ui_mainview.editAmount->setValidator(validatorFloat);
  //Item code (to insert) //
  QRegExp regexpC("[0-9]+[0-9]*[//.]{0,1}[0-9]{0,2}[xX//*]{0,1}[0-9]{0,13}");
  QRegExpValidator * validatorEAN13 = new QRegExpValidator(regexpC, this);
  ui_mainview.editItemCode->setValidator(validatorEAN13);
  QRegExp regexpAN("[A-Za-z_0-9\\\\/\\-]+");//any letter, number, both slashes, dash and lower dash.
  QRegExpValidator *regexpAlpha = new QRegExpValidator(regexpAN, this);
  ui_mainview.editCardAuthNumber->setValidator(regexpAlpha);

  //ui_mainview.editAmount->setInputMask("000,000.00");
TRACE;
}

void lemonView::timerTimeout()
{
  emit signalUpdateClock();

  //remover comas..
//   QString s="1,231.22";
//   QString x = s.replace(QRegExp("[\\$,]"), "");
//   qDebug()<<"s:"<<s<<" x:"<<x;

}

void lemonView::clearLabelPayMsg()
{
  ui_mainview.labelPayMsg->clear();
TRACE;
}

void lemonView::clearLabelInsertCodeMsg()
{
  ui_mainview.labelInsertCodeMsg->clear();
  ui_mainview.labelInsertCodeMsg->hide();
TRACE;
}

void::lemonView::clearLabelSearchMsg()
{
  ui_mainview.labelSearchMsg->clear();
TRACE;
}

void lemonView::setTheSplitterSizes(QList<int> s)
{
  ui_mainview.splitter->setSizes(s);
TRACE;
}

QList<int> lemonView::getTheSplitterSizes()
{
  return ui_mainview.splitter->sizes();
TRACE;
}

//This ensures that when not connected to mysql, the user can configure the db settings and then trying to connect
//with the new settings...
void lemonView::settingsChanged()
{
  //Total label (and currency label)
  refreshTotalLabel();

  ///This is a temporal workaround for the crash. I think is due to a kde bug. It does not crashes with kde 4.1.4 on kubuntu
  //Reconnect to db..
  if (db.isOpen()) db.close();
TRACE;
  qDebug()<<"-Config Changed- reconnecting to database..";

  db.setHostName(Settings::editDBServer());
  db.setDatabaseName(Settings::editDBName());
  db.setUserName(Settings::editDBUsername());
  db.setPassword(Settings::editDBPassword());
  connectToDb();
  setupModel();
  setupHistoryTicketsModel();
  setupClients();

TRACE;
}

void lemonView::settingsChangedOnInitConfig()
{
TRACE;
  qDebug()<<"==> Initial Config Changed- connecting to database and calling login...";
  //the db = QSqlDatabase... thing is causing a crash.
  //QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
  //db = QSqlDatabase::addDatabase("QMYSQL");
  db.setHostName(Settings::editDBServer());
  db.setDatabaseName(Settings::editDBName());
  db.setUserName(Settings::editDBUsername());
  db.setPassword(Settings::editDBPassword());

  ///This is also affected by the weird crash.
  connectToDb();
  setupModel();
  setupHistoryTicketsModel();
  setupClients();

  login();
TRACE;
}

void lemonView::showEnterCodeWidget()
{
  ui_mainview.groupWidgets->setCurrentIndex(pageMain);
  // BFB. Toggle editItemCode and editFilterByDesc.
  if (ui_mainview.editItemCode->hasFocus()){
    ui_mainview.rbFilterByDesc->setChecked(true);
    ui_mainview.editFilterByDesc->setFocus();
TRACE;
  }else
    ui_mainview.editItemCode->setFocus();
  setUpTable();
TRACE;
}

void lemonView::decreaseAmount()
{
TRACE;
}


void lemonView::showSearchItemWidget()
{
  ui_mainview.mainPanel->setCurrentIndex(pageSearch); // searchItem
  ui_mainview.editItemDescSearch->setFocus();
  setUpTable();
TRACE;
}

void lemonView::buttonDone()
{
TRACE;
  ui_mainview.tableSearch->setRowCount(0);
  ui_mainview.labelSearchMsg->setText("");
  ui_mainview.editItemDescSearch->setText("");
  ui_mainview.editItemCode->setCursorPosition(0);
  ui_mainview.mainPanel->setCurrentIndex(0); // back to welcome widget
TRACE;
}

void lemonView::checksChanged()
{
  if (ui_mainview.checkCash->isChecked())
  {
    ui_mainview.stackedWidget->setCurrentIndex(0);
    ui_mainview.editAmount->setFocus();
    ui_mainview.editAmount->setSelection(0,ui_mainview.editAmount->text().length());
TRACE;
  }//cash
  else  //Card, need editCardkNumber...
  {
    ui_mainview.stackedWidget->setCurrentIndex(1);
    ui_mainview.editAmount->setText(QString::number(totalSum));
    ui_mainview.editCardNumber->setFocus();
    ui_mainview.editCardNumber->setSelection(0,ui_mainview.editCardNumber->text().length());
TRACE;
  }
  refreshTotalLabel();
TRACE;
}

void lemonView::clearUsedWidgets()
{
  ui_mainview.editAmount->setText("");
  ui_mainview.editCardNumber->setText("");
  ui_mainview.editCardAuthNumber->setText("");
  ui_mainview.tableWidget->clearContents();
  ui_mainview.tableWidget->setRowCount(0);
  totalSum = 0.0;
  buyPoints = 0;
  ui_mainview.labelDetailUnits->setText("");
  ui_mainview.labelDetailDesc->setText(i18n("No product selected"));
  ui_mainview.labelDetailPrice->setText("");
  ui_mainview.labelDetailDiscount->setText("");
  ui_mainview.labelDetailTotalTaxes->setText("");
  ui_mainview.labelDetailPhoto->clear();
  ui_mainview.labelDetailPoints->clear();
TRACE;
}

void lemonView::askForIdToCancel()
{
  bool continuar=false;
  if (Settings::lowSecurityMode()) {//qDebug()<<"LOW security mode";
    continuar=true;
TRACE;
  } else if (Settings::requiereDelAuth()) {// qDebug()<<"NO LOW security mode, but AUTH REQUIRED!";
    dlgPassword->show();
    dlgPassword->hide();
    dlgPassword->clearLines();
    continuar = dlgPassword->exec();
TRACE;
  } else {//     qDebug()<<"NO LOW security mode, NO AUTH REQUIRED...";
    continuar=true;
TRACE;
  }

  if (continuar) { //show input dialog to get ticket number
    bool ok=false;
    qulonglong id = 0;
    InputDialog *dlg = new InputDialog(this, true, dialogTicket, i18n("Enter the ticket number to cancel"));
    if (dlg->exec())
    {
      id = dlg->iValue;
      ok = true;
TRACE;
    }
    if (ok) {                 // NOTE :
      cancelTransaction(id); //Mark as cancelled in database..  is this transaction
                            //done in the current operation, or a day ago, a month ago, 10 hours ago?
                           //Allow cancelation of same day of sell, or older ones too?
TRACE;
    }//ok=true
TRACE;
  } //continuar
TRACE;
}

///NOTE: Not implemented yet
void lemonView::askForTicketToReturnProduct()
{
  bool continuar=false;
  if (Settings::lowSecurityMode()) {//qDebug()<<"LOW security mode";
    continuar=true;
TRACE;
  } else if (Settings::requiereDelAuth()) {// qDebug()<<"NO LOW security mode, but AUTH REQUIRED!";
    dlgPassword->show();
    dlgPassword->hide();
    dlgPassword->clearLines();
    continuar = dlgPassword->exec();
TRACE;
  } else {//     qDebug()<<"NO LOW security mode, NO AUTH REQUIRED...";
    continuar=true;
TRACE;
  }
  
  if (continuar) { //show input dialog to get ticket number
    bool ok=false;
    qulonglong id = 0;
    InputDialog *dlg = new InputDialog(this, true, dialogTicket, i18n("Enter the ticket number"));
    if (dlg->exec())
    {
      id = dlg->iValue;
      ok = true;
TRACE;
    }
    if (ok) {
      // show dialog to select which items to return.
      
TRACE;
    }//ok=true
TRACE;
  } //continuar
TRACE;
}

void lemonView::focusPayInput()
{
  ui_mainview.groupWidgets->setCurrentIndex(pageMain);
  ui_mainview.editAmount->setFocus();
  ui_mainview.editAmount->setSelection(0, ui_mainview.editAmount->text().length());
TRACE;
}

//This method sends the focus to the amount to be paid only when the code input is empty.
void lemonView::plusPressed()
{
  if ( !ui_mainview.editItemCode->text().isEmpty() )
    doEmitSignalQueryDb();
  else 
    focusPayInput();
TRACE;
}

void lemonView::goSelectCardAuthNumber()
{
  ui_mainview.editCardAuthNumber->setFocus();
TRACE;
}

lemonView::~lemonView()
{
  drawerCreated=false;
  delete drawer;
TRACE;
}


/* Users zone                                                                                                          */
/*=====================================================================================================================*/

QString lemonView::getLoggedUser()
{
  return loggedUser;
TRACE;
}

QString lemonView::getLoggedUserName(QString id)
{
  QString uname = "";
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  uname = myDb->getUserName(id);
TRACE;
  return uname;
}

int lemonView::getUserRole(qulonglong id)
{
  int role = 0;
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  role = myDb->getUserRole(id);
TRACE;
  return role;
}

qulonglong lemonView::getLoggedUserId(QString uname)
{
  unsigned int iD=0;
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  iD = myDb->getUserId(uname);
TRACE;
  return iD;
}

void lemonView::login()
{
  //Make a corteDeCaja
  if (!loggedUser.isEmpty() && operationStarted) {
    corteDeCaja();
    loggedUser = "";
    loggedUserName ="";
    loggedUserRole = roleBasic;
    emit signalNoLoggedUser();
TRACE;
  }

  dlgLogin->clearLines();
  if (!db.isOpen()) {
      qDebug()<<"(login): Calling connectToDb()...";
      connectToDb();
TRACE;
  }

  if (!db.isOpen()) {
TRACE;
    qDebug()<<"(4/login): Still unable to open connection to database....";
    doNotify("dialog-error",i18n("Could not connect to database, please press <i><b>login<b></i> button again to raise a database configuration."));
TRACE;
  } else {
    if ( dlgLogin->exec() ) {
      loggedUser     = dlgLogin->username();
      loggedUserName = getLoggedUserName(loggedUser);
      loggedUserId   = getLoggedUserId(loggedUser);
      loggedUserRole = getUserRole(loggedUserId);
      emit signalLoggedUser();
      //Now check roles instead of names
      if (loggedUserRole == roleAdmin) {
	emit signalAdminLoggedOn();
	//if (!canStartSelling()) startOperation();
TRACE;
      } else {
	emit signalAdminLoggedOff();
        if (loggedUserRole == roleSupervisor)
          emit signalSupervisorLoggedOn();
	else
          slotDoStartOperation();
TRACE;
      }
TRACE;
    } else {
      loggedUser ="";
      loggedUserName = "";
      loggedUserId = 0;
      loggedUserRole = roleBasic;
      emit signalNoLoggedUser();
      if (dlgLogin->wantToQuit()) qApp->quit();
TRACE;
    }
TRACE;
  }
}

bool lemonView::validAdminUser()
{
  bool result = false;
  if (Settings::lowSecurityMode()) result = true;
  else {
      dlgPassword->show();
      dlgPassword->hide();
      dlgPassword->clearLines();
      if (dlgPassword->exec())  result = true;
TRACE;
  }
TRACE;
  return result;
}

/* Item things: shopping list, search, insert, delete, calculate total */
/*--------------------------------------------------------------------*/

void lemonView::doSearchItemDesc()
{
  //clear last search
  ui_mainview.tableSearch->clearContents();
  ui_mainview.tableSearch->setRowCount(0);
  //Search
  QString desc = ui_mainview.editItemDescSearch->text();
  QRegExp regexp = QRegExp(desc);
    if (!regexp.isValid() || desc.isEmpty())  desc = "*";
  if (!db.isOpen()) db.open();

  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QList<qulonglong> pList = myDb->getProductsCode(desc); //busca con regexp...
  //iteramos la lista
  for (int i = 0; i < pList.size(); ++i) {
     qulonglong c = pList.at(i);
     ProductInfo pInfo = myDb->getProductInfo(QString::number(c));
     //insert each product to the search table...
     int rowCount = ui_mainview.tableSearch->rowCount();
      ui_mainview.tableSearch->insertRow(rowCount);
      QTableWidgetItem *tid = new QTableWidgetItem(pInfo.desc);
      if (pInfo.stockqty == 0) {
        QBrush b = QBrush(QColor::fromRgb(255,100,0), Qt::SolidPattern);
        tid->setBackground(b);
TRACE;
      }
      else if (pInfo.stockqty > 0 && pInfo.stockqty < Settings::stockAlertValue() ) {//NOTE:This must be shared between lemon and squeeze
        QBrush b = QBrush(QColor::fromRgb(255,176,73), Qt::SolidPattern);
        tid->setBackground(b);
TRACE;
      }
      ui_mainview.tableSearch->setItem(rowCount, 0, tid);
      //NOTE:bug fixed Sept 26 2008: Without QString::number, no data was inserted.
      // if it is passed a numer as the only parameter to QTableWidgetItem, it is taken as a type
      // and not as a data to display.
      ui_mainview.tableSearch->setItem(rowCount, 1, new QTableWidgetItem(QString::number(pInfo.price)));
      ui_mainview.tableSearch->setItem(rowCount, 2, new QTableWidgetItem(QString::number(pInfo.code)));
      ui_mainview.tableSearch->resizeRowsToContents();
TRACE;
    }
    if (pList.count()>0) ui_mainview.labelSearchMsg->setText(i18np("%1 item found","%1 items found.", pList.count()));
    else ui_mainview.labelSearchMsg->setText(i18n("No items found."));

TRACE;
  }

int lemonView::getItemRow(QString c)
{
  int result = 0;
  for (int row=0; row<ui_mainview.tableWidget->rowCount(); ++row)
  {
    QTableWidgetItem * item = ui_mainview.tableWidget->item(row, colCode);
    QString icode = item->data(Qt::DisplayRole).toString();
    if (icode == c) {
      result = row;
      break;
TRACE;
    }
TRACE;
  }
TRACE;
  return result; //0 if not found
}

void lemonView::refreshTotalLabel()
{
  double sum=0.0;
  qulonglong points=0;
  if (ui_mainview.tableWidget->rowCount()>0) {
    for (int row=0; row<ui_mainview.tableWidget->rowCount(); ++row)
    {
      QTableWidgetItem *item = ui_mainview.tableWidget->item(row, colDue);
      bool isNumber = false;
      if (item->data(Qt::DisplayRole).canConvert(QVariant::String)) {
        QString text = item->data(Qt::DisplayRole).toString();
        double number = text.toDouble(&isNumber);
        if (isNumber) sum += number;
DBX(number);
      }
TRACE;
    }
DBX(sum);
    ProductInfo info;
    QHashIterator<qulonglong, ProductInfo> i(productsHash);
DBX(i.hasNext());
    while (i.hasNext()) {
      i.next();
      info = i.value();
DBX(info.code);
DBX(info.serialNo);
      points += (info.points*info.qtyOnList);
//       qDebug()<<info.desc<<" qtyOnList:"<<info.qtyOnList;
TRACE;
    }
TRACE;
  }
  buyPoints = points;
  if(sum > 0.001 && clientInfo.discount> 0.001 ) discMoney = (clientInfo.discount/100)*sum;
  else discMoney=0.0;
  totalSum = sum - discMoney;
DBX(sum);
DBX(discMoney);
DBX(totalSum);
  ui_mainview.labelTotal->setText(QString("%1").arg(KGlobal::locale()->formatMoney(totalSum)));
  ui_mainview.labelClientDiscounted->setText(i18n("Amount Discounted: %1", KGlobal::locale()->formatMoney(discMoney)));
  long double paid, change;
  bool isNum;
  paid = ui_mainview.editAmount->text().toDouble(&isNum);
  if (isNum) change = paid - totalSum; else change = 0.0;
  if (paid <= 0) change = 0.0;
  ui_mainview.labelChange->setText(QString("%1") .arg(KGlobal::locale()->formatMoney(change)));
TRACE;
}

void lemonView::doEmitSignalQueryDb()
{
  emit signalQueryDb(ui_mainview.editItemCode->text());
TRACE;
}

bool lemonView::incrementTableItemQty(QString code, double q)
{
  double qty  = 1;
  double discount_old=0.0;
  double qty_old=0.0;
  double stockqty=0;
  bool done=false;
  ProductInfo info;

DBX(code);

   if (productsHash.contains(code.toULongLong())) {

    //qDebug()<<"Product on hash: "<<code;
    //get product info...
    info = productsHash.value(code.toULongLong());
    //qDebug()<<"  Product on hash discount :"<<info.disc;
    //qDebug()<<"  Product on hash stock qty:"<<info.stockqty<<"real: "<<(myDb->getProductInfo(code.toULongLong())).stockqty;
    //qDebug()<<"  Inserted at row:"<<info.row;
DBX(info.code);
    stockqty = info.stockqty;
    qty = info.qtyOnList;
    qty_old = qty;
    qty+=q; //kh 2012-07-15
//kh 2012-07-15    if (stockqty>=q+qty) qty+=q; else  ...
    if (stockqty<q+qty) {
      QString msg = "<html><font color=red><b>" + i18n("Product not available in stock") +".</b></font>";
DBX(msg);
      if (ui_mainview.mainPanel->currentIndex() == pageMain) {
         ui_mainview.labelInsertCodeMsg->setText(msg);
         ui_mainview.labelInsertCodeMsg->show();
         QTimer::singleShot(3000, this, SLOT(clearLabelInsertCodeMsg()));
TRACE;
      }
      if (ui_mainview.mainPanel->currentIndex() == pageSearch) {
         ui_mainview.labelSearchMsg->setText(msg);
         ui_mainview.labelSearchMsg->show();
         QTimer::singleShot(3000, this, SLOT(clearLabelSearchMsg()) );
TRACE;
      }
TRACE;
    }
DBX(info.code);
    QTableWidgetItem *itemQ = ui_mainview.tableWidget->item(info.row, colQty);//item qty
    itemQ->setData(Qt::EditRole, QVariant(qty));
    done = true;
    QTableWidgetItem *itemD = ui_mainview.tableWidget->item(info.row, colDisc);//item discount
    discount_old = itemD->data(Qt::DisplayRole).toDouble();
    //calculate new discount
    double discountperitem = (discount_old/qty_old);
    double newdiscount = discountperitem*qty;
    itemD->setData(Qt::EditRole, QVariant(newdiscount));

    info.qtyOnList = qty;
    //qDebug()<<"  New qty on list:"<<info.qtyOnList;
    productsHash.remove(code.toULongLong());
    productsHash.insert(info.code, info);

DBX(info.code);
    //get item Due to update it.
    QTableWidgetItem *itemDue = ui_mainview.tableWidget->item(info.row, colDue); //4 item Due
    itemDue->setData(Qt::EditRole, QVariant((info.price*qty)-newdiscount));//fixed on april 30 2009 00:35. Added *qty
    refreshTotalLabel();
    QTableWidgetItem *item = ui_mainview.tableWidget->item(info.row, colCode);//item code
    displayItemInfo(item); //TODO: Cambiar para desplegar de ProductInfo.
    ui_mainview.editItemCode->clear();
TRACE;
   }//if productsHash.contains...

TRACE;
  return done;
}

void lemonView::insertItem(QString code)
{
  double qty  = 1;
  QString codeX = code;
  qlonglong codeLL;

  ProductInfo PrInf;
  PrInf.code = 0;
  PrInf.desc = "[INVALID]";

  //now code could contain number of items to insert,example: 10x12345678990 or 10*1234567890
  QStringList list = code.split(QRegExp("[xX//*]{1,1}"),QString::SkipEmptyParts);
  if (list.count()==2) {
    qty =   list.takeAt(0).toDouble();
    codeX = list.takeAt(0);
TRACE;
  }
DBX(code);

  codeLL=codeX.toULongLong();
  
DBX(codeX);

  if(codeLL<=MAX_SPECIAL_CODE) {
    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    PrInf = myDb->getProductInfo(codeX); 
    if(PrInf.code) {
DBX(PrInf.code);
      PrInf.code=TMP_CODE_ADD+time(NULL);
      PrInf.price=0;
      PrInf.cost=0;
      codeX=QString::number(PrInf.code);
      }
DBX(PrInf.code);
  //verify item units and qty..
  } else if (productsHash.contains(codeX.toULongLong())) {
    PrInf = productsHash.value(codeX.toULongLong());
TRACE;
  } else {
    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    PrInf = myDb->getProductInfo(codeX); //includes discount and validdiscount
TRACE;
  }
DBX(PrInf.isIndividual);
  
  if(PrInf.isIndividual) {
	if(!askSerialNo()) return;
	}
  else GserialNo="";

DBX(PrInf.code);
DBX(GserialNo);

  if (PrInf.units == uPiece) {
    unsigned int intqty = qty;
    qty = intqty;
TRACE;
  }
  
  if ( qty <= 0) {
TRACE;
	return;
	}

DBX(GserialNo);
  if (codeLL<=MAX_SPECIAL_CODE || PrInf.isIndividual || !incrementTableItemQty(codeX, qty) ) {
    //As it was not incremented on tableView, so there is not in the productsHash... so we get it from db.
    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    if(codeLL>MAX_SPECIAL_CODE) PrInf = myDb->getProductInfo(codeX); //includes discount and validdiscount

    while(PrInf.code>0 && !PrInf.price) {
      InputDialog *dlg = new InputDialog(this, false, dialogMoney, QString(i18n("Enter the price for %1")).arg(PrInf.desc));
      if (dlg->exec()) {
        PrInf.price = dlg->dValue;
	if(!PrInf.cost) PrInf.cost=PrInf.price - calcGrossTax(PrInf.price,PrInf.tax);
DBX(PrInf.price);
DBX(PrInf.cost);
DBX(PrInf.tax);
      } else {
	return;
        }
    }// while(!PrInf.price)

    //verify item units and qty..
    if (PrInf.units == uPiece) { 
      unsigned int intqty = qty;
      qty = intqty;
TRACE;
    }
    
    PrInf.qtyOnList = qty;
    PrInf.serialNo=GserialNo;
    GserialNo="";

DBX(PrInf.code);
DBX(GserialNo);

    QString msg;
    int insertedAtRow = -1;
    bool productFound = false;
    if (PrInf.code > 0) productFound = true;
    double descuento=0.0;
    if (PrInf.validDiscount) descuento = PrInf.disc*qty;//fixed on april 30 2009 00:35. Added *qty
    //qDebug()<<"insertItem:: descuento total del producto:"<<descuento;
DBX(PrInf.price << PrInf.code << codeX);

    if ( !productFound )  msg = "<html><font color=red><b>" +i18n("Product not found in database.") + "</b></font></html>";
    else {

	insertedAtRow = doInsertItem(codeX, PrInf.desc, qty, PrInf.price, descuento, PrInf.unitStr);
        if (codeLL>MAX_SPECIAL_CODE &&  PrInf.stockqty <  qty )
		msg = "<html><font color=red><b>" + i18n("There are only %1 articles of your choice at stock.",PrInf.stockqty) + "</b></font></html>";
TRACE;
	}

    if (!msg.isEmpty()) {
        if (ui_mainview.mainPanel->currentIndex() == pageMain) {
          ui_mainview.labelInsertCodeMsg->setText(msg);
          ui_mainview.labelInsertCodeMsg->show();
          QTimer::singleShot(3000, this, SLOT(clearLabelInsertCodeMsg()));
TRACE;
        }
        if (ui_mainview.mainPanel->currentIndex() == pageSearch) {
          ui_mainview.labelSearchMsg->setText(msg);
          ui_mainview.labelSearchMsg->show();
          QTimer::singleShot(3000, this, SLOT(clearLabelSearchMsg()) );
TRACE;
        }
    ui_mainview.editItemCode->clear();
TRACE;
    }


    PrInf.row = insertedAtRow;
    if (PrInf.row >-1 && PrInf.desc != "[INVALID]" && PrInf.code>0){
      productsHash.insert(codeX.toULongLong(), PrInf);
//       qDebug()<<"INSERTED AT ROW:"<<insertedAtRow;
      QTableWidgetItem *item = ui_mainview.tableWidget->item(PrInf.row, colCode);
      displayItemInfo(item);
TRACE;
    }
TRACE;
  }//if !increment...
TRACE;
}//insertItem


int lemonView::doInsertItem(QString itemCode, QString itemDesc, double itemQty, double itemPrice, double itemDiscount, QString itemUnits)
{
  int rowCount = ui_mainview.tableWidget->rowCount();
  ui_mainview.tableWidget->insertRow(rowCount);
DBX(GserialNo);
DBX(itemCode.toULongLong());
  if(itemCode.toULongLong()< TMP_CODE_ADD) {
	ui_mainview.tableWidget->setItem(rowCount, colCode, new QTableWidgetItem(itemCode));
DBX(itemCode.toULongLong());
  } else {
	  ui_mainview.tableWidget->setItem(rowCount, colCode, new QTableWidgetItem("-"));
DBX(itemCode.toULongLong());
	}

  if(!GserialNo.isEmpty())
     ui_mainview.tableWidget->setItem(rowCount, colDesc, new QTableWidgetItem(QString("%1 (%2)").arg(itemDesc).arg(GserialNo) ));
  else
     ui_mainview.tableWidget->setItem(rowCount, colDesc, new QTableWidgetItem(itemDesc));

  ui_mainview.tableWidget->setItem(rowCount, colPrice, new QTableWidgetItem(""));//must be empty for HACK
  ui_mainview.tableWidget->setItem(rowCount, colQty, new QTableWidgetItem(QString::number(itemQty)));
  ui_mainview.tableWidget->setItem(rowCount, colUnits, new QTableWidgetItem(itemUnits));
  ui_mainview.tableWidget->setItem(rowCount, colDisc, new QTableWidgetItem(""));//must be empty for HACK
  ui_mainview.tableWidget->setItem(rowCount, colDue, new QTableWidgetItem(""));//must be empty for HACK

DBX(itemCode << itemPrice);
  QTableWidgetItem *item = ui_mainview.tableWidget->item(rowCount, colDisc);
  if (itemDiscount>0) {
    QBrush b = QBrush(QColor::fromRgb(255,0,0), Qt::SolidPattern);
    item->setForeground(b);
TRACE;
  }
  //HACK:The next 4 lines are for setting numbers with comas (1,234.00) instead of 1234.00.
  //      seems to be an effect of QVariant(double d)
  item->setData(Qt::EditRole, QVariant(itemDiscount));
  item = ui_mainview.tableWidget->item(rowCount, colDue);
  item->setData(Qt::EditRole, QVariant((itemQty*itemPrice)-itemDiscount)); //fixed on april 30 2009 00:35.
  item = ui_mainview.tableWidget->item(rowCount, colPrice);
  item->setData(Qt::EditRole, QVariant(itemPrice));

  //This resizes the heigh... looks beter...
  ui_mainview.tableWidget->resizeRowsToContents();

  if (productsHash.contains(itemCode.toULongLong())) { //Codigo mudado de int a Unsigned long long: qulonlong
    ProductInfo  PrInf = productsHash.value(itemCode.toULongLong());
    if (PrInf.units != uPiece) itemDoubleClicked(item);//NOTE: Pieces must be id=1 at database!!!! its a workaround.
    //STqDebug()<<"itemDoubleClicked at doInsertItem...";
TRACE;
  }

  refreshTotalLabel();
  // BFB: editFilterbyDesc keeps the focus,
  if (!ui_mainview.editFilterByDesc->hasFocus())
    ui_mainview.editItemCode->setFocus();

  ui_mainview.editItemCode->setText("");
  ui_mainview.editItemCode->setCursorPosition(0);
  ui_mainview.mainPanel->setCurrentIndex(pageMain);

TRACE;
  return rowCount;
}

void lemonView::deleteSelectedItem()
{
  bool continueIt=false;
  bool reinsert = false;
  double qty=0;
  if (ui_mainview.tableWidget->currentRow()!=-1 && ui_mainview.tableWidget->selectedItems().count()>4) {
    if ( !Settings::lowSecurityMode() ) {
        if (Settings::requiereDelAuth() ) {
          dlgPassword->show();
          dlgPassword->hide();
          dlgPassword->clearLines();
          if ( dlgPassword->exec() ) continueIt=true;
TRACE;
        } else continueIt=true; //if requiereDelAuth
TRACE;
    } else continueIt=true; //if no low security

    if (continueIt) {
      int row = ui_mainview.tableWidget->currentRow();
      QTableWidgetItem *item = ui_mainview.tableWidget->item(row, colCode);
      qulonglong code = item->data(Qt::DisplayRole).toULongLong();
      ProductInfo PrInf = productsHash.take(code); //insert it later...
      qty = PrInf.qtyOnList; //this must be the same as obtaining from the table... this arrived on Dec 18 2007
     //if the itemQty is more than 1, decrement it, if its 1, delete it
     //item = ui_mainview.tableWidget->item(row, colDisc);
     //double discount_old = item->data(Qt::DisplayRole).toDouble();
      item = ui_mainview.tableWidget->item(row, colUnits);//get item Units in strings...
      QString iUnitString = item->data(Qt::DisplayRole).toString();
      item = ui_mainview.tableWidget->item(row, colQty); //get Qty
      if ((item->data(Qt::DisplayRole).canConvert(QVariant::Double))) {
        qty = item->data(Qt::DisplayRole).toDouble();
       //NOTE and FIXME:
       //  Here, we are going to delete only items that are bigger than 1. and remove them one by one..
       //  or are we goint to decrement items only sold by pieces?
        if (qty>1 && PrInf.units==uPiece) {
          qty--;
          item->setData(Qt::EditRole, QVariant(qty));
          double price    = PrInf.price;
          double discountperitem = PrInf.disc;
          double newdiscount = discountperitem*qty;
          item = ui_mainview.tableWidget->item(row, colDue);
          item->setData(Qt::EditRole, QVariant((qty*price)-newdiscount));
          item = ui_mainview.tableWidget->item(row, colDisc);
          item->setData(Qt::EditRole, QVariant(newdiscount));
          PrInf.qtyOnList = qty;
          reinsert = true;
TRACE;
        }//if qty>1
        else { //Remove from the productsHash and tableWidget...
          //get item code
          //int removed = productsHash.remove(code);
          productsHash.remove(code);
          ui_mainview.tableWidget->removeRow(row);
          reinsert = false;
TRACE;
        }//qty = 1...
TRACE;
      }//if canConvert
      if (reinsert) productsHash.insert(code, PrInf); //we remove it with .take...
      log(loggedUserId, QDate::currentDate(), QTime::currentTime(), QString("Removing an article from shopping list. Authorized by %1").
       arg(dlgPassword->username()));
TRACE;
    }//continueIt
TRACE;
  }//there is something to delete..
  refreshTotalLabel();
TRACE;
}

void lemonView::itemDoubleClicked(QTableWidgetItem* item)
{
  int row = item->row();
  QTableWidgetItem *i2Modify = ui_mainview.tableWidget->item(row, colCode);
  qulonglong code = i2Modify->data(Qt::DisplayRole).toULongLong();
  ProductInfo PrInf = productsHash.take(code);
  double dmaxItems = PrInf.stockqty;
  QString msg = i18n("Enter the number of %1", PrInf.unitStr); //Added on Dec 15, 2007

  //Launch a dialog to as the new qty
  double dqty = 0.0;
  bool   ok   = false;
  int    iqty = 0;
  if (PrInf.units == uPiece) {
    if (dmaxItems > 0) {
      ok = true;
      iqty = PrInf.qtyOnList+1;
      //NOTE: Present a dialog to enter a qty or increment by one ?
TRACE;
    }
TRACE;
  }
  else {
    ///FIXME: Alert the user why is restricted to a max items!
    InputDialog *dlg = new InputDialog(this, false, dialogMeasures, msg, 0.001, dmaxItems);
    if (dlg->exec() ) {
      dqty = dlg->dValue;
      ok=true;
TRACE;
    }
TRACE;
  }
  if (ok) {
    double newqty = dqty+iqty; //one must be zero
    //modify Qty and discount...
    i2Modify = ui_mainview.tableWidget->item(row, colQty);
    i2Modify->setData(Qt::EditRole, QVariant(newqty));
    double price    = PrInf.price;
    double discountperitem = PrInf.disc;
    double newdiscount = discountperitem*newqty;
    i2Modify = ui_mainview.tableWidget->item(row, colDue);
    i2Modify->setData(Qt::EditRole, QVariant((newqty*price)-newdiscount));
    i2Modify = ui_mainview.tableWidget->item(row, colDisc);
    i2Modify->setData(Qt::EditRole, QVariant(newdiscount));
    PrInf.qtyOnList = newqty;

    ui_mainview.editItemCode->setFocus();
TRACE;
  } else {
    msg = "<html><font color=red><b>"+i18n("Product not available in stock for the requested quantity.") +"</b></font></html>";
    ui_mainview.labelInsertCodeMsg->setText(msg);
    ui_mainview.labelInsertCodeMsg->show();
    QTimer::singleShot(3000, this, SLOT(clearLabelInsertCodeMsg()));
TRACE;
  }
  productsHash.insert(code, PrInf);
  refreshTotalLabel();
TRACE;
}

void lemonView::itemSearchDoubleClicked(QTableWidgetItem *item)
{
  int row = item->row();
  QTableWidgetItem *cItem = ui_mainview.tableSearch->item(row,2); //get item code
  qulonglong code = cItem->data(Qt::DisplayRole).toULongLong();
  qDebug()<<"Linea 981: Data at column 2:"<<cItem->data(Qt::DisplayRole).toString();
  if (productsHash.contains(code)) {
    int pos = getItemRow(QString::number(code));
    if (pos>=0) {
      QTableWidgetItem *thisItem = ui_mainview.tableWidget->item(pos, colCode);
      ProductInfo PrInf = productsHash.value(code);
      if (PrInf.units == uPiece) incrementTableItemQty(QString::number(code), 1);
      else itemDoubleClicked(thisItem);
TRACE;
    }
TRACE;
  }
  else {
    insertItem(QString::number(code));
TRACE;
  }
  ui_mainview.mainPanel->setCurrentIndex(pageMain);
TRACE;
}

//FIXME: Con los nuevos TaxModels, sobra informacion (extrataxes). Verificar cantidades tambien (% y dinero)
void lemonView::displayItemInfo(QTableWidgetItem* item)
{
  int row = item->row();
  qulonglong code  = (ui_mainview.tableWidget->item(row, colCode))->data(Qt::DisplayRole).toULongLong();
  QString desc  = (ui_mainview.tableWidget->item(row, colDesc))->data(Qt::DisplayRole).toString();
  double price = (ui_mainview.tableWidget->item(row, colPrice))->data(Qt::DisplayRole).toDouble();
  if (productsHash.contains(code)) {
    ProductInfo PrInf = productsHash.value(code);
    QString uLabel=PrInf.unitStr; // Dec 15  2007

DBX(PrInf.code);
    double discP=0.0;
    if (PrInf.validDiscount) discP = PrInf.discpercentage;
    QString str;
    QString tTotalTax= i18n("Taxes:");
    QString tTax    = i18n("Tax:");
    QString tUnits  = i18n("Sold by:");
    QString tPrice  = i18n("Price:");
    QString tDisc   = i18n("Discount:");
    QString tPoints = i18n("Points:");
    QPixmap pix;
    pix.loadFromData(PrInf.photo);

    ui_mainview.labelDetailPhoto->setPixmap(pix);
    str = QString("%1 (%2 %)")
        .arg(KGlobal::locale()->formatMoney(PrInf.totaltax)).arg(PrInf.tax);
    ui_mainview.labelDetailTotalTaxes->setText(QString("<html>%1 <b>%2</b></html>")
        .arg(tTotalTax).arg(str));
    str = QString("%1 (%2 %)")
        .arg(KGlobal::locale()->formatMoney(PrInf.totaltax)).arg(PrInf.tax);
    ui_mainview.labelDetailUnits->setText(QString("<html>%1 <b>%2</b></html>")
        .arg(tUnits).arg(uLabel));
    ui_mainview.labelDetailDesc->setText(QString("<html><b>%1</b></html>").arg(desc));
    ui_mainview.labelDetailPrice->setText(QString("<html>%1 <b>%2</b></html>")
        .arg(tPrice).arg(KGlobal::locale()->formatMoney(price)));
    ui_mainview.labelDetailDiscount->setText(QString("<html>%1 <b>%2 (%3 %)</b></html>")
        .arg(tDisc).arg(KGlobal::locale()->formatMoney(PrInf.disc)).arg(discP));
    if (PrInf.points>0) {
      ui_mainview.labelDetailPoints->setText(QString("<html>%1 <b>%2</b></html>")
        .arg(tPoints).arg(PrInf.points));
      ui_mainview.labelDetailPoints->show();
TRACE;
    } else ui_mainview.labelDetailPoints->hide();
TRACE;
  }
TRACE;
}

/* TRANSACTIONS ZONE */
/*------------------*/

QString lemonView::getCurrentTransactionString()
{
TRACE;
  return QString::number(getCurrentTransactionId());
}

qulonglong  lemonView::getCurrentTransactionId()
{
TRACE;
  return currentTransaction.id;
}

void lemonView::createNewTransaction(TransactionType type)
{
  //If there is an operation started, doit...
  if ( operationStarted ) {
    if(currentBalance.id<1) {
	currentTransaction.balanceId=createNewBalance();
TRACE;
	}
TRACE;
    currentTransaction.type = type;
    currentTransaction.amount = 0;
    currentTransaction.date   = QDate::currentDate();
    currentTransaction.time   = QTime::currentTime();
    currentTransaction.paywith= 0;
    currentTransaction.changegiven =0;
    currentTransaction.paymethod = pCash;
    currentTransaction.state = tNotCompleted;
    currentTransaction.userid = loggedUserId;
    currentTransaction.clientid = 0;
    currentTransaction.cardnumber ="-NA-";
    currentTransaction.cardauthnum="-NA-";
    currentTransaction.itemcount= 0;
    currentTransaction.itemlist = "";
    currentTransaction.disc = 0;
    currentTransaction.discmoney = 0;
    currentTransaction.points = 0;
    currentTransaction.profit = 0;
    currentTransaction.providerid = 0;
    currentTransaction.totalTax = 0;
    currentTransaction.specialOrders = "";
    currentTransaction.balanceId =  currentBalance.id;
    currentTransaction.terminalnum=Settings::editTerminalNumber();
    currentTransaction.id=0;

    transactionInProgress = true;
      emit signalUpdateTransactionInfo();
TRACE;
    }
  productsHash.clear();
TRACE;
}

void lemonView::finishCurrentTransaction()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);

  bool canfinish = true;
  TicketInfo ticket;
  
  refreshTotalLabel();
  QString msg;
  ui_mainview.mainPanel->setCurrentIndex(pageMain);
  if (ui_mainview.editAmount->text().isEmpty()) ui_mainview.editAmount->setText("0.0");
  if (ui_mainview.checkCash->isChecked()) {
    if (ui_mainview.editAmount->text().toDouble()<totalSum) {
      canfinish = false;
      ui_mainview.editAmount->setFocus();
      ui_mainview.editAmount->setStyleSheet("background-color: rgb(255,100,0); color:white; selection-color: white; font-weight:bold;");
      ui_mainview.editCardNumber->setStyleSheet("");
      ui_mainview.editAmount->setSelection(0, ui_mainview.editAmount->text().length());
      msg = "<html><font color=red><b>" + i18n("Please fill the correct pay amount before finishing a transaction.") + "</b></font></html>";
      ui_mainview.labelPayMsg->setText(msg);
      QTimer::singleShot(3000, this, SLOT(clearLabelPayMsg()));
TRACE;
    }
TRACE;
  }
  else {
    QString cn =  ui_mainview.editCardNumber->text();
    QString cna = ui_mainview.editCardAuthNumber->text();
    if (!ui_mainview.editCardNumber->hasAcceptableInput() || cn.isEmpty() || cn == "---") {
      canfinish = false;
      ui_mainview.editCardNumber->setFocus();
      ui_mainview.editCardNumber->setStyleSheet("background-color: rgb(255,100,0); color:white; font-weight:bold; selection-color: white;");
      ui_mainview.editAmount->setStyleSheet("");
      ui_mainview.editCardNumber->setSelection(0, ui_mainview.editCardNumber->text().length());
      msg = "<html><font color=red><b>" + i18n("Please enter the card number.") + "</b></font></html>";
TRACE;
    }
    else if (!ui_mainview.editCardAuthNumber->hasAcceptableInput() || cna.isEmpty() || cna.length()<4) {
      canfinish = false;
      ui_mainview.editCardAuthNumber->setFocus();
      ui_mainview.editCardAuthNumber->setStyleSheet("background-color: rgb(255,100,0); color:white; font-weight:bold; selection-color: white;");
      ui_mainview.editAmount->setStyleSheet("");
      ui_mainview.editCardAuthNumber->setSelection(0, ui_mainview.editCardAuthNumber->text().length());
      msg = "<html><font color=red><b>" + i18n("Please enter the Authorisation number from the bank voucher.") + "</b></font></html>";
TRACE;
    }
    if (!msg.isEmpty()) {
      ui_mainview.labelPayMsg->setText(msg);
      QTimer::singleShot(3000, this, SLOT(clearLabelPayMsg()));
TRACE;
    }
TRACE;
  }
  if (ui_mainview.tableWidget->rowCount() == 0) canfinish = false;
  if (!canStartSelling()) {
    canfinish=false;
    doNotify("dialog-error",i18n("Before selling, you must start operations."));
TRACE;
  }

  if (canfinish) // Ticket #52: Allow ZERO DUE.
  {
    ui_mainview.editAmount->setStyleSheet("");
    ui_mainview.editCardNumber->setStyleSheet("");
    PaymentType      pType;
    double           payWith = 0.0;
    double           payTotal = 0.0;
    double           changeGiven = 0.0;
    QString          authnumber = "";
    QString          cardNum = "";
    QString          paidStr = "'[Not Available]'";
    QStringList ilist;
    payTotal = totalSum;
    if (ui_mainview.checkCash->isChecked()) {
      pType = pCash;
      if (!ui_mainview.editAmount->text().isEmpty()) payWith = ui_mainview.editAmount->text().toDouble();
      changeGiven = payWith- totalSum;
TRACE;
    } else {
      pType = pCard;
      if (ui_mainview.editCardNumber->hasAcceptableInput()) {
        cardNum = ui_mainview.editCardNumber->text().replace(0,15,"***************"); //FIXED: Only save last 4 digits;
TRACE;
      }
      if (ui_mainview.editCardAuthNumber->hasAcceptableInput()) authnumber = ui_mainview.editCardAuthNumber->text();
      //cardNum = "'"+cardNum+"'";
      //authnumber = "'"+authnumber+"'";
      payWith = payTotal;
TRACE;
    }


    //new feature from biel : Change sale date time
    bool printticket=true;
    if (!ui_mainview.groupSaleDate->isHidden()) { //not hidden, change date.
      QDateTime datetime = ui_mainview.editTransactionDate->dateTime();
      currentTransaction.date   =  datetime.date();
      currentTransaction.time   =  datetime.time();
      ticket.datetime = datetime;
      if (!Settings::printChangedDateTicket()) printticket = false;
TRACE;
    } else  { // hidden, keep current date as sale date.
      currentTransaction.date   = QDate::currentDate();
      currentTransaction.time   = QTime::currentTime();
      ticket.datetime = QDateTime::currentDateTime();
TRACE;
    }
    
    currentTransaction.amount = totalSum;
DBX(totalSum);
DBX(currentTransaction.totalTax);
    currentTransaction.paywith= payWith;
    currentTransaction.changegiven =changeGiven;
    currentTransaction.paymethod = pType;
    currentTransaction.state = tCompleted;
    currentTransaction.cardnumber = cardNum;
    currentTransaction.cardauthnum= authnumber;
    currentTransaction.clientid = clientInfo.id;
    currentTransaction.disc = clientInfo.discount;
    currentTransaction.discmoney = discMoney; //global variable...
    currentTransaction.points = buyPoints; //global variable...
    currentTransaction.infoText = ticketMsg; //global variable...

    db.transaction();

    currentTransaction.id = myDb->insertTransaction(currentTransaction);
    if (currentTransaction.id <= 0) {
	KMessageBox::detailedError(this, i18n("Lemon has encountered an error when openning database, click details to see the error details.")
		, myDb->lastError(), i18n("Create New Transaction: Error"));
	db.rollback();
TRACE;
	return ;
	}
 
    QStringList productIDs; productIDs.clear();
    int cantidad=0;
    double utilidad=0;

    QHashIterator<qulonglong, ProductInfo> i(productsHash);
    int position=0;
    QList<TicketLineInfo> ticketLines;
    ticketLines.clear();
    TransactionItemInfo tItemInfo;
    
    // NOTE: utilidad (profit): Also take into account client discount! after this...
    
    currentTransaction.totalTax=0;

    currentTransaction.profit=0;

    while (i.hasNext()) {
      i.next();
      position++;
      productIDs.append(QString::number(i.key())+"/"+QString::number(i.value().qtyOnList));
      if (i.value().units == uPiece) cantidad += i.value().qtyOnList; else cantidad += 1; // :)
      utilidad += (i.value().price - i.value().cost - i.value().disc) * i.value().qtyOnList;
      //decrement stock qty, increment soldunits
      myDb->decrementProductStock(i.key(), i.value().qtyOnList, QDate::currentDate() );

      //qDebug()<<"Utilidad acumulada de la venta:"<<utilidad<<" |price:"<<i.value().price<<" cost:"<<i.value().cost<<" desc:"<<i.value().disc;

DBX(i.value().qtyOnList << i.value().price << i.value().cost << calcGrossTax(i.value().price,i.value().tax));
DBX(calcGrossTax(i.value().qtyOnList*i.value().price,i.value().tax));
      currentTransaction.totalTax+=calcGrossTax(i.value().qtyOnList*i.value().price,i.value().tax);
DBX(currentTransaction.totalTax);

      if(i.value().cost) {
	currentTransaction.profit+=i.value().qtyOnList*(i.value().price-i.value().cost-calcGrossTax(i.value().price,i.value().tax));
DBX(i.value().qtyOnList*(i.value().price-i.value().cost-calcGrossTax(i.value().price,i.value().tax)));
        }
DBX(currentTransaction.profit);
      //from Biel
      // save transactionItem
      tItemInfo.transactionid   = currentTransaction.id;
      tItemInfo.position        = position;
      tItemInfo.productCode     = i.key();
      tItemInfo.qty             = i.value().qtyOnList;
DBX(tItemInfo.qty);
      tItemInfo.points          = i.value().points; // qtyOnList; //MCH: changed...
      tItemInfo.unitStr         = i.value().unitStr;
DBX(tItemInfo.unitStr);
      tItemInfo.cost            = i.value().cost;
      tItemInfo.price           = i.value().price;
      tItemInfo.deliveryDateTime= QDateTime::currentDateTime(); // kh20.7.
      tItemInfo.disc            = i.value().disc;
      tItemInfo.total           = (i.value().price - i.value().disc) * i.value().qtyOnList;
      tItemInfo.name            = i.value().desc;
      tItemInfo.serialNo        = i.value().serialNo;
//      tItemInfo.payment         = i.value().payment;
      tItemInfo.payment         = i.value().price; // kh20.7.
// tItemInfo.completeP
// tItemInfo.soid
// tItemInfo.isGroup
// tItemInfo.deliveryDT

      tItemInfo.tax             = i.value().tax;
      myDb->insertTransactionItem(tItemInfo);
TRACE;

      //re-select the transactionItems model
      historyTicketsModel->select();

      // add line to ticketLines 
      TicketLineInfo tLineInfo;
      tLineInfo.qty     = i.value().qtyOnList;
      tLineInfo.unitStr = i.value().unitStr;
      tLineInfo.desc    = i.value().desc;
      tLineInfo.tax     = i.value().tax;
      tLineInfo.price   = i.value().price;
      tLineInfo.disc    = i.value().disc;
      tLineInfo.serialNo = i.value().serialNo;
      tLineInfo.total   = tItemInfo.total;
      ticketLines.append(tLineInfo);
DBX( tLineInfo.qty << tLineInfo.disc << tLineInfo.desc << tLineInfo.total << tLineInfo.tax);
    }
    currentTransaction.itemcount = cantidad;
    // taking into account the client discount. Applied over other products discount.
    // discMoney is the money discounted because of client discount.
DBX(utilidad << discMoney << currentTransaction.totalTax);
    currentTransaction.itemlist  = productIDs.join(",");

    //update transactions
    myDb->updateTransaction(currentTransaction);
TRACE;
    //increment client points
    if(currentTransaction.points) myDb->incrementClientPoints(currentTransaction.clientid, currentTransaction.points);

    if (drawerCreated) {
        //FIXME: What to di first?... add or substract?... when there is No money or there is less money than the needed for the change.. what to do?
        if (ui_mainview.checkCash->isChecked()) {
          drawer->addCash(payWith);
          drawer->substractCash(changeGiven);
          drawer->incCashTransactions();
          //open drawer only if there is a printer available.
          //NOTE: What about CUPS printers?
          if (Settings::openDrawer() && Settings::smallTicketDotMatrix() && Settings::printTicket())
            drawer->open();
        } else {
          drawer->incCardTransactions();
          drawer->addCard(payWith);
        }
        drawer->insertTransactionId(getCurrentTransactionId());
TRACE;
    }
    else {
      doNotify("dialog-information",i18n("The Drawer is not initialized, please start operation first."));
    }


    //update client info in the hash....
    clientInfo.points += buyPoints;
    clientsHash.remove(QString::number(clientInfo.id));
    clientsHash.insert(QString::number(clientInfo.id), clientInfo);
DBX(clientInfo.id);
DBX(clientInfo.name);
DBX(clientInfo.address);
DBX(clientInfo.phone);
    updateClientInfo();

    //Ticket
    ticket.number          = getCurrentTransactionId();
    ticket.total           = payTotal;
    ticket.change          = changeGiven;
    ticket.paidwith        = payWith;
    ticket.itemcount       = cantidad;
    ticket.cardnum         = cardNum;
    ticket.cardAuthNum     = authnumber;
    ticket.paidWithCard    = ui_mainview.checkCard->isChecked();
    ticket.clientDisc      = clientInfo.discount;
    ticket.clientDiscMoney = discMoney;
    ticket.buyPoints       = buyPoints;
    ticket.clientPoints    = clientInfo.points;
    ticket.lines           = ticketLines;
    ticket.clientid        = clientInfo.id;
    ticket.totalTax        = currentTransaction.totalTax;

    if (printticket) printTicket(ticket,currentTransaction.infoText);
    
    transactionInProgress = false;
    updateModelView();
    ui_mainview.editItemCode->setFocus();

    //Check level of cash in drawer
    if (drawer->getAvailableInCash() < Settings::cashMinLevel() && Settings::displayWarningOnLowCash()) {
      doNotify("dialog-warning",i18n("Cash level in drawer is low."));
    }
   }
   
   if (!ui_mainview.groupSaleDate->isHidden()) ui_mainview.groupSaleDate->hide(); //finally we hide the sale  

   updateBalance();
   db.commit();
   ticketMsg="";
}

void lemonView::printTicketSmallPrinterCUPS(TicketInfo ticket,QString tickettext)
{
// some code taken from Daniel O'Neill contribution.
  double tDisc = 0.0;
      PrintTicketInfo ptInfo;

      initPrintTicketInfo(&ptInfo,&ticket,tDisc);

TRACE;
      QPrinter printer;
      printer.setFullPage( true );
DBX(printer.printerName());
DBX(Settings::printTicket());
DBX(Settings::printShowDialog());
      if(!Settings::printShowDialog()) PrintCUPS::printSmallTicket(ptInfo, printer,tickettext);
      else {
	      QPrintDialog printDialog( &printer );
	      printDialog.setWindowTitle(i18n("Print Receipt"));
	      if ( printDialog.exec() ) {
	        PrintCUPS::printSmallTicket(ptInfo, printer,tickettext);
	      }
	}
TRACE;
    }

void lemonView::printTicketBigPrinterCUPS(TicketInfo ticket,QString tickettext)
    {
  double tDisc = 0.0;
      PrintTicketInfo ptInfo;

      initPrintTicketInfo(&ptInfo,&ticket,tDisc);

      QPrinter printer;
      printer.setFullPage( true );
      if(!Settings::printShowDialog()) PrintCUPS::printBigTicket(ptInfo, printer,tickettext);
      else {
  		QPrintDialog printDialog( &printer );
		printDialog.setWindowTitle(i18n("Print Receipt"));

		if ( printDialog.exec() ) {
			PrintCUPS::printBigTicket(ptInfo, printer,tickettext);
			}
	}
TRACE;
    }//bigTicket

void lemonView::printTicket(TicketInfo ticket,QString ticketText)
{

  //TRanslateable strings:
TRACE;
  QString salesperson    = i18n("Salesperson: %1", loggedUserName);
  QString hQty           = i18n("Qty");
  QString hClientDisc    = i18n("Your discount: %1", discMoney);
  QString hClientBuyPoints  = i18n("Your points this buy: %1", buyPoints);
  QString hClientPoints  = i18n("Your total points: %1", clientInfo.points);
  QString hTicket  = i18n("Ticket # %1", ticket.number);
  QString terminal = i18n("Terminal #%1", Settings::editTerminalNumber());
  QString hProduct       = i18n("Product");
  QString hPrice         = i18n("Price");
  QString hDisc          = i18n("Offer");
  QString hTotal         = i18n("Total");
  double tDisc = 0.0;

DBX(ticket.clientid);
  if(ticket.clientid > 1) {
    Azahar * myDb = new Azahar;
    myDb->setDatabase(db);
    ClientInfo clt=myDb->getClientInfo(ticket.clientid);
    if(clt.id>0) {
	ticket.clientStr=clt.fname + " " + clt.name + "\n"
		+ clt.address;
DBX(ticket.clientStr);
        	}
	}

  //HTML Ticket
  QStringList ticketHtml;
  //Ticket header
  ticketHtml.append(QString("<html><body><b>%1 - %2</b> [%3]<br>Ticket #%4 %5 %6<br>")
      .arg(Settings::editStoreName())
      .arg(Settings::storeAddress())
      .arg(Settings::storePhone())
      .arg(ticket.number)
      .arg(salesperson)
      .arg(terminal));
  //Ticket Table header
  ticketHtml.append(QString("<table border=0><tr><th>%1</th><th>%2</th><th>%3</th><th>%4</th><th>%5</th></tr>")
      .arg(hQty).arg(hProduct).arg(hPrice).arg(hDisc).arg(hTotal));
TRACE;
  //TEXT Ticket
  QStringList itemsForPrint;
  QString line;
  line = QString("%1, %2").arg(Settings::editStoreName()).arg(Settings::storeAddress()); //FIXME:Check Address lenght for ticket
  itemsForPrint.append(line);
  line = QString("%1").arg(terminal);
  itemsForPrint.append(line);
  line = QString("%1  %2").arg(hTicket).arg(salesperson);
  itemsForPrint.append(line);
  line = KGlobal::locale()->formatDateTime(ticket.datetime, KLocale::LongDate);
  itemsForPrint.append(line);
  itemsForPrint.append("          ");
  hQty.append("      ").truncate(6);
  hProduct.append("              ").truncate(14);
  hPrice.append("       ").truncate(7);
  hTotal.append("      ").truncate(6);
  //qDebug()<< "Strings:"<< hQty;qDebug()<< ", "<< hProduct<<", "<<hPrice<<", "<<hTotal;
  itemsForPrint.append(hQty +"  "+ hProduct +"  "+ hPrice+ "  "+ hTotal);
  itemsForPrint.append("------  --------------  -------  -------");

TRACE;
  QLocale localeForPrinting; // needed to convert double to a string better 
  for (int i = 0; i < ticket.lines.size(); ++i)
  {
    TicketLineInfo tLine = ticket.lines.at(i);
    QString  idesc =  tLine.desc;
    QString iprice =  localeForPrinting.toString(tLine.price,'f',2);
    QString iqty   =  localeForPrinting.toString(tLine.qty,'f',2);
DBX(iqty);
DBX(tLine.serialNo);
DBX(tLine.tax);
    iqty = iqty+" "+tLine.unitStr;
    QString idiscount =  localeForPrinting.toString(tLine.qty*tLine.disc,'f',2);
    bool hasDiscount = false;
    if (tLine.disc > 0) {
      hasDiscount = true;
      tDisc = tDisc + tLine.qty*tLine.disc;
TRACE;
    }
    QString idue =  localeForPrinting.toString(tLine.total,'f',2);

    
TRACE;
    //HTML Ticket
    ticketHtml.append(QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
        .arg(iqty).arg(idesc).arg(iprice).arg(idiscount).arg(idue));
    //TEXT TICKET
    //adjusting length
    if (idesc.length()>14) idesc.truncate(14); //idesc = idesc.insert(19, '\n');
    else {
      while (idesc.length()<14) idesc = idesc.insert(idesc.length(), ' ');
TRACE;
    }
    iqty.append("      ").truncate(6);
    while (iprice.length()<7) iprice = QString(" ").append(iprice);
    while (idue.length()<7) idue = QString(" ").append(idue);
    
//     while (iqty.length()<7) iqty = iqty.insert(iqty.length(), ' ');
//     while (idiscount.length()<4) idiscount = idiscount.insert(idiscount.length(), ' ');

    line = QString("%1  %2  %3  %4").
        arg(iqty).
        arg(idesc).
        arg(iprice).
        arg(idue);
    itemsForPrint.append(line);
    if (hasDiscount) itemsForPrint.append(QString("        * %1 *     -%2").arg(hDisc).arg(idiscount) );
TRACE;
  }//for each item

TRACE;

  //HTML Ticket
  QString harticles = i18np("%1 article.", "%1 articles.", ticket.itemcount);
  QString htotal    = i18n("A total of");
  ticketHtml.append(QString("</table><br><br><b>%1</b> %2 <b>%3</b>")
      .arg(harticles).arg(htotal).arg(KGlobal::locale()->formatMoney(ticket.total, QString(), 2)));
  ticketHtml.append(i18n("<br>Paid with %1, your change is <b>%2</b><br>",
                          KGlobal::locale()->formatMoney(ticket.paidwith, QString(), 2),
                          KGlobal::locale()->formatMoney(ticket.change, QString(), 2)));
  ticketHtml.append(ticketText);
  ticketHtml.append(Settings::editTicketMessage());
  //Text Ticket
  itemsForPrint.append("  ");
  line = QString("%1  %2 %3").arg(harticles).arg(htotal).arg(KGlobal::locale()->formatMoney(ticket.total, QString(), 2));
  itemsForPrint.append(line);
  if (tDisc > 0) {
    line = i18n("You saved %1", KGlobal::locale()->formatMoney(tDisc, QString(), 2));
    itemsForPrint.append(line);
TRACE;
  }
  if (clientInfo.discount>0) itemsForPrint.append(hClientDisc);
  if (buyPoints>0) itemsForPrint.append(hClientBuyPoints);
  if (clientInfo.points>0) itemsForPrint.append(hClientPoints);
  itemsForPrint.append(" ");
  line = i18n("Paid with %1, your change is %2",
              KGlobal::locale()->formatMoney(ticket.paidwith, QString(), 2), KGlobal::locale()->formatMoney(ticket.change, QString(), 2));
  itemsForPrint.append(line);
  itemsForPrint.append(" ");
  if (ticket.paidWithCard) {
    ticketHtml.append(i18n("<br>Card # %1<br>Authorisation # %2",ticket.cardnum, ticket.cardAuthNum));
    line = i18n("Card Number:%1 \nAuthorisation #:%2",ticket.cardnum,ticket.cardAuthNum);
    itemsForPrint.append(line);
    itemsForPrint.append(" ");
TRACE;
  }
  itemsForPrint.append(ticketText);
  line = QString(Settings::editTicketMessage());
  itemsForPrint.append(line);
  ticketHtml.append("</body></html>");

  //Printing...
  qDebug()<< itemsForPrint.join("\n");

TRACE;
  ///Real printing... [sendind data to print-methods]
  if (Settings::printTicket()) {
    if (Settings::smallTicketDotMatrix()) {
      QString printerFile=Settings::printerDevice();
      if (printerFile.length() == 0) printerFile="/dev/lp0";
      QString printerCodec=Settings::printerCodec();
      PrintDEV::printSmallTicket(printerFile, printerCodec, itemsForPrint.join("\n"));
TRACE;
    } //smallTicket
    else if (Settings::smallTicketCUPS() ) { // some code taken from Daniel O'Neill contribution.
      printTicketSmallPrinterCUPS(ticket,ticketText);
TRACE;
    } else { // bigticket
      qDebug()<<"Printing big receipt using CUPS";
      printTicketBigPrinterCUPS(ticket,ticketText);
TRACE;
      }
TRACE;
  } //printTicket

  freezeWidgets();

  if (Settings::showDialogOnPrinting())
  {
    TicketPopup *popup = new TicketPopup(this, ticketHtml.join(" "), DesktopIcon("lemon-printer", 48), Settings::ticketTime()*1000);
    connect (popup, SIGNAL(onTicketPopupClose()), this, SLOT(unfreezeWidgets()) );
    QApplication::beep();
    popup->popup();
TRACE;
  } else {
    QTimer::singleShot(Settings::ticketTime()*1000, this, SLOT(unfreezeWidgets())); //ticket time used to allow the cashier to see the change to give the user... is configurable.
TRACE;
  }
TRACE;
}



void lemonView::freezeWidgets()
{
  emit signalDisableUI();
}

void lemonView::unfreezeWidgets()
{
  emit signalEnableUI();
  startAgain();
}

void lemonView::startAgain()
{
  qDebug()<<"startAgain(): New Transaction";
  productsHash.clear();
  setupClients(); //clear the clientInfo (sets the default client info)
  clearUsedWidgets();
  buyPoints =0;
  discMoney=0;
  refreshTotalLabel();
  createNewTransaction(tSell);
TRACE;
}

void lemonView::cancelCurrentTransaction()
{
  bool continueIt = false;
  if ( !Settings::lowSecurityMode() ) {
    if (Settings::requiereDelAuth() ) {
      dlgPassword->show();
      dlgPassword->hide();
      dlgPassword->clearLines();
      if ( dlgPassword->exec() ) continueIt=true;
    } else continueIt=true; //if requiereDelAuth
  } else continueIt=true; //if no low security

  if (continueIt) cancelTransaction(getCurrentTransactionId());
}


void lemonView::preCancelCurrentTransaction()
{
TRACE;
  if (ui_mainview.tableWidget->rowCount()==0 ) { //empty transaction
    productsHash.clear();
    setupClients(); //clear the clientInfo (sets the default client info)
    clearUsedWidgets();
    buyPoints =0;
    discMoney=0;
    refreshTotalLabel();
    ///Next two lines were deleted to do not increment transactions number. reuse it.
    //if (Settings::deleteEmptyCancelledTransactions()) deleteCurrentTransaction();
    //else cancelCurrentTransaction();
  }
  else {
    cancelCurrentTransaction();
  }

  //if change sale date is in progress (but cancelled), hide it.
  if (!ui_mainview.groupSaleDate->isHidden()) {
    ui_mainview.groupSaleDate->hide();
  }
}

void lemonView::deleteCurrentTransaction()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
TRACE;
  if (myDb->deleteTransaction(getCurrentTransactionId())) {
    transactionInProgress=false;
    createNewTransaction(tSell);
TRACE;
  }
  else {
    KMessageBox::detailedError(this, i18n("Lemon has encountered an error when querying the database, click details to see the error details."), myDb->lastError(), i18n("Delete Transaction: Error"));
  }
}

//NOTE: This substracts points given to the client, restore stockqty, register a cashout for the money return. All if it applies.
void lemonView::cancelTransaction(qulonglong transactionNumber)
{
  bool enoughCashAvailable=false;
  bool transToCancelIsInProgress=false;
  bool transCompleted=false;
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  //get amount to return
  TransactionInfo tinfo = myDb->getTransactionInfo(transactionNumber);
  if (tinfo.state == tCancelled && tinfo.id >0) transCompleted = true;
  
  if (getCurrentTransactionId() == transactionNumber) {
    ///this transaction is not saved yet (more than the initial data when transaction is created)
    transToCancelIsInProgress = true;
    clearUsedWidgets();
    refreshTotalLabel();
  } else {
    ///this transaction is saved (amount,products,points...)
    clearUsedWidgets();
    refreshTotalLabel();
    if (drawer->getAvailableInCash() > tinfo.amount && tinfo.id>0){ // == or >= or > ?? to dont let empty the drawer
      enoughCashAvailable = true;
    }
  }
  
  //Mark as cancelled if possible

  //Check if payment was with cash.
  if (tinfo.paymethod != 1 ) {
    doNotify("dialog-error",i18n("The ticket cannot be cancelled because it was paid with a credit/debit card."));
    return;
  }
  
  if  (enoughCashAvailable || transToCancelIsInProgress) {
    if (myDb->cancelTransaction(transactionNumber, transToCancelIsInProgress)) {
      log(loggedUserId, QDate::currentDate(), QTime::currentTime(), QString("Cancelling transaction #%1. Authorized by %2").arg(transactionNumber).arg(dlgPassword->username()));
      qDebug()<<"Cancelling ticket was ok";
      if (transCompleted) {
        //if was completed, then return the money...
        if (tinfo.paymethod == 1 ) {//1 is cash
          drawer->substractCash(tinfo.amount);
          if (Settings::openDrawer() && Settings::smallTicketDotMatrix() && Settings::printTicket()) drawer->open();
        }
      }
      transactionInProgress = false; //reset
      createNewTransaction(tSell);
TRACE;
    }
    else { //myDB->cancelTransaction() returned some error...
      qDebug()<<"Not cancelled!";
      if (!transToCancelIsInProgress) {
        doNotify("dialog-error",i18n("Error cancelling ticket: %1",myDb->lastError()));
      } else {
        //Reuse the transaction instead of creating a new one.
        qDebug()<<"Transaction to cancel is in progress. Clearing all to reuse transaction number...";
        productsHash.clear();
        setupClients(); //clear the clientInfo (sets the default client info)
        clearUsedWidgets();
        buyPoints =0;
        discMoney=0;
        refreshTotalLabel();
      }
    }
  } else {
    //not cash available in drawer to return money to the client OR transaction id does not exists
    QString msg;
    if (tinfo.id > 0)
      doNotify("dialog-error",i18n("There is not enough cash available in the drawer."));
    else
      doNotify("dialog-error",i18n("Ticket to cancel does not exists!"));
  }
}


void lemonView::startOperation(const QString &adminUser)
{
  qDebug()<<"Starting operations...";
  bool ok=false;
  double qty=0.0;
  InputDialog *dlg = new InputDialog(this, false, dialogMoney, i18n("Enter the amount of money to deposit in the drawer"));
  dlg->setEnabled(true);
  if (dlg->exec() ) {
    qty = dlg->dValue;
    if (qty >= 0) ok = true; //allow no deposit...
  }
  
  if (ok) {
    if (!drawerCreated) {
      drawer = new Gaveta();
      drawer->setPrinterDevice(Settings::printerDevice());
      drawerCreated = true;
    }
    //NOTE: What about CUPS printers?
     if (Settings::openDrawer() && Settings::smallTicketDotMatrix() && Settings::printTicket()) drawer->open();
   // Set drawer amount.
    drawer->setStartDateTime(QDateTime::currentDateTime());
    drawer->setAvailableInCash(qty);
    drawer->setInitialAmount(qty);
    operationStarted = true;
    createNewTransaction(tSell);
    emit signalStartedOperation();
    log(loggedUserId, QDate::currentDate(), QTime::currentTime(), QString("Operation Started by %1 at terminal %2").
    arg(adminUser).
    arg(Settings::editTerminalNumber()));
  }
  else {
    operationStarted = false;
    emit signalEnableStartOperationAction();
    emit signalNoLoggedUser();
  }
}

//this method is for lemon.cpp's action connect for button, since button's trigger(bool) will cause to ask = trigger's var.
void lemonView::_slotDoStartOperation()
{
TRACE;
  //simply call the other...
  slotDoStartOperation(true);
  qDebug()<<"Called startOp from lemon.cpp (ui,)";
}

void lemonView::slotDoStartOperation(const bool &ask)
{
  //NOTE: For security reasons, we must ask for admin's password.
  //But, can we be more flexible -for one person store- and do not ask for password in low security mode
  // is ask is true we ask for auth, else we dont because it was previously asked for (calling method from corteDeCaja)
  
  qDebug()<<"doStartOperations..";
  if (!operationStarted) {
    bool doit = false;
    if (Settings::lowSecurityMode() || !ask) {
      doit = true;
    } else {
        do  {
          dlgPassword->show();
          dlgPassword->clearLines();
          dlgPassword->hide();
          doit = dlgPassword->exec();
        } while (!doit);
    }//else lowsecurity
    if (doit) startOperation(dlgPassword->username());
  }
}

/* REPORTS ZONE */
/*--------------*/

void lemonView::corteDeCaja()
{
  bool doit = false;
TRACE;
  //ASK for security if no lowSecurityMode.
  if (Settings::lowSecurityMode()) {
    doit = true;
  } else {
    dlgPassword->show();
    dlgPassword->clearLines();
    dlgPassword->hide();
    doit = dlgPassword->exec();
  }//else lowsecurity

  if (doit) {
    qDebug()<<"Doing Balance..";
    preCancelCurrentTransaction();
    QStringList lines;
    QStringList linesHTML;
    QString line;

    QString dId;
    QString dAmount;
    QString dHour;
    QString dMinute;
    QString dPaidWith;
    QString dPayMethod;

    PrintBalanceInfo pbInfo;

    pbInfo.thBalanceId = i18n("Balance Id:%1",closeBalance()) ; //CLOSE BALANCE and ASSIGN ID to pbInfo


    // Create lines to print and/or show on dialog...

    //----------Translated strings--------------------
    QString strTitle      = i18n("%1 at Terminal # %2", loggedUserName, Settings::editTerminalNumber());
    QString strInitAmount = i18n("Initial Amount deposited:");
    QString strInitAmountH= i18n("Deposit");
    QString strInH         = i18n("In");
    QString strOutH        = i18n("Out");
    QString strInDrawerH   = i18n("In Drawer");
    QString strTitlePre    = i18n("Drawer Balance");
    QString strTitleTrans  = i18n("Transactions Details");
    QString strTitleTransH = i18n("Transactions");
    QString strId          = i18n("Id");
    QString strTimeH       = i18n("Time");
    QString strAmount      = i18n("Amount");
    QString strPaidWith    = i18n("Paid");
    QString strPayMethodH =  i18n("Method");

    QPixmap logoPixmap;
    logoPixmap.load(Settings::storeLogo());

    pbInfo.sd.storeName = Settings::editStoreName();
    pbInfo.sd.storeAddr = Settings::storeAddress();
    pbInfo.sd.storeLogo = logoPixmap;
    pbInfo.thTitle     = strTitle;
    pbInfo.thDeposit   = strInitAmountH;
    pbInfo.thIn        = strInH;
    pbInfo.thOut       = strOutH;
    pbInfo.thInDrawer  = strInDrawerH;
    pbInfo.thTitleDetails = strTitleTrans;
    pbInfo.thTrId      = strId;
    pbInfo.thTrTime    = strTimeH;
    pbInfo.thTrAmount  = strAmount;
    pbInfo.thTrPaidW    = strPaidWith;
    pbInfo.thTrPayMethod=strPayMethodH;
    pbInfo.startDate   = i18n("Start: %1",KGlobal::locale()->formatDateTime(drawer->getStartDateTime(), KLocale::LongDate));
    pbInfo.endDate     = i18n("End  : %1",KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(), KLocale::LongDate));
    //Qty's
    pbInfo.initAmount = KGlobal::locale()->formatMoney(drawer->getInitialAmount(), QString(), 2);
    pbInfo.inAmount   = KGlobal::locale()->formatMoney(drawer->getInAmount(), QString(), 2);
    pbInfo.outAmount  = KGlobal::locale()->formatMoney(drawer->getOutAmount(), QString(), 2);
    pbInfo.cashAvailable=KGlobal::locale()->formatMoney(drawer->getAvailableInCash(), QString(), 2);
    pbInfo.sd.logoOnTop = Settings::chLogoOnTop();
    pbInfo.thTitleCFDetails = i18n("Cash flow Details");
    pbInfo.thCFType    = i18n("Type");
    pbInfo.thCFReason  = i18n("Reason");
    pbInfo.thCFDate    = i18n("Time");


  //TODO: Hacer el dialogo de balance mejor, con un look uniforme a los demas dialogos.
  //       Incluso insertar imagenes en el html del dialogo.

    //HTML
    line = QString("<html><body><h3>%1</h3>").arg(strTitle);
    linesHTML.append(line);
    line = QString("<center><table border=1 cellpadding=5><tr><th colspan=4>%9</th></tr><tr><th>%1</th><th>%2</th><th>%3</th><th>%4</th></tr><tr><td>%5</td><td>%6</td><td>%7</td><td>%8</td></tr></table></ceter><br>")
        .arg(strInitAmountH)
        .arg(strInH)
        .arg(strOutH)
        .arg(strInDrawerH)
        .arg(KGlobal::locale()->formatMoney(drawer->getInitialAmount(), QString(), 2))
        .arg(KGlobal::locale()->formatMoney(drawer->getInAmount(), QString(), 2))
        .arg(KGlobal::locale()->formatMoney(drawer->getOutAmount(), QString(), 2))
        .arg(KGlobal::locale()->formatMoney(drawer->getAvailableInCash(), QString(), 2))
        .arg(strTitlePre);
    linesHTML.append(line);
    line = QString("<table border=1 cellpadding=5><tr><th colspan=5>%1</th></tr><tr><th>%2</th><th>%3</th><th>%4</th><th>%5</th><th>%6</th></tr>")
        .arg(strTitleTransH)
        .arg(strId)
        .arg(strTimeH)
        .arg(strAmount)
        .arg(strPaidWith)
        .arg(strPayMethodH);
    linesHTML.append(line);

    //TXT
    lines.append(strTitle);
    line = QString(KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(), KLocale::LongDate));
    lines.append(line);
    lines.append("----------------------------------------");
    line = QString("%1 %2").arg(strInitAmount).arg(KGlobal::locale()->formatMoney(drawer->getInitialAmount(), QString(), 2));
    lines.append(line);
    line = QString("%1 :%2, %3 :%4")
        .arg(strInH)
        .arg(KGlobal::locale()->formatMoney(drawer->getInAmount(), QString(), 2))
        .arg(strOutH)
        .arg(KGlobal::locale()->formatMoney(drawer->getOutAmount(), QString(), 2));
    lines.append(line);
    line = QString(" %1 %2").arg(KGlobal::locale()->formatMoney(drawer->getAvailableInCash(), QString(), 2)).arg(strInDrawerH);
    lines.append(line);
    //Now, add a transactions report per user and for today.
    //At this point, drawer must be initialized and valid.
    line = QString("----------%1----------").arg(strTitleTrans);
    lines.append(line);
    line = QString("%1           %2      %3").arg(strId).arg(strAmount).arg(strPaidWith);
    lines.append(line);
    lines.append("----------  ----------  ----------");
    QList<qulonglong> transactionsByUser = drawer->getTransactionIds();
    QStringList trList;
    //This gets all transactions ids done since last corteDeCaja.
    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    for (int i = 0; i < transactionsByUser.size(); ++i) {
      qulonglong idNum = transactionsByUser.at(i);
      TransactionInfo info;
      info = myDb->getTransactionInfo(idNum);

      dId       = QString::number(info.id);
      dAmount   = QString::number(info.amount);
DBX(info.amount);
      dHour     = info.time.toString("hh");
      dMinute   = info.time.toString("mm");
      dPaidWith = QString::number(info.paywith);

      QString tmp = QString("%1|%2|%3|%4")
        .arg(dId)
        .arg(dHour+":"+dMinute)
        .arg(KGlobal::locale()->formatMoney(info.amount, QString(), 2))
        .arg(KGlobal::locale()->formatMoney(info.paywith, QString(), 2));

      while (dId.length()<10) dId = dId.insert(dId.length(), ' ');
      while (dAmount.length()<14) dAmount = dAmount.insert(dAmount.length(), ' ');
      while ((dHour+dMinute).length()<6) dMinute = dMinute.insert(dMinute.length(), ' ');
      while (dPaidWith.length()<10) dPaidWith = dPaidWith.insert(dPaidWith.length(), ' ');

      //if (info.paymethod == pCash) dPayMethod = i18n("Cash");/*dPaidWith;*/
      dPayMethod = myDb->getPayTypeStr(info.paymethod);//using payType methods
      //else if (info.paymethod == pCard) dPayMethod = i18n("Card");  else dPayMethod = i18n("Unknown");
      line = QString("%1 %2 %3")
        .arg(dId)
        //.arg(dHour)
        //.arg(dMinute)
        .arg(dAmount)
        //.arg(dPaidWith);
        .arg(dPayMethod);
      lines.append(line);
      line = QString("<tr><td>%1</td><td>%2:%3</td><td>%4</td><td>%5</td><td>%6</td></tr>")
        .arg(dId)
        .arg(dHour)
        .arg(dMinute)
        .arg(dAmount)
        .arg(dPaidWith)
        .arg(dPayMethod);
      linesHTML.append(line);
      tmp += "|"+dPayMethod;
      trList.append( tmp );
    }
    pbInfo.trList = trList;

    //get CashOut list and its info...
    QStringList cfList;
    cfList.clear();
    QList<CashFlowInfo> cashflowInfoList = myDb->getCashFlowInfoList( drawer->getCashflowIds() );
    foreach(CashFlowInfo cfInfo, cashflowInfoList) {
        QString amountF = KGlobal::locale()->formatMoney(cfInfo.amount);
DBX(cfInfo.amount);
        //QDateTime dateTime; dateTime.setDate(cfInfo.date); dateTime.setTime(cfInfo.time);
        QString dateF   = KGlobal::locale()->formatTime(cfInfo.time);
        QString data = QString::number(cfInfo.id) + "|" + cfInfo.typeStr + "|" + cfInfo.reason + "|" + amountF + "|" + dateF;
        cfList.append(data);
    }
    pbInfo.cfList = cfList;


    line = QString("</table></body></html>");
    linesHTML.append(line);
    operationStarted = false;

    if (Settings::smallTicketDotMatrix()) {
      //print it on the /dev/lpXX...   send lines to print
      showBalance(linesHTML);
      if (Settings::printBalances()) printBalance(lines);
    } else if (Settings::printBalances()) {
      //print it on cups... send pbInfo instead
      QPrinter printer;
      printer.setFullPage( true );
      if(!Settings::printShowDialog()) PrintCUPS::printSmallBalance(pbInfo, printer);
      else {

	      QPrintDialog printDialog( &printer );
	      printDialog.setWindowTitle(i18n("Print Balance"));
	      if ( printDialog.exec() ) {
		PrintCUPS::printSmallBalance(pbInfo, printer);
	      }
	}
    }
    slotDoStartOperation(false);
  }//if doit
}

void lemonView::endOfDay() {
  bool doit = false;
  //ASK for security if no lowSecurityMode.
  if (Settings::lowSecurityMode()) {
    doit = true;
  } else {
    dlgPassword->show();
    dlgPassword->clearLines();
    dlgPassword->hide();
    doit = dlgPassword->exec();
  }//else lowsecurity

  if (doit) {
    log(loggedUserId, QDate::currentDate(), QTime::currentTime(), QString("End of Day report printed by %1 at terminal %2 on %3").
    arg(dlgPassword->username()).
    arg(Settings::editTerminalNumber()).
    arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));

    // Get every transaction from all day, calculate sales, profit, and profit margin (%). From the same terminal
    AmountAndProfitInfo amountProfit;
    PrintEndOfDayInfo pdInfo;
    QList<TransactionInfo> transactionsList;
    QPixmap logoPixmap;
    logoPixmap.load(Settings::storeLogo());

    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    amountProfit     = myDb->getDaySalesAndProfit(Settings::editTerminalNumber());
    transactionsList = myDb->getDayTransactions(Settings::editTerminalNumber());

    pdInfo.sd.storeName = Settings::editStoreName();
    pdInfo.sd.storeAddr = Settings::storeAddress();
    pdInfo.sd.storeLogo = logoPixmap;
    pdInfo.thTitle   = i18n("End of day report");
    pdInfo.thTicket  = i18n("Id");
    pdInfo.salesPerson = loggedUserName;
    pdInfo.terminal  = i18n("at terminal # %1",Settings::editTerminalNumber());
    pdInfo.thDate    = KGlobal::locale()->formatDateTime(QDateTime::currentDateTime(), KLocale::LongDate);
    pdInfo.thTime    = i18n("Time");
    pdInfo.thAmount  = i18n("Amount");
    pdInfo.thProfit  = i18n("Profit");
    pdInfo.thPayMethod = i18n("Method");
    pdInfo.sd.logoOnTop = Settings::chLogoOnTop();
    pdInfo.thTotalSales  = KGlobal::locale()->formatMoney(amountProfit.amount, QString(), 2);
    pdInfo.thTotalProfit = KGlobal::locale()->formatMoney(amountProfit.profit, QString(), 2);
DBX(amountProfit.amount);

    //each transaction...
    for (int i = 0; i < transactionsList.size(); ++i)
    {
      QLocale localeForPrinting; // needed to convert double to a string better
      TransactionInfo info = transactionsList.at(i);
      QString tid      = QString::number(info.id);
      QString hour     = info.time.toString("hh:mm");
      QString amount   =  localeForPrinting.toString(info.amount,'f',2);
DBX(info.amount);
      QString profit   =  localeForPrinting.toString(info.profit, 'f', 2);
      QString payMethod;
      payMethod        = myDb->getPayTypeStr(info.paymethod);//using payType methods

      QString line     = tid +"|"+ hour +"|"+ amount +"|"+ profit +"|"+ payMethod;
      pdInfo.trLines.append(line);
    } //for each item


    if (Settings::smallTicketDotMatrix()) {
      QString printerFile=Settings::printerDevice();
      if (printerFile.length() == 0) printerFile="/dev/lp0";
      QString printerCodec=Settings::printerCodec();
      qDebug()<<"[Printing report on "<<printerFile<<"]";
      ///TODO: Still missing the code to prepare the lines!
      //PrintDEV::printSmallBalance(printerFile, printerCodec, lines.join("\n"));
    } else if (Settings::smallTicketCUPS()) {
      qDebug()<<"[Printing report on CUPS small size]";
      QPrinter printer;
      printer.setFullPage( true );
      if(!Settings::printShowDialog()) PrintCUPS::printSmallEndOfDay(pdInfo, printer);
      else {
	      QPrintDialog printDialog( &printer );
	      printDialog.setWindowTitle(i18n("Print end of day report"));
	      if ( printDialog.exec() ) {
		PrintCUPS::printSmallEndOfDay(pdInfo, printer);
	      }
      }
    } else { //big printer
      qDebug()<<"[Printing report on CUPS big size]";
      QPrinter printer;
      printer.setFullPage( true );
      if(!Settings::printShowDialog()) PrintCUPS::printBigEndOfDay(pdInfo, printer);
      else {
	      QPrintDialog printDialog( &printer );
	      printDialog.setWindowTitle(i18n("Print end of day report"));
	      if ( printDialog.exec() ) {
		PrintCUPS::printBigEndOfDay(pdInfo, printer);
	      }
      }
    }
  }
}
qulonglong lemonView::createNewBalance()
{
TRACE;
  qulonglong result = 0;
  currentBalance.id = 0;
  currentBalance.done=false;
  currentBalance.dateTimeStart = drawer->getStartDateTime();
  currentBalance.dateTimeEnd   = QDateTime::currentDateTime();
  currentBalance.userid     = loggedUserId;
  currentBalance.username   =  loggedUserName;
  currentBalance.initamount = drawer->getInitialAmount();
  currentBalance.in         = drawer->getInAmount();
  currentBalance.out        = drawer->getOutAmount();
  currentBalance.cash       = drawer->getAvailableInCash();
  currentBalance.card       = drawer->getAvailableInCard();
  currentBalance.terminal   = Settings::editTerminalNumber();
  
  currentBalance.transactions="EMPTY";
  
  //Save balance on Database
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  result = myDb->insertBalance(currentBalance);

  if(result>0) currentBalance.id=result;
  else currentBalance.id=0;
  return result;
}

qulonglong lemonView::closeBalance()
{
  if(currentBalance.id<1) return 0;

  currentBalance.done=true;
  //Save balance on Database
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  qulonglong result=myDb->updateBalance(currentBalance);

  currentBalance.id=0;
  currentBalance.done=false;

  return result;
  }

qulonglong lemonView::updateBalance()
{
  qulonglong result = 0;

  if(currentBalance.id<1) return 0;

  
  QStringList transactionList;
  QList<qulonglong> intList = drawer->getTransactionIds();
  if (intList.isEmpty()) transactionList.append("EMPTY");
  else {
    for (int i = 0; i < intList.size(); ++i) {
      transactionList.append( QString::number(intList.at(i)) );
    }
  }
  currentBalance.transactions=transactionList.join(",");
  currentBalance.dateTimeEnd   = QDateTime::currentDateTime();
  currentBalance.in         = drawer->getInAmount();
  currentBalance.out        = drawer->getOutAmount();
  currentBalance.cash       = drawer->getAvailableInCash();
  currentBalance.card       = drawer->getAvailableInCard();
  
  //Save balance on Database
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  result = myDb->updateBalance(currentBalance);
  return result;
}

void lemonView::showBalance(QStringList lines)
{
  if (Settings::showDialogOnPrinting())
  {
    BalanceDialog *popup = new BalanceDialog(this, lines.join("\n"));
    popup->show();
    popup->hide();
    int result = popup->exec();
    if (result) {
      //qDebug()<<"exec=true";
    }
TRACE;
  }
TRACE;
}

void lemonView::printBalance(QStringList lines)
{
  //Balances are print on small tickets. Getting the selected printed from config.
  if (Settings::printBalances()) {
    if (Settings::smallTicketDotMatrix()) {
      QString printerFile=Settings::printerDevice();
      if (printerFile.length() == 0) printerFile="/dev/lp0";
      QString printerCodec=Settings::printerCodec();
      qDebug()<<"[Printing balance on "<<printerFile<<"]";
      PrintDEV::printSmallBalance(printerFile, printerCodec, lines.join("\n"));
TRACE;
    } // DOT-MATRIX PRINTER on /dev/lpX
TRACE;
  }
TRACE;
}


/* MODEL Zone */

void lemonView::setupModel()
{
  if (!db.isOpen()) {
    connectToDb();
  }
  else {
    //workaround for a stupid crash: when re-connecting after Config, on setTable crashes.
    //Crashes without debug information.
    if (productsModel->tableName() != "products")
      productsModel->setTable("products");
    productsModel->setEditStrategy(QSqlTableModel::OnRowChange);
    ui_mainview.listView->setModel(productsModel);
    ui_mainview.listView->setResizeMode(QListView::Adjust);

    ui_mainview.listView->setModelColumn(productsModel->fieldIndex("photo"));
    ui_mainview.listView->setViewMode(QListView::IconMode);
    ui_mainview.listView->setGridSize(QSize(170,170));
    ui_mainview.listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui_mainview.listView->setMouseTracking(true); //for the tooltip

    ProductDelegate *delegate = new ProductDelegate(ui_mainview.listView);
    ui_mainview.listView->setItemDelegate(delegate);

    productsModel->select();

    ///TODO: -MCH- Remove the filter by Biel?
    //BFB. Added QCompleter to editFilterByDesc
    productsFilterModel->setQuery("");
    QCompleter *completer = new QCompleter(this);
    completer->setModel(productsFilterModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive); 
    //Show all possible results, because completer only works with prefix. The filter is done modifying the model
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    ui_mainview.editFilterByDesc->setCompleter(completer);

    //Categories popuplist
    populateCategoriesHash();
    QHashIterator<QString, int> item(categoriesHash);
    while (item.hasNext()) {
      item.next();
      ui_mainview.comboFilterByCategory->addItem(item.key());
      //qDebug()<<"iterando por el hash en el item:"<<item.key()<<"/"<<item.value();
    }
//kh 2012-07-15
    QAbstractItemModel *model =ui_mainview.comboFilterByCategory->model();
    model->sort(0);

    ui_mainview.comboFilterByCategory->setCurrentIndex(0);
    connect(ui_mainview.comboFilterByCategory,SIGNAL(currentIndexChanged(int)), this, SLOT( setFilter()) );
    connect(ui_mainview.editFilterByDesc,SIGNAL(returnPressed()), this, SLOT( setFilter()) );
    connect(ui_mainview.editFilterByDesc,SIGNAL(textEdited(const QString)), this, SLOT( modifyProductsFilterModel()) );
    connect(ui_mainview.rbFilterByDesc, SIGNAL(toggled(bool)), this, SLOT( setFilter()) );
    connect(ui_mainview.rbFilterByCategory, SIGNAL(toggled(bool)), this, SLOT( setFilter()) );

//kh 2012-07-15    ui_mainview.rbFilterByCategory->setChecked(true);
    ui_mainview.rbFilterByDesc->setChecked(true);
    setFilter();
  }
 }

void lemonView::populateCategoriesHash()
{
  Azahar * myDb = new Azahar;
  myDb->setDatabase(db);
  categoriesHash = myDb->getCategoriesHash();
}

void lemonView::listViewOnMouseMove(const QModelIndex & index)
{
  //NOTE: Problem: here the data on the view does not change. This is because we do not
  //      update this view's data, we modify directly the data at database until we sell a product.
  //      and until that moment we can update this view.
  // UPDATE: We have at productsHash the property qtyOnList, we can use such qty to display available qty.
  //      But if we are working on a network (multiple POS). It will not be true the information.
  QString tprice = i18n("Price: ");
  QString tstock = i18n("Available: ");
  QString tdisc  = i18n("Discount:"); //TODO: Only include if valid until now...
  QString tcategory = i18n("Category:");
  QString tmoreAv = i18n("in stock");
  QString tmoreAv2= i18n("in your shopping cart, Total Available");

  //getting data from model...
  const QAbstractItemModel *model = index.model();
  int row = index.row();
  QModelIndex indx = model->index(row, 1);
  QString desc = model->data(indx, Qt::DisplayRole).toString();
  indx = model->index(row, 2);
  double price = model->data(indx, Qt::DisplayRole).toDouble();
  indx = model->index(row, 4);
  double stockqty = model->data(indx, Qt::DisplayRole).toDouble();
  indx = model->index(row, 0);
  QString code = model->data(indx, Qt::DisplayRole).toString();
  ProductInfo pInfo;
  bool onList=false;
  if (productsHash.contains(code.toULongLong())) {
    pInfo = productsHash.value(code.toULongLong());
    onList = true;
  }

  QString line1 = QString("<p><b><i>%1</i></b><br>").arg(desc);
  QString line2 = QString("<b>%1</b>%2<br>").arg(tprice).arg(KGlobal::locale()->formatMoney(price));
  QString line3;
  if (onList) line3 = QString("<b>%1</b> %2 %5 %6, %3 %7: %4<br></p>").arg(tstock).arg(stockqty).arg(pInfo.qtyOnList).arg(stockqty - pInfo.qtyOnList).arg(pInfo.unitStr).arg(tmoreAv).arg(tmoreAv2);
  else line3 = QString("<b>%1</b> %2 %3 %4<br></p>").arg(tstock).arg(stockqty).arg(pInfo.unitStr).arg(tmoreAv);
  QString text = line1+line2+line3;

  ui_mainview.listView->setToolTip(text);
}

void lemonView::listViewOnClick(const QModelIndex & index)
{
  //getting data from model...
  const QAbstractItemModel *model = index.model();
  int row = index.row();
  QModelIndex indx = model->index(row, 0);
  QString code = model->data(indx, Qt::DisplayRole).toString();
  insertItem(code);
}

//This is done at the end of each transaction...
void lemonView::updateModelView()
{
  //Submit and select causes a flick and costs some milliseconds
  productsModel->submitAll();
  productsModel->select();
}

void lemonView::showProductsGrid(bool show)
{
  if (show) {
    ui_mainview.frameGridView->show();
  }
  else {
    ui_mainview.frameGridView->hide();
  }
}

void lemonView::hideProductsGrid()
{
  ui_mainview.frameGridView->hide();
}

void lemonView::showPriceChecker()
{
  PriceChecker *priceDlg = new PriceChecker(this);
  priceDlg->setDb(db);
  priceDlg->show();
}

void lemonView::setFilter()
{
TRACE;
  //NOTE: This is a QT BUG.
  //   If filter by description is selected and the text is empty, and later is re-filtered
  //   then NO pictures are shown; even if is refiltered again.
  QRegExp regexp = QRegExp(ui_mainview.editFilterByDesc->text());
  
  if (ui_mainview.rbFilterByDesc->isChecked()) {//by description
    if (!regexp.isValid() )  ui_mainview.editFilterByDesc->setText("");
    if (ui_mainview.editFilterByDesc->text()=="*" || ui_mainview.editFilterByDesc->text()=="") productsModel->setFilter("");
    else  {
	productsModel->setFilter(QString("products.name REGEXP '%1'").arg(ui_mainview.editFilterByDesc->text().split("(").at(0).trimmed()));
	}
    // BFB: If the user choose a product from the completer, this product is added to the list.
    modifyProductsFilterModel();
    if (productsFilterModel->rowCount() == 1){
      if (ui_mainview.editFilterByDesc->text() == productsFilterModel->data(productsFilterModel->index(0,0)).toString()){
              qulonglong idProduct = productsFilterModel->data(productsFilterModel->index(0, 1)).toULongLong();
        insertItem(QString::number(idProduct));
        ui_mainview.editFilterByDesc->selectAll();
      }
    } else {
	productsModel->setFilter(QString("(products.name REGEXP '%1*' OR products.code  REGEXP '%1*' OR  products.alphacode REGEXP '%1*')").arg(ui_mainview.editFilterByDesc->text().split("(").at(0).trimmed()));
	}
  }
  else {
    if (ui_mainview.rbFilterByCategory->isChecked()) {//by category
      //Find catId for the text on the combobox.
      int catId=-1;
      QString catText = ui_mainview.comboFilterByCategory->currentText();
      if (categoriesHash.contains(catText)) {
        catId = categoriesHash.value(catText);
      }
      productsModel->setFilter(QString("products.category=%1").arg(catId));
    }else{//by most sold products in current month --biel
      productsModel->setFilter("products.code IN (SELECT * FROM (SELECT product_id FROM (SELECT product_id, sum( units ) AS sold_items FROM transactions t, transactionitems ti WHERE t.id = ti.transaction_id AND t.date > ADDDATE( sysdate( ) , INTERVAL -31 DAY ) GROUP BY ti.product_id) month_sold_items ORDER BY sold_items DESC LIMIT 5) popular_products)");
    }
  }
  productsModel->select();
}

void lemonView::modifyProductsFilterModel()
{    
    if (ui_mainview.editFilterByDesc->text().length() > 2){
        QString sql;
        QString productName=ui_mainview.editFilterByDesc->text().split("(").at(0).trimmed();
        if (KGlobal::locale()->positivePrefixCurrencySymbol())
          sql = QString("select concat(name,  ' (%1 ' ,price , ')' ) as nameprice, code, name from products where (products.name REGEXP '%2*' OR products.code  REGEXP '%2*' OR  products.alphacode REGEXP '%2*')").arg(KGlobal::locale()->currencySymbol()).arg(productName);
        else
          sql = QString("select concat(name,  ' (' ,price , ' %1)' ) as nameprice, code, name from products where products.name (products.name REGEXP '%2*' OR products.code  REGEXP '%2*' OR  products.alphacode REGEXP '%2*')").arg(KGlobal::locale()->currencySymbol()).arg(productName);
        productsFilterModel->setQuery(sql);
    }else{
        productsFilterModel->setQuery("");
    }

}

void lemonView::setupDB()
{
  qDebug()<<"Setting up database...";
  if (db.isOpen()) db.close();
  db.setHostName(Settings::editDBServer());
  db.setDatabaseName(Settings::editDBName());
  db.setUserName(Settings::editDBUsername());
  db.setPassword(Settings::editDBPassword());
  connectToDb();
}



void lemonView::syncSettingsOnDb()
{
  //save new settings on db -to avoid double settings on lemon and squeeze-
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  if (!Settings::storeLogo().isEmpty()) {
    QPixmap p = QPixmap( Settings::storeLogo() );
//kh13.08.    myDb->setConfigStoreLogo(Misc::pixmap2ByteArray(new QPixmap(p), p.size().width(),p.size().height()));
  }
  myDb->setConfigStoreName(Settings::editStoreName());
  myDb->setConfigStoreAddress(Settings::storeAddress());
  myDb->setConfigStorePhone(Settings::storePhone());
  myDb->setConfigSmallPrint(!Settings::bigReceipt());
  myDb->setConfigUseCUPS(!Settings::smallTicketDotMatrix());
  myDb->setConfigLogoOnTop(Settings::chLogoOnTop());
//kh13.08.  myDb->setConfigTaxIsIncludedInPrice(!Settings::addTax());//NOTE: the AddTax means the tax is NOT included in price, thats why this is negated.
  
  delete myDb;
}


void lemonView::connectToDb()
{
  if (!db.open()) {
    db.open(); //try to open connection
    qDebug()<<"(1/connectToDb) Trying to open connection to database..";
  }
  if (!db.isOpen()) {
    db.open(); //try to open connection again...
    qDebug()<<"(2/connectToDb) Trying to open connection to database..";
  }
  if (!db.isOpen()) {
    qDebug()<<"(3/connectToDb) Configuring..";
    emit signalShowDbConfig();
  } else {
    //finally, when connection stablished, setup all models.
    if (!modelsCreated) { //Create models...
      productsModel       = new QSqlTableModel();
      historyTicketsModel = new QSqlRelationalTableModel();
      //BFB. New combo for comboFilterByDesc
      productsFilterModel = new QSqlQueryModel();
      modelsCreated = true;
    }
    setupModel();
    setupHistoryTicketsModel();
    setupClients();
    //pass db to login/pass dialogs
    dlgLogin->setDb(db);
    dlgPassword->setDb(db);

    //checking if is the first run.
    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    if (myDb->getConfigFirstRun())  syncSettingsOnDb();
    delete myDb;

  }
}

void lemonView::setupClients()
{
TRACE;
  qDebug()<<"Setting up clients...";
  ClientInfo info;
  QString clientname
	,mainClient;
  clientsHash.clear();
  ui_mainview.comboClients->clear();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  clientsHash = myDb->getClientsHash();
  mainClient  = myDb->getMainClient();

    //Set by default the 'general' client.
    QHashIterator<QString, ClientInfo> i(clientsHash);
    while (i.hasNext()) {
      i.next();
      info = i.value();

//kh27.7.      ui_mainview.comboClients->addItem(info.name);

//added kh27.7.
   
    clientname="";
    if(!info.fname.isEmpty() && !info.name.isEmpty()) clientname = QString("%1,%2").arg(info.name).arg(info.fname);
    else if(!info.name.isEmpty()) clientname = info.name;
    else if(!info.fname.isEmpty()) clientname = info.fname;

    if(!clientname.isEmpty()) ui_mainview.comboClients->addItem(clientname);
//end added kh27.7.
    }
    
//kh 2012-07-17
    QAbstractItemModel *model =ui_mainview.comboClients->model();
    model->sort(0);

    int idx = ui_mainview.comboClients->findText(mainClient,Qt::MatchCaseSensitive);
    if (idx>-1) ui_mainview.comboClients->setCurrentIndex(idx);
DBX(clientInfo.name);
    clientInfo = clientsHash.value(mainClient);
DBX(clientInfo.id);
DBX(clientInfo.name);
DBX(clientInfo.address);
DBX(clientInfo.phone);
    updateClientInfo();
}

void lemonView::comboClientsOnChange()
{
  QString newClientName    = ui_mainview.comboClients->currentText();
DBX(newClientName);
  if (clientsHash.contains(newClientName)) {
    clientInfo = clientsHash.value(newClientName);
    updateClientInfo();
    refreshTotalLabel();
DBX(clientInfo.id);
DBX(clientInfo.name);
DBX(clientInfo.id);
DBX(clientInfo.name);
DBX(clientInfo.address);
DBX(clientInfo.phone);
    ui_mainview.editItemCode->setFocus();
  }
}

void lemonView::updateClientInfo()
{
  Azahar * myDb = new Azahar;
  myDb->setDatabase(db);
  ClientInfo clt=myDb->getClientInfo(clientInfo.id);
  if(clt.id>0) {
	clientInfo=clt;
	}
TRACE;
DBX(clientInfo.fname << clientInfo.name );
  QString pStr = i18n("Points: %1", clientInfo.points);
  QString dStr = i18n("Discount: %1%",clientInfo.discount);
  ui_mainview.lblClientDiscount->setText(dStr);
  //ui_mainview.lblClientPoints->setText(pStr);
  QPixmap pix;
  pix.loadFromData(clientInfo.photo);
  ui_mainview.lblClientPhoto->setPixmap(pix);
}

void lemonView::setHistoryFilter() {
  historyTicketsModel->setFilter(QString("date <= STR_TO_DATE('%1', '%d/%m/%Y')").
    arg(ui_mainview.editTicketDatePicker->date().toString("dd/MM/yyyy")));
  historyTicketsModel->setSort(historyTicketsModel->fieldIndex("id"),Qt::DescendingOrder); //change this when implemented headers click
}

void lemonView::setupHistoryTicketsModel()
{
  //qDebug()<<"Db name:"<<db.databaseName()<<", Tables:"<<db.tables();
  if (historyTicketsModel->tableName().isEmpty()) {
    if (!db.isOpen()) db.open();
    historyTicketsModel->setTable("v_transactions");
    historyTicketsModel->setRelation(historyTicketsModel->fieldIndex("clientid"), QSqlRelation("clients", "id", "name"));
    historyTicketsModel->setRelation(historyTicketsModel->fieldIndex("userid"), QSqlRelation("users", "id", "username"));
    
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("id"), Qt::Horizontal, i18n("Tr"));
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("clientid"), Qt::Horizontal, i18n("Client"));
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("datetime"), Qt::Horizontal, i18n("Date"));
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("userid"), Qt::Horizontal, i18n("User"));
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("itemcount"), Qt::Horizontal, i18n("Items"));
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("amount"), Qt::Horizontal, i18n("Total"));
    historyTicketsModel->setHeaderData(historyTicketsModel->fieldIndex("disc"), Qt::Horizontal, i18n("Discount"));

    ui_mainview.ticketView->setModel(historyTicketsModel);
    ui_mainview.ticketView->setColumnHidden(historyTicketsModel->fieldIndex("date"), true);
    ui_mainview.ticketView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui_mainview.ticketView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui_mainview.ticketView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui_mainview.ticketView->resizeColumnsToContents();
    ui_mainview.ticketView->setCurrentIndex(historyTicketsModel->index(0, 0));
    
    historyTicketsModel->setSort(historyTicketsModel->fieldIndex("id"),Qt::DescendingOrder);
    historyTicketsModel->select();
  }
  setHistoryFilter();
}

void lemonView::setupTicketView()
{
  if (historyTicketsModel->tableName().isEmpty()) setupHistoryTicketsModel();
  historyTicketsModel->setSort(historyTicketsModel->fieldIndex("id"),Qt::DescendingOrder);
  historyTicketsModel->select();
  QSize tableSize = ui_mainview.ticketView->size();
  int portion = tableSize.width()/7;
  ui_mainview.ticketView->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("id"), portion);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("name"), portion);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("datetime"), portion);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("username"), portion);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("itemcount"), portion);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("amount"), portion);
  ui_mainview.ticketView->horizontalHeader()->resizeSection(historyTicketsModel->fieldIndex("disc"), portion);
}

void lemonView::itemHIDoubleClicked(const QModelIndex &index){
  if (db.isOpen()) {
    //getting data from model...
    const QAbstractItemModel *model = index.model();
    int row = index.row();
    QModelIndex indx = model->index(row, 1); // id = columna 1
    qulonglong transactionId = model->data(indx, Qt::DisplayRole).toULongLong();
    printTicketFromTransaction(transactionId);
    //return to selling tab
    ui_mainview.mainPanel->setCurrentIndex(pageMain);
  }
}

void lemonView::printSelTicket()
{
TRACE;
  QModelIndex index = ui_mainview.ticketView->currentIndex();
  if (historyTicketsModel->tableName().isEmpty()) setupHistoryTicketsModel();
  if (index == historyTicketsModel->index(-1,-1) ) {
    KMessageBox::information(this, i18n("Please select a ticket to print."), i18n("Cannot print ticket"));
  }
  else  {
    qulonglong id = historyTicketsModel->record(index.row()).value("id").toULongLong();
    printTicketFromTransaction(id);
  }
  //return to selling tab
  ui_mainview.mainPanel->setCurrentIndex(pageMain);
}

void lemonView::printTicketFromTransaction(qulonglong transactionNumber){
  TicketInfo oldticket;
  QList<TicketLineInfo> ticketLines;
  ticketLines.clear();
TRACE;
  
  if (!db.isOpen()) db.open();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  
  TransactionInfo trInfo = myDb->getTransactionInfo(transactionNumber);
  QList<TransactionItemInfo> pListItems = myDb->getTransactionItems(transactionNumber);

DBX(trInfo.amount);
DBX(trInfo.totalTax);

  for (int i = 0; i < pListItems.size(); ++i){
    TransactionItemInfo trItem = pListItems.at(i);
    TicketLineInfo tLineInfo;

    // calc TotalTax

    tLineInfo.qty     = trItem.qty;
DBX(tLineInfo.qty);
    tLineInfo.unitStr = trItem.unitStr;
DBX(tLineInfo.unitStr);
    tLineInfo.desc    = trItem.name;
    tLineInfo.price   = trItem.price;
    tLineInfo.disc    = trItem.disc;
    tLineInfo.total   = trItem.total;
    tLineInfo.tax     = trItem.tax;
    tLineInfo.serialNo= trItem.serialNo;
    ticketLines.append(tLineInfo);
  }
  //Ticket
  QDateTime dt;
  dt.setDate(trInfo.date);
  dt.setTime(trInfo.time);
  oldticket.datetime        = dt;
  oldticket.number          = transactionNumber;
  oldticket.total           = trInfo.amount;
  oldticket.clientid        = trInfo.clientid;
  oldticket.change          = trInfo.changegiven;
  oldticket.paidwith        = trInfo.paywith;
  oldticket.itemcount       = trInfo.itemcount;
  oldticket.totalTax        = trInfo.totalTax;
  if (!trInfo.cardnumber.isEmpty())
    oldticket.cardnum       = trInfo.cardnumber.replace(0,15,"***************"); //FIXED: Only save last 4 digits
  else
    oldticket.cardnum       = "";
  oldticket.cardAuthNum     = trInfo.cardauthnum;
  oldticket.paidWithCard    = (trInfo.paymethod == 2) ? true:false;
  oldticket.clientDisc      = 0;
  oldticket.clientDiscMoney = 0;
  oldticket.buyPoints       = 0;
  oldticket.clientPoints    = 0;
  oldticket.clientDiscMoney = trInfo.discmoney;
  oldticket.lines           = ticketLines;
  printTicket(oldticket,trInfo.infoText);
  
}

bool lemonView::askSerialNo()
{
InputDialog *dlg = new InputDialog(this, true, dialogSerialNr, i18n("Enter client and serialnumber of the product."));
for(int n=0;n< ui_mainview.comboClients->count();n++) dlg->comboClients->addItem(ui_mainview.comboClients->itemText(n));
dlg->comboClients->setCurrentIndex(ui_mainview.comboClients->currentIndex());
dlg->reasonEdit->setText(GserialNo);
if (dlg->exec()) {
	ui_mainview.comboClients->setCurrentIndex(dlg->comboClients->currentIndex());
        GserialNo= dlg->reason;
	return true;
	}
return false;
}

void lemonView::showTicketText()
{
InputDialog *dlg = new InputDialog(this, true, dialogTicketInfo, i18n("Enter the infotext for this ticket."));
dlg->reasonEdit->setText(ticketMsg);
if (dlg->exec()) ticketMsg= dlg->reason;
}

void lemonView::showReprintTicket()
{
TRACE;
  ui_mainview.mainPanel->setCurrentIndex(pageReprintTicket);
  QTimer::singleShot(500, this, SLOT(setupTicketView()));
}

void lemonView::cashOut()
{
  bool doit = false;
  //ASK for security if no lowSecurityMode.
TRACE;
  if (Settings::lowSecurityMode()) {
    doit = true;
  } else {
    dlgPassword->show();
    dlgPassword->clearLines();
    dlgPassword->hide();
    doit = dlgPassword->exec();
  }//else lowsecurity
  
  if (doit) {
    log(loggedUserId, QDate::currentDate(), QTime::currentTime(), QString("Cash-OUT by %1 at terminal %2 on %3").
    arg(dlgPassword->username()).
    arg(Settings::editTerminalNumber()).
    arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));
    
    double max = drawer->getAvailableInCash();
    if (!max>0) {
      //KPassivePopup::message( i18n("Error:"),i18n("Cash not available at drawer!"),DesktopIcon("dialog-error", 48), this );

      doNotify("dialog-error",i18n("Cash not available at drawer!"));

    } else {
      InputDialog *dlg = new InputDialog(this, false, dialogCashOut, i18n("Cash Out"), 0.001, max);
      if (dlg->exec() ) {
        Azahar *myDb = new Azahar;
        myDb->setDatabase(db);

        CashFlowInfo info;
        info.amount = dlg->dValue;
DBX(dlg->dValue);
        info.reason = dlg->reason;
        info.date = QDate::currentDate();
        info.time = QTime::currentTime();
        info.terminalNum = Settings::editTerminalNumber();
        info.userid = loggedUserId;
        info.type   = ctCashOut; //Normal cash-out
        qulonglong cfId = myDb->insertCashFlow(info);
        //affect drawer
        //NOTE: What about CUPS printers?
        if (Settings::openDrawer() && Settings::smallTicketDotMatrix()) drawer->open();
        drawer->substractCash(info.amount);
        drawer->insertCashflow(cfId);
      }
    }
  }
}

void lemonView::cashIn()
{
  bool doit = false;
  //ASK for security if no lowSecurityMode.
  if (Settings::lowSecurityMode()) {
    doit = true;
  } else {
    dlgPassword->show();
    dlgPassword->clearLines();
    dlgPassword->hide();
    doit = dlgPassword->exec();
  }//else lowsecurity
  
  if (doit) {
    log(loggedUserId, QDate::currentDate(), QTime::currentTime(), QString("Cash-IN by %1 at terminal %2 on %3").
    arg(dlgPassword->username()).
    arg(Settings::editTerminalNumber()).
    arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")));
    
    InputDialog *dlg = new InputDialog(this, false, dialogCashOut, i18n("Cash In"));
    if (dlg->exec() ) {
      Azahar *myDb = new Azahar;
      myDb->setDatabase(db);

      CashFlowInfo info;
      info.amount = dlg->dValue;
      info.reason = dlg->reason;
      info.date = QDate::currentDate();
      info.time = QTime::currentTime();
      info.terminalNum = Settings::editTerminalNumber();
      info.userid = loggedUserId;
      info.type   = ctCashIn; //normal cash-out
      qulonglong cfId = myDb->insertCashFlow(info);
      //affect drawer
      //NOTE: What about CUPS printers?
      if (Settings::openDrawer() && Settings::smallTicketDotMatrix()) drawer->open();
      drawer->addCash(info.amount);
      drawer->insertCashflow(cfId);
    }
  }
}

void lemonView::cashAvailable()
{
  double available = drawer->getAvailableInCash();
DBX(available);
  doNotify("dialog-information",i18n("There are <b> %1 in cash </b> available at the drawer.", KGlobal::locale()->formatMoney(available)));
}

void lemonView::log(const qulonglong &uid, const QDate &date, const QTime &time, const QString &text)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  myDb->insertLog(uid, date, time, "[ LEMON ] "+text);
}

double lemonView::calcGrossTax(double gross, double taxrate)
{
double retval;
      if(taxrate>=1.00) retval= gross  / (100.0 + taxrate) * taxrate;
      else retval= gross / (1.00 + taxrate) * taxrate ;
DBX(gross << taxrate << retval);
return retval;
}

void lemonView::doNotify(QString icon,QString msg)
    {
    KNotification *notify = new KNotification("information", this);
    notify->setText(msg);
    QPixmap pixmap = DesktopIcon(icon,32);
    notify->setPixmap(pixmap);
    notify->sendEvent();
    }

void lemonView::initPrintTicketInfo(PrintTicketInfo *pti,TicketInfo *ticket,double tDisc)
{
	QPixmap logoPixmap;
	logoPixmap.load(Settings::storeLogo());

	pti->ticketInfo = *ticket;
	pti->sd.storeLogo  = logoPixmap;
	pti->sd.logoOnTop  = Settings::chLogoOnTop();
	pti->sd.storeName  = Settings::editStoreName();
	pti->sd.storeAddr  = Settings::storeAddress();
	pti->sd.storePhone = Settings::storePhone();
	pti->ticketMsg  = Settings::editTicketMessage();
	pti->salesPerson= loggedUserName;
	pti->terminal   = i18n("Terminal #%1", Settings::editTerminalNumber());
	pti->thPhone    = i18n("Phone: ");
	pti->thDate     = KGlobal::locale()->formatDateTime(pti->ticketInfo.datetime, KLocale::LongDate);
	pti->thTicket   = i18n("Ticket # %1", pti->ticketInfo.number);
	pti->thProduct  = i18n("Product");
	pti->thQty      = i18n("Qty");
	pti->thPrice    = i18n("Price");
	pti->thTax      = i18n("Vat");
	pti->thNetPrice = i18n("Netto");
	pti->thTotalTax = i18n("inkl. MwSt");
	pti->thSerialNo = i18n("Serialnr.");
	pti->thDiscount = i18n("Offer");
	pti->thTotal    = i18n("Total");
	pti->thSerialNo = i18n("Serialnr.");
        pti->thCard     = i18n("Card Number  : %1", pti->ticketInfo.cardnum);
        pti->thCardAuth = i18n("Authorization : %1", pti->ticketInfo.cardAuthNum);

        pti->thPaid     = i18n("Paid with %1, your change is %2"
		,KGlobal::locale()->formatMoney(pti->ticketInfo.paidwith, QString(), 2)
		,KGlobal::locale()->formatMoney(pti->ticketInfo.change, QString(), 2) 
		);
//BIG:	pti->thPaid     ="";

        if(pti->ticketInfo.buyPoints)
	        pti->thPoints   = i18n("You got %1 points, your accumulated is :%2", pti->ticketInfo.buyPoints, pti->ticketInfo.clientPoints);

	pti->thArticles = i18np("%1 article.", "%1 articles.", pti->ticketInfo.itemcount);
        pti->tDisc      = KGlobal::locale()->formatMoney(-tDisc, QString(), 2);

	pti->totDisc    = tDisc;
		
	pti->taxes      = i18n("%1",KGlobal::locale()->formatMoney(pti->ticketInfo.totalTax, QString(), 2));
	pti->net        = i18n("%1",KGlobal::locale()->formatMoney(pti->ticketInfo.total-pti->ticketInfo.totalTax, QString(), 2));
        pti->thTotals   = KGlobal::locale()->formatMoney(pti->ticketInfo.total, QString(), 2);
        
        pti->clientDiscMoney = pti->ticketInfo.clientDiscMoney;
}


  
#include "lemonview.moc"


