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
#include <KLocale>
#include <KMessageBox>
#include <KFileDialog>
#include <KStandardDirs>

#include <QByteArray>
#include <QRegExpValidator>
#include <QRegExp>
#include <QtSql>

#include "producteditor.h"
#include "../../dataAccess/azahar.h"
#include "../../src/inputdialog.h"
#include "../../mibitWidgets/mibittip.h"
#include "../../mibitWidgets/mibitfloatpanel.h"
#include "../../src/misc.h"


ProductEditorUI::ProductEditorUI( QWidget *parent )
: QFrame( parent )
{
    setupUi( this );
}

ProductEditor::ProductEditor( QWidget *parent, bool newProduct, const QSqlDatabase& database )
: KDialog( parent )
{
    oldStockQty = 0;
    correctingStockOk = false;
    m_modelAssigned = false;

    db = database;
    
    groupInfo.isAvailable = true;
    groupInfo.cost  = 0;
    groupInfo.price = 0;
    groupInfo.name  = "";
    
    ui = new ProductEditorUI( this );
    setMainWidget( ui );
    setCaption( i18n("Product Editor") );
    setButtons( KDialog::Ok|KDialog::Cancel );
    
    QString path = KStandardDirs::locate("appdata", "styles/");
    path = path+"tip.svg";
    codeTip = new MibitTip(this, ui->editCode, path, DesktopIcon("dialog-warning", 32));
    codeTip->setMaxHeight(65);
    path = KStandardDirs::locate("appdata", "styles/");
    path = path+"floating_bottom.svg";
    groupPanel = new MibitFloatPanel(this, path, Bottom);
    groupPanel->setSize(550,250);
    groupPanel->addWidget(ui->groupsPanel);
    groupPanel->setMode(pmManual);
    groupPanel->setHiddenTotally(true);
    ui->btnShowGroup->setDisabled(true);

    ui->btnChangeCode->setIcon(QIcon(DesktopIcon("edit-clear", 32)));
    //Locate SVG for the tip.
    path = KStandardDirs::locate("appdata", "styles/");
    path = path+"tip.svg";
    codeTip = new MibitTip(this, ui->editCode, path, DesktopIcon("dialog-information", 22));
    codeTip->setSize(100,100);
    panel = new MibitFloatPanel(this, path, Top,300,150);
    panel->setMode(pmManual);
    ui->editNewStock->setEmptyMessage("Enter the new Stock quantity here...");
    ui->editReason->setEmptyMessage("Enter the Reason for the change here...");
    connect(ui->btnCancel, SIGNAL(clicked()), panel, SLOT(hidePanel()));
    connect(ui->btnCancel, SIGNAL(clicked()), this, SLOT(showBtns()));
    connect(panel, SIGNAL(hiddenOnUserRequest()), ui->editCost, SLOT(setFocus()));
    connect(panel, SIGNAL(hiddenOnUserRequest()), this, SLOT(showBtns()));
    connect(ui->btnOk, SIGNAL(clicked()), this, SLOT(updateBtn()));
    connect(ui->editNewStock, SIGNAL(returnPressed()),ui->editReason, SLOT(setFocus()) );
    connect(ui->editReason, SIGNAL(returnPressed()),ui->btnOk, SLOT(setFocus()) );
    ui->editNewStock->setMinimumWidth(250);
    panel->addWidget(ui->groupFloatPanel);
    correctingStockOk = false;
    oldStockQty =0;
    

    //Set Validators for input boxes
    QRegExp regexpC("[0-9]{1,13}"); //(EAN-13 y EAN-8) .. y productos sin codigo de barras?
    QRegExpValidator * validatorEAN13 = new QRegExpValidator(regexpC, this);
    ui->editCode->setValidator(validatorEAN13);
    ui->editUtility->setValidator(new QDoubleValidator(0.00, 999999999999.99, 3,ui->editUtility));
    ui->editCost->setValidator(new QDoubleValidator(0.00, 999999999999.99, 3, ui->editCost));
    ui->editStockQty->setValidator(new QDoubleValidator(0.00,999999999999.99, 3, ui->editStockQty));
    ui->editPoints->setValidator(new QIntValidator(0,999999999, ui->editPoints));
    ui->editFinalPrice->setValidator(new QDoubleValidator(0.00,999999999999.99, 3, ui->editFinalPrice));

    connect( ui->btnPhoto          , SIGNAL( clicked() ), this, SLOT( changePhoto() ) );
    connect( ui->btnCalculatePrice , SIGNAL( clicked() ), this, SLOT( calculatePrice() ) );
    connect( ui->btnChangeCode,      SIGNAL( clicked() ), this, SLOT( changeCode() ) );
    connect( ui->editCode, SIGNAL(editingFinished()), SLOT(checkIfCodeExists()));
    connect( ui->editCode, SIGNAL(editingFinished()), this, SLOT(checkFieldsState()));
    connect( ui->editCode, SIGNAL(returnPressed()), SLOT(checkFieldsState()));
    connect( ui->btnStockCorrect, SIGNAL( clicked() ), this, SLOT( showPanelStock() ) );
    connect( ui->btnAddBrand, SIGNAL( clicked() ), this, SLOT( showPanelBrand() ) );
    connect( ui->btnAddCategory, SIGNAL( clicked() ), this, SLOT( showPanelCategory() ) );
    connect( ui->btnAddUnits, SIGNAL( clicked() ), this, SLOT( showPanelMeasures() ) );
    connect( ui->btnAddProvider, SIGNAL( clicked() ), this, SLOT( showPanelProviders() ) );

    connect( ui->editDesc, SIGNAL(editingFinished()), this, SLOT(checkFieldsState()));
    connect( ui->editStockQty, SIGNAL(editingFinished()), this, SLOT(checkFieldsState()));
    connect( ui->editPoints, SIGNAL(editingFinished()), this, SLOT(checkFieldsState()));
    connect( ui->editCost, SIGNAL(editingFinished()), this, SLOT(checkFieldsState()));
    connect( ui->editFinalPrice, SIGNAL(textEdited(const QString &)), this, SLOT(checkFieldsState()));

    connect( ui->chIsIndividual, SIGNAL(clicked(bool)), SLOT(toggleIndividual(bool)) );
    connect( ui->chIsAGroup, SIGNAL(clicked(bool)), SLOT(toggleGroup(bool)) );
    connect( ui->chIsARaw, SIGNAL(clicked(bool)), SLOT(toggleRaw(bool)) );
    connect( ui->btnCloseGroup, SIGNAL(clicked()), groupPanel, SLOT(hidePanel()) );
    connect( ui->btnShowGroup, SIGNAL(clicked()),  groupPanel, SLOT(showPanel()) );
    connect( ui->editFilter, SIGNAL(textEdited ( const QString &)), SLOT(applyFilter(const QString &)) );
    connect( ui->btnAdd,    SIGNAL(clicked()), SLOT(addItem()) );
    connect( ui->btnRemove, SIGNAL(clicked()), SLOT(removeItem()) );
    connect( ui->groupView, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), SLOT(itemDoubleClicked(QTableWidgetItem*)) );
    
    connect(ui->taxModelCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateTax(int)));
    connect(ui->brandCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateBrand(int)));
    connect(ui->providerCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateProvider(int)));
    connect(ui->categoriesCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCategory(int)));
    connect(ui->measuresCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateMeasure(int)));
    connect(ui->editCode, SIGNAL(textEdited(const QString &)), this, SLOT(updateCode(const QString &)));
    connect(ui->editAlphacode, SIGNAL(textEdited(const QString &)), this, SLOT(updateACode(const QString &)));
    connect(ui->editCost, SIGNAL(textEdited(const QString &)), this, SLOT(updateCost(const QString &)));
    connect(ui->editDesc, SIGNAL(textEdited(const QString &)), this, SLOT(updateDesc(const QString &)));
    connect(ui->editFinalPrice, SIGNAL(textEdited(const QString &)), this, SLOT(updatePrice(const QString &)));
    connect(ui->editPoints, SIGNAL(textEdited(const QString &)), this, SLOT(updatePoints(const QString &)));
    connect(ui->editStockQty, SIGNAL(textEdited(const QString &)), this, SLOT(updateStockQty(const QString &)));


    status = statusNormal;
    modifyCode = false;

    doAdd=newProduct;

    if (newProduct) {
      ui->labelStockQty->setText(i18n("Purchase Qty:"));
      disableStockCorrection();
    } else ui->labelStockQty->setText(i18n("Stock Qty:"));

    //QTimer::singleShot(750, this, SLOT(checkIfCodeExists()));

    ui->editStockQty->setText("0");
    ui->editPoints->setText("0");
    m_pInfo.tax = 0.0;
    m_pInfo.totaltax = 0.0;
    m_pInfo.taxmodelid = 0;
    m_pInfo.brandid = 0;
    m_pInfo.lastProviderId = 0;
    m_pInfo.isAGroup = false;
    m_pInfo.isARawProduct = false;
    m_pInfo.isIndividual = false;
}

