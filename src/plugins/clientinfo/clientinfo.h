#ifndef CLIENTINFO_H
#define CLIENTINFO_H

#include <QSet>
#include <QPointer>
#include <interfaces/ipluginmanager.h>
#include <interfaces/iclientinfo.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/irostermanager.h>
#include <interfaces/ipresencemanager.h>
#include <interfaces/irostersview.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/imainwindow.h>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/istatusicons.h>
#include "clientinfodialog.h"

struct SoftwareItem {
	SoftwareItem() { 
		status = IClientInfo::SoftwareNotLoaded; 
	}
	QString name;
	QString version;
	QString os;
	int status;
};

struct ActivityItem {
	QDateTime requestTime;
	QDateTime dateTime;
	QString text;
};

struct TimeItem {
	TimeItem() { 
		ping = -1;
		delta = 0;
		zone = 0;
	}
	int ping;
	int delta;
	int zone;
};

class ClientInfo :
	public QObject,
	public IPlugin,
	public IClientInfo,
	public IOptionsDialogHolder,
	public IStanzaHandler,
	public IStanzaRequestOwner,
	public IDataLocalizer,
	public IDiscoFeatureHandler
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IClientInfo IOptionsDialogHolder IStanzaHandler IStanzaRequestOwner IDataLocalizer IDiscoFeatureHandler);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.ClientInfo");
public:
	ClientInfo();
	~ClientInfo();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return CLIENTINFO_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin();
	//IOptionsHolder
	virtual QMultiMap<int, IOptionsDialogWidget *> optionsDialogWidgets(const QString &ANodeId, QWidget *AParent);
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IStanzaRequestOwner
	virtual void stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza);
	//IDataLocalizer
	virtual IDataFormLocale dataFormLocale(const QString &AFormType);
	//IDiscoFeatureHandler
	virtual bool execDiscoFeature(const Jid &AStreamJid, const QString &AFeature, const IDiscoInfo &ADiscoInfo);
	virtual Action *createDiscoFeatureAction(const Jid &AStreamJid, const QString &AFeature, const IDiscoInfo &ADiscoInfo, QWidget *AParent);
	//IClientInfo
	virtual QString osVersion() const;
	virtual void showClientInfo(const Jid &AStreamJid, const Jid &AContactJid, int AInfoTypes);
	//Software Version
	virtual bool hasSoftwareInfo(const Jid &AContactJid) const;
	virtual bool requestSoftwareInfo( const Jid &AStreamJid, const Jid &AContactJid);
	virtual int softwareStatus(const Jid &AContactJid) const;
	virtual QString softwareName(const Jid &AContactJid) const;
	virtual QString softwareVersion(const Jid &AContactJid) const;
	virtual QString softwareOs(const Jid &AContactJid) const;
	//Last Activity
	virtual bool hasLastActivity(const Jid &AContactJid) const;
	virtual bool requestLastActivity(const Jid &AStreamJid, const Jid &AContactJid);
	virtual QDateTime lastActivityTime(const Jid &AContactJid) const;
	virtual QString lastActivityText(const Jid &AContactJid) const;
	//Entity Time
	virtual bool hasEntityTime(const Jid &AContactJid) const;
	virtual bool requestEntityTime(const Jid &AStreamJid, const Jid &AContactJid);
	virtual QDateTime entityTime(const Jid &AContactJid) const;
	virtual int entityTimeDelta(const Jid &AContactJid) const;
	virtual int entityTimePing(const Jid &AContactJid) const;
signals:
	//IClientInfo
	void softwareInfoChanged(const Jid &AContactJid);
	void lastActivityChanged(const Jid &AContactJid);
	void entityTimeChanged(const Jid &AContactJid);
protected:
	Action *createInfoAction(const Jid &AStreamJid, const Jid &AContactJid, const QString &AFeature, QObject *AParent) const;
	void deleteSoftwareDialogs(const Jid &AStreamJid);
	void registerDiscoFeatures();
protected slots:
	void onContactStateChanged(const Jid &AStreamJid, const Jid &AContactJid, bool AStateOnline);
	void onRostersViewIndexContextMenu(const QList<IRosterIndex *> &AIndexes, quint32 ALabelId, Menu *AMenu);
	void onClientInfoActionTriggered(bool);
	void onClientInfoDialogClosed(const Jid &AContactJid);
	void onRosterActiveChanged(IRoster *ARoster, bool AActive);
	void onDiscoInfoReceived(const IDiscoInfo &AInfo);
	void onOptionsChanged(const OptionsNode &ANode);
private:
	IPluginManager *FPluginManager;
	IRosterManager *FRosterManager;
	IPresenceManager *FPresenceManager;
	IStanzaProcessor *FStanzaProcessor;
	IRostersViewPlugin *FRostersViewPlugin;
	IServiceDiscovery *FDiscovery;
	IDataForms *FDataForms;
	IOptionsManager *FOptionsManager;
	IStatusIcons *FStatusIcons;
private:
	int FPingHandle;
	int FTimeHandle;
	int FVersionHandle;
	int FActivityHandler;
	QMap<QString, Jid> FSoftwareId;
	QMap<Jid, SoftwareItem> FSoftwareItems;
	QMap<QString, Jid> FActivityId;
	QMap<Jid, ActivityItem> FActivityItems;
	QMap<QString, Jid> FTimeId;
	QMap<Jid, TimeItem> FTimeItems;
	QMap<Jid, ClientInfoDialog *> FClientInfoDialogs;
};

#endif // CLIENTINFO_H
