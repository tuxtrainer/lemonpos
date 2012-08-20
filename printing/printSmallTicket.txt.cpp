bool PrintCUPS::printSmallTicket(const PrintTicketInfo &ptInfo, QPrinter &pr,const QString tickettext)
{
  bool WithSerialNr=false;
  bool result = false;
  start(&pr,20,QFont("Impact", 38));

  //NOTE from Qt for the drawText: The y-position is used as the baseline of the font
  
  // Header: Store Name, Store Address, Store Phone, Store Logo...
  QFont tmpFont = QFont("Bitstream Vera Sans", 18);
  QString text;
  if (ptInfo.sd.logoOnTop) {
    // Store Logo
    painter.drawPixmap(printer->width()/2 - (ptInfo.sd.storeLogo.width()/2), Margin, ptInfo.sd.storeLogo);
    yPos = yPos + ptInfo.sd.storeLogo.height();
    // Store Name
    painter.setFont(header);
    text = ptInfo.sd.storeName;
    fm = painter.fontMetrics();
    textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
    painter.drawText((printer->width()/2)-(textWidth.width()/2),THE_LINE, text);
    // Store Address
    tmpFont = QFont("Bitstream Vera Sans", 18);
    tmpFont.setItalic(true);
    painter.setFont(tmpFont);
    painter.setPen(Qt::darkGray);
    fm = painter.fontMetrics();
    text = ptInfo.sd.storeAddr + ", " +ptInfo.thPhone + ptInfo.sd.storePhone;
    textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
    LINEFEED;
    yPos+=30;
    painter.drawText((printer->width()/2)-(textWidth.width()/2), Margin + yPos + textWidth.height(), text);
    DLINEFEED
TRACE ;
  }
  else {
    // Store Logo
    painter.drawPixmap(printer->width() - ptInfo.sd.storeLogo.width() - Margin, Margin+15, ptInfo.sd.storeLogo);
    // Store Name
    painter.setFont(header);
    text = ptInfo.sd.storeName;
    fm = painter.fontMetrics();
    textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
    painter.drawText(Margin,Margin+textWidth.height(), text);
    // Store Address
    tmpFont.setItalic(true);
    painter.setFont(tmpFont);
    fm = painter.fontMetrics();
    LINEFEED;
    yPos+=15;
    text = ptInfo.sd.storeAddr + ", " +ptInfo.thPhone + ptInfo.sd.storePhone;
    textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
    painter.setPen(Qt::darkGray);
    painter.drawText(Margin, Margin*2 + yPos + textWidth.height(), text);
    DLINEFEED;
TRACE ;
  }
  

  tmpFont = QFont("Bitstream Vera Sans", 17);
  tmpFont.setItalic(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  // Header line
  painter.setPen(QPen(Qt::gray, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos, printer->width()-Margin, Margin+yPos);
  LINEFEED;
  //Date, Ticket number
  painter.setPen(normalPen);
  text = ptInfo.thDate ;
  textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
  painter.drawText(Margin, Margin + yPos + textWidth.height(), text);
  //change font for ticket number... bigger. Check for really big numbers to fit the page.
  tmpFont = QFont("Bitstream Vera Sans", 22);
  tmpFont.setItalic(false);
  tmpFont.setBold(true);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  text = ptInfo.thTicket;
  textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
  painter.drawText(printer->width() - textWidth.width() - Margin, THE_LINE, text);
  //change font again
  tmpFont = QFont("Bitstream Vera Sans", 17);
  tmpFont.setItalic(false);
  tmpFont.setBold(false);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  LINEFEED;
  //Vendor, terminal number
  text = ptInfo.salesPerson + ", " + ptInfo.terminal;
  textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
  painter.drawText(Margin, THE_LINE, text);
  yPos = yPos + 3*fm.lineSpacing();

  for (int i = 0; i < ptInfo.ticketInfo.lines.size(); ++i) {
    TicketLineInfo tLine = ptInfo.ticketInfo.lines.at(i);
    if(!tLine.serialNo.isEmpty()) {
        WithSerialNr=true;
        }
    }

//print clientdata if a procuct with serialnumber has been sold
  if(WithSerialNr  && ptInfo.ticketInfo.clientid > 1) {
     NLINEFEED(1);
     QStringList clientLines=ptInfo.ticketInfo.clientStr.split(QRegExp("[\n,;]"));
     foreach(QString strTmp, clientLines) {
        painter.drawText((printer->width()/2)-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, strTmp).width()/2)-Margin, Margin+yPos, strTmp);
//      painter.drawText(Margin, Margin+yPos, strTmp); //first product description...
        LINEFEED;
        }
     NLINEFEED(1);
     }
  



  // Products Subheader
  int columnQty  = 10;
  int columnDisc = 200;
  int columnTotal= 360;
  if (ptInfo.totDisc <= 0) {
    columnQty  = -150;
TRACE ;
  }
  painter.setPen(Qt::darkBlue);
  tmpFont = QFont("Bitstream Vera Sans", 17 );
  tmpFont.setWeight(QFont::Bold);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  painter.drawText(Margin,Margin+yPos, ptInfo.thProduct);
  text = ptInfo.thQty+"  "+ptInfo.thPrice;
  painter.drawText((printer->width()/3)-columnQty-50, Margin + yPos, text);
  if (ptInfo.totDisc > 0) {
    painter.drawText((printer->width()/3)+columnDisc-50, Margin + yPos, ptInfo.thDiscount);
TRACE ;
  }
  painter.drawText((printer->width()/3)+columnTotal-50, Margin + yPos, ptInfo.thTotal);
  text = ptInfo.thTax;
  painter.drawText(printer->width()-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width())-Margin, Margin + yPos, text);
  
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
TRACE ;
    TicketLineInfo tLine = ptInfo.ticketInfo.lines.at(i);
    bool isGroup = ptInfo.ticketInfo.lines.at(i).isGroup;
    QDateTime deliveryDT = ptInfo.ticketInfo.lines.at(i).deliveryDateTime;
    QString  idesc =  tLine.desc;
    QString iprice =  localeForPrinting.toString(tLine.price,'f',2);
    QString iqty   =  localeForPrinting.toString(tLine.qty, 'f', 2);
    QString itax   =  localeForPrinting.toString(tLine.tax, 'f', 2);