ProductEditor::~ProductEditor()
{
    //remove products filter
    m_model->setFilter("");
    m_model->select();

    
    delete ui;
}

void ProductEditor::applyFilter(const QString &text)
{
  QRegExp regexp = QRegExp(text);
  if (!regexp.isValid())  ui->editFilter->setText("");
  if (text == "*" || text == "") m_model->setFilter("");
  else  m_model->setFilter(QString("products.name REGEXP '%1'").arg(text));
  
  m_model->select();
}

void ProductEditor::setDb(QSqlDatabase database)
{
  db = database;
  if (!db.isOpen()) db.open();
  populateCategoriesCombo();
  populateMeasuresCombo();
  populateProvidersCombo();
  populateBrandsCombo();
  populateTaxModelsCombo();
}

void ProductEditor::populateCategoriesCombo()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ui->categoriesCombo->clear();
  ui->categoriesCombo->addItems(myDb->getCategoriesList());
//kh 2012-07-17
  QAbstractItemModel *model =ui->categoriesCombo->model();
  model->sort(0);
//kh  delete myDb;
}

void ProductEditor::populateMeasuresCombo()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ui->measuresCombo->clear();
  ui->measuresCombo->addItems(myDb->getMeasuresList());
//kh 2012-07-17
  QAbstractItemModel *model =ui->measuresCombo->model();
  model->sort(0);
  delete myDb;
}

int ProductEditor::getCategoryId()
{
  int code=-1;
  QString currentText = ui->categoriesCombo->currentText();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  code = myDb->getCategoryId(currentText);
  delete myDb;
  return code;
}


int ProductEditor::getMeasureId()
{
  int code=-1;
  QString currentText = ui->measuresCombo->currentText();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  code = myDb->getMeasureId(currentText);
  delete myDb;
  return code;
}

void ProductEditor::setCode(qulonglong c)
{
  origCode=c;
qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << ui->editCode->text() << c;
  ui->editCode->setText(QString::number(c));
  //get product Info...
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  m_pInfo = myDb->getProductInfo(QString::number(c));
  //update form with product data
  checkIfCodeExists();
}

