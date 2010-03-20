#include "notifications.h"

#define ADR_NOTIFYID                    Action::DR_Parametr1

#define SVN_NOTIFICATORS                "notificators:notificator[]"
#define SVN_ENABLE_ROSTERICONS          "enableRosterIcons"
#define SVN_ENABLE_PUPUPWINDOWS         "enablePopupWindows"
#define SVN_ENABLE_TRAYICONS            "enableTrayIcons"
#define SVN_ENABLE_TRAYACTIONS          "enableTrayActions"
#define SVN_ENABLE_SOUNDS               "enableSounds"
#define SVN_ENABLE_AUTO_ACTIVATE        "enableAutoActivate"
#define SVN_EXPAND_ROSTER_GROUPS        "expandRosterGroups"
#define SVN_DISABLE_SOUNDS_WHEN_DND     "disableSoundsWhenDND"

Notifications::Notifications()
{
  FAvatars = NULL;
  FRosterPlugin = NULL;
  FStatusIcons = NULL;
  FStatusChanger = NULL;
  FTrayManager = NULL;
  FRostersModel = NULL;
  FRostersViewPlugin = NULL;
  FSettingsPlugin = NULL;
  FMainWindowPlugin = NULL;

  FActivateAll = NULL;
  FRemoveAll = NULL;
  FNotifyMenu = NULL;

  FOptions = 0;
  FNotifyId = 0;
  FSound = NULL;
}

Notifications::~Notifications()
{
  delete FActivateAll;
  delete FRemoveAll;
  delete FNotifyMenu;
  delete FSound;
}

void Notifications::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->name = tr("Notifications Manager"); 
  APluginInfo->description = tr("Allows other modules to notify the user of the events");
  APluginInfo->version = "1.0";
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->homePage = "http://www.vacuum-im.org";
}

bool Notifications::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->pluginInterface("ITrayManager").value(0,NULL);
  if (plugin)
  {
    FTrayManager = qobject_cast<ITrayManager *>(plugin->instance());
    if (FTrayManager)
    {
      connect(FTrayManager->instance(),SIGNAL(notifyActivated(int, QSystemTrayIcon::ActivationReason)),
        SLOT(onTrayNotifyActivated(int, QSystemTrayIcon::ActivationReason)));
      connect(FTrayManager->instance(),SIGNAL(notifyRemoved(int)),SLOT(onTrayNotifyRemoved(int)));
    }
  }

  plugin = APluginManager->pluginInterface("IRostersViewPlugin").value(0,NULL);
  if (plugin)
  {
    FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());
    if (FRostersViewPlugin)
    {
      connect(FRostersViewPlugin->rostersView()->instance(),SIGNAL(notifyActivated(IRosterIndex *, int)),
        SLOT(onRosterNotifyActivated(IRosterIndex *, int)));
      connect(FRostersViewPlugin->rostersView()->instance(),SIGNAL(notifyRemovedByIndex(IRosterIndex *, int)),
        SLOT(onRosterNotifyRemoved(IRosterIndex *, int)));
    }
  }

  plugin = APluginManager->pluginInterface("IRostersModel").value(0,NULL);
  if (plugin)
    FRostersModel = qobject_cast<IRostersModel *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IAvatars").value(0,NULL);
  if (plugin)
    FAvatars = qobject_cast<IAvatars *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IRosterPlugin").value(0,NULL);
  if (plugin)
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IStatusIcons").value(0,NULL);
  if (plugin)
    FStatusIcons = qobject_cast<IStatusIcons *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IStatusChanger").value(0,NULL);
  if (plugin)
    FStatusChanger = qobject_cast<IStatusChanger *>(plugin->instance());

  plugin = APluginManager->pluginInterface("IMainWindowPlugin").value(0,NULL);
  if (plugin)
    FMainWindowPlugin = qobject_cast<IMainWindowPlugin *>(plugin->instance());

  plugin = APluginManager->pluginInterface("ISettingsPlugin").value(0,NULL);
  if (plugin)
  {
    FSettingsPlugin = qobject_cast<ISettingsPlugin *>(plugin->instance());
    if (FSettingsPlugin)
    {
      connect(FSettingsPlugin->instance(),SIGNAL(settingsOpened()),SLOT(onSettingsOpened()));
      connect(FSettingsPlugin->instance(),SIGNAL(settingsClosed()),SLOT(onSettingsClosed()));
    }
  }

  return true;
}