DBX( ptInfo.taxes );
DBX( ptInfo.ticketInfo.totalTax );
DBX( ptInfo.thTax );
DBX( idesc );
DBX( iprice );
DBX( iqty );
DBX( itax );
DBX( tLine.serialNo );
    // We must be aware of the locale. europe uses ',' instead of '.' as the decimals separator.
    // note: QLocale has a method that does this locale aware! if the locale is set correctly
    if (iqty.endsWith(".00") || iqty.endsWith(",00")) { iqty.chop(3); iqty += "   ";}//we chop the trailing zeroes...
    if (itax.endsWith(".00") || itax.endsWith(",00")) { itax.chop(3); }//we chop the trailing zeroes...
    iqty = iqty; //+tLine.unitStr;
    QString idiscount =  localeForPrinting.toString(-(tLine.qty*tLine.disc),'f',2);
    if (tLine.disc <= 0) idiscount = ""; // dont print 0.0
    QString idue =  localeForPrinting.toString(tLine.total,'f',2);
DBX( tLine.disc );
DBX( idiscount );
DBX( ptInfo.totDisc );
    if (ptInfo.totDisc > 0) {
      while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, idesc).width() >= ((printer->width()/3)-Margin-50)) { idesc.chop(2); }
DBX( idiscount );
    } else 
      while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, idesc).width() >= ((printer->width()/3)+150-Margin-50)) { idesc.chop(2); }
    painter.drawText(Margin, Margin+yPos, idesc); //first product description...
    text = iqty+" x " + iprice;
    painter.drawText((printer->width()/3)-columnQty-50, Margin+yPos, text);
    if (ptInfo.totDisc >0) {
      text = idiscount;
      if (text == "0.0") text = ""; //dont print a 0.0 !!!
      painter.drawText((printer->width()/3)+columnDisc-50, Margin+yPos, idiscount);
DBX( idiscount );
    }
    painter.drawText((printer->width()/3)+columnTotal-50, Margin+yPos, idue);
    //taxes
