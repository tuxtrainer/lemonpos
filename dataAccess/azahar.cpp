/**************************************************************************
*   Copyright © 2008-2010 by Miguel Chavez Gamboa                         *
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

#include <QWidget>
#include <QByteArray>
#include "azahar.h"
#include <klocale.h>

#include "azahar_debug.h"

Azahar::Azahar(QWidget * parent): QObject(parent)
{
  errorStr = "";
  m_mainClient = "undefined";
}

Azahar::~Azahar()
{
  //qDebug()<<"*** AZAHAR DESTROYED ***";
}

void Azahar::initDatabase(QString user, QString server, QString password, QString dbname)
{
  QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
  db = QSqlDatabase::addDatabase("QMYSQL");
  db.setHostName(server);
  db.setDatabaseName(dbname);
  db.setUserName(user);
  db.setPassword(password);

  if (!db.isOpen()) db.open();
  if (!db.isOpen()) db.open();
}

void Azahar::setDatabase(const QSqlDatabase& database)
{
  db = database;
  if (!db.isOpen()) db.open();
}

bool Azahar::isConnected()
{
  return db.isOpen();
}

void Azahar::setError(QString err)
{
  errorStr = err;
  qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << err ;
}

QString Azahar::lastError()
{
  return errorStr;
}

bool  Azahar::correctStock(qulonglong pcode, double oldStockQty, double newStockQty, const QString &reason )
{ //each correction is an insert to the table.
  bool result1, result2;
  result1 = result2 = false;
  if (!db.isOpen()) db.open();

  //Check if the desired product is a a group.
  if ( getProductInfo(QString::number(pcode)).isAGroup ) return false;

  QSqlQuery query(db);
  QDate date = QDate::currentDate();
  QTime time = QTime::currentTime();
  query_prepare(query,"INSERT INTO stock_corrections (product_id, new_stock_qty, old_stock_qty, reason, date, time) VALUES(:pcode, :newstock, :oldstock, :reason, :date, :time); ");
  query_bindValueDbgOut(query,":pcode", pcode);
  query_bindValueDbgOut(query,":newstock", newStockQty);
  query_bindValueDbgOut(query,":oldstock", oldStockQty);
  query_bindValueDbgOut(query,":reason", reason);
  query_bindValueDbgOut(query,":date", date.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(query,":time", time.toString("hh:mm"));
  if (!query_exec_s(query)) setError(query.lastError().text()); else result1=true;

  //modify stock
  QSqlQuery query2(db);
  query_prepare(query2,"UPDATE products set stockqty=:newstock where code=:pcode;");
  query_bindValueDbgOut(query2,":pcode", pcode);
  query_bindValueDbgOut(query2,":newstock", newStockQty);
  if (!query_exec_s(query2)) setError(query2.lastError().text()); else result2=true;
  return (result1 && result2);
}

double Azahar::getProductStockQty(qulonglong code)
{
  double result=0.0;
  if (db.isOpen()) {
    QString qry = QString("SELECT stockqty from products WHERE code=%1").arg(code);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
    }
    if (query.size() == -1)
      setError(i18n("Error serching product id %1, Rows affected: %2", code,query.size()));
    else {
      while (query.next()) {
        int fieldStock = query.record().indexOf("stockqty");
        result = query.value(fieldStock).toDouble(); //return stock
      }
    }
  }
  return result;
}

qulonglong Azahar::getProductOfferCode(qulonglong code)
{
  qulonglong result=0;
  if (db.isOpen()) {
    QString qry = QString("SELECT id,product_id from offers WHERE product_id=%1").arg(code);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
    }
    if (query.size() == -1)
      setError(i18n("Error serching offer id %1, Rows affected: %2", code,query.size()));
    else {
      while (query.next()) {
        int fieldId = query.record().indexOf("id");
        result = query.value(fieldId).toULongLong(); //return offer id
      }
    }
  }
  return result;
}

ProductInfo Azahar::findProductInfo(QString code)
{
  ProductInfo info;
  info.code=0;
  info.desc="Ninguno";
  info.price=0;
  info.disc=0;
  info.cost=0;
  info.lastProviderId = 0;
  info.category=0;
  info.taxmodelid=0;
  info.units=0;
  info.points=0;
  info.row=-1;
  info.qtyOnList=-1;
  info.purchaseQty=-1;
  info.discpercentage=0;
  info.validDiscount=false;
  info.alphaCode = "-NA-";
  info.lastProviderId = 0;
  info.isAGroup = false;
  info.isARawProduct = false;
  info.isIndividual = false;
  QString rawCondition;

  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    //QString qry = QString("SELECT * from products where code=%1").arg(code);
    QString qry = QString("SELECT  P.code as CODE, \
    P.alphacode as ALPHACODE, \
    P.name as NAME ,\
    P.price as PRICE, \
    P.cost as COST ,\
    P.stockqty as STOCKQTY, \
    P.units as UNITS, \
    P.points as POINTS, \
    P.photo as PHOTO, \
    P.category as CATID, \
    P.brandid as BRANDID, \
    P.lastproviderid as PROVIDERID, \
    P.taxmodel as TAXMODELID, \
    P.isARawProduct as ISRAW, \
    P.isIndividual as ISINDIVIDUAL, \
    P.isAGroup as ISGROUP, \
    P.groupElements as GE, \
    P.soldunits as SOLDUNITS, \
    U.text as UNITSDESC, \
    C.text as CATEGORY, \
    B.bname  as BRAND, \
    PR.name as LASTPROVIDER ,\
    T.tname as TAXNAME, \
    T.elementsid as TAXELEM \
    FROM products AS P, brands as B, taxmodels as T, providers as PR, categories as C, measures as U \
    WHERE B.brandid=P.brandid AND PR.id=P.lastproviderid AND T.modelid=P.taxmodel \
    AND C.catid=P.category AND U.id=P.units\
    AND (CODE='%1' or ALPHACODE='%1');").arg(code);
    
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
      setError(i18n("Error getting product information for code %1, Rows affected: %2", code,query.size()));
    }
    else {
      while (query.next()) {
        int fieldCODE = query.record().indexOf("CODE");
        int fieldDesc = query.record().indexOf("NAME");
        int fieldPrice= query.record().indexOf("PRICE");
        int fieldPhoto= query.record().indexOf("PHOTO");
        int fieldCost= query.record().indexOf("COST");
        int fieldUnits= query.record().indexOf("UNITS");
        int fieldUnitsDESC= query.record().indexOf("UNITSDESC");
        //int fieldTaxName= query.record().indexOf("TAXNAME");
        int fieldTaxModelId= query.record().indexOf("TAXMODELID");
        //int fieldCategoryName= query.record().indexOf("CATEGORY");
        int fieldCategoryId= query.record().indexOf("CATID");
        int fieldPoints= query.record().indexOf("POINTS");
        //int fieldLastProviderName = query.record().indexOf("LASTPROVIDER");
        int fieldLastProviderId = query.record().indexOf("PROVIDERID");
        int fieldAlphaCode = query.record().indexOf("ALPHACODE");
        //int fieldBrandName = query.record().indexOf("BRAND");
        int fieldBrandId = query.record().indexOf("BRANDID");
        int fieldTaxElem = query.record().indexOf("TAXELEM");
        int fieldStock= query.record().indexOf("STOCKQTY");
        int fieldIsGroup = query.record().indexOf("ISGROUP");
        int fieldIsRaw = query.record().indexOf("ISRAW");
        int fieldIsIndividual = query.record().indexOf("ISINDIVIDUAL");
        int fieldGE = query.record().indexOf("GE");
        int fieldSoldU = query.record().indexOf("SOLDUNITS");

        info.code     = query.value(fieldCODE).toULongLong();
        info.desc     = query.value(fieldDesc).toString();
        info.price    = query.value(fieldPrice).toDouble();
        info.photo    = query.value(fieldPhoto).toByteArray();
        info.cost     = query.value(fieldCost).toDouble();
        info.units    = query.value(fieldUnits).toInt();
        info.unitStr  = query.value(fieldUnitsDESC).toString();
        info.category = query.value(fieldCategoryId).toInt();
        info.points   = query.value(fieldPoints).toInt();
        info.lastProviderId = query.value(fieldLastProviderId).toULongLong();
        info.alphaCode = query.value(fieldAlphaCode).toString();
        info.brandid    = query.value(fieldBrandId).toULongLong();
        info.taxmodelid = query.value(fieldTaxModelId).toULongLong();
        info.taxElements = query.value(fieldTaxElem).toString();
        info.profit  = info.price - info.cost;
        info.stockqty  = query.value(fieldStock).toDouble();
        info.isAGroup  = query.value(fieldIsGroup).toBool();
        info.isARawProduct = query.value(fieldIsRaw).toBool();
        info.isIndividual = query.value(fieldIsIndividual).toBool();
        info.groupElementsStr = query.value(fieldGE).toString();
        info.soldUnits = query.value(fieldSoldU).toDouble();
      }
      //get missing stuff - tax,offers for the requested product
      if (info.isAGroup) //If its a group, the taxmodel is ignored, the tax will be its elements taxes
        info.tax = getGroupAverageTax(info.code);
      else
        info.tax = getTotalTaxPercent(info.taxElements);
      if (getConfigTaxIsIncludedInPrice()) {
        ///tax is included in price... mexico style.
        double pWOtax = info.price/(1+((info.tax)/100));
        info.totaltax = pWOtax*((info.tax)/100); // in money...
      } else {
        ///tax is not included in price... usa style.
        info.totaltax = info.price*(1+(info.tax/100)); //tax in money
      }
      
     //get discount info... if have one.
     QSqlQuery query2(db);
     if (query_exec(query2,QString("Select * from offers where product_id=%1").arg(info.code) )) {
       QList<double> descuentos; descuentos.clear();
       while (query2.next()) // return the valid discount only (and the greater one if many).
         {
           int fieldDisc = query2.record().indexOf("discount");
           int fieldDateStart = query2.record().indexOf("datestart");
           int fieldDateEnd   = query2.record().indexOf("dateend");
           double descP= query2.value(fieldDisc).toDouble();
           QDate dateStart = query2.value(fieldDateStart).toDate();
           QDate dateEnd   = query2.value(fieldDateEnd).toDate();
           QDate now = QDate::currentDate();
           //See if the offer is in a valid range...
           if ((dateStart<dateEnd) && (dateStart<=now) && (dateEnd>=now)  ) {
             //save all discounts here and decide later to return the bigger valid discount.
             descuentos.append(descP);
           }
         }
         //now which is the bigger valid discount?
         qSort(descuentos.begin(), descuentos.end(), qGreater<int>());
         //qDebug()<<"DESCUENTOS ORDENADOS DE MAYOR A MENOR:"<<descuentos;
         if (!descuentos.isEmpty()) {
           //get the first item, which is the greater one.
           info.validDiscount = true;
           info.discpercentage = descuentos.first();
           info.disc = round((info.discpercentage/100 * info.price)*100)/100;
         } else {info.disc = 0; info.validDiscount =false;}
     }
    }
  }
  return info;
}


ProductInfo Azahar::getProductInfo(QString code)
{
  ProductInfo PrInf;
  PrInf.code=0;
  PrInf.desc="Ninguno";
  PrInf.price=0;
  PrInf.disc=0;
  PrInf.cost=0;
  PrInf.lastProviderId = 0;
  PrInf.category=0;
  PrInf.taxmodelid=0;
  PrInf.units=0;
  PrInf.points=0;
  PrInf.row=-1;
  PrInf.qtyOnList=-1;
  PrInf.purchaseQty=-1;
  PrInf.discpercentage=0;
  PrInf.validDiscount=false;
  PrInf.alphaCode = "-NA-";
  PrInf.lastProviderId = 0;
  PrInf.isAGroup = false;
  PrInf.isARawProduct = false;
  PrInf.isIndividual = false;
  QString rawCondition;

  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    //QString qry = QString("SELECT * from products where code=%1").arg(code);
    QString qry = QString("SELECT  P.code as CODE, \
    P.alphacode as ALPHACODE, \
    P.name as NAME ,\
    P.price as PRICE, \
    P.cost as COST ,\
    P.stockqty as STOCKQTY, \
    P.units as UNITS, \
    P.points as POINTS, \
    P.photo as PHOTO, \
    P.category as CATID, \
    P.brandid as BRANDID, \
    P.lastproviderid as PROVIDERID, \
    P.taxmodel as TAXMODELID, \
    P.isARawProduct as ISRAW, \
    P.isIndividual as ISINDIVIDUAL, \
    P.isAGroup as ISGROUP, \
    P.groupElements as GE, \
    P.soldunits as SOLDUNITS, \
    U.text as UNITSDESC, \
    C.text as CATEGORY, \
    B.bname  as BRAND, \
    PR.name as LASTPROVIDER ,\
    T.tname as TAXNAME, \
    T.elementsid as TAXELEM \
    FROM products AS P, brands as B, taxmodels as T, providers as PR, categories as C, measures as U \
    WHERE B.brandid=P.brandid AND PR.id=P.lastproviderid AND T.modelid=P.taxmodel \
    AND C.catid=P.category AND U.id=P.units\
    AND (CODE='%1');").arg(code);
    
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
      setError(i18n("Error getting product information for code %1, Rows affected: %2", code,query.size()));
    }
    else {
      while (query.next()) {
        int fieldCODE = query.record().indexOf("CODE");
        int fieldDesc = query.record().indexOf("NAME");
        int fieldPrice= query.record().indexOf("PRICE");
        int fieldPhoto= query.record().indexOf("PHOTO");
        int fieldCost= query.record().indexOf("COST");
        int fieldUnits= query.record().indexOf("UNITS");
        int fieldUnitsDESC= query.record().indexOf("UNITSDESC");
        //int fieldTaxName= query.record().indexOf("TAXNAME");
        int fieldTaxModelId= query.record().indexOf("TAXMODELID");
        //int fieldCategoryName= query.record().indexOf("CATEGORY");
        int fieldCategoryId= query.record().indexOf("CATID");
        int fieldPoints= query.record().indexOf("POINTS");
        //int fieldLastProviderName = query.record().indexOf("LASTPROVIDER");
        int fieldLastProviderId = query.record().indexOf("PROVIDERID");
        int fieldAlphaCode = query.record().indexOf("ALPHACODE");
        //int fieldBrandName = query.record().indexOf("BRAND");
        int fieldBrandId = query.record().indexOf("BRANDID");
        int fieldTaxElem = query.record().indexOf("TAXELEM");
        int fieldStock= query.record().indexOf("STOCKQTY");
        int fieldIsGroup = query.record().indexOf("ISGROUP");
        int fieldIsRaw = query.record().indexOf("ISRAW");
        int fieldIsIndividual = query.record().indexOf("ISINDIVIDUAL");
DBX(fieldIsIndividual);
        int fieldGE = query.record().indexOf("GE");
        int fieldSoldU = query.record().indexOf("SOLDUNITS");

        PrInf.code     = query.value(fieldCODE).toULongLong();
        PrInf.desc     = query.value(fieldDesc).toString();
        PrInf.price    = query.value(fieldPrice).toDouble();
        PrInf.photo    = query.value(fieldPhoto).toByteArray();
        PrInf.cost     = query.value(fieldCost).toDouble();
        PrInf.units    = query.value(fieldUnits).toInt();
        PrInf.unitStr  = query.value(fieldUnitsDESC).toString();
        PrInf.category = query.value(fieldCategoryId).toInt();
        PrInf.points   = query.value(fieldPoints).toInt();
        PrInf.lastProviderId = query.value(fieldLastProviderId).toULongLong();
        PrInf.alphaCode = query.value(fieldAlphaCode).toString();
        PrInf.brandid    = query.value(fieldBrandId).toULongLong();
        PrInf.taxmodelid = query.value(fieldTaxModelId).toULongLong();
        PrInf.taxElements = query.value(fieldTaxElem).toString();
        PrInf.profit  = PrInf.price - PrInf.cost;
        PrInf.stockqty  = query.value(fieldStock).toDouble();
        PrInf.isAGroup  = query.value(fieldIsGroup).toBool();
        PrInf.isARawProduct = query.value(fieldIsRaw).toBool();
        PrInf.isIndividual = query.value(fieldIsIndividual).toBool();
DBX(PrInf.isIndividual);
        PrInf.groupElementsStr = query.value(fieldGE).toString();
        PrInf.soldUnits = query.value(fieldSoldU).toDouble();
      }
      //get missing stuff - tax,offers for the requested product
      if (PrInf.isAGroup) //If its a group, the taxmodel is ignored, the tax will be its elements taxes
        PrInf.tax = getGroupAverageTax(PrInf.code);
      else
        PrInf.tax = getTotalTaxPercent(PrInf.taxElements);
      if (getConfigTaxIsIncludedInPrice()) {
        ///tax is included in price... mexico style.
        double pWOtax = PrInf.price/(1+((PrInf.tax)/100));
        PrInf.totaltax = pWOtax*((PrInf.tax)/100); // in money...
      } else {
        ///tax is not included in price... usa style.
        PrInf.totaltax = PrInf.price*(1+(PrInf.tax/100)); //tax in money
      }
      
     //get discount PrInf... if have one.
     QSqlQuery query2(db);
     if (query_exec(query2,QString("Select * from offers where product_id=%1").arg(PrInf.code) )) {
       QList<double> descuentos; descuentos.clear();
       while (query2.next()) // return the valid discount only (and the greater one if many).
         {
           int fieldDisc = query2.record().indexOf("discount");
           int fieldDateStart = query2.record().indexOf("datestart");
           int fieldDateEnd   = query2.record().indexOf("dateend");
           double descP= query2.value(fieldDisc).toDouble();
           QDate dateStart = query2.value(fieldDateStart).toDate();
           QDate dateEnd   = query2.value(fieldDateEnd).toDate();
           QDate now = QDate::currentDate();
           //See if the offer is in a valid range...
           if ((dateStart<dateEnd) && (dateStart<=now) && (dateEnd>=now)  ) {
             //save all discounts here and decide later to return the bigger valid discount.
             descuentos.append(descP);
           }
         }
         //now which is the bigger valid discount?
         qSort(descuentos.begin(), descuentos.end(), qGreater<int>());
         //qDebug()<<"DESCUENTOS ORDENADOS DE MAYOR A MENOR:"<<descuentos;
         if (!descuentos.isEmpty()) {
           //get the first item, which is the greater one.
           PrInf.validDiscount = true;
           PrInf.discpercentage = descuentos.first();
           PrInf.disc = round((PrInf.discpercentage/100 * PrInf.price)*100)/100;
         } else {PrInf.disc = 0; PrInf.validDiscount =false;}
     }
    }
  }
  return PrInf;
}

// QList<ProductInfo> Azahar::getTransactionGroupsList(qulonglong tid)
// {
//   QList<ProductInfo> list;
//   QStringList groupsList = getTransactionInfo(tid).groups.split(",");
//   foreach(QString ea, groupsList) {
//     qulonglong c = ea.section('/',0,0).toULongLong();
//     double     q = ea.section('/',1,1).toDouble();
//     ProductInfo pi = getProductInfo(c);
//     pi.qtyOnList = q;
//     list.append(pi);
//   }
//   return list;
// }

qulonglong Azahar::getProductNextCode(QString text)
{
  qulonglong mincode
	,maxcode
  	,code=0;
  QString queryString=text;
  QSqlQuery query(db);

  if(queryString.isEmpty()) queryString="1";
  while(!code) {
	mincode=queryString.toLongLong();
	if(mincode) mincode--;
	if(queryString.length()) maxcode=mincode+exp10(queryString.length()-1);
	else maxcode=mincode+10000000L;

	if (query_exec(query,QString("select max(code) as code from products where code < %1 and code > %2 ;").arg(maxcode).arg(mincode))) {
		if (query.next()) { 
			int fieldId   = query.record().indexOf("code");
			code = query.value(fieldId).toULongLong();
			qulonglong usercode=text.toLongLong();
			return code+1;
			}
		queryString=QString("%1%2").arg(mincode).arg("0");
		}
	}
return 0;
}

qulonglong Azahar::getProductCode(QString text)
{
  QSqlQuery query(db);
  qulonglong code=0;
  if (query_exec(query,QString("select code from products where name='%1';").arg(text))) {
    while (query.next()) { 
      int fieldId   = query.record().indexOf("code");
      code = query.value(fieldId).toULongLong();
    }
  }
  else {
    //qDebug()<<"ERROR: "<<query.lastError();
    setError(query.lastError().text());
  }
  return code;
}

QList<qulonglong> Azahar::getProductsCode(QString regExpName)
{
  QList<qulonglong> result;
  result.clear();
  QSqlQuery query(db);
  QString qry;
  if (regExpName == "*") qry = "SELECT code from products;";
  else qry = QString("select code,name from products WHERE `name` REGEXP '%1'").arg(regExpName);
  if (query_exec(query,qry)) {
    while (query.next()) {
      int fieldId   = query.record().indexOf("code");
      qulonglong id = query.value(fieldId).toULongLong();
      result.append(id);
//       qDebug()<<"APPENDING TO PRODUCTS LIST:"<<id;
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

QStringList Azahar::getProductsList()
{
  QStringList result; result.clear();
  QSqlQuery query(db);
  if (query_exec(query,"select name from products;")) {
    while (query.next()) {
      int fieldText = query.record().indexOf("name");
      QString text = query.value(fieldText).toString();
      result.append(text);
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}


bool Azahar::insertProduct(ProductInfo PrInf)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);

  int g=0; int r=0;
  g = PrInf.isAGroup;
  r = PrInf.isARawProduct;
  qDebug()<<"BEFORE FIX  |  isAGroup:"<<g<<" isARaw:"<<r<<" gElements:"<<PrInf.groupElementsStr;

  //some buggy PrInf can cause troubles.
  bool groupValueOK = false;
  bool rawValueOK = false;
  if (PrInf.isAGroup == 0 || PrInf.isAGroup == 1) groupValueOK=true;
  if (PrInf.isARawProduct == 0 || PrInf.isARawProduct == 1) rawValueOK=true;
  if (!groupValueOK) PrInf.isAGroup = 0;
  if (!rawValueOK) PrInf.isARawProduct = 0;

  qDebug()<<"AFTER FIX  |  isAGroup:"<<PrInf.isAGroup<<" isARaw:"<<PrInf.isARawProduct<<" gElements:"<<PrInf.groupElementsStr;
  
  query_prepare(query,"INSERT INTO products (code, name, price, stockqty, cost, soldunits, datelastsold, units, brandid, taxmodel, photo, category, points, alphacode, lastproviderid, isARawProduct, isAGroup, groupElements ) VALUES (:code, :name, :price, :stock, :cost, :soldunits, :lastsold, :units, :brand, :taxmodel, :photo, :category, :points, :alphacode, :lastproviderid, :isARaw, :isAGroup, :groupE);");
  query_bindValueDbgOut(query,":code", PrInf.code);
  query_bindValueDbgOut(query,":name", PrInf.desc);
  query_bindValueDbgOut(query,":price", PrInf.price);
  query_bindValueDbgOut(query,":stock", PrInf.stockqty);
  query_bindValueDbgOut(query,":cost", PrInf.cost);
  query_bindValueDbgOut(query,":soldunits", 0);
  query_bindValueDbgOut(query,":lastsold", "0000-00-00");
  query_bindValueDbgOut(query,":units", PrInf.units);
  query_bindValueDbgOut(query,":brand", PrInf.brandid);
  query_bindValueDbgOut(query,":taxmodel", PrInf.taxmodelid);
  query_bindValueDbgOut(query,":photo", PrInf.photo);
  query_bindValueDbgOut(query,":category", PrInf.category);
  query_bindValueDbgOut(query,":points", PrInf.points);
  query_bindValueDbgOut(query,":alphacode", PrInf.alphaCode);
  query_bindValueDbgOut(query,":lastproviderid", PrInf.lastProviderId);
  query_bindValueDbgOut(query,":isAGroup", PrInf.isAGroup);
  query_bindValueDbgOut(query,":isARaw", PrInf.isARawProduct);
  query_bindValueDbgOut(query,":groupE", PrInf.groupElementsStr);

  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
  //NOTE: Is it necesary to check if there was an offer for this product code? and delete it.
}


//NOTE: not updated: soldunits, datelastsold
bool Azahar::updateProduct(ProductInfo PrInf, qulonglong oldcode)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  QSqlQuery query_ti(db);
  QSqlQuery query_st(db);

  //some buggy info can cause troubles.
  bool groupValueOK = false;
  bool rawValueOK = false;
  if (PrInf.isAGroup == 0 || PrInf.isAGroup == 1) groupValueOK=true;
  if (PrInf.isARawProduct == 0 || PrInf.isARawProduct == 1) rawValueOK=true;
  if (!groupValueOK) PrInf.isAGroup = 0;
  if (!rawValueOK) PrInf.isARawProduct = 0;
  query_prepare(query,"UPDATE products SET \
  code=:newcode, \
  name=:name, \
  price=:price, \
  cost=:cost, \
  stockqty=:stock, \
  brandid=:brand, \
  units=:measure, \
  taxmodel=:taxmodel, \
  photo=:photo, \
  category=:category, \
  points=:points, \
  alphacode=:alphacode, \
  lastproviderid=:lastproviderid, \
  isARawProduct=:isRaw, \
  isIndividual=:isIndividual, \
  isAGroup=:isGroup, \
  groupElements=:ge \
  WHERE code=:id;");
  query_bindValueDbgOut(query,":newcode", PrInf.code);
  query_bindValueDbgOut(query,":name", PrInf.desc);
  query_bindValueDbgOut(query,":price", PrInf.price);
  query_bindValueDbgOut(query,":stock", PrInf.stockqty);
  query_bindValueDbgOut(query,":cost", PrInf.cost);
  query_bindValueDbgOut(query,":measure", PrInf.units);
  query_bindValueDbgOut(query,":taxmodel", PrInf.taxmodelid);
  query_bindValueDbgOut(query,":brand", PrInf.brandid);
  query_bindValueDbgOut(query,":photo", PrInf.photo);
  query_bindValueDbgOut(query,":category", PrInf.category);
  query_bindValueDbgOut(query,":points", PrInf.points);
  query_bindValueDbgOut(query,":id", oldcode);
  query_bindValueDbgOut(query,":alphacode", PrInf.alphaCode);
  query_bindValueDbgOut(query,":lastproviderid", PrInf.lastProviderId);
  query_bindValueDbgOut(query,":isGroup", PrInf.isAGroup);
  query_bindValueDbgOut(query,":isRaw", PrInf.isARawProduct);
  query_bindValueDbgOut(query,":isIndividual", PrInf.isIndividual);
  query_bindValueDbgOut(query,":ge", PrInf.groupElementsStr);

  query_prepare(query_ti,"UPDATE transactionitems SET product_id = :newcode WHERE product_id = :id;");
  query_bindValueDbgOut(query_ti,":id", oldcode);
  query_bindValueDbgOut(query_ti,":newcode", PrInf.code);

  query_prepare(query_st,"UPDATE stock_corrections SET product_id = :newcode WHERE product_id = :id;");
  query_bindValueDbgOut(query_st,":id", oldcode);
  query_bindValueDbgOut(query_st,":newcode", PrInf.code);

  db.transaction(); //kh 17.7.
  if (!query_exec_s(query) || !query_exec_s(query_ti)  || !query_exec_s(query_st)) {
    setError(query.lastError().text());
    db.rollback(); //kh 17.7.
  } else {
    result=true;
    db.commit(); //kh 17.7.
    }

  return result;
}

bool Azahar::decrementProductStock(qulonglong code, double qty, QDate date)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  double qtys=qty;
  query_prepare(query,"UPDATE products SET datelastsold=:dateSold , stockqty=stockqty-:qty , soldunits=soldunits+:qtys WHERE code=:id");
  query_bindValueDbgOut(query,":id", code);
  query_bindValueDbgOut(query,":qty", qty);
  query_bindValueDbgOut(query,":qtys", qtys);
  query_bindValueDbgOut(query,":dateSold", date.toString("yyyy-MM-dd"));
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  //qDebug()<<"Rows:"<<query.numRowsAffected();
  return result;
}

bool Azahar::decrementGroupStock(qulonglong code, double qty, QDate date)
{
  bool result = true;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);

  ProductInfo PrInf = getProductInfo(QString::number(code));
  QStringList lelem = PrInf.groupElementsStr.split(",");
  foreach(QString ea, lelem) {
    qulonglong c = ea.section('/',0,0).toULongLong();
    double     q = ea.section('/',1,1).toDouble();
    ProductInfo pi = getProductInfo(QString::number(c));
    //FOR EACH ELEMENT, DECREMENT PRODUCT STOCK
    result = result && decrementProductStock(c, q*qty, date);
  }
  
  return result;
}

///NOTE or FIXME: increment is used only when returning a product? or also on purchases??
///               Because this method is decrementing soldunits... not used in lemonview.cpp nor squeezeview !!!!!
bool Azahar::incrementProductStock(qulonglong code, double qty)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  double qtys=qty;
  query_prepare(query,"UPDATE products SET stockqty=stockqty+:qty , soldunits=soldunits-:qtys WHERE code=:id");
  query_bindValueDbgOut(query,":id", code);
  query_bindValueDbgOut(query,":qty", qty);
  query_bindValueDbgOut(query,":qtys", qtys);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  //qDebug()<<"Increment Stock - Rows:"<<query.numRowsAffected();
  return result;
}

bool Azahar::incrementGroupStock(qulonglong code, double qty)
{
  bool result = true;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  
  ProductInfo PrInf = getProductInfo(QString::number(code));
  QStringList lelem = PrInf.groupElementsStr.split(",");
  foreach(QString ea, lelem) {
    qulonglong c = ea.section('/',0,0).toULongLong();
    double     q = ea.section('/',1,1).toDouble();
    ProductInfo pi = getProductInfo(QString::number(c));
    //FOR EACH ELEMENT, DECREMENT PRODUCT STOCK
    result = result && incrementProductStock(c, q*qty);
  }
  
  return result;
}


bool Azahar::deleteProduct(qulonglong code)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query = QString("DELETE FROM products WHERE code=%1").arg(code);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}

double Azahar::getProductDiscount(qulonglong code)
{
  double result = 0.0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query2(db);
    QString qry = QString("SELECT * FROM offers WHERE product_id=%1").arg(code);
    if (!query_exec(query2,qry)) { setError(query2.lastError().text()); }
    else {
      QList<double> descuentos; descuentos.clear();
      while (query2.next())
      {
        int fieldDisc = query2.record().indexOf("discount");
        int fieldDateStart = query2.record().indexOf("datestart");
        int fieldDateEnd   = query2.record().indexOf("dateend");
        double descP= query2.value(fieldDisc).toDouble();
        QDate dateStart = query2.value(fieldDateStart).toDate();
        QDate dateEnd   = query2.value(fieldDateEnd).toDate();
        QDate now = QDate::currentDate();
        //See if the offer is in a valid range...
        if ((dateStart<dateEnd) && (dateStart<=now) && (dateEnd>=now)  ) {
          //save all discounts here and decide later to return the bigger valid discount.
          descuentos.append(descP);
        }
      }
      //now which is the bigger valid discount?
      qSort(descuentos.begin(), descuentos.end(), qGreater<int>());
      if (!descuentos.isEmpty()) {
        //get the first item, which is the greater one.
        result = descuentos.first();
      } else result = 0;
    }
  } else { setError(db.lastError().text()); }
  return result;
}

QList<pieProdInfo> Azahar::getTop5SoldProducts()
{
  QList<pieProdInfo> products; products.clear();
  pieProdInfo ppi;
  QSqlQuery query(db);
  if (query_exec(query,"SELECT name,soldunits,units,code FROM products WHERE (soldunits>0 AND isARawProduct=false) ORDER BY soldunits DESC LIMIT 5")) {
    while (query.next()) {
      int fieldName  = query.record().indexOf("name");
      int fieldUnits = query.record().indexOf("units");
      int fieldSoldU = query.record().indexOf("soldunits");
      int fieldCode  = query.record().indexOf("code");
      int unit       = query.value(fieldUnits).toInt();
      ppi.name    = query.value(fieldName).toString();
      ppi.count   = query.value(fieldSoldU).toDouble();
      ppi.unitStr = getMeasureStr(unit);
      ppi.code    = query.value(fieldCode).toULongLong();
      products.append(ppi);
    }
  }
  else {
    setError(query.lastError().text());
  }
  return products;
}

double Azahar::getTopFiveMaximum()
{
  double result = 0;
  QSqlQuery query(db);
  if (query_exec(query,"SELECT soldunits FROM products WHERE (soldunits>0 AND isARawProduct=false) ORDER BY soldunits DESC LIMIT 5")) {
    if (query.first()) {
      int fieldSoldU = query.record().indexOf("soldunits");
      result   = query.value(fieldSoldU).toDouble();
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

double Azahar::getAlmostSoldOutMaximum(int max)
{
double result = 0;
  QSqlQuery query(db);
  if (query_exec(query,QString("SELECT stockqty FROM products WHERE (isARawProduct=false  AND isAGroup=false AND stockqty<=%1) ORDER BY stockqty DESC LIMIT 5").arg(max))) {
    if (query.first()) {
      int fieldSoldU = query.record().indexOf("stockqty");
      result   = query.value(fieldSoldU).toDouble();
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

QList<pieProdInfo> Azahar::getAlmostSoldOutProducts(int min, int max)
{
  QList<pieProdInfo> products; products.clear();
  pieProdInfo ppi;
  QSqlQuery query(db);
  //NOTE: Check lower limit for the soldunits range...
  query_prepare(query,"SELECT name,stockqty,units,code FROM products WHERE (isARawProduct=false AND isAGroup=false AND stockqty<=:maxV) ORDER BY stockqty ASC LIMIT 5");
  query_bindValueDbgOut(query,":maxV", max);
//   query_bindValueDbgOut(query,":minV", min);
  if (query_exec_s(query)) {
    while (query.next()) {
      int fieldName  = query.record().indexOf("name");
      int fieldUnits = query.record().indexOf("units");
      int fieldStock = query.record().indexOf("stockqty");
      int fieldCode  = query.record().indexOf("code");
      int unit       = query.value(fieldUnits).toInt();
      ppi.name    = query.value(fieldName).toString();
      ppi.count   = query.value(fieldStock).toDouble();
      ppi.code    = query.value(fieldCode).toULongLong();
      ppi.unitStr = getMeasureStr(unit);
      products.append(ppi);
    }
  }
  else {
    setError(query.lastError().text());
    qDebug()<<lastError();
  }
  return products;
}

//returns soldout products only if the product is NOT a group.
QList<ProductInfo> Azahar::getSoldOutProducts()
{
  QList<ProductInfo> products; products.clear();
  ProductInfo PrInf;
  QSqlQuery query(db);
  query_prepare(query,"SELECT code FROM products WHERE stockqty=0 and isAgroup=false ORDER BY code ASC;");
  if (query_exec_s(query)) {
    while (query.next()) {
      int fieldCode  = query.record().indexOf("code");
      PrInf = getProductInfo(query.value(fieldCode).toString());
      products.append(PrInf);
    }
  }
  else {
    setError(query.lastError().text());
    qDebug()<<lastError();
  }
  return products;
}

//also discard group products.
QList<ProductInfo> Azahar::getLowStockProducts(double min)
{
  QList<ProductInfo> products; products.clear();
  ProductInfo PrInf;
  QSqlQuery query(db);
  query_prepare(query,"SELECT code FROM products WHERE (stockqty<=:minV and stockqty>1 and isAGroup=false) ORDER BY code,stockqty ASC;");
  query_bindValueDbgOut(query,":minV", min);
  if (query_exec_s(query)) {
    while (query.next()) {
      int fieldCode  = query.record().indexOf("code");
      PrInf = getProductInfo(query.value(fieldCode).toString());
      products.append(PrInf);
    }
  }
  else {
    setError(query.lastError().text());
    qDebug()<<lastError();
  }
  return products;
}

qulonglong Azahar::getLastProviderId(qulonglong code)
{
  qulonglong result = 0;
  QSqlQuery query(db);
  query_prepare(query,"SELECT lastproviderid FROM products WHERE code=:code");
  query_bindValueDbgOut(query,":code", code);
  if (query_exec_s(query)) {
    while (query.next()) {
      int fieldProv  = query.record().indexOf("lastproviderid");
      result         = query.value(fieldProv).toULongLong();
    }
  }
  else {
    setError(query.lastError().text());
    qDebug()<<lastError();
  }
  return result;
}

bool Azahar::updateProductLastProviderId(qulonglong code, qulonglong provId)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE products SET lastproviderid=:provid WHERE code=:id");
  query_bindValueDbgOut(query,":id", code);
  query_bindValueDbgOut(query,":provid", provId);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  qDebug()<<"Rows Affected:"<<query.numRowsAffected();
  return result;
}

QList<ProductInfo> Azahar::getGroupProductsList(qulonglong id)
{
  qDebug()<<"getGroupProductsList...";
  QList<ProductInfo> pList;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QString ge = getProductGroupElementsStr(id); //DONOT USE getProductInfo... this will cause an infinite loop because at that method this method is called trough getGroupAverageTax
    qDebug()<<"elements:"<<ge;
    if (ge.isEmpty()) return pList;
    QStringList pq = ge.split(",");
    foreach(QString str, pq) {
      qulonglong c = str.section('/',0,0).toULongLong();
      double     q = str.section('/',1,1).toDouble();
      //get PrInf
      ProductInfo pi = getProductInfo(QString::number(c));
      pi.qtyOnList = q;
      pList.append(pi);
      qDebug()<<" code:"<<c<<" qty:"<<q;
    }
  }
  return pList;
}

double Azahar::getGroupAverageTax(qulonglong id)
{
  qDebug()<<"Getting averate tax for id:"<<id;
  double result = 0;
  double sum = 0;
  QList<ProductInfo> pList = getGroupProductsList(id);
  foreach( ProductInfo PrInf, pList) {
    //get each element its taxModel/taxElements.
    sum += getTotalTaxPercent(PrInf.taxElements);
  }

  result = sum/pList.count();
  qDebug()<<"Group average tax: "<<result <<" sum:"<<sum<<" count:"<<pList.count();

  return result;
}

QString Azahar::getProductGroupElementsStr(qulonglong id)
{
  QString result;
  if (db.isOpen()) {
    QString qry = QString("SELECT groupElements from products WHERE code=%1").arg(id);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
    }
    if (query.size() == -1)
      setError(i18n("Error serching product id %1, Rows affected: %2", id,query.size()));
    else {
      while (query.next()) {
        int field = query.record().indexOf("groupElements");
        result    = query.value(field).toString();
      }
    }
  }
  return result;
}


//CATEGORIES
bool Azahar::insertCategory(QString text)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"INSERT INTO categories (text) VALUES (:text);");
  query_bindValueDbgOut(query,":text", text);
  if (!query_exec_s(query)) {
    setError(query.lastError().text());
  }
  else result=true;
  
  return result;
}

QHash<QString, int> Azahar::getCategoriesHash()
{
  QHash<QString, int> result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select * from categories;")) {
      while (myQuery.next()) {
        int fieldId   = myQuery.record().indexOf("catid");
        int fieldText = myQuery.record().indexOf("text");
        int id = myQuery.value(fieldId).toInt();
        QString text = myQuery.value(fieldText).toString();
        result.insert(text, id);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QStringList Azahar::getCategoriesList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select text from categories;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("text");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

qulonglong Azahar::getCategoryId(QString texto)
{
  qulonglong result=0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    QString qryStr = QString("SELECT categories.catid FROM categories WHERE text='%1';").arg(texto);
    if (query_exec(myQuery,qryStr) ) {
      while (myQuery.next()) {
        int fieldId   = myQuery.record().indexOf("catid");
        qulonglong id= myQuery.value(fieldId).toULongLong();
        result = id;
      }
    }
    else {
      setError(myQuery.lastError().text());
    }
  }
  return result;
}

QString Azahar::getCategoryStr(qulonglong id)
{
  QString result;
  QSqlQuery query(db);
  QString qstr = QString("select text from categories where catid=%1;").arg(id);
  if (query_exec(query,qstr)) {
    while (query.next()) {
      int fieldText = query.record().indexOf("text");
      result = query.value(fieldText).toString();
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

bool Azahar::deleteCategory(qulonglong id)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query = QString("DELETE FROM categories WHERE catid=%1").arg(id);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}

//MEASURES
bool Azahar::insertMeasure(QString text)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"INSERT INTO measures (text) VALUES (:text);");
  query_bindValueDbgOut(query,":text", text);
  if (!query_exec_s(query)) {
    setError(query.lastError().text());
  }
  else result=true;
  
  return result;
}

qulonglong Azahar::getMeasureId(QString texto)
{
  qulonglong result=0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    QString qryStr = QString("select measures.id from measures where text='%1';").arg(texto);
    if (query_exec(myQuery,qryStr) ) {
      while (myQuery.next()) {
        int fieldId   = myQuery.record().indexOf("id");
        qulonglong id = myQuery.value(fieldId).toULongLong();
        result = id;
      }
    }
    else {
      setError(myQuery.lastError().text());
    }
  }
  return result;
}

QString Azahar::getMeasureStr(qulonglong id)
{
  QString result;
  QSqlQuery query(db);
  QString qstr = QString("select text from measures where measures.id=%1;").arg(id);
  if (query_exec(query,qstr)) {
    while (query.next()) {
      int fieldText = query.record().indexOf("text");
      result = query.value(fieldText).toString();
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

QStringList Azahar::getMeasuresList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select text from measures;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("text");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

bool Azahar::deleteMeasure(qulonglong id)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query = QString("DELETE FROM measures WHERE id=%1").arg(id);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}

//BRANDS
QStringList Azahar::getBrandsList()
{
  qDebug()<<"getBrandsList...";
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select bname from brands;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("bname");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

//PROVIDERS
QStringList Azahar::getProvidersList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select name from providers;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("name");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QHash<QString, qulonglong> Azahar::getProvidersHash()
{
  QHash<QString, qulonglong> result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select * from providers;")) {
      while (myQuery.next()) {
        int fieldId   = myQuery.record().indexOf("id");
        int fieldText = myQuery.record().indexOf("name");
        int id = myQuery.value(fieldId).toInt();
        QString text = myQuery.value(fieldText).toString();
        result.insert(text, id);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QString Azahar::getProviderName(const qulonglong &id)
{
  QString result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select name from providers where id=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("name");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

qulonglong Azahar::getProviderId(const QString &name)
{
  qulonglong result = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select id from providers where name='%1';").arg(name))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("id");
        result = myQuery.value(fieldText).toULongLong();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

ProviderInfo Azahar::getProviderInfo(qulonglong id)
{
  ProviderInfo ProvInf;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery qC(db);
    if (query_exec(qC,"select * from providers;")) {
      while (qC.next()) {
        int fieldId     = qC.record().indexOf("id");
        int fieldName   = qC.record().indexOf("name");
        int fieldPhone  = qC.record().indexOf("phone");
        int fieldCell   = qC.record().indexOf("cellphone");
        int fieldAdd    = qC.record().indexOf("address");
        if (qC.value(fieldId).toUInt() == id) {
          ProvInf.id       = qC.value(fieldId).toULongLong();
          ProvInf.name     = qC.value(fieldName).toString();
          ProvInf.phone    = qC.value(fieldPhone).toString();
          ProvInf.cell     = qC.value(fieldCell).toString();
          ProvInf.address  = qC.value(fieldAdd).toString();
          break;
        }
      }
    }
    else {
      qDebug()<<"ERROR: "<<qC.lastError();
    }
  }
  return ProvInf;
}

bool Azahar::insertProvider(ProviderInfo ProvInf)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO providers (name, address, phone, cellphone) VALUES(:name, :address, :phone, :cell)");
    query_bindValueDbgOut(query,":name", ProvInf.name);
    query_bindValueDbgOut(query,":address", ProvInf.address);
    query_bindValueDbgOut(query,":phone", ProvInf.phone);
    query_bindValueDbgOut(query,":cell", ProvInf.cell);
    if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;
  }
  return result;
}

bool Azahar::updateProvider(ProviderInfo ProvInf)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE providers SET name=:name, address=:address, phone=:phone, cellphone=:cell WHERE id=:id;");
  query_bindValueDbgOut(query,":id", ProvInf.id);
  query_bindValueDbgOut(query,":name", ProvInf.name);
  query_bindValueDbgOut(query,":address", ProvInf.address);
  query_bindValueDbgOut(query,":phone", ProvInf.phone);
  query_bindValueDbgOut(query,":cell", ProvInf.cell);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;
  
  return result;
}

bool Azahar::deleteProvider(qulonglong pid)
{
  bool result=false;
  if (db.isOpen()) {
    QString qry = QString("DELETE from providers WHERE id=%1").arg(pid);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
    }
    if (query.numRowsAffected() == 1) result = true;
    else setError(i18n("Error deleting provider  id %1, Rows affected: %2", pid,query.numRowsAffected()));
  }
  return result;
}

bool Azahar::deleteProductProvider(qulonglong id)
{
  bool result=false;
  if (db.isOpen()) {
    QString qry = QString("DELETE from products_providers WHERE id=%1").arg(id);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
    }
    if (query.numRowsAffected() == 1) result = true;
    else setError(i18n("Error deleting product_provider  id %1, Rows affected: %2", id,query.numRowsAffected()));
  }
  return result;
}

bool Azahar::providerHasProduct(qulonglong pid, qulonglong code)
{
  bool result = false;
  int count=0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery qC(db);
    if (query_exec(qC,QString("select * from products_providers where provider_id=%1 and product_id=%2;").arg(pid).arg(code))) {
      while (qC.next()) {
        int fieldId     = qC.record().indexOf("id");
        //int fieldName   = qC.record().indexOf("product_id");
        //int fieldPhone  = qC.record().indexOf("provider_id");
        qulonglong id = qC.value(fieldId).toULongLong();
        qDebug()<<"Product id "<<id<<" found";
        count++;
      }
    }
    else {
      qDebug()<<"ERROR: "<<qC.lastError();
    }
  }
  if (count>0) result=true;
  return result;
}

bool Azahar::addProductToProvider(ProductProviderInfo ppi)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO products_providers (provider_id, product_id, price) VALUES(:provid, :prodid, :price)");
    query_bindValueDbgOut(query,":provid", ppi.provId);
    query_bindValueDbgOut(query,":prodid", ppi.prodId);
    query_bindValueDbgOut(query,":price", ppi.price);
    if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;
  }
  return result;
}

// TAX MODELS
double Azahar::getTotalTaxPercent(const QString& elementsid)
{
  double result=0;
  //first parse string to get a list of tax elementids
  QStringList eList = elementsid.split(",");

  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    for (int i = 0; i < eList.size(); ++i) {
      //get amount for each element
      if (query_exec(myQuery,QString("select amount from taxelements where elementid=%1;").arg(eList.at(i).toULongLong()))) {
        while (myQuery.next()) {
          int fieldAmnt = myQuery.record().indexOf("amount");
          double amount = myQuery.value(fieldAmnt).toDouble();
          result += amount;
          qDebug()<<"Sum of taxes ["<<eList.size()<<" elements, i:"<<i<<"] total = "<<result;
        }
      }
      else {
        qDebug()<<"ERROR: "<<myQuery.lastError();
      }
    } //for each one
  }
  return result;
}

TaxModelInfo Azahar::getTaxModelInfo(const qulonglong id)
{
  TaxModelInfo result;
  
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select * from taxmodels where modelid=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldName = myQuery.record().indexOf("tname");
        QString name  = myQuery.value(fieldName).toString();
        int fieldAppw = myQuery.record().indexOf("appway");
        QString appw  = myQuery.value(fieldAppw).toString();
        int fieldElem = myQuery.record().indexOf("elementsid");
        QString elem  = myQuery.value(fieldElem).toString();
        
        result.id     = id;
        result.name   = name;
        result.appway = appw;
        result.elements = elem;
        result.taxAmount =0; //not yet calculated!!!
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QString Azahar::getTaxModelElements(const qulonglong id)
{
  QString result="";
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select elementsid from taxmodels where modelid=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldElem = myQuery.record().indexOf("elementsid");
        result = myQuery.value(fieldElem).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

qulonglong Azahar::getTaxModelId(const QString &text)
{
  qulonglong result = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select modelid from taxmodels where tname='%1';").arg(text))) {
      while (myQuery.next()) {
        int fieldModel = myQuery.record().indexOf("modelid");
        result = myQuery.value(fieldModel).toULongLong();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QStringList Azahar::getTaxModelsList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select tname from taxmodels;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("tname");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QString Azahar::getTaxModelName(const qulonglong &id)
{
  QString result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select tname from taxmodels where modelid=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("tname");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

//OFFERS
bool Azahar::createOffer(OfferInfo OfInf)
{
  bool result=false;
  QString qryStr;
  QSqlQuery query(db);
  if (!db.isOpen()) db.open();
  //NOTE: Now multiple offers supported (to save offers history)
  qryStr = "INSERT INTO offers (discount, datestart, dateend, product_id) VALUES(:discount, :datestart, :dateend, :code)";
  query_prepare(query,qryStr);
  query_bindValueDbgOut(query,":discount", OfInf.discount );
  query_bindValueDbgOut(query,":datestart", OfInf.dateStart.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(query,":dateend", OfInf.dateEnd.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(query,":code", OfInf.productCode);
  if (query_exec_s(query)) result = true; else setError(query.lastError().text());

  return result;
}

bool Azahar::deleteOffer(qlonglong id)
{
  bool result=false;
  if (db.isOpen()) {
    QString qry = QString("DELETE from offers WHERE offers.id=%1").arg(id);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
    }
    if (query.numRowsAffected() == 1) result = true;
    else setError(i18n("Error deleting offer id %1, Rows affected: %2", id,query.numRowsAffected()));
  }
  return result;
}


QString Azahar::getOffersFilterWithText(QString text)
{
  QStringList codes;
  QString result="";
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery qry(db);
    QString qryStr= QString("SELECT P.code, P.name, O.product_id FROM offers AS O, products AS P WHERE P.code = O.product_id and P.name REGEXP '%1' ").arg(text);
    if (!query_exec(qry,qryStr)) setError(qry.lastError().text());
    else {
      codes.clear();
      while (qry.next()) {
        int fieldId   = qry.record().indexOf("code");
        qulonglong c = qry.value(fieldId).toULongLong();
        codes.append(QString("offers.product_id=%1 ").arg(c));
      }
      result = codes.join(" OR ");
    }
  }
  return result;
}

bool Azahar::moveOffer(qulonglong oldp, qulonglong newp)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery q(db);
  QString qs = QString("UPDATE offers SET product_id=%1 WHERE product_id=%2;").arg(newp).arg(oldp);
  if (!query_exec(q, qs )) setError(q.lastError().text()); else result = true;
  return result;
}


//USERS
bool Azahar::insertUser(UserInfo UInfo)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO users (username, password, salt, name, address, phone, phone_movil, role, photo) VALUES(:uname, :pass, :salt, :name, :address, :phone, :cell, :rol, :photo)");
    query_bindValueDbgOut(query,":photo", UInfo.photo);
    query_bindValueDbgOut(query,":uname", UInfo.username);
    query_bindValueDbgOut(query,":name", UInfo.name);
    query_bindValueDbgOut(query,":address", UInfo.address);
    query_bindValueDbgOut(query,":phone", UInfo.phone);
    query_bindValueDbgOut(query,":cell", UInfo.cell);
    query_bindValueDbgOut(query,":pass", UInfo.password);
    query_bindValueDbgOut(query,":salt", UInfo.salt);
    query_bindValueDbgOut(query,":rol", UInfo.role);
    if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;
    qDebug()<<"USER insert:"<<query.lastError();
    //FIXME: We must see error types, which ones are for duplicate KEYS (codes) to advertise the user.
  }//db open
  return result;
}

QHash<QString,UserInfo> Azahar::getUsersHash()
{
  QHash<QString,UserInfo> result;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);

    QString qry = "SELECT * FROM users;";
    if (query_exec(query,qry)) {
      while (query.next()) {
        int fielduId       = query.record().indexOf("id");
        int fieldUsername  = query.record().indexOf("username");
        int fieldPassword  = query.record().indexOf("password");
        int fieldSalt      = query.record().indexOf("salt");
        int fieldName      = query.record().indexOf("name");
        int fieldRole      = query.record().indexOf("role"); // see role numbers at enums.h
        int fieldPhoto     = query.record().indexOf("photo");
        //more fields, now im not interested in that...
        UserInfo UInfo;
        UInfo.id       = query.value(fielduId).toInt();
        UInfo.username = query.value(fieldUsername).toString();
        UInfo.password = query.value(fieldPassword).toString();
        UInfo.salt     = query.value(fieldSalt).toString();
        UInfo.name     = query.value(fieldName).toString();
        UInfo.photo    = query.value(fieldPhoto).toByteArray();
        UInfo.role     = query.value(fieldRole).toInt();
        result.insert(UInfo.username, UInfo);
        //qDebug()<<"got user:"<<UInfo.username;
      }
    }
    else {
      qDebug()<<"**Error** :"<<query.lastError();
    }
  }
 return result;
}

UserInfo Azahar::getUserInfo(const qulonglong &userid)
{
  UserInfo UInfo;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    QString qry = QString("SELECT * FROM users where id=%1;").arg(userid);
    if (query_exec(query,qry)) {
      while (query.next()) {
        int fielduId       = query.record().indexOf("id");
        int fieldUsername  = query.record().indexOf("username");
        int fieldPassword  = query.record().indexOf("password");
        int fieldSalt      = query.record().indexOf("salt");
        int fieldName      = query.record().indexOf("name");
        int fieldRole      = query.record().indexOf("role"); // see role numbers at enums.h
        int fieldPhoto     = query.record().indexOf("photo");
        //more fields, now im not interested in that...
        UInfo.id       = query.value(fielduId).toInt();
        UInfo.username = query.value(fieldUsername).toString();
        UInfo.password = query.value(fieldPassword).toString();
        UInfo.salt     = query.value(fieldSalt).toString();
        UInfo.name     = query.value(fieldName).toString();
        UInfo.photo    = query.value(fieldPhoto).toByteArray();
        UInfo.role     = query.value(fieldRole).toInt();
        //qDebug()<<"got user:"<<UInfo.username;
      }
    }
    else {
      qDebug()<<"**Error** :"<<query.lastError();
    }
  }
  return UInfo; 
}

bool Azahar::updateUser(UserInfo UInfo)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE users SET photo=:photo, username=:uname, name=:name, address=:address, phone=:phone, phone_movil=:cell, salt=:salt, password=:pass, role=:rol  WHERE id=:code;");
  query_bindValueDbgOut(query,":code", UInfo.id);
  query_bindValueDbgOut(query,":photo", UInfo.photo);
  query_bindValueDbgOut(query,":uname", UInfo.username);
  query_bindValueDbgOut(query,":name", UInfo.name);
  query_bindValueDbgOut(query,":address", UInfo.address);
  query_bindValueDbgOut(query,":phone", UInfo.phone);
  query_bindValueDbgOut(query,":cell", UInfo.cell);
  query_bindValueDbgOut(query,":pass", UInfo.password);
  query_bindValueDbgOut(query,":salt", UInfo.salt);
  query_bindValueDbgOut(query,":rol", UInfo.role);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}

QString Azahar::getUserName(QString username)
{
  QString name = "";
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery queryUname(db);
    QString qry = QString("SELECT name FROM users WHERE username='%1'").arg(username);
    if (!query_exec(queryUname,qry)) { setError(queryUname.lastError().text()); }
    else {
      if (queryUname.isActive() && queryUname.isSelect()) { //qDebug()<<"queryUname select && active.";
        if (queryUname.first()) { //qDebug()<<"queryUname.first()=true";
          name = queryUname.value(0).toString();
        }
      }
    }
  } else { setError(db.lastError().text()); }
  return name;
}

QString Azahar::getUserName(qulonglong id)
{
  QString name = "";
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery queryUname(db);
    QString qry = QString("SELECT name FROM users WHERE users.id=%1").arg(id);
    if (!query_exec(queryUname,qry)) { setError(queryUname.lastError().text()); }
    else {
      if (queryUname.isActive() && queryUname.isSelect()) { //qDebug()<<"queryUname select && active.";
        if (queryUname.first()) { //qDebug()<<"queryUname.first()=true";
          name = queryUname.value(0).toString();
        }
      }
    }
  } else { setError(db.lastError().text()); }
  return name;
}

QStringList Azahar::getUsersList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select name from users;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("name");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

unsigned int Azahar::getUserId(QString uname)
{
  unsigned int iD = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery queryId(db);
    QString qry = QString("SELECT id FROM users WHERE username='%1'").arg(uname);
    if (!query_exec(queryId,qry)) { setError(queryId.lastError().text()); }
    else {
      if (queryId.isActive() && queryId.isSelect()) { //qDebug()<<"queryId select && active.";
        if (queryId.first()) { //qDebug()<<"queryId.first()=true";
        iD = queryId.value(0).toUInt();
        }
      }
    }
  } else { setError(db.lastError().text()); }
  return iD;
}

unsigned int Azahar::getUserIdFromName(QString uname)
{
  unsigned int iD = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery queryId(db);
    QString qry = QString("SELECT id FROM users WHERE name='%1'").arg(uname);
    if (!query_exec(queryId,qry)) { setError(queryId.lastError().text()); }
    else {
      if (queryId.isActive() && queryId.isSelect()) { //qDebug()<<"queryId select && active.";
      if (queryId.first()) { //qDebug()<<"queryId.first()=true";
      iD = queryId.value(0).toUInt();
      }
      }
    }
  } else { setError(db.lastError().text()); }
  return iD;
}

int Azahar::getUserRole(const qulonglong &userid)
{
  int role = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery queryId(db);
    QString qry = QString("SELECT role FROM users WHERE id=%1").arg(userid);
    if (!query_exec(queryId,qry)) { setError(queryId.lastError().text()); }
    else {
      if (queryId.isActive() && queryId.isSelect()) { //qDebug()<<"queryId select && active.";
        if (queryId.first()) { //qDebug()<<"queryId.first()=true";
        role = queryId.value(0).toInt();
        }
      }
    }
  } else { setError(db.lastError().text()); }
  return role;
}

bool Azahar::deleteUser(qulonglong id)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query = QString("DELETE FROM users WHERE id=%1").arg(id);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}


//CLIENTS
qlonglong Azahar::insertClient(ClientInfo info)
{
  qlonglong result = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO clients (firstname, name, address, phone, phone_movil, points, discount, photo, since) VALUES(:firstname, :name, :address, :phone, :cell,:points, :discount, :photo, :since)");
    query_bindValueDbgOut(query,":photo", info.photo);
    query_bindValueDbgOut(query,":points", info.points);
    query_bindValueDbgOut(query,":discount", info.discount);
    query_bindValueDbgOut(query,":firstname", info.fname);
    query_bindValueDbgOut(query,":name", info.name);
    query_bindValueDbgOut(query,":address", info.address);
    query_bindValueDbgOut(query,":phone", info.phone);
    query_bindValueDbgOut(query,":cell", info.cell);
    query_bindValueDbgOut(query,":since", info.since);
    if (!query_exec_s(query)) {
	setError(query.lastError().text());
    } else {
	result=query.lastInsertId().toULongLong();
	}
  }
  return result;
}

bool Azahar::updateClient(ClientInfo info)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
DBX(info.id);
DBX(info.fname);
DBX(info.name);

  query_prepare(query,"UPDATE clients SET photo=:photo, firstname=:fname, name=:name, address=:address, phone=:phone, phone_movil=:cell, points=:points, discount=:disc, since=:since  WHERE id=:code;");
  query_bindValueDbgOut(query,":photo", info.photo);
  query_bindValueDbgOut(query,":name", info.name);
  query_bindValueDbgOut(query,":fname", info.fname);
  query_bindValueDbgOut(query,":address", info.address);
  query_bindValueDbgOut(query,":phone", info.phone);
  query_bindValueDbgOut(query,":cell", info.cell);
  query_bindValueDbgOut(query,":points", info.points);
  query_bindValueDbgOut(query,":disc", info.discount);
  query_bindValueDbgOut(query,":since", info.since);
  query_bindValueDbgOut(query,":code", info.id);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;

  return result;
}

bool Azahar::incrementClientPoints(qulonglong id, qulonglong points)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE clients SET points=points+:points WHERE id=:code;");
  query_bindValueDbgOut(query,":code", id);
  query_bindValueDbgOut(query,":points", points);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;
  return result;
}

bool Azahar::decrementClientPoints(qulonglong id, qulonglong points)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE clients SET points=points-:points WHERE id=:code;");
  query_bindValueDbgOut(query,":code", id);
  query_bindValueDbgOut(query,":points", points);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result = true;
  return result;
}

ClientInfo Azahar::getClientInfo(qulonglong clientId) //NOTE:FALTA PROBAR ESTE METODO.
{
  ClientInfo info;
    info.id=0;

    if (!db.isOpen()) db.open();
    if (!db.isOpen()) return info;

    QSqlQuery qC(db);
    if (query_exec(qC,QString("select * from clients where id = %1;").arg(clientId)) 
	&& qC.next()) {
      int fieldId     = qC.record().indexOf("id");
      int fieldName   = qC.record().indexOf("name");
      int fieldFName   = qC.record().indexOf("firstname");
      int fieldPoints = qC.record().indexOf("points");
      int fieldPhoto  = qC.record().indexOf("photo");
      int fieldDisc   = qC.record().indexOf("discount");
      int fieldSince  = qC.record().indexOf("since");
      int fieldPhone  = qC.record().indexOf("phone");
      int fieldCell   = qC.record().indexOf("phone_movil");
      int fieldAdd    = qC.record().indexOf("address");
      info.id          = qC.value(fieldId).toUInt();
      info.fname       = qC.value(fieldFName).toString();
      info.name       = qC.value(fieldName).toString();
      info.points     = qC.value(fieldPoints).toULongLong();
      info.discount   = qC.value(fieldDisc).toDouble();
      info.photo      = qC.value(fieldPhoto).toByteArray();
      info.since      = qC.value(fieldSince).toDate();
      info.phone      = qC.value(fieldPhone).toString();
      info.cell       = qC.value(fieldCell).toString();
      info.address    = qC.value(fieldAdd).toString();
      }
 return info;
}

QString Azahar::getMainClient()
{
 QString result;
 ClientInfo info;
TRACE ;
  if (m_mainClient == "undefined") {
    if (!db.isOpen()) db.open();
    if (db.isOpen()) {
      QSqlQuery qC(db);
TRACE ;
      if (query_exec(qC,"select * from clients where id=1;")) {
        while (qC.next()) {
          int fieldName   = qC.record().indexOf("name");
          int fieldFName   = qC.record().indexOf("firstname");
DBX(fieldFName);
          info.fname       = qC.value(fieldFName).toString();
          info.name       = qC.value(fieldName).toString();
          m_mainClient = QString("%1,%2").arg(info.name).arg(info.fname) ;
          result = m_mainClient;
        }
      }
      else {
        qDebug()<<"ERROR: "<<qC.lastError();
      }
    }
  } else result = m_mainClient;
return result;
}

QHash<QString, ClientInfo> Azahar::getClientsHash()
{
 QHash<QString, ClientInfo> result;
 ClientInfo info;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery qC(db);
    if (query_exec(qC,"select * from clients;")) {
      while (qC.next()) {
        int fieldId     = qC.record().indexOf("id");
        int fieldFName   = qC.record().indexOf("firstname");
        int fieldName   = qC.record().indexOf("name");
        int fieldPoints = qC.record().indexOf("points");
        int fieldPhoto  = qC.record().indexOf("photo");
        int fieldDisc   = qC.record().indexOf("discount");
        int fieldSince  = qC.record().indexOf("since");
        info.id = qC.value(fieldId).toUInt();
        info.fname       = qC.value(fieldFName).toString();
        info.name       = qC.value(fieldName).toString();
        info.points     = qC.value(fieldPoints).toULongLong();
        info.discount   = qC.value(fieldDisc).toDouble();
        info.photo      = qC.value(fieldPhoto).toByteArray();
        info.since      = qC.value(fieldSince).toDate();
	
	QString clientname="";
	if(!info.fname.isEmpty() && !info.name.isEmpty()) clientname = QString("%1,%2").arg(info.name).arg(info.fname);
	else if(!info.name.isEmpty()) clientname = info.name;
	else if(!info.fname.isEmpty()) clientname = info.fname;
	
        result.insert(clientname, info);
        if (info.id == 1) m_mainClient = clientname;
//kh27.7.        result.insert(info.name, info);
//kh27.7.        if (info.id == 1) m_mainClient = info.name;
      }
    }
    else {
      qDebug()<<"ERROR: "<<qC.lastError();
    }
  }
  return result;
}

QStringList Azahar::getClientsList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
TRACE ;
    if (query_exec(myQuery,"select name from clients;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("name");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

unsigned int Azahar::getClientId(QString uname)
{
  unsigned int iD = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery queryId(db);
TRACE ;
    QString qry = QString("SELECT clients.id FROM clients WHERE clients.name='%1'").arg(uname);
    if (query_exec(queryId,qry)) { setError(queryId.lastError().text()); }
    else {
      if (queryId.isActive() && queryId.isSelect()) { //qDebug()<<"queryId select && active.";
       if (queryId.first()) { //qDebug()<<"queryId.first()=true";
        iD = queryId.value(0).toUInt();
       }
      }
    }
  } else { setError(db.lastError().text()); }
  return iD;
}

bool Azahar::deleteClient(qulonglong id)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query = QString("DELETE FROM clients WHERE id=%1").arg(id);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}


//TRANSACTIONS

TransactionInfo Azahar::getTransactionInfo(qulonglong id)
{
  TransactionInfo info;
  info.id = 0;
  QString qry = QString("SELECT * FROM transactions WHERE id=%1").arg(id);
  QSqlQuery query;
  if (!query_exec(query,qry)) { qDebug()<<query.lastError(); }
  else {
    while (query.next()) {
      int fieldId = query.record().indexOf("id");
      int fieldAmount = query.record().indexOf("amount");
      int fieldDate   = query.record().indexOf("date");
      int fieldTime   = query.record().indexOf("time");
      int fieldPaidWith = query.record().indexOf("paidwith");
      int fieldPayMethod = query.record().indexOf("paymethod");
      int fieldType      = query.record().indexOf("type");
      int fieldChange    = query.record().indexOf("changegiven");
      int fieldState     = query.record().indexOf("state");
      int fieldUserId    = query.record().indexOf("userid");
      int fieldClientId  = query.record().indexOf("clientid");
      int fieldCardNum   = query.record().indexOf("cardnumber");
      int fieldCardAuth  = query.record().indexOf("cardauthnumber");
      int fieldItemCount = query.record().indexOf("itemcount");
      int fieldItemsList = query.record().indexOf("itemsList");
      int fieldDiscount  = query.record().indexOf("disc");
      int fieldDiscMoney = query.record().indexOf("discmoney");
      int fieldPoints    = query.record().indexOf("points");
      int fieldUtility   = query.record().indexOf("profit");
      int fieldTerminal  = query.record().indexOf("terminalnum");
      int fieldTax       = query.record().indexOf("totalTax");
      int fieldinfoText  = query.record().indexOf("infoText");
      int fieldSpecialOrders = query.record().indexOf("specialOrders");
      
      info.id     = query.value(fieldId).toULongLong();
      info.amount = query.value(fieldAmount).toDouble();
      info.date   = query.value(fieldDate).toDate();
      info.time   = query.value(fieldTime).toTime();
      info.paywith= query.value(fieldPaidWith).toDouble();
      info.paymethod = query.value(fieldPayMethod).toInt();
      info.type      = query.value(fieldType).toInt();
      info.changegiven = query.value(fieldChange).toDouble();
      info.state     = query.value(fieldState).toInt();
      info.userid    = query.value(fieldUserId).toULongLong();
      info.clientid  = query.value(fieldClientId).toULongLong();
      info.cardnumber= query.value(fieldCardNum).toString();//.replace(0,15,"***************"); //FIXED: Only save last 4 digits;
      info.cardauthnum=query.value(fieldCardAuth).toString();
      info.itemcount = query.value(fieldItemCount).toInt();
      info.itemlist  = query.value(fieldItemsList).toString();
      info.disc      = query.value(fieldDiscount).toDouble();
      info.discmoney = query.value(fieldDiscMoney).toDouble();
      info.points    = query.value(fieldPoints).toULongLong();
      info.profit    = query.value(fieldUtility).toDouble();
      info.terminalnum=query.value(fieldTerminal).toInt();
      info.totalTax   = query.value(fieldTax).toDouble();
      info.specialOrders = query.value(fieldSpecialOrders).toString();
      info.infoText = query.value(fieldinfoText).toString();
    }
  }
  return info;
}

ProfitRange Azahar::getMonthProfitRange()
{
  QList<TransactionInfo> monthTrans = getMonthTransactionsForPie();
  ProfitRange range;
  QList<double> profitList;
  TransactionInfo info;
  for (int i = 0; i < monthTrans.size(); ++i) {
    info = monthTrans.at(i);
    profitList.append(info.profit);
  }

  if (!profitList.isEmpty()) {
   qSort(profitList.begin(),profitList.end()); //sorting in ascending order (1,2,3..)
   range.min = profitList.first();
   range.max = profitList.last();
  } else {range.min=0.0; range.max=0.0;}

  return range;
}

ProfitRange Azahar::getMonthSalesRange()
{
  QList<TransactionInfo> monthTrans = getMonthTransactionsForPie();
  ProfitRange range;
  QList<double> salesList;
  TransactionInfo info;
  for (int i = 0; i < monthTrans.size(); ++i) {
    info = monthTrans.at(i);
    salesList.append(info.amount);
  }
  
  if (!salesList.isEmpty()) {
    qSort(salesList.begin(),salesList.end()); //sorting in ascending order (1,2,3..)
    range.min = salesList.first();
    range.max = salesList.last();
  } else {range.min=0.0; range.max=0.0;}

  return range;
}

QList<TransactionInfo> Azahar::getMonthTransactionsForPie()
{
  ///just return the amount and the profit.
  QList<TransactionInfo> result;
  TransactionInfo info;
  QSqlQuery qryTrans(db);
  QDate today = QDate::currentDate();
  QDate startDate = QDate(today.year(), today.month(), 1); //get the 1st of the month.
  //NOTE: in the next query, the state and type are hardcoded (not using the enums) because problems when preparing query.
  query_prepare(qryTrans,"SELECT date,SUM(amount),SUM(profit) from transactions where (date BETWEEN :dateSTART AND :dateEND ) AND (type=1) AND (state=2) GROUP BY date ASC;");
  query_bindValueDbgOut(qryTrans,":dateSTART", startDate.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(qryTrans,":dateEND", today.toString("yyyy-MM-dd"));
  //tCompleted=2, tSell=1. With a placeholder, the value is inserted as a string, and cause the query to fail.
  if (!query_exec_s(qryTrans) ) {
    int errNum = qryTrans.lastError().number();
    QSqlError::ErrorType errType = qryTrans.lastError().type();
    QString errStr = qryTrans.lastError().text();
    QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
    setError(details);
  } else {
    while (qryTrans.next()) {
      int fieldAmount = qryTrans.record().indexOf("SUM(amount)");
      int fieldProfit = qryTrans.record().indexOf("SUM(profit)");
      int fieldDate = qryTrans.record().indexOf("date");
      info.amount = qryTrans.value(fieldAmount).toDouble();
      info.profit = qryTrans.value(fieldProfit).toDouble();
      info.date = qryTrans.value(fieldDate).toDate();
      result.append(info);
      //qDebug()<<"APPENDING:"<<info.date<< " Sales:"<<info.amount<<" Profit:"<<info.profit;
    }
    //qDebug()<<"executed query:"<<qryTrans.executedQuery();
    //qDebug()<<"Qry size:"<<qryTrans.size();
    
  }
  return result;
}

QList<TransactionInfo> Azahar::getMonthTransactions()
{
  QList<TransactionInfo> result;
  TransactionInfo info;
  QSqlQuery qryTrans(db);
  QDate today = QDate::currentDate();
  QDate startDate = QDate(today.year(), today.month(), 1); //get the 1st of the month.
  //NOTE: in the next query, the state and type are hardcoded (not using the enums) because problems when preparing query.
  query_prepare(qryTrans,"SELECT id,date from transactions where (date BETWEEN :dateSTART AND :dateEND ) AND (type=1) AND (state=2) ORDER BY date,id ASC;");
  query_bindValueDbgOut(qryTrans,":dateSTART", startDate.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(qryTrans,":dateEND", today.toString("yyyy-MM-dd"));
  //tCompleted=2, tSell=1. With a placeholder, the value is inserted as a string, and cause the query to fail.
  if (!query_exec_s(qryTrans) ) {
    int errNum = qryTrans.lastError().number();
    QSqlError::ErrorType errType = qryTrans.lastError().type();
    QString errStr = qryTrans.lastError().text();
    QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
    setError(details);
  } else {
    while (qryTrans.next()) {
      int fieldId = qryTrans.record().indexOf("id");
      info = getTransactionInfo(qryTrans.value(fieldId).toULongLong());
      result.append(info);
      //qDebug()<<"APPENDING: id:"<<info.id<<" "<<info.date;
    }
    //qDebug()<<"executed query:"<<qryTrans.executedQuery();
    //qDebug()<<"Qry size:"<<qryTrans.size();
    
  }
  return result;
}

QList<TransactionInfo> Azahar::getDayTransactions(int terminal)
{
    QList<TransactionInfo> result;
    TransactionInfo info;
    QSqlQuery qryTrans(db);
    QDate today = QDate::currentDate();
    //NOTE: in the next query, the state and type are hardcoded (not using the enums) because problems when preparing query.
    if(terminal >=0) {
    query_prepare(qryTrans,"SELECT id,time,paidwith,paymethod,amount,profit from transactions where (date = :today) AND (terminalnum=:terminal) AND (type=1) AND (state=2) ORDER BY id ASC;");
	query_bindValueDbgOut(qryTrans,":terminal",terminal);
    } else  {
    query_prepare(qryTrans,"SELECT id,time,paidwith,paymethod,amount,profit from transactions where (date = :today) AND (type=1) AND (state=2) ORDER BY id ASC;");
    }
    query_bindValueDbgOut(qryTrans,":today", today.toString("yyyy-MM-dd"));
    //tCompleted=2, tSell=1. With a placeholder, the value is inserted as a string, and cause the query to fail.
    if (!query_exec_s(qryTrans) ) {
      int errNum = qryTrans.lastError().number();
      QSqlError::ErrorType errType = qryTrans.lastError().type();
      QString errStr = qryTrans.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else {
      while (qryTrans.next()) {
        int fieldAmount = qryTrans.record().indexOf("amount");
        int fieldProfit = qryTrans.record().indexOf("profit");
        info.id = qryTrans.value(qryTrans.record().indexOf("id")).toULongLong();
        info.amount = qryTrans.value(fieldAmount).toDouble();
        info.profit = qryTrans.value(fieldProfit).toDouble();
        info.paymethod = qryTrans.value(qryTrans.record().indexOf("paymethod")).toInt();
        info.paywith = qryTrans.value(qryTrans.record().indexOf("paidwith")).toDouble();
        info.time = qryTrans.value(qryTrans.record().indexOf("time")).toTime();
        info.totalTax = qryTrans.value(qryTrans.record().indexOf("totalTax")).toDouble();
        result.append(info);
        //qDebug()<<"APPENDING:"<<info.id<< " Sales:"<<info.amount<<" Profit:"<<info.profit;
      }
      //qDebug()<<"executed query:"<<qryTrans.executedQuery();
      //qDebug()<<"Qry size:"<<qryTrans.size();

    }
    return result;
}

AmountAndProfitInfo  Azahar::getDaySalesAndProfit(int terminal)
{
    AmountAndProfitInfo result;
    QSqlQuery qryTrans(db);
    QDate today = QDate::currentDate();
    //NOTE: in the next query, the state and type are hardcoded (not using the enums) because problems when preparing query.
    if(terminal >=0) {
        query_prepare(qryTrans,"SELECT SUM(amount),SUM(profit) from transactions where (date = :today) AND (terminalnum=:terminal) AND (type=1) AND (state=2) GROUP BY date ASC;");
    	query_bindValueDbgOut(qryTrans,":terminal", terminal);
    } else {
  	query_prepare(qryTrans,"SELECT SUM(amount),SUM(profit) from transactions where (date = :today) AND (type=1) AND (state=2) GROUP BY date ASC;");
    }
    query_bindValueDbgOut(qryTrans,":today", today.toString("yyyy-MM-dd"));
DBX(terminal);
    //tCompleted=2, tSell=1. With a placeholder, the value is inserted as a string, and cause the query to fail.
    if (!query_exec_s(qryTrans) ) {
TRACE;
      int errNum = qryTrans.lastError().number();
      QSqlError::ErrorType errType = qryTrans.lastError().type();
      QString errStr = qryTrans.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else {
TRACE;
      while (qryTrans.next()) {
        int fieldAmount = qryTrans.record().indexOf("SUM(amount)");
        int fieldProfit = qryTrans.record().indexOf("SUM(profit)");
DBX(fieldAmount);
DBX(fieldProfit);
        result.amount = qryTrans.value(fieldAmount).toDouble();
        result.profit = qryTrans.value(fieldProfit).toDouble();
DBX(result.amount);
DBX(result.profit);
        //qDebug()<<"APPENDING:"<<info.date<< " Sales:"<<info.amount<<" Profit:"<<info.profit;
      }
      //qDebug()<<"executed query:"<<qryTrans.executedQuery();
      //qDebug()<<"Qry size:"<<qryTrans.size();

    }
    return result;
}

//this returns the sales and profit from the 1st day of the month until today
AmountAndProfitInfo  Azahar::getMonthSalesAndProfit()
{
  AmountAndProfitInfo result;
  QSqlQuery qryTrans(db);
  QDate today = QDate::currentDate();
  QDate startDate = QDate(today.year(), today.month(), 1); //get the 1st of the month.
  //NOTE: in the next query, the state and type are hardcoded (not using the enums) because problems when preparing query.
  query_prepare(qryTrans,"SELECT date,SUM(amount),SUM(profit) from transactions where (date BETWEEN :dateSTART AND :dateEND) AND (type=1) AND (state=2) GROUP BY type ASC;"); //group by type is to get the sum of all trans
  query_bindValueDbgOut(qryTrans,":dateSTART", startDate.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(qryTrans,":dateEND", today.toString("yyyy-MM-dd"));
  //tCompleted=2, tSell=1. With a placeholder, the value is inserted as a string, and cause the query to fail.
  if (!query_exec_s(qryTrans) ) {
    int errNum = qryTrans.lastError().number();
    QSqlError::ErrorType errType = qryTrans.lastError().type();
    QString errStr = qryTrans.lastError().text();
    QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
    setError(details);
  } else {
    while (qryTrans.next()) {
      int fieldAmount = qryTrans.record().indexOf("SUM(amount)");
      int fieldProfit = qryTrans.record().indexOf("SUM(profit)");
      result.amount = qryTrans.value(fieldAmount).toDouble();
      result.profit = qryTrans.value(fieldProfit).toDouble();
      //qDebug()<<"APPENDING --  Sales:"<<result.amount<<" Profit:"<<result.profit;
    }
    //qDebug()<<"executed query:"<<qryTrans.executedQuery();
    //qDebug()<<"Qry size:"<<qryTrans.size();
    
  }
  return result;
}

//TRANSACTIONS
qulonglong Azahar::insertTransaction(TransactionInfo info)
{
  qulonglong result=0;
  QSqlQuery query2(db);
  query_prepare(query2,"INSERT INTO transactions (  \
	  clientid\
	, userid\
	, type\
	, amount\
	, date\
	, time\
	, paidwith\
	, paymethod\
	, changegiven\
	, state\
	, cardnumber\
	, itemcount\
	, itemslist\
	, points\
	, discmoney\
	, disc\
	, cardauthnumber\
	, profit\
	, terminalnum\
	, providerid\
	, specialOrders\
	, infoText\
	, balanceId\
	, totalTax\
) VALUES ( \
  	  :clientid\
	, :userid\
	, :type\
	, :amount\
	, :date\
	, :time\
	, :paidwith\
	, :paymethod\
	, :changegiven\
	, :state\
	, :cardnumber\
	, :itemcount\
	, :itemslist\
	, :points\
	, :discmoney\
	, :disc\
	, :cardauthnumber\
	, :profit\
	, :terminalnum\
	, :providerid\
	, :specialOrders\
	, :infoText\
	, :balanceId\
	, :totalTax)");
  
  query_bindValueDbgOut(query2,":userid", info.userid);
  query_bindValueDbgOut(query2,":clientid", info.clientid);
  query_bindValueDbgOut(query2,":type", info.type);
  query_bindValueDbgOut(query2,":amount", info.amount);
  query_bindValueDbgOut(query2,":date", info.date.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(query2,":time", info.time.toString("hh:mm"));
  query_bindValueDbgOut(query2,":paidwith", info.paywith );
  query_bindValueDbgOut(query2,":paymethod", info.paymethod);
  query_bindValueDbgOut(query2,":changegiven", info.changegiven);
  query_bindValueDbgOut(query2,":state", info.state);
  query_bindValueDbgOut(query2,":cardnumber", info.cardnumber); //.replace(0,15,"***************")); //FIXED: Only save last 4 digits
  query_bindValueDbgOut(query2,":itemcount", info.itemcount);
  query_bindValueDbgOut(query2,":itemslist", info.itemlist);
  query_bindValueDbgOut(query2,":points", info.points);
  query_bindValueDbgOut(query2,":discmoney", info.discmoney);
  query_bindValueDbgOut(query2,":disc", info.disc);
  query_bindValueDbgOut(query2,":cardauthnumber", info.cardauthnum);
  query_bindValueDbgOut(query2,":profit", info.profit);
  query_bindValueDbgOut(query2,":terminalnum", info.terminalnum);
  query_bindValueDbgOut(query2,":providerid", info.providerid);
  query_bindValueDbgOut(query2,":specialOrders", info.specialOrders);
  query_bindValueDbgOut(query2,":infoText", info.infoText);
  query_bindValueDbgOut(query2,":balanceId", info.balanceId);
  query_bindValueDbgOut(query2,":totalTax", info.totalTax);
//  if (!query_exec_s(query2) ) {
  if (!query_exec_s(query2) ) {
    int errNum = query2.lastError().number();
    QSqlError::ErrorType errType = query2.lastError().type();
    QString errStr = query2.lastError().text();
    QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
    setError(details);
  } else result=query2.lastInsertId().toULongLong();
  return result;
}

bool Azahar::updateTransaction(TransactionInfo info)
{
  bool result=false;
  QSqlQuery query2(db);
  query_prepare(query2,"UPDATE transactions SET \
	disc=:disc\
	, discmoney=:discMoney\
	, amount=:amount\
	, date=:date\
	, time=:time\
	, paidwith=:paidw\
	, changegiven=:change\
	, paymethod=:paymethod\
	, state=:state\
	, cardnumber=:cardnumber\
	, itemcount=:itemcount\
	, itemslist=:itemlist\
	, cardauthnumber=:cardauthnumber\
	, profit=:profit\
	, terminalnum=:terminalnum\
	, points=:points\
	, clientid=:clientid\
	, specialOrders=:sorders\
	, infoText=:infoText\
	, balanceId=:balance\
	, totalTax=:tax \
	WHERE id=:code");
  query_bindValueDbgOut(query2,":disc", info.disc);
  query_bindValueDbgOut(query2,":discMoney", info.discmoney);
  query_bindValueDbgOut(query2,":code", info.id);
  query_bindValueDbgOut(query2,":amount", info.amount);
  query_bindValueDbgOut(query2,":date", info.date.toString("yyyy-MM-dd"));
  query_bindValueDbgOut(query2,":time", info.time.toString("hh:mm"));
  query_bindValueDbgOut(query2,":paidw", info.paywith );
  query_bindValueDbgOut(query2,":change", info.changegiven);
  query_bindValueDbgOut(query2,":paymethod", info.paymethod);
  query_bindValueDbgOut(query2,":state", info.state);
  query_bindValueDbgOut(query2,":cardnumber", info.cardnumber);
  query_bindValueDbgOut(query2,":itemcount", info.itemcount);
  query_bindValueDbgOut(query2,":itemlist", info.itemlist);
  query_bindValueDbgOut(query2,":cardauthnumber", info.cardauthnum);
//kh18.07. changed from utilities to profit
  query_bindValueDbgOut(query2,":profit", info.profit);
  query_bindValueDbgOut(query2,":terminalnum", info.terminalnum);
  query_bindValueDbgOut(query2,":points", info.points);
  query_bindValueDbgOut(query2,":clientid", info.clientid);
  query_bindValueDbgOut(query2,":tax", info.totalTax);
  query_bindValueDbgOut(query2,":sorders", info.specialOrders);
  query_bindValueDbgOut(query2,":infoText", info.infoText);
  query_bindValueDbgOut(query2,":balance", info.balanceId);
DBX( info.balanceId);
  if (!query_exec_s(query2) ) {
TRACE ;
    int errNum = query2.lastError().number();
DBX( errNum);
    QSqlError::ErrorType errType = query2.lastError().type();
    QString errStr = query2.lastError().text();
    QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
    setError(details);
DBX( details);
  } else result=true;
  return result;
}

bool Azahar::deleteTransaction(qulonglong id)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
TRACE ;
    QString qry = QString("DELETE FROM transactions WHERE id=%1").arg(id);
    if (!query_exec(query,qry)) {
      result = false;
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString errStr = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else {
      result = true;
    }
  }
  return result;
}


//NOTE: Is it convenient to reuse empty transactions or simply delete them?
bool Azahar::deleteEmptyTransactions()
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    QString qry = QString("DELETE FROM transactions WHERE itemcount<=0 and amount<=0");
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString errStr = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else {
      result = true;
    }
  }
  return result;
}

qulonglong Azahar::getEmptyTransactionId()
{
  qulonglong result = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    QString qry = QString("SELECT id from transactions WHERE itemcount<=0 and amount<=0");
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString errStr = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else {
      while (query.next()) {
        int fieldId = query.record().indexOf("id");
        result      = query.value(fieldId).toULongLong();
        return result;
      }
    }
  }
  return result;
}

bool Azahar::cancelTransaction(qulonglong id, bool inProgress)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  bool ok = db.isOpen();
  
  TransactionInfo tinfo = getTransactionInfo(id);
  bool transCompleted = false;
  bool alreadyCancelled = false;
  bool transExists = false;
  if (tinfo.id    >  0)          transExists      = true;
  if (tinfo.state == tCompleted && transExists) transCompleted   = true;
  if (tinfo.state == tCancelled && transExists) alreadyCancelled = true;
  
  
  if (ok) {
    QSqlQuery query(db);
    QString qry;
    
    if (!inProgress && !alreadyCancelled && transExists) {
      qry = QString("UPDATE transactions SET  state=%1 WHERE id=%2")
      .arg(tCancelled)
      .arg(id);
      if (!query_exec(query,qry)) {
        int errNum = query.lastError().number();
        QSqlError::ErrorType errType = query.lastError().type();
        QString errStr = query.lastError().text();
        QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
        setError(details);
      } else { //Cancelled...
        result = true;
        qDebug()<<"Marked as Cancelled!";
      }
      ///not in progress, it means stockqty,points... are affected.
      if (transCompleted) {
        if (tinfo.points >0) decrementClientPoints(tinfo.clientid,tinfo.points);
        //TODO: when cancelling a transacion, take into account the groups sold to be returned. new feature
        QStringList soProducts;
        ///if there is any special order (product)
        if ( !tinfo.specialOrders.isEmpty() ) {
          //get each special order
          QStringList pSoList = tinfo.specialOrders.split(",");
          for (int i = 0; i < pSoList.size(); ++i) {
            QStringList l = pSoList.at(i).split("/");
            if ( l.count()==2 ) { //==2 means its complete, having product and qty
              qulonglong soid = l.at(0).toULongLong();
              //set as cancelled
              specialOrderSetStatus(soid, 4); //4 == cancelled
              //get each product of the special order to increment its stock later
              soProducts.append( getSpecialOrderProductsStr(soid) ); //are normal products (raw or not)
            } //if count
          } //for
        }//if there are special orders
        QString soProductsStr = soProducts.join(",");
        ///increment stock for each product. including special orders and groups
        QStringList plist = (tinfo.itemlist.split(",") + soProductsStr.split(","));
        qDebug()<<"[*****] plist: "<< plist;
        for (int i = 0; i < plist.size(); ++i) {
          QStringList l = plist.at(i).split("/");
          if ( l.count()==2 ) { //==2 means its complete, having product and qty
            //check if the product is a group
            //NOTE: rawProducts ? affect stock when cancelling = YES but only if affected when sold one of its parents (specialOrders) and stockqty is set. But they would not be here, if not at specialOrders List
            ProductInfo pi = getProductInfo(l.at(0));
            if ( pi.isAGroup ) 
              incrementGroupStock(l.at(0).toULongLong(), l.at(1).toDouble()); //code at 0, qty at 1
            else //there is a normal product
              incrementProductStock(l.at(0).toULongLong(), l.at(1).toDouble()); //code at 0, qty at 1
          }
        }//for each product
        ///save cashout for the money return
        qDebug()<<"Saving cashout-cancel";
        CashFlowInfo cinfo;
        cinfo.userid = tinfo.userid;
        cinfo.amount = tinfo.amount;
        cinfo.date   = QDate::currentDate();
        cinfo.time   = QTime::currentTime();
        cinfo.terminalNum = tinfo.terminalnum;
        cinfo.type   = ctCashOutMoneyReturnOnCancel;
        cinfo.reason = i18n("Money return on cancelling ticket #%1 ", id);
        insertCashFlow(cinfo);
      }//transCompleted
    } //not in progress
  }
  if ( alreadyCancelled ) {
    //The transaction was already canceled
    setError(i18n("Ticket #%1 was already canceled.", id));
    result = false;
    qDebug()<<"Transaction already cancelled...";
  }
  return result;
}



QList<TransactionInfo> Azahar::getLastTransactions(int pageNumber,int numItems,QDate beforeDate)
{
  QList<TransactionInfo> result;
  result.clear();
  QSqlQuery query(db);
  QString qry;
  qry = QString("SELECT * from transactions where type=1 and  date <= STR_TO_DATE('%1', '%d/%m/%Y') order by date desc, id desc LIMIT %2,%3").arg(beforeDate.toString("dd/MM/yyyy")).arg((pageNumber-1)*numItems+1).arg(numItems);
  if (query_exec(query,qry)) {
    while (query.next()) {
      TransactionInfo info;
      int fieldId = query.record().indexOf("id");
      int fieldAmount = query.record().indexOf("amount");
      int fieldDate   = query.record().indexOf("date");
      int fieldTime   = query.record().indexOf("time");
      int fieldPaidWith = query.record().indexOf("paidwith");
      int fieldPayMethod = query.record().indexOf("paymethod");
      int fieldType      = query.record().indexOf("type");
      int fieldChange    = query.record().indexOf("changegiven");
      int fieldState     = query.record().indexOf("state");
      int fieldUserId    = query.record().indexOf("userid");
      int fieldClientId  = query.record().indexOf("clientid");
      int fieldCardNum   = query.record().indexOf("cardnumber");
      int fieldCardAuth  = query.record().indexOf("cardauthnumber");
      int fieldItemCount = query.record().indexOf("itemcount");
      int fieldItemsList = query.record().indexOf("itemsList");
      int fieldDiscount  = query.record().indexOf("disc");
      int fieldDiscMoney = query.record().indexOf("discmoney");
      int fieldPoints    = query.record().indexOf("points");
      int fieldUtility   = query.record().indexOf("profit");
      int fieldTerminal  = query.record().indexOf("terminalnum");
      int fieldTax    = query.record().indexOf("totalTax");
      int fieldSOrd      = query.record().indexOf("specialOrders");
      int fieldinfoText      = query.record().indexOf("infoText");
      int fieldBalance   = query.record().indexOf("balanceId");
      info.id     = query.value(fieldId).toULongLong();
      info.amount = query.value(fieldAmount).toDouble();
      info.date   = query.value(fieldDate).toDate();
      info.time   = query.value(fieldTime).toTime();
      info.paywith= query.value(fieldPaidWith).toDouble();
      info.paymethod = query.value(fieldPayMethod).toInt();
      info.type      = query.value(fieldType).toInt();
      info.changegiven = query.value(fieldChange).toDouble();
      info.state     = query.value(fieldState).toInt();
      info.userid    = query.value(fieldUserId).toULongLong();
      info.clientid  = query.value(fieldClientId).toULongLong();
      info.cardnumber= query.value(fieldCardNum).toString();
      info.cardauthnum=query.value(fieldCardAuth).toString();
      info.itemcount = query.value(fieldItemCount).toInt();
      info.itemlist  = query.value(fieldItemsList).toString();
      info.disc      = query.value(fieldDiscount).toDouble();
      info.discmoney = query.value(fieldDiscMoney).toDouble();
      info.points    = query.value(fieldPoints).toULongLong();
      info.profit    = query.value(fieldUtility).toDouble();
      info.terminalnum=query.value(fieldTerminal).toInt();
      info.totalTax  = query.value(fieldTax).toDouble();
      info.specialOrders  = query.value(fieldSOrd).toString();
      info.infoText  = query.value(fieldinfoText).toString();
      info.balanceId = query.value(fieldBalance).toULongLong();
DBX( query.value(fieldBalance).toULongLong() );
      result.append(info);
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

//NOTE: The next method is not used... Also, for what pourpose? is it missing the STATE condition?
QList<TransactionInfo> Azahar::getTransactionsPerDay(int pageNumber,int numItems, QDate beforeDate)
{
  QList<TransactionInfo> result;
  result.clear();
  QSqlQuery query(db);
  QString qry;
  qry = QString("SELECT date, count(1) as transactions, sum(itemcount) as itemcount, sum(amount) as amount FROM transactions WHERE TYPE =1 AND date <= STR_TO_DATE('%1', '%d/%m/%Y') GROUP BY date(DATE) ORDER BY date DESC LIMIT %2,%3").arg(beforeDate.toString("dd/MM/yyyy")).arg((pageNumber-1)*numItems+1).arg(numItems);
  if (query_exec(query,qry)) {
    while (query.next()) {
      TransactionInfo info;
      int fieldTransactions = query.record().indexOf("transactions");
      int fieldAmount = query.record().indexOf("amount");
      int fieldDate   = query.record().indexOf("date");
      int fieldItemCount   = query.record().indexOf("itemcount");
      info.amount = query.value(fieldAmount).toDouble();
      info.date   = query.value(fieldDate).toDate();
      info.state     = query.value(fieldTransactions).toInt();
      info.itemcount = query.value(fieldItemCount).toInt();
      result.append(info);
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}


// TRANSACTIONITEMS
bool Azahar::insertTransactionItem(TransactionItemInfo info)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO transactionitems (transaction_id, position, product_id, serialno, qty, points, unitstr, cost, price, disc, total, name, payment, completePayment, soId, isGroup, deliveryDateTime, tax) VALUES(:transactionid, :position, :productCode, :serialNo, :qty, :points, :unitStr, :cost, :price, :disc, :total, :name, :payment, :completeP, :soid, :isGroup, :deliveryDT, :tax)");
    query_bindValueDbgOut(query,":transactionid", info.transactionid);
    query_bindValueDbgOut(query,":position", info.position);
    query_bindValueDbgOut(query,":productCode", info.productCode);
    query_bindValueDbgOut(query,":qty", info.qty);
    query_bindValueDbgOut(query,":points", info.points);
    query_bindValueDbgOut(query,":unitStr", info.unitStr);
    query_bindValueDbgOut(query,":cost", info.cost);
    query_bindValueDbgOut(query,":price", info.price);
    query_bindValueDbgOut(query,":disc", info.disc);
    query_bindValueDbgOut(query,":total", info.total);
    query_bindValueDbgOut(query,":name", info.name);
    query_bindValueDbgOut(query,":payment", info.payment);
    query_bindValueDbgOut(query,":completeP", info.completePayment);
    query_bindValueDbgOut(query,":serialNo", info.serialNo);
    query_bindValueDbgOut(query,":soid", info.soId);
    query_bindValueDbgOut(query,":isGroup", info.isGroup);
    query_bindValueDbgOut(query,":deliveryDT", info.deliveryDateTime);
    query_bindValueDbgOut(query,":tax", info.tax);
    if (!query_exec_s(query)) {
      setError(query.lastError().text());
      qDebug()<<"Insert TransactionItems error:"<<query.lastError().text();
    } else result = true;
  }
  return result;
}

bool Azahar::deleteAllTransactionItem(qulonglong id)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    QString qry = QString("DELETE FROM transactionitems WHERE transaction_id=%1").arg(id);
    if (!query_exec(query,qry)) {
      result = false;
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString errStr = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else {
      result = true;
    }
  }
  return result;
}

QList<TransactionItemInfo> Azahar::getTransactionItems(qulonglong id)
{
  QList<TransactionItemInfo> result;
  result.clear();
  QSqlQuery query(db);
  QString qry = QString("SELECT * FROM transactionitems WHERE transaction_id=%1 ORDER BY POSITION").arg(id);
  if (query_exec(query,qry)) {
    while (query.next()) {
      TransactionItemInfo info;
      int fieldPosition = query.record().indexOf("position");
      int fieldProductCode   = query.record().indexOf("product_id");
      int fieldQty     = query.record().indexOf("qty");
      int fieldPoints  = query.record().indexOf("points");
      int fieldCost    = query.record().indexOf("cost");
      int fieldPrice   = query.record().indexOf("price");
      int fieldDisc    = query.record().indexOf("disc");
      int fieldTotal   = query.record().indexOf("total");
      int fieldName    = query.record().indexOf("name");
      int fieldUStr    = query.record().indexOf("unitstr");
      int fieldPayment = query.record().indexOf("payment");
      int fieldCPayment = query.record().indexOf("completePayment");
      int fieldSoid = query.record().indexOf("soId");
      int fieldIsG = query.record().indexOf("isGroup");
      int fieldDDT = query.record().indexOf("deliveryDateTime");
      int fieldSN = query.record().indexOf("serialNo");
      int fieldTax = query.record().indexOf("tax");
      
      info.transactionid     = id;
      info.position      = query.value(fieldPosition).toInt();
      info.productCode   = query.value(fieldProductCode).toULongLong();
      info.qty           = query.value(fieldQty).toDouble();
      info.points        = query.value(fieldPoints).toDouble();
      info.unitStr       = query.value(fieldUStr).toString();
      info.cost          = query.value(fieldCost).toDouble();
      info.price         = query.value(fieldPrice).toDouble();
      info.disc          = query.value(fieldDisc).toDouble();
      info.total         = query.value(fieldTotal).toDouble();
      info.name          = query.value(fieldName).toString();
      info.payment       = query.value(fieldPayment).toDouble();
      info.completePayment  = query.value(fieldCPayment).toBool();
      info.soId          = query.value(fieldSoid).toString();
      info.isGroup       = query.value(fieldIsG).toBool();
      info.deliveryDateTime=query.value(fieldDDT).toDateTime();
      info.serialNo      =query.value(fieldSN).toString();
      info.tax           = query.value(fieldTax).toDouble();
      result.append(info);
    }
  }
  else {
    setError(query.lastError().text());
    qDebug()<<"Get TransactionItems error:"<<query.lastError().text();
  }
  return result;
}

//BALANCES

qulonglong Azahar::insertBalance(BalanceInfo info)
{
  qulonglong result =0;
  if (!db.isOpen()) db.open();
  if (db.isOpen())
  {
    QSqlQuery queryBalance(db);
    query_prepare(queryBalance,"INSERT INTO balances (balances.datetime_start, balances.datetime_end, balances.userid, balances.usern, balances.initamount, balances.in, balances.out, balances.cash, balances.card, balances.transactions, balances.terminalnum, balances.cashflows, balances.done) VALUES (:date_start, :date_end, :userid, :user, :initA, :in, :out, :cash, :card, :transactions, :terminalNum, :cashflows, :isDone)");
    query_bindValueDbgOut(queryBalance,":date_start", info.dateTimeStart.toString("yyyy-MM-dd hh:mm:ss"));
    query_bindValueDbgOut(queryBalance,":date_end", info.dateTimeEnd.toString("yyyy-MM-dd hh:mm:ss"));
    query_bindValueDbgOut(queryBalance,":userid", info.userid);
    query_bindValueDbgOut(queryBalance,":user", info.username);
    query_bindValueDbgOut(queryBalance,":initA", info.initamount);
    query_bindValueDbgOut(queryBalance,":in", info.in);
    query_bindValueDbgOut(queryBalance,":out", info.out);
    query_bindValueDbgOut(queryBalance,":cash", info.cash);
    query_bindValueDbgOut(queryBalance,":card", info.card);
    query_bindValueDbgOut(queryBalance,":transactions", info.transactions);
    query_bindValueDbgOut(queryBalance,":terminalNum", info.terminal);
    query_bindValueDbgOut(queryBalance,":cashflows", info.cashflows);
    query_bindValueDbgOut(queryBalance,":isDone", info.done);
    
    if (!query_exec_s(queryBalance) ) {
      int errNum = queryBalance.lastError().number();
      QSqlError::ErrorType errType = queryBalance.lastError().type();
      QString errStr = queryBalance.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else result = queryBalance.lastInsertId().toULongLong();
  }
  return result;
}


BalanceInfo Azahar::getBalanceInfo(qulonglong id)
{
  BalanceInfo info;
  info.id = 0;
  QString qry = QString("SELECT * FROM balances WHERE id=%1").arg(id);
  QSqlQuery query;
  if (!query_exec(query,qry)) { qDebug()<<query.lastError(); }
  else {
    while (query.next()) {
      int fieldId = query.record().indexOf("id");
      int fieldDtStart = query.record().indexOf("datetime_start");
      int fieldDtEnd   = query.record().indexOf("datetime_end");
      int fieldUserId  = query.record().indexOf("userid");
      int fieldUsername= query.record().indexOf("usern");
      int fieldInitAmount = query.record().indexOf("initamount");
      int fieldIn      = query.record().indexOf("in");
      int fieldOut     = query.record().indexOf("out");
      int fieldCash    = query.record().indexOf("cash");
      int fieldTransactions    = query.record().indexOf("transactions");
      int fieldCard    = query.record().indexOf("card");
      int fieldTerminalNum   = query.record().indexOf("terminalnum");
      int fieldCashFlows     = query.record().indexOf("cashflows");
      int fieldDone    = query.record().indexOf("done");
      info.id     = query.value(fieldId).toULongLong();
      info.dateTimeStart = query.value(fieldDtStart).toDateTime();
      info.dateTimeEnd   = query.value(fieldDtEnd).toDateTime();
      info.userid   = query.value(fieldUserId).toULongLong();
      info.username= query.value(fieldUsername).toString();
      info.initamount = query.value(fieldInitAmount).toDouble();
      info.in      = query.value(fieldIn).toDouble();
      info.out = query.value(fieldOut).toDouble();
      info.cash   = query.value(fieldCash).toDouble();
      info.card   = query.value(fieldCard).toDouble();
      info.transactions= query.value(fieldTransactions).toString();
      info.terminal = query.value(fieldTerminalNum).toInt();
      info.cashflows= query.value(fieldCashFlows).toString();
      info.done = query.value(fieldDone).toBool();
    }
  }
  return info;
}

bool Azahar::updateBalance(BalanceInfo info)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen())
  {
    QSqlQuery queryBalance(db);
    query_prepare(queryBalance,"UPDATE balances SET balances.datetime_start=:date_start, balances.datetime_end=:date_end, balances.userid=:userid, balances.usern=:user, balances.initamount=:initA, balances.in=:in, balances.out=:out, balances.cash=:cash, balances.card=:card, balances.transactions=:transactions, balances.terminalnum=:terminalNum, cashflows=:cashflows, done=:isDone WHERE balances.id=:bid");
    query_bindValueDbgOut(queryBalance,":date_start", info.dateTimeStart.toString("yyyy-MM-dd hh:mm:ss"));
    query_bindValueDbgOut(queryBalance,":date_end", info.dateTimeEnd.toString("yyyy-MM-dd hh:mm:ss"));
    query_bindValueDbgOut(queryBalance,":userid", info.userid);
    query_bindValueDbgOut(queryBalance,":user", info.username);
    query_bindValueDbgOut(queryBalance,":initA", info.initamount);
    query_bindValueDbgOut(queryBalance,":in", info.in);
    query_bindValueDbgOut(queryBalance,":out", info.out);
    query_bindValueDbgOut(queryBalance,":cash", info.cash);
    query_bindValueDbgOut(queryBalance,":card", info.card);
    query_bindValueDbgOut(queryBalance,":transactions", info.transactions);
    query_bindValueDbgOut(queryBalance,":terminalNum", info.terminal);
    query_bindValueDbgOut(queryBalance,":cashflows", info.cashflows);
    query_bindValueDbgOut(queryBalance,":bid", info.id);
    query_bindValueDbgOut(queryBalance,":isDone", info.done);
    
    if (!query_exec_s(queryBalance) ) {
      int errNum = queryBalance.lastError().number();
      QSqlError::ErrorType errType = queryBalance.lastError().type();
      QString errStr = queryBalance.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else result = true;
  }
  return result;
}

qulonglong Azahar::insertCashFlow(CashFlowInfo info)
{
  qulonglong result =0;
  if (!db.isOpen()) db.open();
  if (db.isOpen())
  {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO cashflow ( cashflow.userid, cashflow.type, cashflow.reason, cashflow.amount, cashflow.date, cashflow.time, cashflow.terminalnum) VALUES (:userid, :type, :reason, :amount, :date, :time,  :terminalNum)");
    query_bindValueDbgOut(query,":date", info.date.toString("yyyy-MM-dd"));
    query_bindValueDbgOut(query,":time", info.time.toString("hh:mm:ss"));
    query_bindValueDbgOut(query,":userid", info.userid);
    query_bindValueDbgOut(query,":terminalNum", info.terminalNum);
    query_bindValueDbgOut(query,":reason", info.reason);
    query_bindValueDbgOut(query,":amount", info.amount);
    query_bindValueDbgOut(query,":type", info.type);
    
    if (!query_exec_s(query) ) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString errStr = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else result = query.lastInsertId().toULongLong();
  }
  return result;
}

QList<CashFlowInfo> Azahar::getCashFlowInfoList(const QList<qulonglong> &idList)
{
  QList<CashFlowInfo> result;
  result.clear();
  if (idList.count() == 0) return result;
  QSqlQuery query(db);

  foreach(qulonglong currId, idList) {
    QString qry = QString(" \
    SELECT CF.id as id, \
    CF.type as type, \
    CF.userid as userid, \
    CF.amount as amount, \
    CF.reason as reason, \
    CF.date as date, \
    CF.time as time, \
    CF.terminalNum as terminalNum, \
    CFT.text as typeStr \
    FROM cashflow AS CF, cashflowtypes AS CFT \
    WHERE id=%1 AND (CFT.typeid = CF.type) ORDER BY CF.id;").arg(currId);
    if (query_exec(query,qry)) {
      while (query.next()) {
        CashFlowInfo info;
        int fieldId      = query.record().indexOf("id");
        int fieldType    = query.record().indexOf("type");
        int fieldUserId  = query.record().indexOf("userid");
        int fieldAmount  = query.record().indexOf("amount");
        int fieldReason  = query.record().indexOf("reason");
        int fieldDate    = query.record().indexOf("date");
        int fieldTime    = query.record().indexOf("time");
        int fieldTermNum = query.record().indexOf("terminalNum");
        int fieldTStr    = query.record().indexOf("typeStr");

        info.id          = query.value(fieldId).toULongLong();
        info.type        = query.value(fieldType).toULongLong();
        info.userid      = query.value(fieldUserId).toULongLong();
        info.amount      = query.value(fieldAmount).toDouble();
        info.reason      = query.value(fieldReason).toString();
        info.typeStr     = query.value(fieldTStr).toString();
        info.date        = query.value(fieldDate).toDate();
        info.time        = query.value(fieldTime).toTime();
        info.terminalNum = query.value(fieldTermNum).toULongLong();
        result.append(info);
      }
    }
    else {
      setError(query.lastError().text());
    }
  } //foreach
  return result;
}

QList<CashFlowInfo> Azahar::getCashFlowInfoList(const QDateTime &start, const QDateTime &end)
{
  QList<CashFlowInfo> result;
  result.clear();
  QSqlQuery query(db);

  query_prepare(query," \
  SELECT CF.id as id, \
  CF.type as type, \
  CF.userid as userid, \
  CF.amount as amount, \
  CF.reason as reason, \
  CF.date as date, \
  CF.time as time, \
  CF.terminalNum as terminalNum, \
  CFT.text as typeStr \
  FROM cashflow AS CF, cashflowtypes AS CFT \
  WHERE (date BETWEEN :dateSTART AND :dateEND) AND (CFT.typeid = CF.type) ORDER BY CF.id");
  query_bindValueDbgOut(query,":dateSTART", start.date());
  query_bindValueDbgOut(query,":dateEND", end.date());
  if (query_exec_s(query)) {
    while (query.next()) {
      QTime ttime = query.value(query.record().indexOf("time")).toTime();
      if ( (ttime >= start.time()) &&  (ttime <= end.time()) ) {
        //its inside the requested time period.
        CashFlowInfo info;
        int fieldId      = query.record().indexOf("id");
        int fieldType    = query.record().indexOf("type");
        int fieldUserId  = query.record().indexOf("userid");
        int fieldAmount  = query.record().indexOf("amount");
        int fieldReason  = query.record().indexOf("reason");
        int fieldDate    = query.record().indexOf("date");
        int fieldTime    = query.record().indexOf("time");
        int fieldTermNum = query.record().indexOf("terminalNum");
        int fieldTStr    = query.record().indexOf("typeStr");
        
        info.id          = query.value(fieldId).toULongLong();
        info.type        = query.value(fieldType).toULongLong();
        info.userid      = query.value(fieldUserId).toULongLong();
        info.amount      = query.value(fieldAmount).toDouble();
        info.reason      = query.value(fieldReason).toString();
        info.typeStr     = query.value(fieldTStr).toString();
        info.date        = query.value(fieldDate).toDate();
        info.time        = query.value(fieldTime).toTime();
        info.terminalNum = query.value(fieldTermNum).toULongLong();
        result.append(info);
      } //if time...
    } //while
  }// if query
  else {
    setError(query.lastError().text());
  }
  return result;
}

//TransactionTypes
QString  Azahar::getPayTypeStr(qulonglong type)
{
  QString result;
  QSqlQuery query(db);
  QString qstr = QString("select text from paytypes where paytypes.typeid=%1;").arg(type);
  if (query_exec(query,qstr)) {
    while (query.next()) {
      int fieldText = query.record().indexOf("text");
      result = query.value(fieldText).toString();
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

qulonglong  Azahar::getPayTypeId(QString type)
{
  qulonglong result=0;
  QSqlQuery query(db);
  QString qstr = QString("select typeid from paytypes where paytypes.text='%1';").arg(type);
  if (query_exec(query,qstr)) {
    while (query.next()) {
      int fieldText = query.record().indexOf("typeid");
      result = query.value(fieldText).toULongLong();
    }
  }
  else {
    setError(query.lastError().text());
  }
  return result;
}

//Brand
QString Azahar::getBrandName(const qulonglong &id)
{
  qDebug()<<"getBrandName...";
  QString result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select bname from brands where brandid=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("bname");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

qulonglong Azahar::getBrandId(const QString &name)
{
  qulonglong result = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select brandid from brands where bname='%1';").arg(name))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("brandid");
        result = myQuery.value(fieldText).toULongLong();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

qulonglong Azahar::insertBrand(const QString &name)
{
  qulonglong result =0;
  if (!db.isOpen()) db.open();
  if (db.isOpen())
  {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO brands (bname) VALUES (:brand)");
    query_bindValueDbgOut(query,":brand", name);
    
    if (!query_exec_s(query) ) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString errStr = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
      setError(details);
    } else result = query.lastInsertId().toULongLong();
  }
  return result;
}

//LOGS

bool Azahar::insertLog(const qulonglong &userid, const QDate &date, const QTime &time, const QString actionStr)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery query(db);
    query_prepare(query,"INSERT INTO logs (userid, date, time, action) VALUES(:userid, :date, :time, :action);");
    query_bindValueDbgOut(query,":userid", userid);
    query_bindValueDbgOut(query,":date", date.toString("yyyy-MM-dd"));
    query_bindValueDbgOut(query,":time", time.toString("hh:mm"));
    query_bindValueDbgOut(query,":action", actionStr);
    if (!query_exec_s(query)) {
      setError(query.lastError().text());
      qDebug()<<"ERROR ON SAVING LOG:"<<query.lastError().text();
    } else result = true;
  }
  return result;
}


bool Azahar::getConfigFirstRun()
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select firstrun from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("firstrun");
        QString value = myQuery.value(fieldText).toString();
        if (value == "yes, it is February 6 1978")
          result = true;
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

bool Azahar::getConfigTaxIsIncludedInPrice()
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select taxIsIncludedInPrice from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("taxIsIncludedInPrice");
        result = myQuery.value(fieldText).toBool();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

void Azahar::cleanConfigFirstRun()
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set firstrun='yes, i like the rainy days';"))) {
      qDebug()<<"Change config firstRun...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void Azahar::setConfigTaxIsIncludedInPrice(bool option)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set taxIsIncludedInPrice=%1;").arg(option))) {
      qDebug()<<"Change config taxIsIncludedInPrice...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

QPixmap  Azahar::getConfigStoreLogo()
{
  QPixmap result;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select storeLogo from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("storeLogo");
        result.loadFromData(myQuery.value(fieldText).toByteArray());
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QString  Azahar::getConfigStoreName()
{
  QString result;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select storeName from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("storeName");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QString  Azahar::getConfigStoreAddress()
{
  QString result;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select storeAddress from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("storeAddress");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QString  Azahar::getConfigStorePhone()
{
  QString result;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select storePhone from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("storePhone");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

bool     Azahar::getConfigSmallPrint()
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select smallPrint from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("smallPrint");
        result = myQuery.value(fieldText).toBool();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

bool     Azahar::getConfigLogoOnTop()
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select logoOnTop from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("logoOnTop");
        result = myQuery.value(fieldText).toBool();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

bool     Azahar::getConfigUseCUPS()
{
  bool result = false;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select useCUPS from config;"))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("useCUPS");
        result = myQuery.value(fieldText).toBool();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

void   Azahar::setConfigStoreLogo(const QByteArray &baPhoto)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    query_prepare(myQuery,"update config set storeLogo=:logo;");
    query_bindValueDbgOut(myQuery,":logo", baPhoto);
    if (query_exec_s(myQuery)) {
      qDebug()<<"Change config store logo...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void   Azahar::setConfigStoreName(const QString &str)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set storeName='%1';").arg(str))) {
      qDebug()<<"Change config storeName...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void   Azahar::setConfigStoreAddress(const QString &str)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set storeAddress='%1';").arg(str))) {
      qDebug()<<"Change config storeAddress...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void   Azahar::setConfigStorePhone(const QString &str)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set storePhone='%1';").arg(str))) {
      qDebug()<<"Change config taxIsIncludedInPrice...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void   Azahar::setConfigSmallPrint(bool yes)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set smallPrint=%1;").arg(yes))) {
      qDebug()<<"Change config SmallPrint...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void   Azahar::setConfigUseCUPS(bool yes)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set useCUPS=%1;").arg(yes))) {
      qDebug()<<"Change config useCUPS...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void   Azahar::setConfigLogoOnTop(bool yes)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update config set logoOnTop=%1;").arg(yes))) {
      qDebug()<<"Change config logoOnTop...";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}


//SPECIAL ORDERS
QList<SpecialOrderInfo> Azahar::getAllSOforSale(qulonglong saleId)
{
  QList<SpecialOrderInfo> list;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QString qry = QString("SELECT orderid,saleid from special_orders where saleid=%1").arg(saleId);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
      setError(i18n("Error getting special Order information, id: %1, Rows affected: %2", saleId,query.size()));
    }
    else {
      while (query.next()) {
        int fieldId  = query.record().indexOf("orderid");
        qulonglong num = query.value(fieldId).toULongLong();
        SpecialOrderInfo soInfo = getSpecialOrderInfo(num);
        list.append(soInfo);
      }
    }
  }
  return list;
}

//NOTE: Here the question is, what status to take into account? pending,inprogress,ready...
//      We will return all with status < 3 .
QList<SpecialOrderInfo> Azahar::getAllReadySOforSale(qulonglong saleId)
{
  QList<SpecialOrderInfo> list;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QString qry = QString("SELECT orderid,saleid from special_orders where saleid=%1 and status<3").arg(saleId);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
      setError(i18n("Error getting special Order information, id: %1, Rows affected: %2", saleId,query.size()));
    }
    else {
      while (query.next()) {
        int fieldId  = query.record().indexOf("orderid");
        qulonglong num = query.value(fieldId).toULongLong();
        SpecialOrderInfo soInfo = getSpecialOrderInfo(num);
        list.append(soInfo);
      }
    }
  }
  return list;
}

int  Azahar::getReadySOCountforSale(qulonglong saleId)
{
  int count=0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QString qry = QString("SELECT orderid from special_orders where saleid=%1 and status<3").arg(saleId);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
      setError(i18n("Error getting special Order information, id: %1, Rows affected: %2", saleId,query.size()));
    }
    else {
//       while (query.next()) {
//         count++;
//       }
    count = query.size();
    }
  }
  return count;
}

void Azahar::specialOrderSetStatus(qulonglong id, int status)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update special_orders set status=%1 where orderid=%2;").arg(status).arg(id))) {
      qDebug()<<"Status Order updated";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

void Azahar::soTicketSetStatus(qulonglong ticketId, int status)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update special_orders set status=%1 where saleid=%2;").arg(status).arg(ticketId))) {
      qDebug()<<"Status Order updated";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

QString Azahar::getSpecialOrderProductsStr(qulonglong id)
{
  QString result = "";
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select groupElements from special_orders where orderid=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("groupElements");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QList<ProductInfo> Azahar::getSpecialOrderProductsList(qulonglong id)
{
  QList<ProductInfo> pList;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QString ge = getSpecialOrderProductsStr(id);
    QStringList pq = ge.split(",");
    foreach(QString str, pq) {
      qulonglong c = str.section('/',0,0).toULongLong();
      double     q = str.section('/',1,1).toDouble();
      //get info
      ProductInfo pi = getProductInfo(QString::number(c));
      pi.qtyOnList = q;
      pList.append(pi);
    }
  }
  return pList;
}

QString  Azahar::getSONotes(qulonglong id)
{
  QString result = "";
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select notes from special_orders where orderid=%1;").arg(id))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("notes");
        result = myQuery.value(fieldText).toString();
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

qulonglong Azahar::insertSpecialOrder(SpecialOrderInfo info)
{
  qulonglong result = 0;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"INSERT INTO special_orders (name, price, qty, cost, units, groupElements, status, saleid, notes, payment, completePayment, dateTime, deliveryDateTime,clientId,userId) VALUES (:name, :price, :qty, :cost, :units,  :groupE, :status, :saleId, :notes, :payment, :completeP, :dateTime, :deliveryDT, :client, :user);");
  query_bindValueDbgOut(query,":name", info.name);
  query_bindValueDbgOut(query,":price", info.price);
  query_bindValueDbgOut(query,":qty", info.qty);
  query_bindValueDbgOut(query,":cost", info.cost);
  query_bindValueDbgOut(query,":status", info.status);
  query_bindValueDbgOut(query,":units", info.units);
  query_bindValueDbgOut(query,":groupE", info.groupElements);
  query_bindValueDbgOut(query,":saleId", info.saleid);
  query_bindValueDbgOut(query,":notes", info.notes);
  query_bindValueDbgOut(query,":payment", info.payment);
  query_bindValueDbgOut(query,":completeP", info.completePayment);
  query_bindValueDbgOut(query,":dateTime", info.dateTime);
  query_bindValueDbgOut(query,":deliveryDT", info.deliveryDateTime);
  query_bindValueDbgOut(query,":user", info.userId);
  query_bindValueDbgOut(query,":client", info.clientId);
  
  if (!query_exec_s(query)) setError(query.lastError().text()); else {
    result=query.lastInsertId().toULongLong();
  }
  
  return result;
}

//saleid, dateTime/userid/clientid are not updated.
bool Azahar::updateSpecialOrder(SpecialOrderInfo info)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE special_orders SET  name=:name, price=:price, qty=:qty, cost=:cost, units=:units,  groupElements=:groupE, status=:status, notes=:notes, payment=:payment, completePayment=:completeP, deliveryDateTime=:deliveryDT WHERE orderid=:code;");
  query_bindValueDbgOut(query,":code", info.orderid);
  query_bindValueDbgOut(query,":name", info.name);
  query_bindValueDbgOut(query,":price", info.price);
  query_bindValueDbgOut(query,":qty", info.qty);
  query_bindValueDbgOut(query,":cost", info.cost);
  query_bindValueDbgOut(query,":status", info.status);
  query_bindValueDbgOut(query,":units", info.units);
  query_bindValueDbgOut(query,":groupE", info.groupElements);
  query_bindValueDbgOut(query,":notes", info.notes);
  query_bindValueDbgOut(query,":payment", info.payment);
  query_bindValueDbgOut(query,":completeP", info.completePayment);
  query_bindValueDbgOut(query,":deliveryDT", info.deliveryDateTime);
  
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}

bool Azahar::deleteSpecialOrder(qulonglong id)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query = QString("DELETE FROM special_orders WHERE orderid=%1").arg(id);
  if (!query_exec_s(query)) setError(query.lastError().text()); else result=true;
  return result;
}

SpecialOrderInfo Azahar::getSpecialOrderInfo(qulonglong id)
{
  SpecialOrderInfo info;
  info.orderid=0;
  info.name="Ninguno";
  info.price=0;

  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QString qry = QString("SELECT * from special_orders where orderid=%1").arg(id);
    QSqlQuery query(db);
    if (!query_exec(query,qry)) {
      int errNum = query.lastError().number();
      QSqlError::ErrorType errType = query.lastError().type();
      QString error = query.lastError().text();
      QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),error);
      setError(i18n("Error getting special Order information, id: %1, Rows affected: %2", id,query.size()));
    }
    else {
      while (query.next()) {
        int fieldDesc = query.record().indexOf("name");
        int fieldPrice= query.record().indexOf("price");
        int fieldQty= query.record().indexOf("qty");
        int fieldCost= query.record().indexOf("cost");
        int fieldUnits= query.record().indexOf("units");
        int fieldStatus= query.record().indexOf("status");
        int fieldSaleId= query.record().indexOf("saleid");
        int fieldGroupE = query.record().indexOf("groupElements");
        int fieldPayment = query.record().indexOf("payment");
        int fieldCPayment = query.record().indexOf("completePayment");
        int fieldDateT   = query.record().indexOf("dateTime");
        int fieldDDT     = query.record().indexOf("deliveryDateTime");
        int fieldNotes   = query.record().indexOf("notes");
        int fieldClientId = query.record().indexOf("clientId");
        int fieldUserId = query.record().indexOf("userId");
        
        info.orderid=id;
        info.name      = query.value(fieldDesc).toString();
        info.price    = query.value(fieldPrice).toDouble();
        info.qty      = query.value(fieldQty).toDouble();
        info.cost     = query.value(fieldCost).toDouble();
        info.units    = query.value(fieldUnits).toInt();
        info.status   = query.value(fieldStatus).toInt();
        info.saleid   = query.value(fieldSaleId).toULongLong();
        info.groupElements = query.value(fieldGroupE).toString();
        info.payment  = query.value(fieldPayment).toDouble();
        info.completePayment  = query.value(fieldCPayment).toBool();
        info.dateTime = query.value(fieldDateT).toDateTime();
        info.deliveryDateTime = query.value(fieldDDT).toDateTime();
        info.notes = query.value(fieldNotes).toString();
        info.clientId = query.value(fieldClientId).toULongLong();
        info.userId = query.value(fieldUserId).toULongLong();
      }
      //get units descriptions
      qry = QString("SELECT * from measures WHERE id=%1").arg(info.units);
      QSqlQuery query3(db);
      if (query3.exec(qry)) {
        while (query3.next()) {
          int fieldUD = query3.record().indexOf("text");
          info.unitStr=query3.value(fieldUD).toString();
        }//query3 - get descritptions
      }
    }
  }
  return info;
}

double Azahar::getSpecialOrderAverageTax(qulonglong id)
{
  double result = 0;
  double sum = 0;
  QList<ProductInfo> pList = getSpecialOrderProductsList(id);
  foreach( ProductInfo info, pList) {
    sum += info.tax;
  }

  result = sum/pList.count();
  qDebug()<<"SO average tax: "<<result <<" sum:"<<sum<<" count:"<<pList.count();

  return result;
}

QStringList Azahar::getStatusList()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,"select text from so_status;")) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("text");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

QStringList Azahar::getStatusListExceptDelivered()
{
  QStringList result;
  result.clear();
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("select text from so_status where id!=%1;").arg(stDelivered))) {
      while (myQuery.next()) {
        int fieldText = myQuery.record().indexOf("text");
        QString text = myQuery.value(fieldText).toString();
        result.append(text);
      }
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
  return result;
}

int Azahar::getStatusId(QString texto)
{
  qulonglong result=0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    QString qryStr = QString("SELECT so_status.id FROM so_status WHERE text='%1';").arg(texto);
    if (query_exec(myQuery,qryStr) ) {
      while (myQuery.next()) {
        int fieldId   = myQuery.record().indexOf("id");
        qulonglong id= myQuery.value(fieldId).toULongLong();
        result = id;
      }
    }
    else {
      setError(myQuery.lastError().text());
    }
  }
  return result;
}

QString Azahar::getRandomMessage(QList<qulonglong> &excluded, const int &season)
{
  QString result;
  QString firstMsg;
  qulonglong firstId = 0;
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    QString qryStr = QString("SELECT message,id FROM random_msgs WHERE season=%1 order by count ASC;").arg(season);
    if (query_exec(myQuery,qryStr) ) {
      while (myQuery.next()) {
        int fieldId      = myQuery.record().indexOf("id");
        int fieldMsg     = myQuery.record().indexOf("message");
        qulonglong id    = myQuery.value(fieldId).toULongLong();
        if ( firstMsg.isEmpty() ) {
          firstMsg = myQuery.value(fieldMsg).toString(); //get the first msg.
          firstId  = myQuery.value(fieldId).toULongLong();
        }
        qDebug()<<"Examining msg Id: "<<id;
        //check if its not on the excluded list.
        if ( !excluded.contains(id) ) {
          //ok, return the msg, increment count.
          result = myQuery.value(fieldMsg).toString();
          randomMsgIncrementCount(id);
          //modify the excluded list, insert this one.
          excluded.append(id);
          //we exit from the while loop.
          qDebug()<<" We got msg:"<<result;
          break;
        }
      }
    }
    else {
      setError(myQuery.lastError().text());
    }
  }
  if (result.isEmpty() && firstId > 0) {
    result = firstMsg;
    randomMsgIncrementCount(firstId);
    excluded << firstId;
    qDebug()<<"Returning the fist message!";
  }
  return result;
}

void Azahar::randomMsgIncrementCount(qulonglong id)
{
  if (!db.isOpen()) db.open();
  if (db.isOpen()) {
    QSqlQuery myQuery(db);
    if (query_exec(myQuery,QString("update random_msgs set count=count+1 where id=%1;").arg(id))) {
      qDebug()<<"Random Messages Count updated";
    }
    else {
      qDebug()<<"ERROR: "<<myQuery.lastError();
    }
  }
}

bool Azahar::insertRandomMessage(const QString &msg, const int &season)
{
  bool result = false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"INSERT INTO random_msgs (message, season, count) VALUES (:message, :season, :count);");
  query_bindValueDbgOut(query,":msg", msg);
  query_bindValueDbgOut(query,":season", season);
  query_bindValueDbgOut(query,":count", 0);
  if (!query_exec_s(query)) {
    setError(query.lastError().text());
  }
  else result=true;
  return result;
}


bool Azahar::newStockTaking()
{
bool result=false;
if (!db.isOpen()) db.open();
QSqlQuery query(db);
query_prepare(query,"INSERT INTO stocktakings (code,alphacode,category,name,measure,price,cost,qty,counted,kennung)"
	"SELECT code ,alphacode ,category ,name ,measure ,price ,cost ,qty ,qty ,YEAR(CURDATE()) *10000 + MONTH(CURDATE())*100 + DAYOFMONTH(CURDATE())"
	"FROM v_stocktake;");
  if (!query_exec_s(query)) {
    setError(query.lastError().text());
  }
  else result=true;
  
  return result;
}
int Azahar::getCountOpenStockTaking()
{
int anz=0;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"SELECT count(*) as anzahl FROM stocktakings where closed <> true;");
  if (query_exec_s(query) && query.size()==1) {
	 query.next();
	 anz = query.value(query.record().indexOf("anzahl")).toInt();
    }
  return anz;
  }


QString Azahar::getLastStockTakingId()
{
QString kennung="";
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"SELECT max(kennung) as kennung FROM stocktakings;");
  if (query_exec_s(query) && query.size()>0) {
	query.next();
	kennung = query.value(query.record().indexOf("kennung")).toString();
    }
  return kennung;
  }

bool Azahar::closeStockTaking(QString takeid)
{
  if (!db.isOpen()) db.open();
  QSqlQuery query_read(db);
  QSqlQuery query_update_products(db);
  QSqlQuery query_update_stocktakings(db);
  QSqlQuery query_update_stockcorr(db);

  query_prepare(query_read,"DELETE FROM stocktakings WHERE closed = false and qty=counted");
  if (!query_exec_s(query_read)) {
        setError(query_read.lastError().text());
	//FEHLERMELDUNG
	return false;
	}
TRACE;

  query_prepare(query_read,"SELECT id,code,qty,counted,price FROM stocktakings WHERE qty<> counted && closed = false and kennung = :kennung");
  query_bindValueDbgOut(query_read,":kennung",takeid);
  if (!query_exec_s(query_read)) {
        setError(query_read.lastError().text());
	//FEHLERMELDUNG
	return false;
	}
TRACE;

  db.transaction();

  int fieldId      = query_read.record().indexOf("id");
  int fieldCode    = query_read.record().indexOf("code");
  int fieldQty     = query_read.record().indexOf("qty");
  int FieldCounted = query_read.record().indexOf("counted");

  while (query_read.next()) {
	qlonglong id   = query_read.value(fieldId).toULongLong();
	qlonglong code = query_read.value(fieldCode).toULongLong();
	int qty        = query_read.value(fieldQty).toInt();
	int counted    = query_read.value(FieldCounted).toInt();
DBX(code << qty << counted);

	query_prepare(query_update_products,"UPDATE products set stockqty = stockqty - :newqty where code = :code");
	query_bindValueDbgOut(query_update_products,":newqty",qty-counted);
	query_bindValueDbgOut(query_update_products,":code",code);
	if (!query_exec_s(query_update_products)) {
        	setError(query_update_products.lastError().text());
		db.rollback();
		return false;
		}

	query_prepare(query_update_stocktakings,"UPDATE stocktakings set closed = true where id = :id");
	query_bindValueDbgOut(query_update_stocktakings,":id",id);
	if (!query_exec_s(query_update_stocktakings)) {
        	setError(query_update_stocktakings.lastError().text());
		db.rollback();
		return false;
		}
DBX(id);
	}

db.commit();
return true;
}

bool Azahar::updateStockTaking(QString takeid,qlonglong code,qlonglong amount)
{
  bool result=false;
  if (!db.isOpen()) db.open();
  QSqlQuery query(db);
  query_prepare(query,"UPDATE stocktakings set counted = :counted where kennung = :takeid and code = :code;");
  query_bindValueDbgOut(query,":counted",amount);
  query_bindValueDbgOut(query,":takeid",takeid);
  query_bindValueDbgOut(query,":code",code);
  if (!query_exec_s(query)) {
    setError(query.lastError().text());
  }
  else result=true;
  
  return result;
}


QList<StocktakeInfo> Azahar::getStockTake()
{
  QList<StocktakeInfo> result;
  StocktakeInfo info;
  QSqlQuery qryStt(db);
  QString kennung = getLastStockTakingId();
  if (!query_exec(qryStt,QString("SELECT * FROM stocktakings where kennung = %1 order by category, code").arg(kennung)) ) {
    int errNum = qryStt.lastError().number();
    QSqlError::ErrorType errType = qryStt.lastError().type();
    QString errStr = qryStt.lastError().text();
    QString details = i18n("Error #%1, Type:%2\n'%3'",QString::number(errNum), QString::number(errType),errStr);
    setError(details);
  } else {
    while (qryStt.next()) {
      info.code        = qryStt.value(qryStt.record().indexOf("code")).toULongLong();
      info.alphacode = qryStt.value(qryStt.record().indexOf("alphacode")).toString();
      info.category  = qryStt.value(qryStt.record().indexOf("category")).toString();
      info.name      = qryStt.value(qryStt.record().indexOf("name")).toString();
      info.measure   = qryStt.value(qryStt.record().indexOf("measure")).toString();
      info.price     = qryStt.value(qryStt.record().indexOf("price")).toDouble();
      info.cost     = qryStt.value(qryStt.record().indexOf("cost")).toDouble();
      info.qty       = qryStt.value(qryStt.record().indexOf("qty")).toDouble();
      info.onstock   = qryStt.value(qryStt.record().indexOf("counted")).toDouble();
      result.append(info);
    }
  }
  return result;
}


#include "azahar.moc"