bool Notifications::initObjects()
{
  FSoundOnOff = new Action(this);
  FSoundOnOff->setToolTip(tr("Enable/Disable notifications sound"));
  FSoundOnOff->setIcon(RSR_STORAGE_MENUICONS, checkOption(EnableSounds) ? MNI_NOTIFICATIONS_SOUND_ON : MNI_NOTIFICATIONS_SOUND_OFF);
  connect(FSoundOnOff,SIGNAL(triggered(bool)),SLOT(onSoundOnOffActionTriggered(bool)));

  FActivateAll = new Action(this);
  FActivateAll->setVisible(false);
  FActivateAll->setText(tr("Activate All Notifications"));
  FActivateAll->setIcon(RSR_STORAGE_MENUICONS,MNI_NOTIFICATIONS_ACTIVATE_ALL);
  connect(FActivateAll,SIGNAL(triggered(bool)),SLOT(onTrayActionTriggered(bool)));

  FRemoveAll = new Action(this);
  FRemoveAll->setVisible(false);
  FRemoveAll->setText(tr("Remove All Notifications"));
  FRemoveAll->setIcon(RSR_STORAGE_MENUICONS,MNI_NOTIFICATIONS_REMOVE_ALL);
  connect(FRemoveAll,SIGNAL(triggered(bool)),SLOT(onTrayActionTriggered(bool)));

  FNotifyMenu = new Menu;
  FNotifyMenu->setTitle(tr("Pending Notifications"));
  FNotifyMenu->setIcon(RSR_STORAGE_MENUICONS,MNI_NOTIFICATIONS);
  FNotifyMenu->menuAction()->setVisible(false);

  if (FSettingsPlugin)
  {
    FSettingsPlugin->openOptionsNode(ON_NOTIFICATIONS,tr("Notifications"),tr("Notification options"),MNI_NOTIFICATIONS,ONO_NOTIFICATIONS);
    FSettingsPlugin->insertOptionsHolder(this);
  }

  if (FTrayManager)
  {
    FTrayManager->addAction(FActivateAll,AG_TMTM_NOTIFICATIONS,false);
    FTrayManager->addAction(FRemoveAll,AG_TMTM_NOTIFICATIONS,false);
    FTrayManager->addAction(FNotifyMenu->menuAction(),AG_TMTM_NOTIFICATIONS,false);
  }

  if (FMainWindowPlugin)
  {
    FMainWindowPlugin->mainWindow()->topToolBarChanger()->insertAction(FSoundOnOff,TBG_MWTTB_NOTIFICATIONS_SOUND);
  }

  return true;
}

QWidget *Notifications::optionsWidget(const QString &ANode, int &AOrder)
{
  if (ANode == ON_NOTIFICATIONS)
  {
    AOrder = OWO_NOTIFICATIONS;
    OptionsWidget *widget = new OptionsWidget(this);
    foreach(QString id, FNotificators.keys())
    {
      Notificator notificator = FNotificators.value(id);
      widget->appendKindsWidget(new NotifyKindsWidget(this,id,notificator.title,notificator.kindMask,widget));
    }
    connect(widget,SIGNAL(optionsAccepted()),SIGNAL(optionsAccepted()));
    connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogAccepted()),widget,SLOT(apply()));
    connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogRejected()),SIGNAL(optionsRejected()));
    return widget;
  }
  return NULL;
}

QList<int> Notifications::notifications() const
{
  return FNotifyRecords.keys();
}

INotification Notifications::notificationById(int ANotifyId) const
{
  return FNotifyRecords.value(ANotifyId).notification;
}