void ProductEditor::setCategory(QString str)
{
 int idx = ui->categoriesCombo->findText(str,Qt::MatchCaseSensitive);
 if (idx > -1) ui->categoriesCombo->setCurrentIndex(idx);
 else {
  qDebug()<<"Str not found:"<<str;
  }
}

void ProductEditor::setTax(const QString &str)
{
  int idx = ui->taxModelCombo->findText(str,Qt::MatchCaseSensitive);
  if (idx > -1) ui->taxModelCombo->setCurrentIndex(idx);
  else {
    qDebug()<<"Str not found:"<<str;
  }
}

void ProductEditor::setCategory(int i)
{
 QString text = getCategoryStr(i);
 setCategory(text);
}

void ProductEditor::setMeasure(int i)
{
 QString text = getMeasureStr(i);
 setMeasure(text);
}

void ProductEditor::setTax(const int &i)
{
  QString text = getTaxModelStr(i);
  setTax(text);
}

void ProductEditor::setBrand(const int &i)
{
  QString text = getBrandStr(i);
  setBrand(text);
}

void ProductEditor::setProvider(const int &i)
{
  QString text = getProviderStr(i);
  setProvider(text);
}

void ProductEditor::setMeasure(QString str)
{
int idx = ui->measuresCombo->findText(str,Qt::MatchCaseSensitive);
 if (idx > -1) ui->measuresCombo->setCurrentIndex(idx);
 else {
  qDebug()<<"Str not found:"<<str;
  }
}

void ProductEditor::setBrand(const QString &str)
{
  int idx = ui->brandCombo->findText(str,Qt::MatchCaseSensitive);
  if (idx > -1) 
    ui->brandCombo->setCurrentIndex(idx);
  else {
    qDebug()<<"Str not found:"<<str;
  }
}

void ProductEditor::setProvider(const QString &str)
{
  int idx = ui->providerCombo->findText(str,Qt::MatchCaseSensitive);
  if (idx > -1) ui->providerCombo->setCurrentIndex(idx);
  else {
    qDebug()<<"Str not found:"<<str;
  }
}

void ProductEditor::setTaxModel(qulonglong id)
{
  m_pInfo.taxmodelid = id;
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QString str = myDb->getTaxModelName(id);

  int idx = ui->taxModelCombo->findText(str,Qt::MatchCaseSensitive);
  if (idx > -1) ui->taxModelCombo->setCurrentIndex(idx);
  else {
    qDebug()<<"TaxModel Str not found:"<<str;
  }
  updateTax(0);
}

void ProductEditor::updateGroupNRaw()
{
  m_pInfo.isAGroup = isGroup();
  m_pInfo.isARawProduct = isRaw();
  m_pInfo.isIndividual = isIndividual();
  m_pInfo.groupElementsStr = getGroupElementsStr();
}

void ProductEditor::updateTax(int)
{
  //get data from selected model.
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  //refresh taxmodel if changed...
  m_pInfo.taxmodelid   = myDb->getTaxModelId(ui->taxModelCombo->currentText());
  m_pInfo.taxElements  = myDb->getTaxModelElements(m_pInfo.taxmodelid);
  m_pInfo.tax          = myDb->getTotalTaxPercent(m_pInfo.taxElements);
  //way to apply the tax?? -- There is a problem if the cost includes taxes already.
  double cost    = ui->editCost->text().toDouble();
  double profit = ui->editUtility->text().toDouble();
  //Utility is calculated before taxes... Taxes include profit... is it ok?
  profit = ((profit/100)*cost);
  m_pInfo.totaltax = (cost + profit)*m_pInfo.tax; //taxes in money.
  qDebug()<<"Updating TaxModel:"<<m_pInfo.taxmodelid;
}

void ProductEditor::updateCategory(int)
{
  int code=-1;
  QString currentText = ui->categoriesCombo->currentText();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  code = myDb->getCategoryId(currentText);
  m_pInfo.category = code;
}

void ProductEditor::updateMeasure(int)
{
  int code=-1;
  QString currentText = ui->measuresCombo->currentText();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  code = myDb->getMeasureId(currentText);
  m_pInfo.units = code;
}

void ProductEditor::updateCode(const QString &str)
{
  m_pInfo.code = str.toULongLong();
qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << ui->editCode->text() << m_pInfo.code;
}

void ProductEditor::updateACode(const QString &str)
{
  m_pInfo.alphaCode = str;
}

void ProductEditor::updateCost(const QString &str)
{
  m_pInfo.cost = str.toDouble();
}

void ProductEditor::updatePrice(const QString &str)
{
  m_pInfo.price = str.toDouble();
}

void ProductEditor::updatePoints(const QString &str)
{
  m_pInfo.points = str.toULongLong();
}

void ProductEditor::updateStockQty(const QString &str)
{
  m_pInfo.stockqty = str.toDouble();
}

void ProductEditor::updateDesc(const QString &str)
{
  m_pInfo.desc = str;
}

void ProductEditor::updateBrand(int)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  m_pInfo.brandid = myDb->getBrandId(ui->brandCombo->currentText());
}

void ProductEditor::updateProvider(int)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  m_pInfo.lastProviderId = myDb->getProviderId(ui->providerCombo->currentText());
}

QString ProductEditor::getCategoryStr(int c)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QString str = myDb->getCategoryStr(c);
  delete myDb;
  return str;
}

QString ProductEditor::getMeasureStr(int c)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QString str = myDb->getMeasureStr(c);
  delete myDb;
  return str;
}

QString ProductEditor::getBrandStr(int c)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QString str = myDb->getBrandName(c);
  return str;
}

QString ProductEditor::getProviderStr(int c)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QString str = myDb->getProviderName(c);
  return str;
}