DBX( idiscount );
    text = itax+"%";
DBX( itax );
    painter.drawText(printer->width()-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width())-Margin, Margin + yPos, text);

    LINEFEED;

    ///serialnr
    if(!tLine.serialNo.isEmpty()) {
	WithSerialNr=true;
        text = ptInfo.thSerialNo + " " + tLine.serialNo;
        painter.drawText(4*Margin, Margin + yPos, text);
        LINEFEED;
        }

    ///check if there is a Special Order or group, to print contents

    if ( !tLine.geForPrint.isEmpty() ) {
      QStringList tmpList = tLine.geForPrint.split("|");
      QStringList strList;
      tmpList.removeFirst();
      //check text lenght
      double maxL = ((printer->width())-100); 
      foreach(QString strTmp, tmpList) {
        fm = painter.fontMetrics();
        QString strCopy = strTmp;
        double realTrozos = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, strTmp).width() / maxL;
        int trozos   = realTrozos;
        double diff = (realTrozos - trozos);
        if (diff > 0.25 && trozos > 0) trozos += 1;
        int tamTrozo = 0;
        if (trozos > 0) {
          tamTrozo = (strTmp.length()/trozos);
TRACE ;
        } else {
          tamTrozo = strTmp.length();
TRACE ;
        }
        
        QStringList otherList;
        for (int x = 1; x <= trozos; x++) {
          //we repeat for each trozo
          if (x*(tamTrozo-1) < strCopy.length())
            strCopy.insert(x*(tamTrozo-1), "|  "); //create a section
          otherList = strCopy.split("|");
TRACE ;
        }
        if (!otherList.isEmpty()) strList << otherList;
        if (trozos < 1) strList << strTmp;
        qDebug()<<"Trozos:"<<trozos<<" tamTrozo:"<<tamTrozo<<" realTrozos:"<<QString::number(realTrozos,'f', 2)<<" maxL:"<<maxL<<" strTmp.width in pixels:"<<fm.size(Qt::TextExpandTabs | Qt::TextDontClip, strTmp).width()<<" diff:"<<diff;
TRACE ;
      }

      foreach(QString str, strList) {
        painter.drawText(Margin, Margin+yPos, str);
        LINEFEED;
TRACE ;
      }
      //is payment complete?
      if ( Margin + yPos > printer->height() - Margin ) {
        formfeed();
TRACE ;
      }
      QString sp; QString sp2;
      if (tLine.completePayment) {
        sp  = ptInfo.paymentStrComplete + QString::number(tLine.payment*tLine.qty, 'f',2);
        sp2 = ptInfo.lastPaymentStr;
TRACE ;
      }
      else {
        sp = ptInfo.paymentStrPrePayment + QString::number(tLine.payment*tLine.qty, 'f',2);
        sp2 = ptInfo.nextPaymentStr;
TRACE ;
      }
      if (isGroup) sp  = ""; ///hack!
      //change font for the PAYMENT MSG
      tmpFont = QFont("Bitstream Vera Sans", 17 );
      tmpFont.setWeight(QFont::Bold);
      painter.setFont(tmpFont);
      fm = painter.fontMetrics();
      painter.drawText(Margin, Margin+yPos, sp);
      LINEFEED;
      //print the negative qty for the payment.
      // The negative qty is one of the next:
      //     * When completing the SO: the amount of the pre-payment.
      //     * When starting the SO:   the remaining amount to pay.
      if (isGroup) sp2  = ""; ///hack!
      painter.drawText(Margin, Margin+yPos, sp2);
      sp = QString::number(-((tLine.price-tLine.payment)*tLine.qty), 'f',2);
      if (isGroup) sp  = ""; ///hack!
      painter.drawText((printer->width()/3)+columnTotal, Margin+yPos, sp);
      tmpFont = QFont("Bitstream Vera Sans", 17 );
      tmpFont.setWeight(QFont::Normal);
      painter.setFont(tmpFont);
      fm = painter.fontMetrics();
      LINEFEED;
      ///Check for delivery date and if its a SO
      if (!isGroup && tLine.payment>0 ) {
        sp = ptInfo.deliveryDateStr + deliveryDT.toString(); //TODO:hey i18n stuff!!!
        tmpFont = QFont("Bitstream Vera Sans", 17 );
        tmpFont.setWeight(QFont::Bold);
        painter.setFont(tmpFont);
        fm = painter.fontMetrics();
        painter.drawText(Margin, Margin+yPos, sp);
        LINEFEED;
        tmpFont = QFont("Bitstream Vera Sans", 17 );
        tmpFont.setWeight(QFont::Normal);
        painter.setFont(tmpFont);
        fm = painter.fontMetrics();
TRACE ;
      }
