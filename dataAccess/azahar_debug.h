#define query_exec(X,S) query_execDBG(__FI##LE__,__LI##NE__,__FUNC##TION__,&X,S)
#define query_exec_s(X) query_execDBG_S(__FI##LE__,__LI##NE__,__FUNC##TION__,&X)
	
#define query_bindValueDbgOut(V,X,Y) query_bindValueDbg(__FI##LE__,__LI##NE__,__FUNC##TION__,V,X,Y)

#define query_bindValueDbg(FI,L,FU,V,X,Y) \
	qDebug() << "SQL:bindValue" << FI << L << FU << #V << X << Y; V.bindValue(X,Y)

#define query_prepare(V,X) query_prepareDbg(__FI##LE__,__LI##NE__,__FUNC##TION__,V,X)

#define query_prepareDbg(FI,L,FU,V,X) \
	qDebug() << "SQL:prepare" << FI << L << FU << #V << X ; V.prepare(X)

#define Q_EXEC_S q.exec();

int query_execDBG_S(QString fi,int L,QString fu,QSqlQuery *Q)
	{
	int retval=Q->exec();
	qDebug() << "SQL:exec_s" << fi << L << fu <<"="<< retval;
	return retval;
	}
int query_execDBG(QString fi,int L,QString fu,QSqlQuery *Q, QString S)
	{
	int retval=Q->exec(S);
	qDebug() << "SQL:exec" << fi << L << fu << S <<"="<< retval ;
	return retval;
	}
/*
#1,$s:myQuery.prepare(:query_prepare(myQuery,:g
#1,$s:qryTrans.prepare(:query_prepare(qryTrans,:g
#1,$s:query2.prepare(:query_prepare(query2,:g
#1,$s:queryBalance.prepare(:query_prepare(queryBalance,:g
#1,$s:query.prepare(:query_prepare(query,:g
*/


/*
:1,$s:myQuery.exec():query_exec_s(myQuery):g
:1,$s:qC.exec():query_exec_s(qC):g
:1,$s:q.exec():query_exec_s(q):g
:1,$s:qry.exec():query_exec_s(qry):g
:1,$s:query2.exec():query_exec_s(query2):g
:1,$s:query3.exec():query_exec_s(query3):g
:1,$s:query.exec():query_exec_s(query):g
:1,$s:query.exec():query_exec_s(query):g
:1,$s:queryId.exec():query_exec_s(queryId):g
:1,$s:queryUname.exec():query_exec_s(queryUname):g
:1,$s:myQuery.exec(:query_exec(myQuery,:g
:1,$s:qC.exec(:query_exec(qC,:g
:1,$s:q.exec(:query_exec(q,:g
:1,$s:qry.exec(:query_exec(qry,:g
:1,$s:query2.exec(:query_exec(query2,:g
:1,$s:query3.exec(:query_exec(query3,:g
:1,$s:query.exec(:query_exec(query,:g
:1,$s:queryId.exec(:query_exec(queryId,:g
:1,$s:queryUname.exec(:query_exec(queryUname,:g
*/