QString ProductEditor::getTaxModelStr(int c)
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  QString str = myDb->getTaxModelName(c);
  return str;
}

void ProductEditor::changePhoto()
{
 QString fname = KFileDialog::getOpenFileName();
  if (!fname.isEmpty()) {
    QPixmap p = QPixmap(fname);
    setPhoto(p);
    //update photo ba to the m_pInfo
    m_pInfo.photo = Misc::pixmap2ByteArray(new QPixmap(p));
  }
}

void ProductEditor::calculatePrice()
{
 double finalPrice=0.0;
 if (ui->editCost->text().isEmpty()) {
   ui->editCost->setFocus();
 }
 else if (ui->editUtility->text().isEmpty()) {
   ui->editUtility->setFocus();
 }
 else {
  //TODO: if TAXes are included in cost...
  double cost    = ui->editCost->text().toDouble();
  double profit = ui->editUtility->text().toDouble();
  //Utility is calculated before taxes... Taxes include profit... is it ok?
  profit = ((profit/100)*cost);
  finalPrice = cost + profit + m_pInfo.totaltax;
  
  // BFB: avoid more than 2 decimal digits in finalPrice. Round.
  ui->editFinalPrice->setText(QString::number(finalPrice,'f',2));
  ui->editFinalPrice->selectAll();
  ui->editFinalPrice->setFocus();
  m_pInfo.price = finalPrice;
  }
}

void ProductEditor::changeCode()
{
  //this enables the code editing... to prevent unwanted code changes...
  enableCode();
  ui->editCode->setFocus();
  ui->editCode->selectAll();
}

void ProductEditor::showPanelStock()
{
  //setting validators
  QRegExp regexpA("[0-9]*[//.]{0,1}[0-9][0-9]*"); //Cualquier numero flotante (0.1, 100, 0, .10, 100.0, 12.23)
  QRegExpValidator * validatorFloat = new QRegExpValidator(regexpA,this);
  ui->editNewStock->setValidator(validatorFloat);
  
  ui->editReason->show();
  ui->editNewStock->show();
  ui->groupFloatPanel->setTitle(i18n("Stock Correction"));
  ui->editNewStock->setEmptyMessage(i18n("Enter the new stock quantity here"));
  ui->editReason->setEmptyMessage(i18n("Enter the reason for the change here"));
  ui->btnOk->setText(i18n("Update Stock"));
  ui->groupFloatPanel->setWhatsThis("Stock Correction");
  panel->showPanel();
  ui->editNewStock->setFocus();
  enableButtonOk(false);
  enableButtonCancel(false);
}

void ProductEditor::showPanelBrand()
{
  //remove validator 
  ui->editNewStock->setValidator(0);
  
  ui->editReason->hide();
  ui->groupFloatPanel->setTitle(i18n("Create New Brand"));
  ui->editNewStock->setEmptyMessage(i18n("Enter the new brand name here"));
  ui->btnOk->setText(i18n("Create Brand"));
  ui->groupFloatPanel->setWhatsThis("New Brand");
  panel->showPanel();
  ui->editNewStock->setFocus();
  enableButtonOk(false);
  enableButtonCancel(false);
}

void ProductEditor::showPanelCategory()
{
  //remove validator
  ui->editNewStock->setValidator(0);
  ui->editReason->hide();
  ui->groupFloatPanel->setTitle(i18n("Create New Category"));
  ui->editNewStock->setEmptyMessage(i18n("Enter the new category name here"));
  ui->btnOk->setText(i18n("Create Category"));
  ui->groupFloatPanel->setWhatsThis("New Category");
  panel->showPanel();
  ui->editNewStock->setFocus();
  enableButtonOk(false);
  enableButtonCancel(false);
}

void ProductEditor::showPanelMeasures()
{
  //remove validator
  ui->editNewStock->setValidator(0);
  ui->editReason->hide();
  ui->groupFloatPanel->setTitle(i18n("Create New Weight or Measure"));
  ui->editNewStock->setEmptyMessage(i18n("Enter the new measure name here"));
  ui->btnOk->setText(i18n("Create Measure"));
  ui->groupFloatPanel->setWhatsThis("New Measure or weight");
  panel->showPanel();
  ui->editNewStock->setFocus();
  enableButtonOk(false);
  enableButtonCancel(false);
}

void ProductEditor::showPanelProviders()
{
  //remove validator
  ui->editNewStock->setValidator(0);
  ui->editReason->hide();
  ui->groupFloatPanel->setTitle(i18n("Create New Provider"));
  ui->editNewStock->setEmptyMessage(i18n("Enter the new provider name here"));
  ui->btnOk->setText(i18n("Create Provider"));
  ui->groupFloatPanel->setWhatsThis("New Provider");
  panel->showPanel();
  ui->editNewStock->setFocus();
  enableButtonOk(false);
  enableButtonCancel(false);
}

