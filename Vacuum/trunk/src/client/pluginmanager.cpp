#include <QtDebug>
#include "pluginmanager.h"

#include <QTimer>
#include <QDir>
#include <QLibrary>
#include <QStack>
#include <QMultiMap>

//PluginManager
PluginManager::PluginManager(QApplication *AParent) : QObject(AParent)
{
  connect(AParent,SIGNAL(aboutToQuit()),SLOT(onAboutToQuit()));
}

PluginManager::~PluginManager()
{

}

PluginList PluginManager::getPlugins() const
{
  QList<IPlugin *> plugins;
  
  PluginItem *pluginItem;
  foreach(pluginItem, FPluginItems)
    plugins.append(pluginItem->plugin()); 
  
  return plugins;
}

PluginList PluginManager::getPlugins(const QString &AInterface) const
{
  QList<IPlugin *> plugins;
  if (!FPlugins.contains(AInterface))
  {
    foreach(PluginItem *pluginItem, FPluginItems)
      if (pluginItem->plugin()->instance()->inherits(AInterface.toLatin1().data()))
        plugins.append(pluginItem->plugin());
    FPlugins.insert(AInterface,plugins);
  }
  else
    plugins = FPlugins.value(AInterface);

  return plugins;
}

IPlugin *PluginManager::getPlugin(const QUuid &AUuid) const 
{ 
  PluginItem *pluginItem;
  foreach (pluginItem, FPluginItems)
    if (pluginItem->uuid() == AUuid) 
      return pluginItem->plugin(); 

  return NULL;
}

const PluginInfo *PluginManager::getPluginInfo(const QUuid &AUuid) const
{
  PluginItem *pluginItem;
  foreach (pluginItem, FPluginItems)
    if (pluginItem->uuid() == AUuid) 
      return pluginItem->info(); 

  return NULL;
}

QList<QUuid> PluginManager::getDependencesOn(const QUuid &AUuid) const
{
  static QStack<QUuid> deepStack;
  deepStack.push(AUuid);

  QList<QUuid> plugins;
  PluginItem *pluginItem;
  foreach(pluginItem, FPluginItems)
    if (!deepStack.contains(pluginItem->uuid()) && pluginItem->info()->dependences.contains(AUuid))
    {
      plugins += getDependencesOn(pluginItem->uuid());
      plugins.append(pluginItem->uuid());
    }

  deepStack.pop(); 
  return plugins;
}

QList<QUuid> PluginManager::getDependencesFor(const QUuid &AUuid) const
{
  static QStack<QUuid> deepStack;
  deepStack.push(AUuid);

  QList<QUuid> plugins;
  PluginItem *pluginItem = getPluginItem(AUuid);
  if (pluginItem)
  { 
    QUuid depend;
    foreach(depend, pluginItem->info()->dependences)
    {
      if (!deepStack.contains(depend) && getPlugin(depend))
      {
        plugins.append(depend);
        plugins += getDependencesFor(depend); 
      }
    }
  }

  deepStack.pop(); 
  return plugins;
}

void PluginManager::loadPlugins()
{
  QDir dir(QApplication::applicationDirPath());
  if (!dir.cd("plugins")) 
  {
    qDebug() << "CANT FIND PLUGINS DIRECTORY";
    return;
  }

  QString file;
  QStringList files = dir.entryList(QDir::Files);
  foreach (file, files) 
  {
    if (QLibrary::isLibrary(file)) 
    {
      QPluginLoader *loader = new QPluginLoader(dir.filePath(file));
      if (loader->load()) 
      {
        IPlugin *plugin = qobject_cast<IPlugin*>(loader->instance());
        if (plugin)	
        {
          QUuid uid = plugin->pluginUuid();
          if (!getPlugin(uid))
          {
            PluginItem* pluginItem = new PluginItem(uid,loader,this);
            FPluginItems.append(pluginItem);
            qDebug() << "Plugin loaded:"  << pluginItem->info()->name;
          } 
          else
          {
            qDebug() << "DUBLICATE UUID IN:" << loader->fileName();
            delete loader;
          }
        } 
        else
        {
          qDebug() << "WRONG PLUGIN INTERFACE IN:" << loader->fileName();
          delete loader;
        }
      } 
      else 
      {
        qDebug() << "WRONG LIBRARY BUILD CONTEXT" << loader->fileName();
        delete loader;
      }
    }
  }

  int i=0;
  while (i<FPluginItems.count())
  {
    PluginItem *pluginItem = FPluginItems.at(i);
    if (!checkDependences(pluginItem))
    {
      qDebug() << "UNLOADING PLUGIN: Dependences not exists" << pluginItem->info()->name;
      if (unloadPlugin(pluginItem->uuid())) 
        i = 0;
    }
    else if (!checkConflicts(pluginItem))
    {
      QUuid uuid;
      QList<QUuid> conflicts = getConflicts(pluginItem);
      foreach(uuid, conflicts)
      {
        qDebug() << "UNLOADING PLUGIN: Conflicts with another plugin" << uuid;
        if (unloadPlugin(uuid)) 
          i = 0;
      }
    }
    else i++;
  }
}

