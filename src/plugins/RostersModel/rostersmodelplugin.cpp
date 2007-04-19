#include <QtDebug>
#include "rostersmodelplugin.h"

#include <QTreeView>

RostersModelPlugin::RostersModelPlugin()
{
  FRostersModel = NULL;
  FRosterPlugin = NULL;
  FPresencePlugin = NULL;
}

RostersModelPlugin::~RostersModelPlugin()
{
  if (FRostersModel)
  {
    emit modelDestroyed(FRostersModel);
    delete FRostersModel;
  }
}

//IPlugin
void RostersModelPlugin::pluginInfo(PluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Creating and handling roster tree");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Rosters Model"); 
  APluginInfo->uid = ROSTERSMODEL_UUID;
  APluginInfo->version = "0.1";
}

bool RostersModelPlugin::initPlugin(IPluginManager *APluginManager)
{
  IPlugin *plugin = APluginManager->getPlugins("IRosterPlugin").value(0,NULL);
  if (plugin)
  {
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
    if (FRosterPlugin)
    {
      connect(FRosterPlugin->instance(), SIGNAL(rosterAdded(IRoster *)),SLOT(onRosterAdded(IRoster *))); 
      connect(FRosterPlugin->instance(), SIGNAL(rosterRemoved(IRoster *)),SLOT(onRosterRemoved(IRoster *))); 
    }
  }

  plugin = APluginManager->getPlugins("IPresencePlugin").value(0,NULL);
  if (plugin)
  {
    FPresencePlugin = qobject_cast<IPresencePlugin *>(plugin->instance());
    if (FPresencePlugin)
    {
      connect(FPresencePlugin->instance(), SIGNAL(presenceAdded(IPresence *)),SLOT(onPresenceAdded(IPresence *))); 
      connect(FPresencePlugin->instance(), SIGNAL(presenceRemoved(IPresence *)),SLOT(onPresenceRemoved(IPresence *))); 
    }
  }
  return true;
}

bool RostersModelPlugin::startPlugin()
{
  createRostersModel();
  return true;
}

//IRostersModelPlugin
IRostersModel *RostersModelPlugin::rostersModel()
{
   return FRostersModel;
}

IRosterIndex *RostersModelPlugin::addStream(IRoster *ARoster, IPresence *APresence)
{
  return FRostersModel->appendStream(ARoster,APresence);
}

void RostersModelPlugin::removeStream(const Jid &AStreamJid)
{
  FRostersModel->removeStream(AStreamJid.pFull());
}

void RostersModelPlugin::createRostersModel()
{
  if (!FRostersModel)
  {
    FRostersModel = new RostersModel(this);
    emit modelCreated(FRostersModel);
  }
}

void RostersModelPlugin::onRosterAdded(IRoster *ARoster)
{
  IPresence *presence =0;
  if (FPresencePlugin)
    presence = FPresencePlugin->getPresence(ARoster->streamJid());
  if (presence)
    addStream(ARoster,presence);
}

void RostersModelPlugin::onRosterRemoved(IRoster *ARoster)
{
  removeStream(ARoster->streamJid());
}

void RostersModelPlugin::onPresenceAdded(IPresence *APresence)
{
  IRoster *roster =0;
  if (FRosterPlugin)
    roster = FRosterPlugin->getRoster(APresence->streamJid());
  if (roster)
    addStream(roster,APresence);
}

void RostersModelPlugin::onPresenceRemoved(IPresence *APresence)
{
  removeStream(APresence->streamJid());
}

Q_EXPORT_PLUGIN2(RostersModelPlugin, RostersModelPlugin)