void ProductEditor::updateBtn()
{
//qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << m_pInfo.isIndividual;

  if (ui->groupFloatPanel->whatsThis().contains("Stock", Qt::CaseInsensitive)) {
    ///correcting stock
    correctingStockOk = false;
    oldStockQty = ui->editStockQty->text().toDouble();
    if (!ui->editNewStock->text().isEmpty() && !ui->editReason->text().isEmpty()) {
      ui->editStockQty->setText(ui->editNewStock->text());
      reason = ui->editReason->text();
      correctingStockOk = true;
      panel->hidePanel();
      enableButtonOk(true);
      enableButtonCancel(true);
      ui->editNewStock->clear();
    } else ui->editNewStock->setFocus();
  } else if (ui->groupFloatPanel->whatsThis().contains("Brand", Qt::CaseInsensitive)) {
    ///creating new brand
    correctingStockOk = false;
    if (!ui->editNewStock->text().isEmpty()) { //jus reusing the line edit
      //send new brand to db
      Azahar *myDb = new Azahar;
      myDb->setDatabase(db);
      myDb->insertBrand(ui->editNewStock->text());
      //repopulate brands list
      populateBrandsCombo();
      //select the new brand... we suppose that the user wants to use the new one
      int idx = ui->brandCombo->findText(ui->editNewStock->text(),Qt::MatchCaseSensitive);
      if (idx > -1) ui->brandCombo->setCurrentIndex(idx); else qDebug()<<"brand not found on combo box:"<<ui->editNewStock->text();
      //update brands model
      emit updateBrandsModel();
      //finally close panel and reenable buttons
      panel->hidePanel();
      enableButtonOk(true);
      enableButtonCancel(true);
      //and clear the edits.
      ui->editNewStock->clear();
    }
  } else if (ui->groupFloatPanel->whatsThis().contains("Category", Qt::CaseInsensitive)) {
    ///creating new category
    correctingStockOk = false;
    if (!ui->editNewStock->text().isEmpty()) { //jus reusing the line edit
      //send new category to db
      Azahar *myDb = new Azahar;
      myDb->setDatabase(db);
      myDb->insertCategory(ui->editNewStock->text());
      //repopulate categories list
      populateCategoriesCombo();
      //select the new cat... we suppose that the user wants to use the new one
      int idx = ui->categoriesCombo->findText(ui->editNewStock->text(),Qt::MatchCaseSensitive);
      if (idx > -1) ui->categoriesCombo->setCurrentIndex(idx); else qDebug()<<"category not found on combo box:"<<ui->editNewStock->text();
      //update Cat model
      emit updateCategoriesModel();
      //finally close panel and reenable buttons
      panel->hidePanel();
      enableButtonOk(true);
      enableButtonCancel(true);
      ui->editNewStock->clear();
    }
  }
  else if (ui->groupFloatPanel->whatsThis().contains("Measure", Qt::CaseInsensitive)) {
    ///creating new category
    correctingStockOk = false;
    if (!ui->editNewStock->text().isEmpty()) { //jus reusing the line edit
      //send new measure to db
      Azahar *myDb = new Azahar;
      myDb->setDatabase(db);
      myDb->insertMeasure(ui->editNewStock->text());
      //repopulate measures list
      populateMeasuresCombo();
      //select the new measures... we suppose that the user wants to use the new one
      int idx = ui->measuresCombo->findText(ui->editNewStock->text(),Qt::MatchCaseSensitive);
      if (idx > -1) ui->measuresCombo->setCurrentIndex(idx); else qDebug()<<"measure not found on combo box:"<<ui->editNewStock->text();
      //update model
      emit updateMeasuresModel();
      //finally close panel and reenable buttons
      panel->hidePanel();
      enableButtonOk(true);
      enableButtonCancel(true);
      ui->editNewStock->clear();
    }
  }
  else if (ui->groupFloatPanel->whatsThis().contains("Provider", Qt::CaseInsensitive)) {
    ///creating new provider
    correctingStockOk = false;
    if (!ui->editNewStock->text().isEmpty()) { //jus reusing the line edit
      //send new provider to db
      Azahar *myDb = new Azahar;
      myDb->setDatabase(db);
      ProviderInfo pinfo;
      pinfo.name = ui->editNewStock->text();
      pinfo.address = "";
      pinfo.phone = "";
      pinfo.cell = "";
      myDb->insertProvider(pinfo);
      //repopulate providers list
      populateProvidersCombo();
      //select the new provider... we suppose that the user wants to use the new one
      int idx = ui->providerCombo->findText(ui->editNewStock->text(),Qt::MatchCaseSensitive);
      if (idx > -1) ui->providerCombo->setCurrentIndex(idx); else qDebug()<<"provider not found on combo box:"<<ui->editNewStock->text();
      //update model
      emit updateProvidersModel();
      //finally close panel and reenable buttons
      panel->hidePanel();
      enableButtonOk(true);
      enableButtonCancel(true);
      ui->editNewStock->clear();
    }
  }
}

void ProductEditor::showBtns()
{
  enableButtonOk(true);
  enableButtonCancel(true);
}


