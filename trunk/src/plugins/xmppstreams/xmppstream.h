#ifndef XMPPSTREAM_H
#define XMPPSTREAM_H

#include <QTimer>
#include <QMutex>
#include <QMultiMap>
#include <QDomDocument>
#include <QInputDialog>
#include <definitions/namespaces.h>
#include <definitions/xmppstanzahandlerorders.h>
#include <interfaces/ixmppstreams.h>
#include <interfaces/iconnectionmanager.h>
#include <utils/errorhandler.h>
#include <utils/versionparser.h>
#include "streamparser.h"

enum StreamState
{
	SS_OFFLINE,
	SS_CONNECTING,
	SS_INITIALIZE,
	SS_FEATURES,
	SS_ONLINE,
	SS_DISCONNECTING,
	SS_ERROR
};

class XmppStream :
			public QObject,
			public IXmppStream,
			public IXmppStanzaHadler
{
	Q_OBJECT;
	Q_INTERFACES(IXmppStream IXmppStanzaHadler);
public:
	XmppStream(IXmppStreams *AXmppStreams, const Jid &AStreamJid);
	~XmppStream();
	virtual QObject *instance() { return this; }
	//IXmppStanzaHandler
	virtual bool xmppStanzaIn(IXmppStream *AXmppStream, Stanza &AStanza, int AOrder);
	virtual bool xmppStanzaOut(IXmppStream *AXmppStream, Stanza &AStanza, int AOrder);
	//IXmppStream
	virtual bool isOpen() const;
	virtual bool open();
	virtual void close();
	virtual void abort(const QString &AError);
	virtual QString streamId() const;
	virtual QString errorString() const;;
	virtual Jid streamJid() const;
	virtual void setStreamJid(const Jid &AJid);
	virtual QString password() const;
	virtual void setPassword(const QString &APassword);
	virtual QString getSessionPassword(bool AAskIfNeed = true);
	virtual QString defaultLang() const;
	virtual void setDefaultLang(const QString &ADefLang);
	virtual bool isEncryptionRequired() const;
	virtual void setEncryptionRequired(bool ARequire);
	virtual IConnection *connection() const;
	virtual void setConnection(IConnection *AConnection);
	virtual bool isKeepAliveTimerActive() const;
	virtual void setKeepAliveTimerActive(bool AActive);
	virtual qint64 sendStanza(Stanza &AStanza);
	virtual void insertXmppDataHandler(int AOrder, IXmppDataHandler *AHandler);
	virtual void removeXmppDataHandler(int AOrder, IXmppDataHandler *AHandler);
	virtual void insertXmppStanzaHandler(int AOrder, IXmppStanzaHadler *AHandler);
	virtual void removeXmppStanzaHandler(int AOrder, IXmppStanzaHadler *AHandler);
signals:
	void opened();
	void aboutToClose();
	void closed();
	void error(const QString &AError);
	void jidAboutToBeChanged(const Jid &AAfter);
	void jidChanged(const Jid &ABefore);
	void connectionChanged(IConnection *AConnection);
	void dataHandlerInserted(int AOrder, IXmppDataHandler *AHandler);
	void dataHandlerRemoved(int AOrder, IXmppDataHandler *AHandler);
	void stanzaHandlerInserted(int AOrder, IXmppStanzaHadler *AHandler);
	void stanzaHandlerRemoved(int AOrder, IXmppStanzaHadler *AHandler);
	void streamDestroyed();
protected:
	void startStream();
	void processFeatures();
	void clearActiveFeatures();
	void setStreamState(StreamState AState);
	bool startFeature(const QString &AFeatureNS, const QDomElement &AFeatureElem);
	bool processDataHandlers(QByteArray &AData, bool ADataOut);
	bool processStanzaHandlers(Stanza &AStanza, bool AStanzaOut);
	qint64 sendData(QByteArray AData);
	QByteArray receiveData(qint64 ABytes);
protected slots:
	//IStreamConnection
	void onConnectionConnected();
	void onConnectionReadyRead(qint64 ABytes);
	void onConnectionError(const QString &AError);
	void onConnectionDisconnected();
	//StreamParser
	void onParserOpened(QDomElement AElem);
	void onParserElement(QDomElement AElem);
	void onParserError(const QString &AError);
	void onParserClosed();
	//IXmppFeature
	void onFeatureFinished(bool ARestart);
	void onFeatureError(const QString &AError);
	void onFeatureDestroyed();
	//KeepAlive
	void onKeepAliveTimeout();
private:
	IXmppStreams *FXmppStreams;
	IConnection *FConnection;
private:
	QDomElement FServerFeatures;
	QList<QString>	FAvailFeatures;
	QList<IXmppFeature *> FActiveFeatures;
private:
	QMultiMap<int, IXmppDataHandler *> FDataHandlers;
	QMultiMap<int, IXmppStanzaHadler *> FStanzaHandlers;
private:
	bool FOpen;
	bool FEncrypt;
	Jid FStreamJid;
	Jid FOfflineJid;
	QString FStreamId;
	QString FPassword;
	QString FDefLang;
	QString FErrorString;
	StreamParser FParser;
	QTimer FKeepAliveTimer;
	StreamState FStreamState;
private:
	QMutex FPasswordMutex;
	QString FSessionPassword;
	QInputDialog *FPasswordDialog;
};

#endif // XMPPSTREAM_H
