#ifndef CHATSTATES_H
#define CHATSTATES_H

#include <QMap>
#include <QTimer>
#include <interfaces/ipluginmanager.h>
#include <interfaces/ichatstates.h>
#include <interfaces/ipresencemanager.h>
#include <interfaces/istanzaprocessor.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/iservicediscovery.h>
#include <interfaces/imessagearchiver.h>
#include <interfaces/idataforms.h>
#include <interfaces/inotifications.h>
#include <interfaces/isessionnegotiation.h>
#include <interfaces/imultiuserchat.h>
#include "statewidget.h"

struct ChatParams
{
	ChatParams() {
		userState = IChatStates::StateUnknown;
		selfState = IChatStates::StateUnknown;
		notifyId = 0;
		selfLastActive = 0;
		canSendStates = false;
	}
	int userState;
	int selfState;
	int notifyId;
	uint selfLastActive;
	bool canSendStates;
};

class ChatStates :
	public QObject,
	public IPlugin,
	public IChatStates,
	public IStanzaHandler,
	public IArchiveHandler,
	public IOptionsDialogHolder,
	public ISessionNegotiator
{
	Q_OBJECT;
	Q_INTERFACES(IPlugin IChatStates IStanzaHandler IArchiveHandler IOptionsDialogHolder ISessionNegotiator);
public:
	ChatStates();
	~ChatStates();
	//IPlugin
	virtual QObject *instance() { return this; }
	virtual QUuid pluginUuid() const { return CHATSTATES_UUID; }
	virtual void pluginInfo(IPluginInfo *APluginInfo);
	virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
	virtual bool initObjects();
	virtual bool initSettings();
	virtual bool startPlugin();
	//IArchiveHandler
	virtual bool archiveMessageEdit(int AOrder, const Jid &AStreamJid, Message &AMessage, bool ADirectionIn);
	//IOptionsHolder
	virtual QMultiMap<int, IOptionsDialogWidget *> optionsDialogWidgets(const QString &ANodeId, QWidget *AParent);
	//ISessionNegotiator
	virtual int sessionInit(const IStanzaSession &ASession, IDataForm &ARequest);
	virtual int sessionAccept(const IStanzaSession &ASession, const IDataForm &ARequest, IDataForm &ASubmit);
	virtual int sessionApply(const IStanzaSession &ASession);
	virtual void sessionLocalize(const IStanzaSession &ASession, IDataForm &AForm);
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IChatStates
	virtual int permitStatus(const Jid &AContactJid) const;
	virtual void setPermitStatus(const Jid &AContactJid, int AStatus);
	virtual bool isEnabled(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual bool isSupported(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual int userChatState(const Jid &AStreamJid, const Jid &AContactJid) const;
	virtual int selfChatState(const Jid &AStreamJid, const Jid &AContactJid) const;
signals:
	void permitStatusChanged(const Jid &AContactJid, int AStatus) const;
	void supportStatusChanged(const Jid &AStreamJid, const Jid &AContactJid, bool ASupported) const;
	void userChatStateChanged(const Jid &AStreamJid, const Jid &AContactJid, int AState) const;
	void selfChatStateChanged(const Jid &AStreamJid, const Jid &AContactJid, int AState) const;
protected:
	bool isSendingPossible(const Jid &AStreamJid, const Jid &AContactJid) const;
	void sendStateMessage(const Jid &AStreamJid, const Jid &AContactJid, int AState) const;
	void resetSupported(const Jid &AContactJid = Jid::null);
	void setSupported(const Jid &AStreamJid, const Jid &AContactJid, bool ASupported);
	void setUserState(const Jid &AStreamJid, const Jid &AContactJid, int AState);
	void setSelfState(const Jid &AStreamJid, const Jid &AContactJid, int AState, bool ASend = true);
	void notifyUserState(const Jid &AStreamJid, const Jid &AContactJid);
	void registerDiscoFeatures();
protected slots:
	void onPresenceOpened(IPresence *APresence);
	void onPresenceItemReceived(IPresence *APresence, const IPresenceItem &AItem, const IPresenceItem &ABefore);
	void onPresenceClosed(IPresence *APresence);
	void onMultiUserChatCreated(IMultiUserChat *AMultiChat);
	void onMultiUserPresenceReceived(IMultiUser *AUser, int AShow, const QString &AStatus);
	void onChatWindowCreated(IMessageChatWindow *AWindow);
	void onChatWindowActivated();
	void onChatWindowTextChanged();
	void onChatWindowClosed();
	void onChatWindowDestroyed(IMessageChatWindow *AWindow);
	void onUpdateSelfStates();
	void onOptionsOpened();
	void onOptionsClosed();
	void onOptionsChanged(const OptionsNode &ANode);
	void onStanzaSessionTerminated(const IStanzaSession &ASession);
private:
	IPresenceManager *FPresenceManager;
	IMessageWidgets *FMessageWidgets;
	IStanzaProcessor *FStanzaProcessor;
	IOptionsManager *FOptionsManager;
	IServiceDiscovery *FDiscovery;
	IMessageArchiver *FMessageArchiver;
	IDataForms *FDataForms;
	INotifications *FNotifications;
	ISessionNegotiation *FSessionNegotiation;
	IMultiUserChatManager *FMultiChatManager;
private:
	QMap<Jid,int> FSHIMessagesIn;
	QMap<Jid,int> FSHIMessagesOut;
private:
	QTimer FUpdateTimer;
	QMap<Jid, int> FPermitStatus;
	QMap<Jid, QList<Jid> > FNotSupported;
	QMap<Jid, QMap<Jid, ChatParams> > FChatParams;
	QMap<Jid, QMap<Jid, QString> > FStanzaSessions;
	QMap<QTextEdit *, IMessageChatWindow *> FChatByEditor;
};

#endif // CHATSTATES_H