void ProductEditor::checkIfCodeExists()
{

  qDebug()<<"Checking if code exists:"<<ui->editCode->text();
qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << doAdd << ui->editCode->text() << m_pInfo.code << origCode;
  enableButtonOk( false );
  QString codeStr = ui->editCode->text();
  if (codeStr.isEmpty()) {
    if(m_pInfo.code>0) {
	ui->editCode->setText(QString("%1").arg(m_pInfo.code));
	codeStr = ui->editCode->text();
    } else {
     codeStr="-1";
     }
  }

  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ProductInfo pInfo = myDb->getProductInfo(codeStr);

qDebug() << __FILE__ << __LINE__ << __FUNCTION__ <<origCode << codeStr << m_pInfo.code;
  if(doAdd || origCode != codeStr.toLongLong()) {
	if (pInfo.code > 0 || codeStr.isEmpty() ) {
		pInfo.code = myDb->getProductNextCode(codeStr);
		if(m_pInfo.code != pInfo.code) ui->editCode->setStyleSheet(tr2i18n("background: wheat;", 0));
		ui->editCode->setText(QString("%1").arg(pInfo.code));
		pInfo.code=0;
		}
	}
qDebug() << __FILE__ << __LINE__ << __FUNCTION__ <<origCode << codeStr << m_pInfo.code;

  if (pInfo.code > 0) { //code exists...
    status = statusMod;
    if (!modifyCode){
//    Modify existing product...
      ui->editDesc->setText(pInfo.desc);
      ui->editAlphacode->setText(pInfo.alphaCode);
      ui->editStockQty->setText(QString::number(pInfo.stockqty));
      setCategory(pInfo.category);
      setMeasure(pInfo.units);
      ui->editCost->setText(QString::number(pInfo.cost));
      ui->editFinalPrice->setText(QString::number(pInfo.price));
      ui->editPoints->setText(QString::number(pInfo.points));
      setTax(pInfo.taxmodelid);
      setBrand(pInfo.brandid);
      setProvider(pInfo.lastProviderId);
      setIsAGroup(pInfo.isAGroup);
      setGroupElements(pInfo.groupElementsStr);
      setIsARaw(pInfo.isARawProduct);
      ui->chIsIndividual->setChecked(pInfo.isIndividual);
      ui->taxModelCombo->setDisabled(pInfo.isAGroup); // DISABLE TAX MODEL if the product is a group. Tax will be the content's taxes.
      if (!pInfo.isAGroup && !pInfo.isARawProduct) {
        ui->chIsAGroup->setEnabled(true);
        ui->chIsARaw->setEnabled(true);
      }
      if (!pInfo.photo.isEmpty()) {
        QPixmap photo;
        photo.loadFromData(pInfo.photo);
        setPhoto(photo);
      }
    }//if !modifyCode
//kh    else {
//kh      codeTip->showTip(i18n("This code is for product named %1",pInfo.desc), 4000);
//kh      enableButtonOk( false );
//kh    }
  } else { //code does not exists...
//Add product
    status = statusNormal;
    if (!modifyCode) {
      //clear all used edits
      ui->editDesc->clear();
      ui->editStockQty->clear();
      setCategory(1);
      setMeasure(1);
      setBrand(1);
      setProvider(1);
      setTax(1);
      ui->editCost->clear();
      ui->editFinalPrice->clear();
      ui->editPoints->clear();
      ui->editUtility->clear();
      ui->editFinalPrice->clear();
      ui->labelPhoto->setText("No Photo");
      }
      //qDebug()<< "no product found with code "<<codeStr<<" .query.size()=="<<query.size();
  }
  delete myDb;
}


void ProductEditor::checkFieldsState()
{
  bool ready = false;
  if ( !ui->editCode->text().isEmpty()    &&
    !ui->editDesc->text().isEmpty()       &&
    //!ui->editStockQty->text().isEmpty()   &&   Comment: This requirement was removed in order to use check-in/check-out procedures.
    !ui->editPoints->text().isEmpty()     &&
    !ui->editCost->text().isEmpty()       &&
    !ui->editFinalPrice->text().isEmpty()
    )  {
    ready = true;
  }
  enableButtonOk(ready);
  
  if (!ready  && ui->editCode->hasFocus() && ui->editCode->isReadOnly() ) {
    ui->editDesc->setFocus();
  }
}

void ProductEditor::setPhoto(QPixmap p)
{
  int max = 150;
  QPixmap newPix;
  if ((p.height() > max) || (p.width() > max) ) {
    if (p.height() == p.width()) {
      newPix = p.scaled(QSize(max, max));
    }
    else if (p.height() > p.width() ) {
      newPix = p.scaledToHeight(max);
    }
    else  {
      newPix = p.scaledToWidth(max);
    }
  } else newPix=p;
  ui->labelPhoto->setPixmap(newPix);
  pix=newPix;
}

void ProductEditor::slotButtonClicked(int button)
{
  //update all information...
  updateTax(0);
  updateBrand(0);
  updateProvider(0);
  updateMeasure(0);
  updateCategory(0);
  updateCode(ui->editCode->text());
  updateACode(ui->editAlphacode->text());
  updateCost(ui->editCost->text());
  updateDesc(ui->editDesc->text());
  updatePoints(ui->editPoints->text());
  updatePrice(ui->editFinalPrice->text());
  updateStockQty(ui->editStockQty->text());
  updateGroupNRaw();
  
  
  if (button == KDialog::Ok) {
    if (status == statusNormal) QDialog::accept();
    else {
      qDebug()<< "Button = OK, status == statusMOD";
      done(statusMod);
    }
  }
  else QDialog::reject();
}

void ProductEditor::setModel(QSqlRelationalTableModel *model)
{
  ui->sourcePView->setModel(model);
  ui->sourcePView->setModelColumn(1);
  m_model = model;
  m_modelAssigned = true;

  //clear any filter
  m_model->setFilter("");
  m_model->select();
}

