bool PrintCUPS::printBigTicket(const PrintTicketInfo &ptInfo, QPrinter &pr,const QString tickettext)
{
  bool WithSerialNr=false;
  bool result = false;
  QFont sectionHeader = QFont("Bitstream Vera Sans", 14);
  
  start(&pr,20,QFont("Impact", 38));
  
  // Header: Store Name, Store Address, Store Phone, Store Logo...
  painter.setFont(header);
  QString text = ptInfo.sd.storeName;
  textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
  painter.drawText(Margin,Margin*2+textWidth.height(), text);
  LINEFEED;
  // Store Address
  QFont tmpFont = QFont("Bitstream Vera Sans", 10);
  tmpFont.setItalic(true);
  painter.setFont(tmpFont);
  fm = painter.fontMetrics();
  painter.setPen(Qt::darkGray);
  painter.drawText(Margin, Margin*2 + yPos + textWidth.height() -5, printer->width(), fm.lineSpacing(), Qt::TextExpandTabs | Qt::TextDontClip, ptInfo.sd.storeAddr + ", " +ptInfo.thPhone + ptInfo.sd.storePhone);
  LINEFEED;
  // Store Logo
  painter.drawPixmap(printer->width() - ptInfo.sd.storeLogo.width() - Margin, Margin, ptInfo.sd.storeLogo);
  // Header line
  painter.setPen(QPen(Qt::gray, 5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, 105, printer->width()-Margin, 105);
  yPos = yPos + 3 * fm.lineSpacing(); // 3times the height of the line
  // Ticket Number, Date
  LINEFEED;
  painter.setPen(normalPen);
  text = ptInfo.thDate;
  textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
  painter.drawText(printer->width()-Margin-textWidth.width(), THE_LINE, text);
  LINEFEED;
  // Vendor name, terminal number
  text = ptInfo.salesPerson + ", " + ptInfo.terminal;
  textWidth = fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text);
  painter.drawText(printer->width()-Margin-textWidth.width(), THE_LINE, text);
  yPos = yPos + 3*fm.lineSpacing();

  for (int i = 0; i < ptInfo.ticketInfo.lines.size(); ++i) {
    TicketLineInfo tLine = ptInfo.ticketInfo.lines.at(i);
    if(!tLine.serialNo.isEmpty()) {
        WithSerialNr=true;
        }
    }

//print clientdata if a procuct with serialnumber has been sold
  if(WithSerialNr  && ptInfo.ticketInfo.clientid > 1) {
     NLINEFEED(3);
     QStringList clientLines=ptInfo.ticketInfo.clientStr.split(QRegExp("[\n,;]"));
     foreach(QString strTmp, clientLines) {
//      painter.drawText((printer->width()/2)-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, strTmp).width()/2)-Margin, Margin+yPos, strTmp);
        painter.drawText(Margin, Margin+yPos, strTmp); //first product description...
        LINEFEED;
        }
     NLINEFEED(3);
     }
 
  // Products Subheader
  int columnQty  = 0;
  if (ptInfo.totDisc <= 0) {
    columnQty  = 90;
TRACE ;
  }
  int columnPrice= columnQty+100;
  int columnDisc = columnPrice+100;
  int columnTotal = 0;
  if ( ptInfo.totDisc >0 )
    columnTotal = columnDisc+100;
  else
    columnTotal = columnPrice+100;
  
  painter.setPen(Qt::darkBlue);
  tmpFont = QFont("Bitstream Vera Sans", 10 );
  tmpFont.setWeight(QFont::Bold);
  painter.setFont(tmpFont);
  painter.drawText(Margin,Margin+yPos, ptInfo.thProduct);
  text = ptInfo.thQty;
  painter.drawText(printer->width()/2+columnQty, Margin + yPos, text);
  text = ptInfo.thPrice;
  painter.drawText(printer->width()/2+columnPrice, Margin + yPos, text);
  text = ptInfo.thDiscount;
  if (ptInfo.totDisc > 0)
    painter.drawText(printer->width()/2+columnDisc, Margin + yPos, text);
  text = ptInfo.thTotal;
  painter.drawText(printer->width()/2+columnTotal, Margin + yPos, text);
  LINEFEED;
  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);
  painter.setFont(QFont("Bitstream Vera Sans", 10 ));
  LINEFEED;
  // End of Header Information.
  
  // Content : Each product
  QLocale localeForPrinting;
  for (int i = 0; i < ptInfo.ticketInfo.lines.size(); ++i)
  {
    //Check if space for the next text line
    if ( Margin + yPos > printer->height() - Margin ) {
      formfeed();
TRACE ;
    }
    TicketLineInfo tLine = ptInfo.ticketInfo.lines.at(i);
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
    if (iqty.endsWith(".00") || iqty.endsWith(",00")) { iqty.chop(3); iqty += "   ";}//we chop the trailing zeroes...
    if (itax.endsWith(".00") || itax.endsWith(",00")) { itax.chop(3); }//we chop the trailing zeroes...
    iqty = iqty+" "+tLine.unitStr;
    QString idiscount =  localeForPrinting.toString(-(tLine.qty*tLine.disc),'f',2);
    if (tLine.disc <= 0) idiscount = ""; // dont print 0.0
    QString idue =  localeForPrinting.toString(tLine.total,'f',2);
    if ( ptInfo.totDisc > 0 )
      while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, idesc).width() >= ((printer->width()/2)-Margin-40)) { idesc.chop(2); }
    else
      while (fm.size(Qt::TextExpandTabs | Qt::TextDontClip, idesc).width() >= ((printer->width()/2)-Margin-40+90)) { idesc.chop(2); }
    painter.drawText(Margin, Margin+yPos, idesc); //first product description...
    text = iqty+" x " + iprice;
    painter.drawText(printer->width()/2+columnQty, Margin+yPos, text);
    text = iprice;
    painter.drawText(printer->width()/2+columnPrice, Margin+yPos, text);
    if ( ptInfo.totDisc > 0 ) {
DBX(idiscount);
      text = idiscount;
      if (text == "0.0") text = ""; //dont print a 0.0 !!!
      painter.drawText(printer->width()/2+columnDisc, Margin+yPos, text);
    }
