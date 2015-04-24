#ifndef PRIVACYLISTS_H
#define PRIVACYLISTS_H

#include <QTimer>
#include <interfaces/ipluginmanager.h>
#include <interfaces/iprivacylists.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/ixmppstreammanager.h>
#include <interfaces/ipresencemanager.h>
#include <interfaces/irostersview.h>
#include <interfaces/imultiuserchat.h>
#include "editlistsdialog.h"

class PrivacyLists :
	public QObject,
	public IPlugin,
	public IPrivacyLists,
	public IStanzaHandler,
	public IStanzaRequestOwner
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IPrivacyLists IStanzaHandler IStanzaRequestOwner);
	Q_PLUGIN_METADATA(IID "org.vacuum-im.plugins.PrivacyLists");
public:
	PrivacyLists();
	~PrivacyLists();
	virtual QObject *instance() { return this; }
	//IPlugin
	virtual QUuid pluginUuid() const { return PRIVACYLISTS_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings() { return true; }
	virtual bool startPlugin() { return true; }
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IStanzaRequestOwner
	virtual void stanzaRequestResult(const Jid &AStreamJid, const Stanza &AStanza);
	//IPrivacyLists
	virtual bool isReady(const Jid &AStreamJid) const;
	virtual IPrivacyRule groupAutoListRule(const QString &AGroup, const QString &AAutoList) const;
	virtual IPrivacyRule contactAutoListRule(const Jid &AContactJid, const QString &AAutoList) const;
	virtual bool isGroupAutoListed(const Jid &AStreamJid, const QString &AGroup, const QString &AList) const;
	virtual bool isContactAutoListed(const Jid &AStreamJid, const Jid &AContactJid, const QString &AList) const;
	virtual void setGroupAutoListed(const Jid &AStreamJid, const QString &AGroup, const QString &AList, bool APresent);
	virtual void setContactAutoListed(const Jid &AStreamJid, const Jid &AContactJid, const QString &AList, bool APresent);
	virtual IPrivacyRule offRosterRule() const;
	virtual bool isOffRosterBlocked(const Jid &AStreamJid) const;
	virtual void setOffRosterBlocked(const Jid &AStreamJid, bool ABlocked);
	virtual bool isAutoPrivacy(const Jid &AStreamJid) const;
	virtual void setAutoPrivacy(const Jid &AStreamJid, const QString &AAutoList);
	virtual int denyedStanzas(const IRosterItem &AItem, const IPrivacyList &AList) const;
	virtual QHash<Jid,int> denyedContacts(const Jid &AStreamJid, const IPrivacyList &AList, int AFilter=IPrivacyRule::AnyStanza) const;
	virtual QString activeList(const Jid &AStreamJid, bool APending = false) const;
	virtual QString setActiveList(const Jid &AStreamJid, const QString &AList);
	virtual QString defaultList(const Jid &AStreamJid, bool APending = false) const;
	virtual QString setDefaultList(const Jid &AStreamJid, const QString &AList);
	virtual IPrivacyList privacyList(const Jid &AStreamJid, const QString &AList, bool APending = false) const;
	virtual QList<IPrivacyList> privacyLists(const Jid &AStreamJid, bool APending = false) const;
	virtual QString loadPrivacyList(const Jid &AStreamJid, const QString &AList);
	virtual QString savePrivacyList(const Jid &AStreamJid, const IPrivacyList &AList);
	virtual QString removePrivacyList(const Jid &AStreamJid, const QString &AList);
	virtual QDialog *showEditListsDialog(const Jid &AStreamJid, QWidget *AParent = NULL);
signals:
	void privacyOpened(const Jid &AStreamJid);
	void privacyClosed(const Jid &AStreamJid);
	void listLoaded(const Jid &AStreamJid, const QString &AList);
	void listRemoved(const Jid &AStreamJid, const QString &AList);
	void listAboutToBeChanged(const Jid &AStreamJid, const IPrivacyList &AList);
	void activeListAboutToBeChanged(const Jid &AStreamJid, const QString &AList);
	void activeListChanged(const Jid &AStreamJid, const QString &AList);
	void defaultListChanged(const Jid &AStreamJid, const QString &AList);
	void requestCompleted(const QString &AId);
	void requestFailed(const QString &AId, const XmppError &AError);
protected:
	QString loadPrivacyLists(const Jid &AStreamJid);
	Menu *createPrivacyMenu(Menu *AMenu) const;
	void createAutoPrivacyStreamActions(const QStringList &AStreams, Menu *AMenu) const;
	void createAutoPrivacyContactActions(const QStringList &AStreams, const QStringList &AContacts, Menu *AMenu) const;
	void createAutoPrivacyGroupActions(const QStringList &AStreams, const QStringList &AGroups, Menu *AMenu) const;
	Menu *createSetActiveMenu(const Jid &AStreamJid, const QList<IPrivacyList> &ALists, Menu *AMenu) const;
	Menu *createSetDefaultMenu(const Jid &AStreamJid, const QList<IPrivacyList> &ALists, Menu *AMenu) const;
	bool isMatchedJid(const Jid &AMask, const Jid &AJid) const;
	void sendOnlinePresences(const Jid &AStreamJid, const IPrivacyList &AAutoList);
	void sendOfflinePresences(const Jid &AStreamJid, const IPrivacyList &AAutoList);
	void setPrivacyLabel(const Jid &AStreamJid, const Jid &AContactJid, bool AVisible);
	void updatePrivacyLabels(const Jid &AStreamJid);
	bool isAllStreamsReady(const QStringList &AStreams) const;
	bool isSelectionAccepted(const QList<IRosterIndex *> &ASelected) const;
protected slots:
	void onListAboutToBeChanged(const Jid &AStreamJid, const IPrivacyList &AList);
	void onListChanged(const Jid &AStreamJid, const QString &AList);
	void onActiveListAboutToBeChanged(const Jid &AStreamJid, const QString &AList);
	void onActiveListChanged(const Jid &AStreamJid, const QString &AList);
	void onApplyAutoLists();
protected slots:
	void onXmppStreamOpened(IXmppStream *AXmppStream);
	void onXmppStreamClosed(IXmppStream *AXmppStream);
	void onRosterIndexCreated(IRosterIndex *AIndex);
	void onRostersViewIndexMultiSelection(const QList<IRosterIndex *> &ASelected, bool &AAccepted);
	void onRostersViewIndexContextMenu(const QList<IRosterIndex *> &AIndexes, quint32 ALabelId, Menu *AMenu);
	void onRostersViewIndexToolTips(IRosterIndex *AIndex, quint32 ALabelId, QMap<int, QString> &AToolTips);
	void onUpdateNewRosterIndexes();
	void onShowEditListsDialog(bool);
	void onSetActiveListByAction(bool);
	void onSetDefaultListByAction(bool);
	void onChangeStreamsAutoPrivacy(bool);
	void onChangeContactsAutoListed(bool APresent);
	void onChangeGroupsAutoListed(bool APresent);
	void onChangeStreamsOffRosterBlocked(bool ABlocked);
	void onEditListsDialogDestroyed(const Jid &AStreamJid);
	void onMultiUserChatCreated(IMultiUserChat *AMultiChat);
private:
	IXmppStreamManager *FXmppStreamManager;
	IRostersModel *FRostersModel;
	IRostersView *FRostersView;
	IRostersViewPlugin *FRostersViewPlugin;
	IStanzaProcessor *FStanzaProcessor;
	IRosterManager *FRosterManager;
	IPresenceManager *FPresenceManager;
private:
	QMap<Jid,int> FSHIPrivacy;
	QMap<Jid,int> FSHIRosterIn;
	QMap<Jid,int> FSHIRosterOut;
	QMap<QString, IPrivacyList> FSaveRequests;
	QMap<QString, QString> FLoadRequests;
	QMap<QString, QString> FActiveRequests;
	QMap<QString, QString> FDefaultRequests;
	QMap<QString, QString> FRemoveRequests;
	QMap<Jid, QStringList > FStreamRequests;
private:
	quint32 FPrivacyLabelId;
	QTimer FApplyAutoListsTimer;
	QList<IRosterIndex *> FNewRosterIndexes;
	QMap<Jid, QString> FApplyAutoLists;
	QMap<Jid, QString> FActiveLists;
	QMap<Jid, QString> FDefaultLists;
	QMap<Jid, QSet<Jid> > FLabeledContacts;
	QMap<Jid, QSet<Jid> > FOfflinePresences;
	QMap<Jid, EditListsDialog *> FEditListsDialogs;
	QMap<Jid, QMap<QString,IPrivacyList> > FPrivacyLists;
};

#endif // PRIVACYLISTS_H