///TODO: Need to decide how to select the qty for each product of the group
void ProductEditor::addItem()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  
  //get selected items from source view
  QItemSelectionModel *selectionModel = ui->sourcePView->selectionModel();
  QModelIndexList indexList = selectionModel->selectedRows(); // pasar el indice que quiera (0=code, 1=name)
  foreach(QModelIndex index, indexList) {
    qulonglong code = index.data().toULongLong();
    
    bool exists = false;
    ProductInfo pInfo;
    //get product info from hash or db
    if (groupInfo.productsList.contains(code)) {
      pInfo = groupInfo.productsList.take(code);
      pInfo.qtyOnList += 1; //increment it
      exists = true;
    } else {
      pInfo = myDb->getProductInfo(QString::number(code));
      pInfo.qtyOnList = 1;
    }
    
    //check if the product to be added is not the same of the pack product
    if (pInfo.code == ui->editCode->text().toULongLong()) continue; //HEY PURIST, WHEN I GOT SOME TIME I WILL CLEAN IT
      
      // Insert/Update GroupView
      if (!exists) {
        // Insert into the groupView
        int rowCount = ui->groupView->rowCount();
        ui->groupView->insertRow(rowCount);
        ui->groupView->setItem(rowCount, 0, new QTableWidgetItem(QString::number(1)));
        ui->groupView->setItem(rowCount, 1, new QTableWidgetItem(pInfo.desc));
      } else {
        //simply update the groupView with the new qty
        for (int ri=0; ri<ui->groupView->rowCount(); ++ri)
        {
          QTableWidgetItem * item = ui->groupView->item(ri, 1);
          QString name = item->data(Qt::DisplayRole).toString();
          if (name == pInfo.desc) {
            //update
            QTableWidgetItem *itemQ = ui->groupView->item(ri, 0);//item qty
            itemQ->setData(Qt::EditRole, QVariant(pInfo.qtyOnList));
            continue; //HEY PURIST, WHEN I GOT SOME TIME I WILL CLEAN IT
          }
        }
      }
      // update info of the group
      groupInfo.count = groupInfo.count+1;
      groupInfo.cost  += pInfo.cost;
      groupInfo.price += pInfo.price;
      //NOTE:group price is not affected by any product discount, it takes normal price.
      bool yes = false;
      if (pInfo.stockqty > 0 )
        yes = true;
      groupInfo.isAvailable = (groupInfo.isAvailable && yes );
      // Insert product to the group hash
      groupInfo.productsList.insert(code, pInfo);
  }
  ui->groupView->resizeRowsToContents();
  ui->groupView->resizeColumnsToContents();
  ui->groupView->clearSelection();
  ui->sourcePView->clearSelection();
  
  //update cost and price on the form
  ui->editCost->setText(QString::number(groupInfo.cost));
  ui->editFinalPrice->setText(QString::number(groupInfo.price));

  //update m_pInfo
  m_pInfo.groupElementsStr = getGroupElementsStr();
  
  //qDebug()<<"There are "<<groupInfo.count<<" items in group. The cost is:"<<groupInfo.cost<<", The price is:"<<groupInfo.price<<" And is available="<<groupInfo.isAvailable;
  
  delete myDb;
}

void ProductEditor::removeItem()
{
  if (ui->groupView->currentRow() != -1 ){
    //get selected item from group view
    int row = ui->groupView->currentRow();
    QTableWidgetItem *item = ui->groupView->item(row, 1);
    QString name = item->data(Qt::DisplayRole).toString();
    Azahar *myDb = new Azahar;
    myDb->setDatabase(db);
    //get code from db
    qulonglong code = myDb->getProductCode(name);
    ProductInfo pInfo = groupInfo.productsList.take(code); //insert it later...
    double qty = pInfo.qtyOnList; //from hash | must be the same on groupView
    if (qty>1) {
      qty--;
      item = ui->groupView->item(row, 0);
      item->setData(Qt::EditRole, QVariant(qty));
      pInfo.qtyOnList = qty;
      groupInfo.productsList.insert(code, pInfo);
    } else {
      //delete it from groupView, already removed from hash
      ui->groupView->removeRow(row);
    }
    // update info of the group
    groupInfo.count -= 1;
    groupInfo.cost  -= pInfo.cost;
    groupInfo.price -= pInfo.price;
    bool yes = false;
    if (pInfo.stockqty > 0 ) //TODO:Falta checar la cantidad que se desea en elgrupo de cada producto
      yes = true;
    groupInfo.isAvailable = (groupInfo.isAvailable && yes );
    delete myDb;
  } //there is something selected
  
  //update cost and price on the form
  ui->editCost->setText(QString::number(groupInfo.cost));
  ui->editFinalPrice->setText(QString::number(groupInfo.price));
  
  //update m_pInfo
  m_pInfo.groupElementsStr = getGroupElementsStr();
  
  qDebug()<<"There are "<<groupInfo.count<<" items in group. The cost is:"<<groupInfo.cost<<", The price is:"<<groupInfo.price<<" And is available="<<groupInfo.isAvailable;
}

void ProductEditor::itemDoubleClicked(QTableWidgetItem* item)
{
  int row = item->row();
  QTableWidgetItem *itm = ui->groupView->item(row, 1);
  QString name = itm->data(Qt::DisplayRole).toString();
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  //get code from db
  qulonglong code = myDb->getProductCode(name);
  ProductInfo pInfo = groupInfo.productsList.take(code); //insert it later...
  double qty = pInfo.qtyOnList+1; //from hash | must be the same on groupView
  
  //modify pInfo
  pInfo.qtyOnList = qty; //increment it one by one
  //reinsert it to the hash
  groupInfo.productsList.insert(code, pInfo);
  //modify groupView
  itm = ui->groupView->item(row, 0);
  itm->setData(Qt::EditRole, QVariant(qty));
  // update info of the group
  groupInfo.count += 1;
  groupInfo.cost  += pInfo.cost;
  groupInfo.price += pInfo.price;
  bool yes = false;
  if (pInfo.stockqty > 0 ) //TODO:Falta checar la cantidad que se desea en elgrupo de cada producto
    yes = true;
  groupInfo.isAvailable = (groupInfo.isAvailable && yes );
  
  //update cost and price on the form
  ui->editCost->setText(QString::number(groupInfo.cost));
  ui->editFinalPrice->setText(QString::number(groupInfo.price));

  //update m_pInfo
  m_pInfo.groupElementsStr = getGroupElementsStr();
  
  //qDebug()<<"There are "<<groupInfo.count<<" items in group. The cost is:"<<groupInfo.cost<<", The price is:"<<groupInfo.price<<" And is available="<<groupInfo.isAvailable;
  delete myDb;
}