DBX(idue);
    painter.drawText(printer->width()/2+columnTotal, Margin+yPos, idue);
TRACE ;
  //taxes
    text = itax+"%";
DBX(text);
    painter.drawText(printer->width()-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, text).width())-Margin, Margin + yPos, text);

    LINEFEED;

  //serialnr
    if(!tLine.serialNo.isEmpty()) {
        WithSerialNr=true;

	text = ptInfo.thSerialNo + " " + tLine.serialNo;
  	painter.drawText(4*Margin, Margin + yPos, text);
	LINEFEED;
	}
    
  } //for each item

    //Check for client discount
DBX( ptInfo.clientDiscMoney );
    if (ptInfo.clientDiscMoney > 0.001) {
DBX( ptInfo.clientDiscMoney );
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
  
  //now the totals...
  //Check if space for the next text 3 lines
  if ( (Margin + yPos +fm.lineSpacing()*3) > printer->height() - Margin ) {
    formfeed();
TRACE ;
  }

  painter.setPen(QPen(Qt::darkGray, 1, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
  painter.drawLine(Margin, Margin + yPos - 8, printer->width()-Margin, Margin + yPos - 8);
  painter.setPen(normalPen);
  painter.setFont(tmpFont);
  LINEFEED;
  if ( ptInfo.totDisc >0 )
    painter.drawText((printer->width()/2)+columnDisc, Margin + yPos , ptInfo.tDisc);
  painter.drawText((printer->width()/2)+columnQty, Margin + yPos , ptInfo.thArticles);
  painter.drawText((printer->width()/2)+columnTotal, Margin+yPos, ptInfo.thTotals);
  DLINEFEED;

//TotalTax 
  painter.drawText((printer->width()/2)+columnQty, Margin+yPos, ptInfo.thTotalTax);
  painter.drawText((printer->width()/2)+columnTotal, Margin+yPos, ptInfo.taxes);
  LINEFEED;

//without Tax 
  painter.drawText((printer->width()/2)+columnQty, Margin+yPos, ptInfo.thNetPrice);
  painter.drawText((printer->width()/2)+columnTotal, Margin+yPos, ptInfo.net);
  NLINEFEED(3);

 //ticket message.
 if(!tickettext.isEmpty()) {
	painter.drawText((printer->width()/2)-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, tickettext ).width()/2)-Margin, Margin+yPos, tickettext);
	NLINEFEED(2);
	}

 tmpFont = QFont("Bitstream Vera Sans", 12);
 tmpFont.setItalic(true);
 painter.setPen(Qt::darkGreen);
 painter.setFont(tmpFont);
 QStringList msgs = ptInfo.ticketMsg.split(QRegExp("[,;|]"));
DBX(msgs);

 for (int i = 0; i < msgs.size(); ++i) {
  	painter.drawText((printer->width()/2)-(fm.size(Qt::TextExpandTabs | Qt::TextDontClip, msgs.at(i) ).width()/2)-Margin, Margin+yPos, msgs.at(i));
        LINEFEED;
        }

TRACE ;
 
 result = end();
 
 return result;
 }