int Notifications::appendNotification(const INotification &ANotification)
{
  NotifyRecord record;
  int notifyId = ++FNotifyId;
  record.notification = ANotification;
  emit notificationAppend(notifyId, record.notification);

  bool isDND = FStatusChanger!=NULL ? FStatusChanger->statusItemShow(STATUS_MAIN_ID)==IPresence::DoNotDisturb : false;

  QIcon icon = qvariant_cast<QIcon>(record.notification.data.value(NDR_ICON));
  QString toolTip = record.notification.data.value(NDR_TOOLTIP).toString();
  
  if (FRostersModel && FRostersViewPlugin && checkOption(EnableRosterIcons) && (record.notification.kinds & INotification::RosterIcon)>0)
  {
    Jid streamJid = record.notification.data.value(NDR_ROSTER_STREAM_JID).toString();
    Jid contactJid = record.notification.data.value(NDR_ROSTER_CONTACT_JID).toString();
    int order = record.notification.data.value(NDR_ROSTER_NOTIFY_ORDER).toInt();
    int flags = IRostersView::LabelBlink|IRostersView::LabelVisible;
    flags = flags | (checkOption(ExpandRosterGroups) ? IRostersView::LabelExpandParents : 0);
    QList<IRosterIndex *> indexes = FRostersModel->getContactIndexList(streamJid,contactJid,true);
    record.rosterId = FRostersViewPlugin->rostersView()->appendNotify(indexes,order,icon,toolTip,flags);
  }
  
  if (checkOption(EnablePopupWindows) && (record.notification.kinds & INotification::PopupWindow)>0)
  {
    record.widget = new NotifyWidget(record.notification);
    connect(record.widget,SIGNAL(notifyActivated()),SLOT(onWindowNotifyActivated()));
    connect(record.widget,SIGNAL(notifyRemoved()),SLOT(onWindowNotifyRemoved()));
    connect(record.widget,SIGNAL(windowDestroyed()),SLOT(onWindowNotifyDestroyed()));
    record.widget->appear();
  }

  if (FTrayManager)
  {
    if (checkOption(EnableTrayIcons) && (record.notification.kinds & INotification::TrayIcon)>0)
      record.trayId = FTrayManager->appendNotify(icon,toolTip,true);
    
    if (!toolTip.isEmpty() && checkOption(EnableTrayActions) && (record.notification.kinds & INotification::TrayAction)>0)
    {
      record.action = new Action(FNotifyMenu);
      record.action->setIcon(icon);
      record.action->setText(toolTip);
      record.action->setData(ADR_NOTIFYID,notifyId);
      connect(record.action,SIGNAL(triggered(bool)),SLOT(onActionNotifyActivated(bool)));
      FNotifyMenu->addAction(record.action);
    }
  }

  if (QSound::isAvailable() && !(isDND && checkOption(DisableSoundsWhenDND)) && checkOption(EnableSounds) && (record.notification.kinds & INotification::PlaySound)>0)
  {
    QString soundName = record.notification.data.value(NDR_SOUND_FILE).toString();
    QString soundFile = FileStorage::staticStorage(RSR_STORAGE_SOUNDS)->fileFullName(soundName);
    if (!soundFile.isEmpty() && (FSound==NULL || FSound->isFinished()))
    {
      delete FSound;
      FSound = new QSound(soundFile);
      FSound->play();
    }
  }

  if (checkOption(EnableAutoActivate) && (record.notification.kinds & INotification::AutoActivate)>0)
  {
    FDelayedActivations.append(notifyId);
    QTimer::singleShot(0,this,SLOT(onActivateDelayedActivations()));
  }

  if (FNotifyRecords.isEmpty())
  {
    FActivateAll->setVisible(true);
    FRemoveAll->setVisible(true);
  }
  FNotifyMenu->menuAction()->setVisible(!FNotifyMenu->isEmpty());

  FNotifyRecords.insert(notifyId,record);
  emit notificationAppended(notifyId, record.notification);
  return notifyId;
}

void Notifications::activateNotification(int ANotifyId)
{
  if (FNotifyRecords.contains(ANotifyId))
  {
    emit notificationActivated(ANotifyId);
  }
}

void Notifications::removeNotification(int ANotifyId)
{
  if (FNotifyRecords.contains(ANotifyId))
  {
    NotifyRecord record = FNotifyRecords.take(ANotifyId);
    if (FRostersViewPlugin && record.rosterId!=0)
    {
      FRostersViewPlugin->rostersView()->removeNotify(record.rosterId);
    }
    if (FTrayManager && record.trayId!=0)
    {
      FTrayManager->removeNotify(record.trayId);
    }
    if (record.widget != NULL)
    {
      record.widget->deleteLater();
    }
    if (record.action != NULL)
    {
      FNotifyMenu->removeAction(record.action);
      delete record.action;
    }
    if (FNotifyRecords.isEmpty())
    {
      FActivateAll->setVisible(false);
      FRemoveAll->setVisible(false);
    }
    FNotifyMenu->menuAction()->setVisible(!FNotifyMenu->isEmpty());
    emit notificationRemoved(ANotifyId);
  }
}

