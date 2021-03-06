#ifndef MULTIUSERCHATWINDOW_H
#define MULTIUSERCHATWINDOW_H

#include <QStandardItemModel>
#include <interfaces/imultiuserchat.h>
#include <interfaces/imessagewidgets.h>
#include <interfaces/imessageprocessor.h>
#include <interfaces/imessagestylemanager.h>
#include <interfaces/imessagearchiver.h>
#include <interfaces/ixmppstreammanager.h>
#include <interfaces/irostersview.h>
#include <interfaces/istatusicons.h>
#include <interfaces/istatuschanger.h>
#include <interfaces/irostermanager.h>
#include <interfaces/ipresencemanager.h>
#include <interfaces/irecentcontacts.h>
#include <interfaces/istanzaprocessor.h>
#include "edituserslistdialog.h"
#include "inputtextdialog.h"
#include "usersproxymodel.h"
#include "ui_multiuserchatwindow.h"

struct WindowStatus {
	QDateTime startTime;
	QDateTime createTime;
	QDate lastDateSeparator;
};

struct WindowContent {
	QString html;
	IMessageStyleContentOptions options;
};

struct UserStatus {
	QString lastStatusShow;
};

class MultiUserChatWindow :
	public QMainWindow,
	public IMultiUserChatWindow,
	public IStanzaHandler,
	public IMessageHandler,
	public IMessageEditSendHandler
{
	Q_OBJECT;
	Q_INTERFACES(IMessageWindow IMultiUserChatWindow IMessageTabPage IStanzaHandler IMessageHandler IMessageEditSendHandler);
public:
	MultiUserChatWindow(IMultiUserChatManager *AChatPlugin, IMultiUserChat *AMultiChat);
	~MultiUserChatWindow();
	virtual QMainWindow *instance() { return this; }
	//IMessageWindow
	virtual Jid streamJid() const;
	virtual Jid contactJid() const;
	virtual IMessageAddress *address() const;
	virtual IMessageInfoWidget *infoWidget() const;
	virtual IMessageViewWidget *viewWidget() const;
	virtual IMessageEditWidget *editWidget() const;
	virtual IMessageMenuBarWidget *menuBarWidget() const;
	virtual IMessageToolBarWidget *toolBarWidget() const;
	virtual IMessageStatusBarWidget *statusBarWidget() const;
	virtual IMessageReceiversWidget *receiversWidget() const;
	//ITabWindowPage
	virtual QString tabPageId() const;
	virtual bool isVisibleTabPage() const;
	virtual bool isActiveTabPage() const;
	virtual void assignTabPage();
	virtual void showTabPage();
	virtual void showMinimizedTabPage();
	virtual void closeTabPage();
	virtual QIcon tabPageIcon() const;
	virtual QString tabPageCaption() const;
	virtual QString tabPageToolTip() const;
	virtual IMessageTabPageNotifier *tabPageNotifier() const;
	virtual void setTabPageNotifier(IMessageTabPageNotifier *ANotifier);
	//IStanzaHandler
	virtual bool stanzaReadWrite(int AHandlerId, const Jid &AStreamJid, Stanza &AStanza, bool &AAccept);
	//IMessageEditSendHandler
	virtual bool messageEditSendPrepare(int AOrder, IMessageEditWidget *AWidget);
	virtual bool messageEditSendProcesse(int AOrder, IMessageEditWidget *AWidget);
	//IMessageHandler
	virtual bool messageCheck(int AOrder, const Message &AMessage, int ADirection);
	virtual bool messageDisplay(const Message &AMessage, int ADirection);
	virtual INotification messageNotify(INotifications *ANotifications, const Message &AMessage, int ADirection);
	virtual bool messageShowWindow(int AMessageId);
	virtual bool messageShowWindow(int AOrder, const Jid &AStreamJid, const Jid &AContactJid, Message::MessageType AType, int AShowMode);
	//IMultiUserChatWindow
	virtual IMultiUserChat *multiUserChat() const;
	virtual IMessageChatWindow *openChatWindow(const Jid &AContactJid);
	virtual IMessageChatWindow *findChatWindow(const Jid &AContactJid) const;
	virtual void contextMenuForRoom(Menu *AMenu);
	virtual void contextMenuForUser(IMultiUser *AUser, Menu *AMenu);
	virtual void toolTipsForUser(IMultiUser *AUser, QMap<int,QString> &AToolTips);
	virtual void exitAndDestroy(const QString &AStatus, int AWaitClose = 15000);
signals:
	//ITabWindowPage
	void tabPageAssign();
	void tabPageShow();
	void tabPageShowMinimized();
	void tabPageClose();
	void tabPageClosed();
	void tabPageChanged();
	void tabPageActivated();
	void tabPageDeactivated();
	void tabPageDestroyed();
	void tabPageNotifierChanged();
	//IMessageWindow
	void widgetLayoutChanged();
	//IMultiUserChatWindow
	void multiChatContextMenu(Menu *AMenu);
	void multiUserContextMenu(IMultiUser *AUser, Menu *AMenu);
	void multiUserToolTips(IMultiUser *AUser, QMap<int,QString> &AToolTips);
	void privateChatWindowCreated(IMessageChatWindow *AWindow);
	void privateChatWindowDestroyed(IMessageChatWindow *AWindow);
protected:
	void connectMultiChatSignals();
	void createMessageWidgets();
	void createStaticRoomActions();
	void saveWindowState();
	void loadWindowState();
	void saveWindowGeometry();
	void loadWindowGeometry();
	void refreshCompleteNicks();
	void updateListItem(const Jid &AContactJid);
	void updateRecentItemActiveTime();
	void highlightUserRole(IMultiUser *AUser);
	void highlightUserAffiliation(IMultiUser *AUser);
	bool execShortcutCommand(const QString &AText);
	void showDateSeparator(IMessageViewWidget *AView, const QDateTime &ADateTime);
protected:
	bool isMentionMessage(const Message &AMessage) const;
	void setMultiChatMessageStyle();
	void showMultiChatTopic(const QString &ATopic, const QString &ANick = QString::null);
	void showMultiChatStatusMessage(const QString &AMessage, int AType=0, int AStatus=0, bool ADontSave=false, const QDateTime &ATime=QDateTime::currentDateTime());
	bool showMultiChatStatusCodes(const QList<int> &ACodes, const QString &ANick=QString::null, const QString &AMessage=QString::null);
	void showMultiChatUserMessage(const Message &AMessage, const QString &ANick);
	void requestMultiChatHistory();
	void updateMultiChatWindow();
	void removeMultiChatActiveMessages();
protected:
	IMessageChatWindow *getPrivateChatWindow(const Jid &AContactJid);
	void setPrivateChatMessageStyle(IMessageChatWindow *AWindow);
	void fillPrivateChatContentOptions(IMessageChatWindow *AWindow, IMessageStyleContentOptions &AOptions) const;
	void showPrivateChatStatusMessage(IMessageChatWindow *AWindow, const QString &AMessage, int AStatus=0, const QDateTime &ATime=QDateTime::currentDateTime());
	void showPrivateChatMessage(IMessageChatWindow *AWindow, const Message &AMessage);
	void requestPrivateChatHistory(IMessageChatWindow *AWindow);
	void updatePrivateChatWindow(IMessageChatWindow *AWindow);
	void removePrivateChatActiveMessages(IMessageChatWindow *AWindow);
protected:
	bool event(QEvent *AEvent);
	void showEvent(QShowEvent *AEvent);
	void closeEvent(QCloseEvent *AEvent);
	bool eventFilter(QObject *AObject, QEvent *AEvent);
protected slots:
	void onChatAboutToConnect();
	void onChatOpened();
	void onChatNotify(const QString &ANotify);
	void onChatError(const QString &AMessage);
	void onChatClosed();
	void onRoomNameChanged(const QString &AName);
	//Occupant
	void onUserPresence(IMultiUser *AUser, int AShow, const QString &AStatus);
	void onUserDataChanged(IMultiUser *AUser, int ARole, const QVariant &ABefore, const QVariant &AAfter);
	void onUserNickChanged(IMultiUser *AUser, const QString &AOldNick, const QString &ANewNick);
	void onPresenceChanged(int AShow, const QString &AStatus);
	void onSubjectChanged(const QString &ANick, const QString &ASubject);
	void onServiceMessageReceived(const Message &AMessage);
	void onInviteDeclined(const Jid &AContactJid, const QString &AReason);
	//Moderator
	void onUserKicked(const QString &ANick, const QString &AReason, const QString &AByUser);
	//Administrator
	void onUserBanned(const QString &ANick, const QString &AReason, const QString &AByUser);
	void onAffiliationListReceived(const QString &AAffiliation, const QList<IMultiUserListItem> &AList);
	//Owner
	void onConfigFormReceived(const IDataForm &AForm);
	void onRoomDestroyed(const QString &AReason);
protected slots:
	void onMultiChatWindowActivated();
	void onMultiChatNotifierActiveNotifyChanged(int ANotifyId);
	void onMultiChatEditWidgetKeyEvent(QKeyEvent *AKeyEvent, bool &AHooked);
	void onMultiChatHorizontalSplitterMoved(int APos, int AIndex);
	void onMultiChatUserItemDoubleClicked(const QModelIndex &AIndex);
	void onMultiChatContentAppended(const QString &AHtml, const IMessageStyleContentOptions &AOptions);
	void onMultiChatMessageStyleOptionsChanged(const IMessageStyleOptions &AOptions, bool ACleared);
protected slots:
	void onPrivateChatWindowActivated();
	void onPrivateChatWindowClosed();
	void onPrivateChatWindowDestroyed();
	void onPrivateChatClearWindowActionTriggered(bool);
	void onPrivateChatContextMenuRequested(Menu *AMenu);
	void onPrivateChatToolTipsRequested(QMap<int,QString> &AToolTips);
	void onPrivateChatNotifierActiveNotifyChanged(int ANotifyId);
	void onPrivateChatContentAppended(const QString &AHtml, const IMessageStyleContentOptions &AOptions);
	void onPrivateChatMessageStyleOptionsChanged(const IMessageStyleOptions &AOptions, bool ACleared);
protected slots:
	void onRoomActionTriggered(bool);
	void onNickCompleteMenuActionTriggered(bool);
	void onOpenPrivateChatWindowActionTriggered(bool);
	void onChangeUserRoleActionTriggeted(bool);
	void onChangeUserAffiliationActionTriggered(bool);
	void onDataFormMessageDialogAccepted();
	void onAffiliationListDialogAccepted();
	void onConfigFormDialogAccepted();
protected slots:
	void onStatusIconsChanged();
	void onAutoRejoinAfterKick();
	void onShortcutActivated(const QString &AId, QWidget *AWidget);
	void onArchiveRequestFailed(const QString &AId, const XmppError &AError);
	void onArchiveMessagesLoaded(const QString &AId, const IArchiveCollectionBody &ABody);
	void onStyleOptionsChanged(const IMessageStyleOptions &AOptions, int AMessageType, const QString &AContext);
private:
	Ui::MultiUserChatWindowClass ui;
private:
	IMessageWidgets *FMessageWidgets;
	IMessageProcessor *FMessageProcessor;
	IMessageStyleManager *FMessageStyleManager;
	IMessageArchiver *FMessageArchiver;
	IDataForms *FDataForms;
	IStatusIcons *FStatusIcons;
	IStatusChanger *FStatusChanger;
	IMultiUserChat *FMultiChat;
	IMultiUserChatManager *FMultiChatManager;
	IRecentContacts *FRecentContacts;
	IStanzaProcessor *FStanzaProcessor;
private:
	IMessageAddress *FAddress;
	IMessageInfoWidget *FInfoWidget;
	IMessageViewWidget *FViewWidget;
	IMessageEditWidget *FEditWidget;
	IMessageMenuBarWidget *FMenuBarWidget;
	IMessageToolBarWidget *FToolBarWidget;
	IMessageStatusBarWidget *FStatusBarWidget;
	IMessageTabPageNotifier *FTabPageNotifier;
private:
	Action *FClearChat;
	Action *FEnterRoom;
	Action *FExitRoom;
private:
	Action *FChangeNick;
	Action *FInviteContact;
	Action *FRequestVoice;
	Action *FChangeTopic;
	Action *FBanList;
	Action *FMembersList;
	Action *FAdminsList;
	Action *FOwnersList;
	Action *FConfigRoom;
	Action *FDestroyRoom;
private:
	int FSHIAnyStanza;
	QString FLastAffiliation;
	QDateTime FLastStanzaTime;
private:
	int FUsersListWidth;
	bool FShownDetached;
	bool FDestroyOnChatClosed;
	QString FTabPageToolTip;
	QList<int> FActiveMessages;
	QList<IMessageChatWindow *> FChatWindows;
	QMap<IMessageChatWindow *, QTimer *> FDestroyTimers;
	QMultiMap<IMessageChatWindow *,int> FActiveChatMessages;
	QMap<int, IDataDialogWidget *> FDataFormMessages;
	QHash<IMultiUser *, UserStatus> FUserStatus;
	QMap<IMessageViewWidget *, WindowStatus> FWindowStatus;
	QMap<QString, IMessageChatWindow *> FHistoryRequests;
	QMap<IMessageChatWindow *, QList<Message> > FPendingMessages;
	QMap<IMessageChatWindow *, QList<WindowContent> > FPendingContent;
private:
	UsersProxyModel *FUsersProxy;
	QStandardItemModel *FUsersModel;
	QHash<IMultiUser *, QStandardItem *> FUsers;
private:
	int FStartCompletePos;
	QString FCompleteNickStarts;
	QString FCompleteNickLast;
	QList<QString> FCompleteNicks;
	QList<QString>::const_iterator FCompleteIt;
};

#endif // MULTIUSERCHATWINDOW_H
