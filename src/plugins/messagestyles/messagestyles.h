#ifndef MESSAGESTYLES_H
#define MESSAGESTYLES_H

#include <definitions/optionvalues.h>
#include <definitions/optionnodes.h>
#include <definitions/optionnodeorders.h>
#include <definitions/optionwidgetorders.h>
#include <definitions/vcardvaluenames.h>
#include <definitions/menuicons.h>
#include <interfaces/ipluginmanager.h>
#include <interfaces/imessagestyles.h>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/iavatars.h>
#include <interfaces/ivcard.h>
#include <interfaces/iroster.h>
#include <interfaces/ipresence.h>
#include <interfaces/istatusicons.h>
#include <utils/message.h>
#include <utils/options.h>
#include "styleoptionswidget.h"

class MessageStyles :
			public QObject,
			public IPlugin,
			public IMessageStyles,
			public IOptionsHolder
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IMessageStyles IOptionsHolder);
public:
	MessageStyles();
	~MessageStyles();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return MESSAGESTYLES_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects() {return true; }
	virtual bool initSettings();
	virtual bool startPlugin() { return true; }
	//IOptionsHolder
	virtual IOptionsWidget *optionsWidget(const QString &ANodeId, int &AOrder, QWidget *AParent);
	//IMessageStyles
	virtual QList<QString> pluginList() const;
	virtual IMessageStylePlugin *pluginById(const QString &APluginId) const;
	virtual IMessageStyle *styleForOptions(const IMessageStyleOptions &AOptions) const;
	virtual IMessageStyleOptions styleOptions(const OptionsNode &ANode, int AMessageType) const;
	virtual IMessageStyleOptions styleOptions(int AMessageType, const QString &AContext = QString::null) const;
	//Other functions
	virtual QString userAvatar(const Jid &AContactJid) const;
	virtual QString userName(const Jid &AStreamJid, const Jid &AContactJid = Jid()) const;
	virtual QString userIcon(const Jid &AStreamJid, const Jid &AContactJid = Jid()) const;
	virtual QString userIcon(const Jid &AContactJid, int AShow, const QString &ASubscription, bool AAsk) const;
	virtual QString timeFormat(const QDateTime &AMessageTime, const QDateTime &ACurTime = QDateTime::currentDateTime()) const;
signals:
	void styleOptionsChanged(const IMessageStyleOptions &AOptions, int AMessageType, const QString &AContext) const;
protected:
	void appendPendingChanges(int AMessageType, const QString &AContext);
protected slots:
	void onVCardChanged(const Jid &AContactJid);
	void onOptionsChanged(const OptionsNode &ANode);
	void onApplyPendingChanges();
private:
	IAvatars *FAvatars;
	IStatusIcons *FStatusIcons;
	IVCardPlugin *FVCardPlugin;
	IRosterPlugin *FRosterPlugin;
	IOptionsManager *FOptionsManager;
private:
	mutable QMap<Jid, QString> FStreamNames;
	QList< QPair<int,QString> > FPendingChages;
	QMap<QString, IMessageStylePlugin *> FStylePlugins;
};

#endif // MESSAGESTYLES_H