void PluginManager::initPlugins()
{
  int i =0;
  QMultiMap<int,IPlugin *> pluginOrder;
  while (i<FPluginItems.count())
  {
    PluginItem *pluginItem = FPluginItems.at(i);
    int initOrder = 0;
    IPlugin *plugin = pluginItem->plugin();
    if(plugin->initConnections(this,initOrder))
    {
      pluginOrder.insert(initOrder,plugin);
      i++;
    }
    else
    {
      qDebug() << "UNLOADING PLUGIN: Plugin returned false on init" << pluginItem->info()->name;
      i = 0;
      pluginOrder.clear();
      unloadPlugin(pluginItem->uuid());
    } 
  }

  foreach(IPlugin *plugin, pluginOrder)
    plugin->initObjects();

  foreach(IPlugin *plugin, pluginOrder)
    plugin->initSettings();
}

void PluginManager::startPlugins()
{
  foreach(PluginItem *pluginItem, FPluginItems)
    pluginItem->plugin()->startPlugin();
}

bool PluginManager::unloadPlugin(const QUuid &AUuid)
{
  PluginItem *pluginItem = getPluginItem(AUuid);
  if (pluginItem)
  {
    QUuid uuid;
    QList<QUuid> depends = getDependencesOn(AUuid);
    foreach(uuid, depends)
    {
      PluginItem *depend = getPluginItem(uuid);
      if (depend)
      {
        qDebug() << "UNLOADING PLUGIN: Depends on unloading plugin" << depend->info()->name;
        FPluginItems.removeAt(FPluginItems.indexOf(depend));  
        delete depend;
      }
    }
    qDebug() << "Unloading plugin:" << pluginItem->info()->name;
    FPluginItems.removeAt(FPluginItems.indexOf(pluginItem)); 
    FPlugins.clear();
    delete pluginItem;
  }
  return true;
}

void PluginManager::quit() 
{
  QTimer::singleShot(10,parent(),SLOT(quit())); 
}

PluginItem *PluginManager::getPluginItem(const QUuid &AUuid) const
{ 
  PluginItem *pluginItem;
  foreach(pluginItem, FPluginItems)
    if (pluginItem->uuid() == AUuid)
      return pluginItem;

  return NULL;
}

bool PluginManager::checkDependences(PluginItem *APluginItem) const
{ 
  QUuid depend;
  foreach(depend, APluginItem->info()->dependences)
  {
    bool cheked = getPlugin(depend);
    if (!cheked)
    {
      int i=0;
      while (!cheked && i<FPluginItems.count())
        cheked = FPluginItems.at(i++)->info()->implements.contains(depend);    
    }
    if (!cheked)
      return false;
  }
  return true;
}

bool PluginManager::checkConflicts(PluginItem *APluginItem) const
{ 
  int i=0;
  bool hasConflict = false;
  while (!hasConflict && i<APluginItem->info()->conflicts.count()) 
  {
    int j=0;
    QUuid conflict = APluginItem->info()->conflicts.at(i) ;
    while (!hasConflict && j<FPluginItems.count())
    {
      hasConflict = FPluginItems.at(j)->uuid() == conflict  ||
                    FPluginItems.at(j)->info()->implements.contains(conflict);
      j++;
    } 
    i++;
  }
  return !hasConflict;
}

QList<QUuid> PluginManager::getConflicts(PluginItem *APluginItem) const 
{
  QSet<QUuid> plugins;
  QUuid conflict;
  foreach(conflict, APluginItem->info()->conflicts)
  {
    int i=0;
    while (i<FPluginItems.count())
    {
      if (FPluginItems.at(i)->uuid() == conflict || FPluginItems.at(i)->info()->implements.contains(conflict))
        plugins += FPluginItems.at(i)->uuid(); 
      i++;
    }
  }
  return plugins.toList(); 
}

void PluginManager::onAboutToQuit()
{
  qDebug() << "\n\n<--Quiting application--> \n\n";
  emit aboutToQuit();
}