TRACE ;
    } /// is group or specialorder ?
    //Check if space for the next text line
    if ( Margin + yPos > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
TRACE ;
  } //for each item

    //Check for client discount
DBX( ptInfo.clientDiscMoney );
    if (ptInfo.clientDiscMoney > 0) {
      text = ptInfo.clientDiscountStr;
      tmpFont.setWeight(QFont::Bold);
      tmpFont.setItalic(true);
      painter.setFont(tmpFont);
      painter.drawText(Margin, Margin + yPos , text);
      text = localeForPrinting.toString(-ptInfo.clientDiscMoney,'f',2);
      painter.drawText((printer->width()/3)+columnTotal, Margin + yPos , text);
      LINEFEED;
      tmpFont.setWeight(QFont::Normal);
      tmpFont.setItalic(false);
      painter.setFont(tmpFont);
TRACE ;
    }
TRACE ;
    //now the totals...
    //Check if space for the next text 3 lines
    if ( (Margin + yPos +fm.lineSpacing()*3) > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
    painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
    painter.setPen(normalPen);
    tmpFont.setWeight(QFont::Bold);
    painter.setFont(tmpFont);
    LINEFEED;
    text = ptInfo.thArticles; 
    painter.drawText((printer->width()/3)-columnQty-90, Margin + yPos , text);
    if (ptInfo.totDisc >0) {
      painter.drawText((printer->width()/3)+columnDisc-90, Margin+yPos, ptInfo.tDisc);
TRACE ;
    }
TRACE ;
    painter.drawText((printer->width()/3)+columnTotal-90, Margin+yPos, ptInfo.thTotals);

    // TotalTax
    DLINEFEED;
    text = QString("%1	%2").arg(ptInfo.thTotalTax).arg(ptInfo.taxes);
    painter.drawText(printer->width()-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width())-Margin, Margin + yPos, text);

    LINEFEED;
    text = QString("%1	%2").arg(ptInfo.thNetPrice).arg(ptInfo.net);
    painter.drawText(printer->width()-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width())-Margin, Margin + yPos, text);

    NLINEFEED(3); 
    
    // Paid with and change
    if ( (Margin + yPos +fm.lineSpacing()*3) > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
TRACE ;
    tmpFont.setWeight(QFont::Normal);
    painter.setFont(tmpFont);
    text = ptInfo.thPaid;
    painter.drawText(Margin, Margin + yPos , text);
    LINEFEED;

    if (ptInfo.ticketInfo.paidWithCard) {
      painter.setFont(tmpFont);
      text = ptInfo.thCard;
      painter.drawText(Margin, Margin + yPos , text);
      LINEFEED;
      painter.setFont(tmpFont);
      text = ptInfo.thCardAuth;
      painter.drawText(Margin, Margin + yPos , text);
      LINEFEED;
TRACE ;
    }
   
    //Points if is not the default user TODO: configure this to allow or not to print this info in case the store does not use points
    if (ptInfo.ticketInfo.clientid > 1 && ptInfo.ticketInfo.buyPoints > 0) { //no default user
      LINEFEED;
      QStringList strPoints = ptInfo.thPoints.split("|");
      foreach(QString strTmp, strPoints) {
        painter.drawText(Margin, Margin+yPos, strTmp);
        LINEFEED;
TRACE ;
      }
TRACE ;
    }
TRACE ;
    if ( (Margin + yPos +fm.lineSpacing()*2) > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
DBX(tickettext);
    //Random Message
    double maxL = ((printer->width())-100);
    QStringList strList;
    QString strTmp ;
    if(!tickettext.isEmpty() && !ptInfo.randomMsg.isEmpty()) strTmp = tickettext + "\n" + ptInfo.randomMsg;
    else if(!tickettext.isEmpty()) strTmp = tickettext;
    else if(!ptInfo.randomMsg.isEmpty()) strTmp = ptInfo.randomMsg;

    if (!strTmp.isEmpty()) {
      tmpFont = QFont("Bitstream Vera Sans", 16);
      tmpFont.setItalic(true);
      painter.setFont(tmpFont);
      fm = painter.fontMetrics();
      DLINEFEED;
        //fixing message lenght
        QString strCopy = strTmp;
        double realTrozos = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, strTmp).width() / maxL;
        int trozos   = realTrozos;
        double diff = (realTrozos - trozos);
        if (diff > 0.25 && trozos > 0) trozos += 1;
        int tamTrozo = 0;
        if (trozos > 0) {
          tamTrozo = (strTmp.length()/trozos);
TRACE ;
        } else {
          tamTrozo = strTmp.length();
TRACE ;
        }

TRACE ;
        QStringList otherList;
        for (int x = 1; x <= trozos; x++) {
          //we repeat for each trozo
          if (x*(tamTrozo-1) < strCopy.length())
            strCopy.insert(x*(tamTrozo-1), "|  "); //create a section
          otherList = strCopy.split("|");
TRACE ;
        }
        if (!otherList.isEmpty()) strList << otherList;
        if (trozos < 1) strList << strTmp;
        qDebug()<<"rm : Trozos:"<<trozos<<" tamTrozo:"<<tamTrozo<<" realTrozos:"<<QString::number(realTrozos,'f', 2)<<" maxL:"<<maxL<<" strTmp.width in pixels:"<<fm.size(Qt::TextExpandTabs | Qt::TextDontClip, strTmp).width()<<" diff:"<<diff;
        //end if fixing the lenght of the message.
      foreach(QString str, strList) {
        painter.drawText(Margin, Margin+yPos, str);
        LINEFEED;
TRACE ;
      }
TRACE ;
    }

TRACE ;
    // The ticket message (tanks...)
    tmpFont = QFont("Bitstream Vera Sans", 18);
    tmpFont.setItalic(true);
    painter.setPen(Qt::darkGreen);
    painter.setFont(tmpFont);
    fm = painter.fontMetrics();
DBX(ptInfo.ticketMsg);
    DLINEFEED;
    QStringList msgs = ptInfo.ticketMsg.split(QRegExp("[,;|]"));
    for (int i = 0; i < msgs.size(); ++i) {
DBX(msgs.at(i));
    	painter.drawText((printer->width()/2)-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, msgs.at(i) ).width()/2)-Margin, Margin+yPos, msgs.at(i));
        LINEFEED;
        }

    result = end();
      
  return result;
}