bool Notifications::checkOption(INotifications::Option AOption) const
{
  return (FOptions & AOption) > 0;
}

void Notifications::setOption(INotifications::Option AOption, bool AValue)
{
  if (checkOption(AOption) != AValue)
  {
    AValue ? FOptions |= AOption : FOptions &= ~AOption;
    if (AOption == EnableSounds)
    {
      FSoundOnOff->setIcon(RSR_STORAGE_MENUICONS, AValue ? MNI_NOTIFICATIONS_SOUND_ON : MNI_NOTIFICATIONS_SOUND_OFF);
    }
    emit optionChanged(AOption,AValue);
  }
}

void Notifications::insertNotificator(const QString &AId, const QString &ATitle, uchar AKindMask, uchar ADefault)
{
  if (!FNotificators.contains(AId))
  {
    Notificator notificator;
    notificator.title = ATitle;
    notificator.kinds = 0xFF;
    notificator.defaults = ADefault;
    notificator.kindMask = AKindMask;
    FNotificators.insert(AId,notificator);
  }
}

uchar Notifications::notificatorKinds(const QString &AId) const
{
  if (FNotificators.contains(AId))
  {
    Notificator &notificator = FNotificators[AId];
    if (notificator.kinds == 0xFF)
    {
      ISettings *settings = FSettingsPlugin!=NULL ? FSettingsPlugin->settingsForPlugin(NOTIFICATIONS_UUID) : NULL;
      notificator.kinds = settings!=NULL ? settings->valueNS(SVN_NOTIFICATORS,AId,notificator.defaults).toUInt() & notificator.kindMask : notificator.defaults;
    }
    return notificator.kinds;
  }
  return 0xFF;
}

void Notifications::setNotificatorKinds(const QString &AId, uchar AKinds)
{
  if (FNotificators.contains(AId))
  {
    Notificator &notificator = FNotificators[AId];
    notificator.kinds = AKinds & notificator.kindMask;
    if (FSettingsPlugin)
      FSettingsPlugin->settingsForPlugin(NOTIFICATIONS_UUID)->setValueNS(SVN_NOTIFICATORS,AId,notificator.kinds);
  }
}

void Notifications::removeNotificator(const QString &AId)
{
  FNotificators.remove(AId);
}

QImage Notifications::contactAvatar(const Jid &AContactJid) const
{
  return FAvatars!=NULL ? FAvatars->avatarImage(AContactJid) : QImage();
}

QIcon Notifications::contactIcon(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FStatusIcons!=NULL ? FStatusIcons->iconByJid(AStreamJid,AContactJid) : QIcon();
}

QString Notifications::contactName(const Jid &AStreamJId, const Jid &AContactJid) const
{
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJId) : NULL;
  QString name = roster!=NULL ? roster->rosterItem(AContactJid).name : AContactJid.node();
  if (name.isEmpty())
    name = AContactJid.bare();
  return name;
}

int Notifications::notifyIdByRosterId(int ARosterId) const
{
  QMap<int,NotifyRecord>::const_iterator it = FNotifyRecords.constBegin();
  for (; it!=FNotifyRecords.constEnd(); it++)
    if (it.value().rosterId == ARosterId)
      return it.key();
  return -1;
}

int Notifications::notifyIdByTrayId(int ATrayId) const
{
  QMap<int,NotifyRecord>::const_iterator it = FNotifyRecords.constBegin();
  for (; it!=FNotifyRecords.constEnd(); it++)
    if (it.value().trayId == ATrayId)
      return it.key();
  return -1;
}

int Notifications::notifyIdByWidget(NotifyWidget *AWidget) const
{
  QMap<int,NotifyRecord>::const_iterator it = FNotifyRecords.constBegin();
  for (; it!=FNotifyRecords.constEnd(); it++)
    if (it.value().widget == AWidget)
      return it.key();
  return -1;
}

void Notifications::onActivateDelayedActivations()
{
  foreach(int notifyId, FDelayedActivations)
    activateNotification(notifyId);
  FDelayedActivations.clear();
}

