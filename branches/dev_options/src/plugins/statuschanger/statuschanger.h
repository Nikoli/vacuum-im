#ifndef STATUSCHANGER_H
#define STATUSCHANGER_H

#include <QSet>
#include <QPair>
#include <QPointer>
#include <QDateTime>
#include <definations/actiongroups.h>
#include <definations/rosterlabelorders.h>
#include <definations/optionvalues.h>
#include <definations/optionnodes.h>
#include <definations/optionwidgetorders.h>
#include <definations/rosterindextyperole.h>
#include <definations/rosterfootertextorders.h>
#include <definations/notificationdataroles.h>
#include <definations/resources.h>
#include <definations/menuicons.h>
#include <definations/soundfiles.h>
#include <interfaces/ipluginmanager.h>
#include <interfaces/istatuschanger.h>
#include <interfaces/ipresence.h>
#include <interfaces/iroster.h>
#include <interfaces/imainwindow.h>
#include <interfaces/irostersview.h>
#include <interfaces/irostersmodel.h>
#include <interfaces/iaccountmanager.h>
#include <interfaces/itraymanager.h>
#include <interfaces/ioptionsmanager.h>
#include <interfaces/istatusicons.h>
#include <interfaces/inotifications.h>
#include <utils/options.h>
#include "editstatusdialog.h"
#include "modifystatusdialog.h"

struct StatusItem {
  int code;
  QString name;
  int show;
  QString text;
  int priority;
};

class StatusChanger : 
  public QObject,
  public IPlugin,
  public IStatusChanger,
  public IOptionsHolder
{
  Q_OBJECT;
  Q_INTERFACES(IPlugin IStatusChanger IOptionsHolder);
public:
  StatusChanger();
  ~StatusChanger();
  virtual QObject *instance() { return this; }
  //IPlugin
  virtual QUuid pluginUuid() const { return STATUSCHANGER_UUID; }
  virtual void pluginInfo(IPluginInfo *APluginInfo);
  virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
  virtual bool initObjects();
  virtual bool initSettings();
  virtual bool startPlugin();
  //IOptionsHolder
  virtual IOptionsWidget *optionsWidget(const QString &ANodeId, int &AOrder, QWidget *AParent);
  //IStatusChanger
  virtual Menu *statusMenu() const;
  virtual Menu *streamMenu(const Jid &AStreamJid) const;
  virtual int mainStatus() const;
  virtual void setMainStatus(int AStatusId);
  virtual int streamStatus(const Jid &AStreamJid) const;
  virtual void setStreamStatus(const Jid &AStreamJid, int AStatusId);
  virtual QString statusItemName(int AStatusId) const;
  virtual int statusItemShow(int AStatusId) const;
  virtual QString statusItemText(int AStatusId) const;
  virtual int statusItemPriority(int AStatusId) const;
  virtual QList<int> statusItems() const;
  virtual QList<int> activeStatusItems() const;
  virtual QList<int> statusByShow(int AShow) const;
  virtual int statusByName(const QString &AName) const;
  virtual int addStatusItem(const QString &AName, int AShow, const QString &AText, int APriority);
  virtual void updateStatusItem(int AStatusId, const QString &AName, int AShow, const QString &AText, int APriority);
  virtual void removeStatusItem(int AStatusId);
  virtual QIcon iconByShow(int AShow) const;
  virtual QString nameByShow(int AShow) const;
signals:
  void statusAboutToBeChanged(const Jid &AStreamJid, int AStatusId);
  void statusChanged(const Jid &AStreamJid, int AStatusId);
  void statusItemAdded(int AStatusId);
  void statusItemChanged(int AStatusId);
  void statusItemRemoved(int AStatusId);
protected:
  void createDefaultStatus();
  void setMainStatusId(int AStatusId);
  void setStreamStatusId(IPresence *APresence, int AStatusId);
  Action *createStatusAction(int AStatusId, const Jid &AStreamJid, QObject *AParent) const;
  void updateStatusAction(int AStatusId, Action *AAction) const;
  void createStatusActions(int AStatusId);
  void updateStatusActions(int AStatusId);
  void removeStatusActions(int AStatusId);
  void createStreamMenu(IPresence *APresence);
  void updateStreamMenu(IPresence *APresence);
  void removeStreamMenu(IPresence *APresence);
  int visibleMainStatusId() const;
  void updateMainMenu();
  void updateTrayToolTip();
  void updateMainStatusActions();
  void insertConnectingLabel(IPresence *APresence);
  void removeConnectingLabel(IPresence *APresence);
  void autoReconnect(IPresence *APresence);
  int createTempStatus(IPresence *APresence, int AShow, const QString &AText, int APriority);
  void removeTempStatus(IPresence *APresence);
  void resendUpdatedStatus(int AStatusId);
  void removeAllCustomStatuses();
  void insertStatusNotification(IPresence *APresence);
  void removeStatusNotification(IPresence *APresence);
protected slots:
  void onSetStatusByAction(bool);
  void onPresenceAdded(IPresence *APresence);
  void onPresenceChanged(IPresence *APresence, int AShow, const QString &AStatus, int APriority);
  void onPresenceRemoved(IPresence *APresence);
  void onRosterOpened(IRoster *ARoster);
  void onRosterClosed(IRoster *ARoster);
  void onStreamJidChanged(const Jid &ABefour, const Jid &AAfter);
  void onRosterIndexContextMenu(IRosterIndex *AIndex, Menu *AMenu);
  void onDefaultStatusIconsChanged();
  void onOptionsOpened();
  void onOptionsClosed();
  void onOptionsChanged(const OptionsNode &ANode);
  void onProfileOpened(const QString &AProfile);
  void onReconnectTimer();
  void onEditStatusAction(bool);
  void onModifyStatusAction(bool);
  void onAccountOptionsChanged(IAccount *AAccount, const OptionsNode &ANode);
  void onNotificationActivated(int ANotifyId);
private:
  IPresencePlugin *FPresencePlugin;
  IRosterPlugin *FRosterPlugin;
  IMainWindowPlugin *FMainWindowPlugin;
  IRostersView *FRostersView;
  IRostersViewPlugin *FRostersViewPlugin;
  IRostersModel *FRostersModel;
  IOptionsManager *FOptionsManager;
  ITrayManager *FTrayManager;
  IAccountManager *FAccountManager;
  IStatusIcons *FStatusIcons;
  INotifications *FNotifications;
private:
  Menu *FMainMenu;
  Action *FModifyStatus;
  QMap<IPresence *, Menu *> FStreamMenu;
  QMap<IPresence *, Action *> FMainStatusActions;
private:
  int FConnectingLabel;
  IPresence *FChangingPresence;
  QMap<int, StatusItem> FStatusItems;
  QSet<IPresence *> FMainStatusStreams;
  QMap<IPresence *, int> FLastOnlineStatus;
  QMap<IPresence *, int> FCurrentStatus;
  QMap<IPresence *, int> FConnectStatus;
  QMap<IPresence *, int> FTempStatus;
  QMap<IPresence *, int> FNotifyId;
  QMap<IPresence *, QPair<QDateTime,int> > FPendingReconnect;
  QPointer<EditStatusDialog> FEditStatusDialog;
  QPointer<ModifyStatusDialog> FModifyStatusDialog;
};

#endif // STATUSCHANGER_H