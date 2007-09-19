#include <QtDebug>
#include "rosterplugin.h"

RosterPlugin::RosterPlugin()
{
  FStanzaProcessor = NULL;
}

RosterPlugin::~RosterPlugin()
{

}

void RosterPlugin::pluginInfo(PluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Managing roster and subscriptions");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Roster and Subscriptions Manager"); 
  APluginInfo->uid = ROSTER_UUID;
  APluginInfo->version = "0.1";
  APluginInfo->dependences.append(XMPPSTREAMS_UUID); 
  APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
}

bool RosterPlugin::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->getPlugins("IXmppStreams").value(0,NULL);
  if (plugin)
  {
    connect(plugin->instance(), SIGNAL(added(IXmppStream *)),
      SLOT(onStreamAdded(IXmppStream *))); 
    connect(plugin->instance(), SIGNAL(removed(IXmppStream *)),
      SLOT(onStreamRemoved(IXmppStream *))); 
  }

  plugin = APluginManager->getPlugins("IStanzaProcessor").value(0,NULL);
  if (plugin) 
    FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());

  plugin = APluginManager->getPlugins("ISettingsPlugin").value(0,NULL);
  if (plugin) 
    FSettingsPlugin = qobject_cast<ISettingsPlugin *>(plugin->instance());
  
  return FStanzaProcessor!=NULL;
}

//IRosterPlugin
IRoster *RosterPlugin::addRoster(IXmppStream *AXmppStream)
{
  Roster *roster = (Roster *)getRoster(AXmppStream->jid());
  if (!roster)
  {
    roster = new Roster(AXmppStream, FStanzaProcessor);
    connect(roster,SIGNAL(destroyed(QObject *)),SLOT(onRosterDestroyed(QObject *)));
    FCleanupHandler.add(roster); 
    FRosters.append(roster); 
  }
  return roster;
}

IRoster *RosterPlugin::getRoster(const Jid &AStreamJid) const
{
  foreach(Roster *roster, FRosters)
    if (roster->streamJid() == AStreamJid)
      return roster;
  return NULL;    
}

void RosterPlugin::loadRosterItems(const Jid &AStreamJid)
{
  Roster *roster = (Roster *)getRoster(AStreamJid);
  if (roster)
    roster->loadRosterItems(rosterFile(AStreamJid));
}

void RosterPlugin::removeRoster(IXmppStream *AXmppStream)
{
  Roster *roster = (Roster *)getRoster(AXmppStream->jid());
  if (roster)
  {
    disconnect(roster,SIGNAL(destroyed(QObject *)),this,SLOT(onRosterDestroyed(QObject *)));
    FRosters.removeAt(FRosters.indexOf(roster));  
    delete roster;
  }
}

QString RosterPlugin::rosterFile(const Jid &AStreamJid) const
{
  QString fileName = AStreamJid.pBare()+".xml";
  fileName.replace("@","_at_");
  QDir dir;
  if (FSettingsPlugin)
    dir.setPath(FSettingsPlugin->homeDir().path());
  if (!dir.exists("rosters"))
    dir.mkdir("rosters");
  fileName = dir.path()+"/rosters/"+fileName;
  return fileName;
}

void RosterPlugin::onRosterOpened()
{
  Roster *roster = qobject_cast<Roster *>(sender());
  if (roster)
    emit rosterOpened(roster); 
}

void RosterPlugin::onRosterItemPush(IRosterItem *ARosterItem)
{
  Roster *roster = qobject_cast<Roster *>(sender());
  if (roster)
    emit rosterItemPush(roster,ARosterItem); 
}

void RosterPlugin::onRosterItemRemoved(IRosterItem *ARosterItem)
{
  Roster *roster = qobject_cast<Roster *>(sender());
  if (roster)
    emit rosterItemRemoved(roster,ARosterItem); 
}

void RosterPlugin::onRosterSubscription(const Jid &AJid, IRoster::SubsType ASType, const QString &AStatus)
{
  Roster *roster = qobject_cast<Roster *>(sender());
  if (roster)
    emit rosterSubscription(roster,AJid,ASType,AStatus);
}

void RosterPlugin::onRosterClosed()
{
  Roster *roster = qobject_cast<Roster *>(sender());
  if (roster)
    emit rosterClosed(roster); 
}

void RosterPlugin::onStreamAdded(IXmppStream *AXmppStream)
{
  IRoster *roster = addRoster(AXmppStream);
  connect(roster->instance(),SIGNAL(opened()),SLOT(onRosterOpened()));
  connect(roster->instance(),SIGNAL(closed()),SLOT(onRosterClosed()));
  connect(roster->instance(),SIGNAL(itemPush(IRosterItem *)),SLOT(onRosterItemPush(IRosterItem *)));
  connect(roster->instance(),SIGNAL(itemRemoved(IRosterItem *)),SLOT(onRosterItemRemoved(IRosterItem *)));
  connect(roster->instance(),SIGNAL(subscription(const Jid &, IRoster::SubsType, const QString &)),
    SLOT(onRosterSubscription(const Jid &, IRoster::SubsType, const QString &)));
  emit rosterAdded(roster); 
}

void RosterPlugin::onStreamRemoved(IXmppStream *AXmppStream)
{
  IRoster *roster = getRoster(AXmppStream->jid());
  if (roster)
  {
    roster->saveRosterItems(rosterFile(roster->streamJid()));
    emit rosterRemoved(roster);
    removeRoster(AXmppStream);
  }
}

void RosterPlugin::onRosterDestroyed(QObject *AObject)
{
  Roster *roster = qobject_cast<Roster *>(AObject);
  if (FRosters.contains(roster))
    FRosters.removeAt(FRosters.indexOf(roster)); 
}

Q_EXPORT_PLUGIN2(RosterPlugin, RosterPlugin)