void Notifications::onSoundOnOffActionTriggered(bool)
{
  setOption(EnableSounds, !checkOption(EnableSounds));
}

void Notifications::onTrayActionTriggered(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    foreach(int notifyId, FNotifyRecords.keys())
    {
      if (action == FActivateAll)
        activateNotification(notifyId);
      else if (action == FRemoveAll)
        removeNotification(notifyId);
    }
  }
}

void Notifications::onRosterNotifyActivated(IRosterIndex * /*AIndex*/, int ANotifyId)
{
  activateNotification(notifyIdByRosterId(ANotifyId));
}

void Notifications::onRosterNotifyRemoved(IRosterIndex * /*AIndex*/, int ANotifyId)
{
  removeNotification(notifyIdByRosterId(ANotifyId));
}

void Notifications::onTrayNotifyActivated(int ANotifyId, QSystemTrayIcon::ActivationReason AReason)
{
  if (AReason == QSystemTrayIcon::Trigger)
  {
    activateNotification(notifyIdByTrayId(ANotifyId));
  }
}

void Notifications::onTrayNotifyRemoved(int ANotifyId)
{
  removeNotification(notifyIdByTrayId(ANotifyId));
}

void Notifications::onWindowNotifyActivated()
{
  activateNotification(notifyIdByWidget(qobject_cast<NotifyWidget*>(sender())));
}

void Notifications::onWindowNotifyRemoved()
{
  removeNotification(notifyIdByWidget(qobject_cast<NotifyWidget*>(sender())));
}

void Notifications::onWindowNotifyDestroyed()
{
  int notifyId = notifyIdByWidget(qobject_cast<NotifyWidget*>(sender()));
  if (FNotifyRecords.contains(notifyId))
    FNotifyRecords[notifyId].widget = NULL;
}

void Notifications::onActionNotifyActivated(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    int notifyId = action->data(ADR_NOTIFYID).toInt();
    activateNotification(notifyId);
  }
}

void Notifications::onSettingsOpened()
{
  ISettings *settings = FSettingsPlugin->settingsForPlugin(NOTIFICATIONS_UUID);
  setOption(EnableRosterIcons, settings->value(SVN_ENABLE_ROSTERICONS,true).toBool());
  setOption(EnablePopupWindows, settings->value(SVN_ENABLE_PUPUPWINDOWS,true).toBool());
  setOption(EnableTrayIcons, settings->value(SVN_ENABLE_TRAYICONS,true).toBool());
  setOption(EnableTrayActions, settings->value(SVN_ENABLE_TRAYACTIONS,true).toBool());
  setOption(EnableSounds, settings->value(SVN_ENABLE_SOUNDS,true).toBool());
  setOption(EnableAutoActivate, settings->value(SVN_ENABLE_AUTO_ACTIVATE,true).toBool());
  setOption(ExpandRosterGroups, settings->value(SVN_EXPAND_ROSTER_GROUPS,true).toBool());
  setOption(DisableSoundsWhenDND,settings->value(SVN_DISABLE_SOUNDS_WHEN_DND,false).toBool());
}

void Notifications::onSettingsClosed()
{
  ISettings *settings = FSettingsPlugin->settingsForPlugin(NOTIFICATIONS_UUID);
  settings->setValue(SVN_ENABLE_ROSTERICONS,checkOption(EnableRosterIcons));
  settings->setValue(SVN_ENABLE_PUPUPWINDOWS,checkOption(EnablePopupWindows));
  settings->setValue(SVN_ENABLE_TRAYICONS,checkOption(EnableTrayIcons));
  settings->setValue(SVN_ENABLE_TRAYACTIONS,checkOption(EnableTrayActions));
  settings->setValue(SVN_ENABLE_SOUNDS,checkOption(EnableSounds));
  settings->setValue(SVN_ENABLE_AUTO_ACTIVATE, checkOption(EnableAutoActivate));
  settings->setValue(SVN_EXPAND_ROSTER_GROUPS, checkOption(ExpandRosterGroups));
  settings->setValue(SVN_DISABLE_SOUNDS_WHEN_DND, checkOption(DisableSoundsWhenDND));
}

Q_EXPORT_PLUGIN2(plg_notifications, Notifications)