QString ProductEditor::getGroupElementsStr()
{
  QStringList list;
  foreach(ProductInfo info, groupInfo.productsList) {
    list.append(QString::number(info.code)+"/"+QString::number(info.qtyOnList));
  }
  return list.join(",");
}

///FIXME: Still need to code this method!!!!!!   UNUSED.
QString ProductEditor::getSpecialOrdersStr()
{
  QStringList list;
  
  return list.join(",");
}

bool ProductEditor::isGroup()
{
  bool result=false;
  if (groupInfo.count>0 && ui->chIsAGroup->isChecked())
    result = true;
  return result;
}

bool ProductEditor::isRaw()
{
  return ui->chIsARaw->isChecked();
}

bool ProductEditor::isIndividual()
{
qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << ui->chIsIndividual->isChecked();
  return ui->chIsIndividual->isChecked();
}

GroupInfo ProductEditor::getGroupHash()
{
  return groupInfo;
}

void ProductEditor::toggleIndividual(bool checked)
{
  m_pInfo.isIndividual      = ui->chIsIndividual->isChecked();
qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << m_pInfo.isIndividual;
}

void ProductEditor::toggleGroup(bool checked)
{
  if (checked) {
    groupPanel->showPanel();
    ui->btnStockCorrect->setDisabled(true);
    ui->chIsARaw->setDisabled(true);
    ui->chIsARaw->setChecked(false);
    ui->btnShowGroup->setEnabled(true);
  } else {
    groupPanel->hidePanel();
    ui->btnStockCorrect->setEnabled(true);
    ui->btnShowGroup->setDisabled(true);
    ui->chIsARaw->setEnabled(true);
  }
  //update m_pInfo
  m_pInfo.isAGroup      = ui->chIsAGroup->isChecked();
  m_pInfo.isARawProduct = ui->chIsARaw->isChecked();
}

void ProductEditor::toggleRaw(bool checked)
{
  if (checked){
    ui->chIsAGroup->setDisabled(true);
    ui->chIsAGroup->setChecked(false);
    ui->btnShowGroup->setDisabled(true);
  } else {
    ui->chIsAGroup->setEnabled(true);
  }
  //update m_pInfo
  m_pInfo.isAGroup      = ui->chIsAGroup->isChecked();
  m_pInfo.isARawProduct = ui->chIsARaw->isChecked();
}

void ProductEditor::setIsAGroup(bool value)
{
  ui->chIsAGroup->setChecked(value);
  ui->chIsAGroup->setEnabled(value);
  ui->btnShowGroup->setEnabled(value);
  ui->btnStockCorrect->setDisabled(value); //dont allow grouped products to make stock correction
  ui->chIsARaw->setEnabled(!value);
  //update m_pInfo
  m_pInfo.isAGroup = value;
}

void ProductEditor::setIsARaw(bool value)
{
  ui->chIsARaw->setChecked(value);
  ui->chIsARaw->setEnabled(value);
  ui->btnShowGroup->setEnabled(!value);
  //update m_pInfo
  m_pInfo.isARawProduct = value;
}

void ProductEditor::setGroupElements(QString e)
{
  QStringList list = e.split(",");
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ProductInfo pInfo;
  for (int i=0; i<list.count(); ++i) {
    QStringList tmp = list.at(i).split("/");
    if (tmp.count() == 2) { //ok 2 fields
      qulonglong  code  = tmp.at(0).toULongLong();
      double      qty   = tmp.at(1).toDouble();
      pInfo = myDb->getProductInfo(QString::number(code));
      pInfo.qtyOnList = qty;
      
      //Insert it to the hash
      groupInfo.productsList.insert(code, pInfo);
      //update groupInfo
      groupInfo.count += qty;
      groupInfo.cost  += pInfo.cost;
      groupInfo.price += pInfo.price;
      bool yes = false;
      if (pInfo.stockqty >= qty ) yes = true;
      groupInfo.isAvailable = (groupInfo.isAvailable && yes );
      //insert it to the groupView
      int rowCount = ui->groupView->rowCount();
      ui->groupView->insertRow(rowCount);
      ui->groupView->setItem(rowCount, 0, new QTableWidgetItem(QString::number(qty)));
      ui->groupView->setItem(rowCount, 1, new QTableWidgetItem(pInfo.desc));
    }
  }
  ui->groupView->resizeRowsToContents();
  ui->groupView->resizeColumnsToContents();
  //update m_pInfo
  m_pInfo.groupElementsStr = e;
  delete myDb;
}

double ProductEditor::getGRoupStockMax()
{
  //get each content stockqty, then get the max of them.
  //maybe in the future, now return 1
  return 1; // stockqty on grouped products will not be stored, only check for contents availability
}

void ProductEditor::populateProvidersCombo()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ui->providerCombo->clear();
  ui->providerCombo->addItems(myDb->getProvidersList());
//kh 2012-07-17
  QAbstractItemModel *model =ui->providerCombo->model();
  model->sort(0);
}

void ProductEditor::populateBrandsCombo()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ui->brandCombo->clear();
  ui->brandCombo->addItems(myDb->getBrandsList());
//kh 2012-07-17
  QAbstractItemModel *model =ui->brandCombo->model();
  model->sort(0);
}

void ProductEditor::populateTaxModelsCombo()
{
  Azahar *myDb = new Azahar;
  myDb->setDatabase(db);
  ui->taxModelCombo->clear();
  ui->taxModelCombo->addItems(myDb->getTaxModelsList());
}

#include "producteditor.moc"
