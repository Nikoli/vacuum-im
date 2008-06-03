#include "privacylists.h"

#define SHC_PRIVACY         "/iq[@type='set']/query[@xmlns='"NS_JABBER_PRIVACY"']"
#define SHC_ROSTER          "/iq/query[@xmlns='"NS_JABBER_ROSTER"']"

#define PRIVACY_TIMEOUT     60000
#define AUTO_LISTS_TIMEOUT  2000

#define ADR_STREAM_JID      Action::DR_StreamJid
#define ADR_CONTACT_JID     Action::DR_Parametr1
#define ADR_GROUP_NAME      Action::DR_Parametr2
#define ADR_LISTNAME        Action::DR_Parametr3

#define IN_PRIVACY          "psi/eye"

QStringList PrivacyLists::FAutoLists = QStringList()  
                                    << PRIVACY_LIST_VISIBLE
                                    << PRIVACY_LIST_CONFERENCES 
                                    << PRIVACY_LIST_INVISIBLE 
                                    << PRIVACY_LIST_IGNORE 
                                    << PRIVACY_LIST_SUBSCRIPTION;

PrivacyLists::PrivacyLists()
{
  FRostersModel = NULL;
  FRostersView = NULL;
  FRostersViewPlugin = NULL;
  FStanzaProcessor = NULL;
  FRosterPlugin = NULL;
  FMultiChatPlugin = NULL;

  FRosterLabelId = -1;
  FApplyAutoListsTimer.setSingleShot(true);
  FApplyAutoListsTimer.setInterval(AUTO_LISTS_TIMEOUT);
  connect(&FApplyAutoListsTimer,SIGNAL(timeout()),SLOT(onApplyAutoLists()));

  connect(this,SIGNAL(listAboutToBeChanged(const Jid &, const IPrivacyList &)),SLOT(onListAboutToBeChanged(const Jid &, const IPrivacyList &)));
  connect(this,SIGNAL(listLoaded(const Jid &, const QString &)),SLOT(onListChanged(const Jid &, const QString &)));
  connect(this,SIGNAL(listRemoved(const Jid &, const QString &)),SLOT(onListChanged(const Jid &, const QString &)));
  connect(this,SIGNAL(activeListAboutToBeChanged(const Jid &, const QString &)),SLOT(onActiveListAboutToBeChanged(const Jid &, const QString &)));
  connect(this,SIGNAL(activeListChanged(const Jid &, const QString &)),SLOT(onActiveListChanged(const Jid &, const QString &)));
}

PrivacyLists::~PrivacyLists()
{

}

void PrivacyLists::pluginInfo(PluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Enabling or disabling communication with other entities on a network.");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Privacy Lists"); 
  APluginInfo->uid = PRIVACYLISTS_UUID;
  APluginInfo->version = "0.1";
  APluginInfo->dependences.append(STANZAPROCESSOR_UUID);
}

bool PrivacyLists::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->getPlugins("IStanzaProcessor").value(0,NULL);
  if (plugin) 
    FStanzaProcessor = qobject_cast<IStanzaProcessor *>(plugin->instance());

  plugin = APluginManager->getPlugins("IXmppStreams").value(0,NULL);
  if (plugin)
  {
    connect(plugin->instance(), SIGNAL(opened(IXmppStream *)), SLOT(onStreamOpened(IXmppStream *))); 
    connect(plugin->instance(), SIGNAL(closed(IXmppStream *)), SLOT(onStreamClosed(IXmppStream *))); 
  }

  plugin = APluginManager->getPlugins("IRostersModel").value(0,NULL);
  if (plugin) 
  {
    FRostersModel = qobject_cast<IRostersModel *>(plugin->instance());
    if (FRostersModel)
    {
      connect(FRostersModel->instance(),SIGNAL(indexCreated(IRosterIndex *,IRosterIndex *)),
        SLOT(onRosterIndexCreated(IRosterIndex *,IRosterIndex *)));
    }
  }

  plugin = APluginManager->getPlugins("IRostersViewPlugin").value(0,NULL);
  if (plugin) 
    FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());

  plugin = APluginManager->getPlugins("IRosterPlugin").value(0,NULL);
  if (plugin) 
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());

  plugin = APluginManager->getPlugins("IPresencePlugin").value(0,NULL);
  if (plugin) 
    FPresencePlugin = qobject_cast<IPresencePlugin *>(plugin->instance());

  plugin = APluginManager->getPlugins("IMultiUserChatPlugin").value(0,NULL);
  if (plugin) 
  {
    connect(plugin->instance(),SIGNAL(multiUserChatCreated(IMultiUserChat *)),SLOT(onMultiUserChatCreated(IMultiUserChat *)));
  }

  return FStanzaProcessor!=NULL;
}

bool PrivacyLists::initObjects()
{
  if (FRostersViewPlugin)
  {
    FRostersView = FRostersViewPlugin->rostersView();
    FRosterLabelId = FRostersView->createIndexLabel(RLO_PRIVACY,Skin::getSkinIconset(SYSTEM_ICONSETFILE)->iconByName(IN_PRIVACY));
    connect(FRostersView,SIGNAL(labelToolTips(IRosterIndex *, int, QMultiMap<int,QString> &)), 
      SLOT(onRosterLabelToolTips(IRosterIndex *, int, QMultiMap<int,QString> &)));
    connect(FRostersView,SIGNAL(contextMenu(IRosterIndex *, Menu *)), SLOT(onRostersViewContextMenu(IRosterIndex *, Menu *)));
  }
  return true;
}

bool PrivacyLists::readStanza(int AHandlerId, const Jid &AStreamJid, const Stanza &AStanza, bool &AAccept)
{
  if (FSHIPrivacy.value(AStreamJid)==AHandlerId && (AStanza.from().isEmpty() || (AStreamJid && AStanza.from())))
  {
    QDomElement queryElem = AStanza.firstElement("query",NS_JABBER_PRIVACY);
    QDomElement listElem = queryElem.firstChildElement("list");
    QString listName = listElem.attribute("name");
    if (!listName.isEmpty())
    {
      bool needLoad = FRemoveRequests.key(listName).isEmpty();
      QHash<QString,IPrivacyList>::const_iterator it = FSaveRequests.constBegin();
      while (needLoad && it!=FSaveRequests.constEnd())
      {
        needLoad = it.value().name != listName;
        it++;
      }
      if (needLoad)
        loadPrivacyList(AStreamJid,listName);

      AAccept = true;
      Stanza result("iq");
      result.setType("result").setId(AStanza.id());
      FStanzaProcessor->sendStanzaOut(AStreamJid,result);
    }
  }
  else if (FSHIRosterIn.value(AStreamJid)==AHandlerId || FSHIRosterOut.value(AStreamJid)==AHandlerId)
  {
    IPresence *presence = FPresencePlugin!=NULL ? FPresencePlugin->getPresence(AStreamJid) : NULL;
    if (presence && presence->isOpen() && !activeList(AStreamJid).isEmpty())
    {
      bool directionIn = FSHIRosterIn.value(AStreamJid)==AHandlerId;
      QDomElement itemElem = AStanza.firstElement("query",NS_JABBER_ROSTER).firstChildElement("item");
      while (!itemElem.isNull())
      {
        IRosterItem ritem;
        ritem.isValid = true;
        ritem.itemJid = itemElem.attribute("jid");
        ritem.subscription = itemElem.attribute("subscription");
        QDomElement groupElem = itemElem.firstChildElement("group");
        while (!groupElem.isNull())
        {
          ritem.groups += groupElem.text();
          groupElem = groupElem.nextSiblingElement("group");
        }

        int stanzas = denyedStanzas(ritem,privacyList(AStreamJid,activeList(AStreamJid)));
        bool denyed = (stanzas & IPrivacyRule::PresencesOut)>0;
        if (denyed && !FOfflinePresences.value(AStreamJid).contains(ritem.itemJid))
        {
          presence->sendPresence(ritem.itemJid,IPresence::Offline,"",0);
          FOfflinePresences[AStreamJid]+=ritem.itemJid;
        }
        else if (!denyed && directionIn && FOfflinePresences.value(AStreamJid).contains(ritem.itemJid))
        {
          if (ritem.subscription==SUBSCRIPTION_BOTH || ritem.subscription==SUBSCRIPTION_FROM)
            presence->sendPresence(ritem.itemJid,presence->show(),presence->status(),presence->priority());
          FOfflinePresences[AStreamJid]-=ritem.itemJid;
        }

        if (directionIn)
        {
          denyed = (stanzas & IPrivacyRule::AnyStanza)>0;
          if (FLabeledContacts.value(AStreamJid).contains(ritem.itemJid)!=denyed)
            setPrivacyLabel(AStreamJid,ritem.itemJid,denyed);
        }

        itemElem = itemElem.nextSiblingElement("item");
      }
    }
  }
  return false;
}

void PrivacyLists::iqStanza(const Jid &AStreamJid, const Stanza &AStanza)
{
  if (FLoadRequests.contains(AStanza.id()))
  {
    if (AStanza.type() == "result")
    {
      QHash<QString,IPrivacyList> &lists = FPrivacyLists[AStreamJid];
      QDomElement queryElem = AStanza.firstElement("query",NS_JABBER_PRIVACY);
      
      QDomElement listElem = queryElem.firstChildElement("list");
      while (!listElem.isNull())
      {
        QString listName = listElem.attribute("name");
        IPrivacyList &list = lists[listName];
        list.name = listName;
        list.rules.clear();

        QDomElement ruleElem = listElem.firstChildElement("item");
        while (!ruleElem.isNull())
        {
          IPrivacyRule rule;
          rule.order = ruleElem.attribute("order").toInt();
          rule.type = ruleElem.attribute("type");
          rule.value = ruleElem.attribute("value");
          if (rule.type == PRIVACY_TYPE_JID)
            rule.value = Jid(rule.value).prepared().eFull();
          rule.action = ruleElem.attribute("action");
          rule.stanzas = IPrivacyRule::EmptyType;
          if (!ruleElem.firstChildElement("message").isNull())
            rule.stanzas |= IPrivacyRule::Messages;
          if (!ruleElem.firstChildElement("iq").isNull())
            rule.stanzas |= IPrivacyRule::Queries;
          if (!ruleElem.firstChildElement("presence-in").isNull())
            rule.stanzas |= IPrivacyRule::PresencesIn;
          if (!ruleElem.firstChildElement("presence-out").isNull())
            rule.stanzas |= IPrivacyRule::PresencesOut;
          if (rule.stanzas == IPrivacyRule::EmptyType)
            rule.stanzas = IPrivacyRule::AnyStanza;
          list.rules.append(rule);
          ruleElem = ruleElem.nextSiblingElement("item");
        }
        qSort(list.rules);

        if (!list.rules.isEmpty())
          emit listLoaded(AStreamJid,list.name);
        else
          loadPrivacyList(AStreamJid,listName);
        
        listElem = listElem.nextSiblingElement("list");
      }
      
      QDomElement activeElem = queryElem.firstChildElement("active");
      if (!activeElem.isNull())
      {
        QString activeListName = activeElem.attribute("name");
        FActiveLists.insert(AStreamJid,activeListName);
        emit activeListChanged(AStreamJid,activeListName);
      }

      QDomElement defaultElem =queryElem.firstChildElement("default");
      if (!defaultElem.isNull())
      {
        QString defaultListName = defaultElem.attribute("name");
        FDefaultLists.insert(AStreamJid,defaultListName);
        emit defaultListChanged(AStreamJid,defaultListName);
      }

      if (FLoadRequests.value(AStanza.id()).isEmpty())
      {
        if (lists.isEmpty())
          setAutoPrivacy(AStreamJid,PRIVACY_LIST_AUTO_VISIBLE);
        else if (defaultList(AStreamJid)!=activeList(AStreamJid))
          setActiveList(AStreamJid,defaultList(AStreamJid));
      }
    }
    else if (AStanza.type() == "error")
    {
      QString listName = FLoadRequests.value(AStanza.id());
      if (FPrivacyLists.value(AStreamJid).contains(listName))
      {
        FPrivacyLists[AStreamJid].remove(listName);
        emit listRemoved(AStreamJid,listName);
      }
    }
    FLoadRequests.remove(AStanza.id());
  }
  else if (FSaveRequests.contains(AStanza.id()))
  {
    if (AStanza.type() == "result")
    {
      IPrivacyList list = FSaveRequests.value(AStanza.id());
      FPrivacyLists[AStreamJid].insert(list.name,list);
      emit listLoaded(AStreamJid,list.name);
    }
    FSaveRequests.remove(AStanza.id());
  }
  else if (FActiveRequests.contains(AStanza.id()))
  {
    if (AStanza.type() == "result")
    {
      QString activeListName = FActiveRequests.value(AStanza.id());
      FActiveLists.insert(AStreamJid,activeListName);
      emit activeListChanged(AStreamJid,activeListName);
    }
    FActiveRequests.remove(AStanza.id());
  }
  else if (FDefaultRequests.contains(AStanza.id()))
  {
    if (AStanza.type() == "result")
    {
      QString defaultListName = FDefaultRequests.value(AStanza.id());
      FDefaultLists.insert(AStreamJid,defaultListName);
      emit defaultListChanged(AStreamJid,defaultListName);
    }
    FDefaultRequests.remove(AStanza.id());
  }
  else if (FRemoveRequests.contains(AStanza.id()))
  {
    if (AStanza.type() == "result")
    {
      QString listName = FRemoveRequests.value(AStanza.id());
      FPrivacyLists[AStreamJid].remove(listName);
      emit listRemoved(AStreamJid,listName);
    }
    FRemoveRequests.remove(AStanza.id());
  }
  FStreamRequests[AStreamJid].removeAt(FStreamRequests[AStreamJid].indexOf(AStanza.id()));

  if (AStanza.type() == "result")
    emit requestCompleted(AStanza.id());
  else
    emit requestFailed(AStanza.id(),ErrorHandler(AStanza.element()).message());
}

void PrivacyLists::iqStanzaTimeOut(const QString &AId)
{
  if (FLoadRequests.contains(AId))
    FLoadRequests.remove(AId);
  else if (FSaveRequests.contains(AId))
    FSaveRequests.remove(AId);
  else if (FActiveRequests.contains(AId))
    FActiveRequests.remove(AId);
  else if (FDefaultRequests.contains(AId))
    FDefaultRequests.remove(AId);
  else if (FRemoveRequests.contains(AId))
    FRemoveRequests.remove(AId);
  
  foreach(Jid streamJid, FStreamRequests.keys())
    FStreamRequests[streamJid].removeAt(FStreamRequests[streamJid].indexOf(AId));

  emit requestFailed(AId,ErrorHandler(ErrorHandler::REQUEST_TIMEOUT).message());
}

bool PrivacyLists::isReady(const Jid &AStreamJid) const
{
  return FPrivacyLists.contains(AStreamJid);
}

IPrivacyRule PrivacyLists::autoListRule(const Jid &AContactJid, const QString &AList) const
{
  IPrivacyRule rule;
  rule.order = 0;
  rule.type = PRIVACY_TYPE_JID;
  rule.value = AContactJid.prepared().eFull();
  rule.stanzas = IPrivacyRule::EmptyType;
  if (AList == PRIVACY_LIST_VISIBLE)
  {
    rule.action = PRIVACY_ACTION_ALLOW;
    rule.stanzas = IPrivacyRule::PresencesOut;
  }
  else if (AList == PRIVACY_LIST_INVISIBLE)
  {
    rule.action = PRIVACY_ACTION_DENY;
    rule.stanzas = IPrivacyRule::PresencesOut;
  }
  else if (AList == PRIVACY_LIST_IGNORE)
  {
    rule.action = PRIVACY_ACTION_DENY;
    rule.stanzas = IPrivacyRule::AnyStanza;
  }
  else if (AList == PRIVACY_LIST_CONFERENCES)
  {
    rule.action = PRIVACY_ACTION_ALLOW;
    rule.stanzas = IPrivacyRule::AnyStanza;
  }
  return rule;
}

IPrivacyRule PrivacyLists::autoListRule(const QString &AGroup, const QString &AAutoList) const
{
  IPrivacyRule rule = autoListRule(Jid(),AAutoList);
  rule.type = PRIVACY_TYPE_GROUP;
  rule.value = AGroup;
  return rule;
}

bool PrivacyLists::isAutoListed(const Jid &AStreamJid, const Jid &AContactJid, const QString &AList) const
{
  IPrivacyRule rule = autoListRule(AContactJid,AList);
  return privacyList(AStreamJid,AList,true).rules.contains(rule);
}

bool PrivacyLists::isAutoListed(const Jid &AStreamJid, const QString &AGroup, const QString &AList) const
{
  IPrivacyRule rule = autoListRule(AGroup,AList);
  return privacyList(AStreamJid,AList,true).rules.contains(rule);
}

void PrivacyLists::setAutoListed(const Jid &AStreamJid, const Jid &AContactJid, const QString &AList, bool AInserted)
{
  IPrivacyRule rule = autoListRule(AContactJid,AList);
  if (isReady(AStreamJid) && rule.stanzas != IPrivacyRule::EmptyType)
  {
    IPrivacyList list = privacyList(AStreamJid,AList,true);
    list.name = AList;
    if (AInserted != list.rules.contains(rule))
    {
      if (AInserted)
      {
        setAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_VISIBLE,false);
        setAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_CONFERENCES,false);
        setAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_INVISIBLE,false);
        setAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_IGNORE,false);
        list.rules.append(rule);
      }
      else
        list.rules.removeAt(list.rules.indexOf(rule));

      for (int i=0; i<list.rules.count();i++)
        list.rules[i].order=i;

      !list.rules.isEmpty() ? savePrivacyList(AStreamJid,list) : removePrivacyList(AStreamJid,AList);
    }
  }
}

void PrivacyLists::setAutoListed(const Jid &AStreamJid, const QString &AGroup, const QString &AListName, bool AInserted)
{
  IPrivacyRule rule = autoListRule(AGroup,AListName);
  if (isReady(AStreamJid) && !AGroup.isEmpty() && rule.stanzas != IPrivacyRule::EmptyType)
  {
    IPrivacyList list = privacyList(AStreamJid,AListName,true);
    list.name = AListName;
    if (AInserted != list.rules.contains(rule))
    {
      if (AInserted)
      {
        setAutoListed(AStreamJid,AGroup,PRIVACY_LIST_VISIBLE,false);
        setAutoListed(AStreamJid,AGroup,PRIVACY_LIST_INVISIBLE,false);
        setAutoListed(AStreamJid,AGroup,PRIVACY_LIST_IGNORE,false);
      }

      IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
      QStringList groups = roster!=NULL ? (roster->groups()<<AGroup).toList() : QStringList(AGroup);
      QString groupWithDelim = roster!=NULL ? AGroup + roster->groupDelimiter() : AGroup;
      qSort(groups);
      foreach(QString group, groups)
      {
        if(group==AGroup || group.startsWith(groupWithDelim))
        {
          rule.value = group;
          if (AInserted)
          {
            if (!isAutoListed(AStreamJid,group,PRIVACY_LIST_VISIBLE)    &&
                !isAutoListed(AStreamJid,group,PRIVACY_LIST_INVISIBLE)  &&
                !isAutoListed(AStreamJid,group,PRIVACY_LIST_IGNORE))
            {
              list.rules.append(rule);
            }
          }
          else
            list.rules.removeAt(list.rules.indexOf(rule));
        }
      }

      for (int i=0; i<list.rules.count();i++)
        list.rules[i].order=i;

      !list.rules.isEmpty() ? savePrivacyList(AStreamJid,list) : removePrivacyList(AStreamJid,AListName);
    }
  }
}

IPrivacyRule PrivacyLists::offRosterRule() const
{
  IPrivacyRule rule;
  rule.type = PRIVACY_TYPE_SUBSCRIPTION;
  rule.value = SUBSCRIPTION_NONE;
  rule.action = PRIVACY_ACTION_DENY;
  rule.stanzas = IPrivacyRule::AnyStanza;
  return rule;
}

bool PrivacyLists::isOffRosterBlocked(const Jid &AStreamJid) const
{
  IPrivacyRule rule = offRosterRule();
  IPrivacyList list = privacyList(AStreamJid,PRIVACY_LIST_SUBSCRIPTION,true);
  return list.rules.contains(rule);
}

void PrivacyLists::setOffRosterBlocked(const Jid &AStreamJid, bool ABlocked)
{
  IPrivacyRule rule = offRosterRule();
  IPrivacyList list = privacyList(AStreamJid,PRIVACY_LIST_SUBSCRIPTION,true);
  if (list.rules.contains(rule) != ABlocked)
  {
    list.name = PRIVACY_LIST_SUBSCRIPTION;
    if (ABlocked)
      list.rules.append(rule);
    else
      list.rules.removeAt(list.rules.indexOf(rule));

    for (int i=0; i<list.rules.count();i++)
      list.rules[i].order = i;

    !list.rules.isEmpty() ? savePrivacyList(AStreamJid,list) : removePrivacyList(AStreamJid,list.name);
  }
}

bool PrivacyLists::isAutoPrivacy(const Jid &AStreamJid) const
{
  if (isReady(AStreamJid))
  {
    QString listName = activeList(AStreamJid,true);
    return listName==PRIVACY_LIST_AUTO_VISIBLE || listName==PRIVACY_LIST_AUTO_INVISIBLE;
  }
  return false;
}

void PrivacyLists::setAutoPrivacy(const Jid &AStreamJid, const QString &AAutoList)
{
  if (isReady(AStreamJid) && activeList(AStreamJid,true)!=AAutoList)
  {
    if (AAutoList == PRIVACY_LIST_AUTO_VISIBLE)
    {
      FApplyAutoLists.insert(AStreamJid,AAutoList);
      onApplyAutoLists();
      setDefaultList(AStreamJid,AAutoList);
      setActiveList(AStreamJid,AAutoList);
    }
    else if (AAutoList == PRIVACY_LIST_AUTO_INVISIBLE)
    {
      FApplyAutoLists.insert(AStreamJid,AAutoList);
      onApplyAutoLists();
      setDefaultList(AStreamJid,AAutoList);
      setActiveList(AStreamJid,AAutoList);
    }
    else
    {
      FApplyAutoLists.remove(AStreamJid);
      setDefaultList(AStreamJid,"");
      setActiveList(AStreamJid,"");
    }
  }
}

int PrivacyLists::denyedStanzas(const IRosterItem &AItem, const IPrivacyList &AList) const
{
  int denyed = 0;
  int allowed = 0;
  foreach(IPrivacyRule rule, AList.rules)
  {
    int stanzas = 0;
    if (rule.type == PRIVACY_TYPE_ALWAYS)
    {
      stanzas = rule.stanzas;
    }
    else if (rule.type == PRIVACY_TYPE_GROUP)
    {
      if (AItem.groups.contains(rule.value))
        stanzas = rule.stanzas;
    }
    else if (rule.type == PRIVACY_TYPE_SUBSCRIPTION)
    {
      if (AItem.subscription == rule.value)
        stanzas = rule.stanzas;
    }
    else if (rule.type == PRIVACY_TYPE_JID)
    {
      if (isMatchedJid(rule.value,AItem.itemJid))
        stanzas = rule.stanzas;
    }

    if (rule.action == PRIVACY_ACTION_DENY)
      denyed |= stanzas & (~allowed);
    else
      allowed |= stanzas & (~denyed);
  }
  return denyed;
}

QHash<Jid,int> PrivacyLists::denyedContacts(const Jid &AStreamJid, const IPrivacyList &AList, int AFilter) const
{
  QHash<Jid,int> denyed;
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
  QList<IRosterItem> ritems = roster!=NULL ? roster->rosterItems() : QList<IRosterItem>();
  foreach(IRosterItem ritem,ritems)
  {
    int stanzas = denyedStanzas(ritem,AList);
    if ((stanzas & AFilter) > 0)
      denyed[ritem.itemJid] = stanzas;
  }
  return denyed;
}

QString PrivacyLists::activeList(const Jid &AStreamJid, bool APending) const
{
  if (APending)
  {
    foreach(QString id, FStreamRequests.value(AStreamJid))
    {
      if (FActiveRequests.contains(id))
        return FActiveRequests.value(id);
    }
  }
  return FActiveLists.value(AStreamJid);
}

QString PrivacyLists::setActiveList(const Jid &AStreamJid, const QString &AList)
{
  if (isReady(AStreamJid) && AList!=activeList(AStreamJid))
  {
    Stanza set("iq");
    set.setType("set").setId(FStanzaProcessor->newId());
    QDomElement queryElem = set.addElement("query",NS_JABBER_PRIVACY);
    QDomElement activeElem = queryElem.appendChild(set.createElement("active")).toElement();
    if (!AList.isEmpty())
      activeElem.setAttribute("name",AList);
    
    emit activeListAboutToBeChanged(AStreamJid,AList);
    if (FStanzaProcessor->sendIqStanza(this,AStreamJid,set,PRIVACY_TIMEOUT))
    {
      FStreamRequests[AStreamJid].prepend(set.id());
      FActiveRequests.insert(set.id(),AList);
      return set.id();
    }
  }
  return QString();
}

QString PrivacyLists::defaultList(const Jid &AStreamJid, bool APending) const
{
  if (APending)
  {
    foreach(QString id, FStreamRequests.value(AStreamJid))
    {
      if (FDefaultRequests.contains(id))
        return FDefaultRequests.value(id);
    }
  }
  return FDefaultLists.value(AStreamJid);
}

QString PrivacyLists::setDefaultList(const Jid &AStreamJid, const QString &AList)
{
  if (isReady(AStreamJid) && AList!=defaultList(AStreamJid))
  {
    Stanza set("iq");
    set.setType("set").setId(FStanzaProcessor->newId());
    QDomElement queryElem = set.addElement("query",NS_JABBER_PRIVACY);
    QDomElement defaultElem = queryElem.appendChild(set.createElement("default")).toElement();
    if (!AList.isEmpty())
      defaultElem.setAttribute("name",AList);
    if (FStanzaProcessor->sendIqStanza(this,AStreamJid,set,PRIVACY_TIMEOUT))
    {
      FStreamRequests[AStreamJid].prepend(set.id());
      FDefaultRequests.insert(set.id(),AList);
      return set.id();
    }
  }
  return QString();
}

IPrivacyList PrivacyLists::privacyList(const Jid &AStreamJid, const QString &AList, bool APending) const
{
  if (APending)
  {
    foreach(QString id, FStreamRequests.value(AStreamJid))
    {
      if (FSaveRequests.value(id).name == AList)
        return FSaveRequests.value(id);
      else if (FRemoveRequests.value(id) == AList)
        return IPrivacyList();
    }
  }
  return FPrivacyLists.value(AStreamJid).value(AList);
}

QList<IPrivacyList> PrivacyLists::privacyLists(const Jid &AStreamJid, bool APending) const
{
  if (APending)
  {
    QList<IPrivacyList> lists;
    QList<QString> listNames = FPrivacyLists.value(AStreamJid).keys();
    foreach(QString listName, listNames)
    {
      IPrivacyList list = privacyList(AStreamJid,listName,APending);
      if (list.name == listName)
        lists.append(list);
    }
    foreach(QString id, FStreamRequests.value(AStreamJid))
    {
      if (FSaveRequests.contains(id) && !listNames.contains(FSaveRequests.value(id).name))
        lists.append(FSaveRequests.value(id));
    }
  }
  return FPrivacyLists.value(AStreamJid).values();
}

QString PrivacyLists::loadPrivacyList(const Jid &AStreamJid, const QString &AList)
{
  if (isReady(AStreamJid) && !AList.isEmpty())
  {
    Stanza load("iq");
    load.setType("get").setId(FStanzaProcessor->newId());
    QDomElement elem = load.addElement("query",NS_JABBER_PRIVACY);
    elem.appendChild(load.createElement("list")).toElement().setAttribute("name",AList);
    if (FStanzaProcessor->sendIqStanza(this,AStreamJid,load,PRIVACY_TIMEOUT))
    {
      FStreamRequests[AStreamJid].prepend(load.id());
      FLoadRequests.insert(load.id(),AList);
      return load.id();
    }
  }
  return QString();
}

QString PrivacyLists::savePrivacyList(const Jid &AStreamJid, const IPrivacyList &AList)
{
  if (isReady(AStreamJid) && !AList.name.isEmpty() && !AList.rules.isEmpty())
  {
    if (privacyList(AStreamJid,AList.name,true) != AList)
    {
      Stanza save("iq");
      save.setType("set").setId(FStanzaProcessor->newId());
      QDomElement queryElem = save.addElement("query",NS_JABBER_PRIVACY);
      QDomElement listElem =  queryElem.appendChild(save.createElement("list")).toElement();
      listElem.setAttribute("name",AList.name);

      foreach(IPrivacyRule item, AList.rules)
      {
        QDomElement itemElem = listElem.appendChild(save.createElement("item")).toElement();
        itemElem.setAttribute("order",item.order);
        itemElem.setAttribute("action",item.action);
        if (!item.type.isEmpty())
          itemElem.setAttribute("type",item.type);
        if (item.type!=PRIVACY_TYPE_ALWAYS && !item.value.isEmpty())
          itemElem.setAttribute("value",item.value);
        if (item.stanzas != IPrivacyRule::AnyStanza)
        {
          if (item.stanzas & IPrivacyRule::Messages)
            itemElem.appendChild(save.createElement("message"));
          if (item.stanzas & IPrivacyRule::Queries)
            itemElem.appendChild(save.createElement("iq"));
          if (item.stanzas & IPrivacyRule::PresencesIn)
            itemElem.appendChild(save.createElement("presence-in"));
          if (item.stanzas & IPrivacyRule::PresencesOut)
            itemElem.appendChild(save.createElement("presence-out"));
        }
      }

      emit listAboutToBeChanged(AStreamJid,AList);
      if (FStanzaProcessor->sendIqStanza(this,AStreamJid,save,PRIVACY_TIMEOUT))
      {
        FStreamRequests[AStreamJid].prepend(save.id());
        FSaveRequests.insert(save.id(),AList);
        return save.id();
      }
    }
    else
      return QString("");
  }
  return QString();
}

QString PrivacyLists::removePrivacyList(const Jid &AStreamJid, const QString &AList)
{
  if (isReady(AStreamJid) && !AList.isEmpty())
  {
    Stanza remove("iq");
    remove.setType("set").setId(FStanzaProcessor->newId());
    QDomElement queryElem = remove.addElement("query",NS_JABBER_PRIVACY);
    queryElem.appendChild(remove.createElement("list")).toElement().setAttribute("name",AList);
    if (FStanzaProcessor->sendIqStanza(this,AStreamJid,remove,PRIVACY_TIMEOUT))
    {
      FStreamRequests[AStreamJid].prepend(remove.id());
      FRemoveRequests.insert(remove.id(),AList);
      return remove.id();
    }
  }
  return QString();
}

QDialog *PrivacyLists::showEditListsDialog(const Jid &AStreamJid, QWidget *AParent)
{
  EditListsDialog *dialog = FEditListsDialogs.value(AStreamJid);
  if (isReady(AStreamJid))
  {
    if (!dialog)
    {
      dialog = new EditListsDialog(this,FRosterPlugin!= NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL, AStreamJid,AParent);
      connect(dialog,SIGNAL(destroyed(const Jid &)),SLOT(onEditListsDialogDestroyed(const Jid &)));
      FEditListsDialogs.insert(AStreamJid,dialog);
    }
    dialog->show();
  }
  return dialog;
}

QString PrivacyLists::loadPrivacyLists(const Jid &AStreamJid)
{
  if (FStanzaProcessor)
  {
    Stanza load("iq");
    load.setType("get").setId(FStanzaProcessor->newId());
    load.addElement("query",NS_JABBER_PRIVACY);
    if (FStanzaProcessor->sendIqStanza(this,AStreamJid,load,PRIVACY_TIMEOUT))
    {
      FLoadRequests.insert(load.id(),"");
      return load.id();
    }
  }
  return QString();
}

Menu *PrivacyLists::createPrivacyMenu(Menu *AMenu) const
{
  Menu *pmenu = new Menu(AMenu);
  pmenu->setTitle(tr("Privacy"));
  pmenu->setIcon(SYSTEM_ICONSETFILE,IN_PRIVACY);
  AMenu->addAction(pmenu->menuAction(),AG_PRIVACYLISTS_ROSTER,true);
  return pmenu;
}

void PrivacyLists::createAutoPrivacyStreamActions(const Jid &AStreamJid, Menu *AMenu) const
{
  QString activeListName = activeList(AStreamJid);
  Action *action = new Action(AMenu);
  action->setText(tr("Visible Mode"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_LISTNAME,PRIVACY_LIST_AUTO_VISIBLE);
  action->setCheckable(true);
  action->setChecked(activeListName == PRIVACY_LIST_AUTO_VISIBLE);
  connect(action,SIGNAL(triggered(bool)),SLOT(onSetAutoPrivacyByAction(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);

  action = new Action(AMenu);
  action->setText(tr("Invisible Mode"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_LISTNAME,PRIVACY_LIST_AUTO_INVISIBLE);
  action->setCheckable(true);
  action->setChecked(activeListName == PRIVACY_LIST_AUTO_INVISIBLE);
  connect(action,SIGNAL(triggered(bool)),SLOT(onSetAutoPrivacyByAction(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);

  if (!activeListName.isEmpty())
  {
    action = new Action(AMenu);
    action->setText(tr("Disable privacy lists"));
    action->setData(ADR_STREAM_JID,AStreamJid.full());
    action->setData(ADR_LISTNAME,"");
    connect(action,SIGNAL(triggered(bool)),SLOT(onSetAutoPrivacyByAction(bool)));
    AMenu->addAction(action,AG_DEFAULT,false);
  }

  if (isAutoPrivacy(AStreamJid))
  {
    action = new Action(AMenu);
    action->setText(tr("Block off roster contacts"));
    action->setData(ADR_STREAM_JID,AStreamJid.full());
    action->setData(ADR_LISTNAME,PRIVACY_LIST_SUBSCRIPTION);
    action->setCheckable(true);
    action->setChecked(isOffRosterBlocked(AStreamJid));
    connect(action,SIGNAL(triggered(bool)),SLOT(onChangeOffRosterBlocked(bool)));
    AMenu->addAction(action,AG_DEFAULT+200,false);
  }
}

void PrivacyLists::createAutoPrivacyContactActions(const Jid &AStreamJid, const Jid &AContactJid, Menu *AMenu) const
{
  Action *action = new Action(AMenu);
  action->setText(tr("Visible to contact"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_CONTACT_JID,AContactJid.prepared().eBare());
  action->setData(ADR_LISTNAME,PRIVACY_LIST_VISIBLE);
  action->setCheckable(true);
  action->setChecked(isAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_VISIBLE));
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeContactAutoListed(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);

  action = new Action(AMenu);
  action->setText(tr("Invisible to contact"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_CONTACT_JID,AContactJid.prepared().eBare());
  action->setData(ADR_LISTNAME,PRIVACY_LIST_INVISIBLE);
  action->setCheckable(true);
  action->setChecked(isAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_INVISIBLE));
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeContactAutoListed(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);

  action = new Action(AMenu);
  action->setText(tr("Ignore contact"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_CONTACT_JID,AContactJid.prepared().eBare());
  action->setData(ADR_LISTNAME,PRIVACY_LIST_IGNORE);
  action->setCheckable(true);
  action->setChecked(isAutoListed(AStreamJid,AContactJid,PRIVACY_LIST_IGNORE));
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeContactAutoListed(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);
}

void PrivacyLists::createAutoPrivacyGroupActions(const Jid &AStreamJid, const QString &AGroup, Menu *AMenu) const
{
  Action *action = new Action(AMenu);
  action->setText(tr("Visible to group"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_GROUP_NAME,AGroup);
  action->setData(ADR_LISTNAME,PRIVACY_LIST_VISIBLE);
  action->setCheckable(true);
  action->setChecked(isAutoListed(AStreamJid,AGroup,PRIVACY_LIST_VISIBLE));
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupAutoListed(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);

  action = new Action(AMenu);
  action->setText(tr("Invisible to group"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_GROUP_NAME,AGroup);
  action->setData(ADR_LISTNAME,PRIVACY_LIST_INVISIBLE);
  action->setCheckable(true);
  action->setChecked(isAutoListed(AStreamJid,AGroup,PRIVACY_LIST_INVISIBLE));
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupAutoListed(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);

  action = new Action(AMenu);
  action->setText(tr("Ignore group"));
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_GROUP_NAME,AGroup);
  action->setData(ADR_LISTNAME,PRIVACY_LIST_IGNORE);
  action->setCheckable(true);
  action->setChecked(isAutoListed(AStreamJid,AGroup,PRIVACY_LIST_IGNORE));
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupAutoListed(bool)));
  AMenu->addAction(action,AG_DEFAULT,false);
}

Menu *PrivacyLists::createSetActiveMenu(const Jid &AStreamJid, const QList<IPrivacyList> &ALists, Menu *AMenu) const
{
  QString alist = activeList(AStreamJid);

  Menu *amenu = new Menu(AMenu);
  amenu->setTitle(tr("Set Active list"));

  Action *action = new Action(amenu);
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_LISTNAME,"");
  action->setCheckable(true);
  action->setChecked(alist.isEmpty());
  action->setText(tr("<None>"));
  connect(action,SIGNAL(triggered(bool)),SLOT(onSetActiveListByAction(bool)));
  amenu->addAction(action,AG_DEFAULT-100,false);

  foreach(IPrivacyList list,ALists)
  {
    action = new Action(amenu);
    action->setData(ADR_STREAM_JID,AStreamJid.full());
    action->setData(ADR_LISTNAME,list.name);
    action->setCheckable(true);
    action->setChecked(list.name == alist);
    action->setText(list.name);
    connect(action,SIGNAL(triggered(bool)),SLOT(onSetActiveListByAction(bool)));
    amenu->addAction(action,AG_DEFAULT,true);
  }
  AMenu->addAction(amenu->menuAction(),AG_DEFAULT+200,false);
  return amenu;
}

Menu *PrivacyLists::createSetDefaultMenu(const Jid &AStreamJid, const QList<IPrivacyList> &ALists, Menu *AMenu) const
{
  QString dlist = defaultList(AStreamJid);

  Menu *dmenu = new Menu(AMenu);
  dmenu->setTitle(tr("Set Default list"));

  Action *action = new Action(dmenu);
  action->setData(ADR_STREAM_JID,AStreamJid.full());
  action->setData(ADR_LISTNAME,"");
  action->setCheckable(true);
  action->setChecked(dlist.isEmpty());
  action->setText(tr("<None>"));
  connect(action,SIGNAL(triggered(bool)),SLOT(onSetDefaultListByAction(bool)));
  dmenu->addAction(action,AG_DEFAULT-100,false);

  foreach(IPrivacyList list,ALists)
  {
    action = new Action(dmenu);
    action->setData(ADR_STREAM_JID,AStreamJid.full());
    action->setData(ADR_LISTNAME,list.name);
    action->setCheckable(true);
    action->setChecked(list.name == dlist);
    action->setText(list.name);
    connect(action,SIGNAL(triggered(bool)),SLOT(onSetDefaultListByAction(bool)));
    dmenu->addAction(action,AG_DEFAULT,true);
  }
  AMenu->addAction(dmenu->menuAction(),AG_DEFAULT+200,false);
  return dmenu;
}

bool PrivacyLists::isMatchedJid(const Jid &AMask, const Jid &AJid) const
{
  return  ( (AMask.pDomain() == AJid.pDomain()) &&
            (AMask.node().isEmpty() || AMask.pNode()==AJid.pNode()) &&
            (AMask.resource().isEmpty() || AMask.resource()==AJid.resource()) );
}

void PrivacyLists::sendOnlinePresences(const Jid &AStreamJid, const IPrivacyList &AAutoList)
{
  IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(AStreamJid) : NULL;
  IPresence *presence = FPresencePlugin!=NULL ? FPresencePlugin->getPresence(AStreamJid) : NULL;
  if (presence)
  {
    QSet<Jid> denyed = denyedContacts(AStreamJid,AAutoList,IPrivacyRule::PresencesOut).keys().toSet();
    QSet<Jid> online = FOfflinePresences.value(AStreamJid) - denyed;
    if (presence->isOpen())
    {
      foreach(Jid contactJid, online)
      {
        IRosterItem ritem = roster!=NULL ? roster->rosterItem(contactJid) : IRosterItem();
        if (ritem.subscription==SUBSCRIPTION_BOTH || ritem.subscription==SUBSCRIPTION_FROM)
          presence->sendPresence(contactJid,presence->show(),presence->status(),presence->priority());
      }
      presence->setPresence(presence->show(),presence->status(),presence->priority());
    }
    FOfflinePresences[AStreamJid] -= online;
  }
}

void PrivacyLists::sendOfflinePresences(const Jid &AStreamJid, const IPrivacyList &AAutoList)
{
  IPresence *presence = FPresencePlugin!=NULL ? FPresencePlugin->getPresence(AStreamJid) : NULL;
  if (presence)
  {
    QSet<Jid> denyed = denyedContacts(AStreamJid,AAutoList,IPrivacyRule::PresencesOut).keys().toSet();
    QSet<Jid> offline = denyed - FOfflinePresences.value(AStreamJid);
    if (presence->isOpen())
    {
      foreach(Jid contactJid, offline)
        presence->sendPresence(contactJid,IPresence::Offline,"",0);
    }
    FOfflinePresences[AStreamJid] += offline;
  }
}

void PrivacyLists::setPrivacyLabel(const Jid &AStreamJid, const Jid &AContactJid, bool AVisible)
{
  if (FRostersModel)
  {
    IRosterIndexList indexList = FRostersModel->getContactIndexList(AStreamJid,AContactJid);
    foreach(IRosterIndex *index, indexList)
    {
      if (AVisible)
      {
        FLabeledContacts[AStreamJid]+=AContactJid;
        FRostersView->insertIndexLabel(FRosterLabelId,index);
      }
      else
      {
        FLabeledContacts[AStreamJid]-=AContactJid;
        FRostersView->removeIndexLabel(FRosterLabelId,index);
      }
    }
  }
}

void PrivacyLists::updatePrivacyLabels(const Jid &AStreamJid)
{
  if (FRostersModel)
  {
    QSet<Jid> denyed = denyedContacts(AStreamJid,privacyList(AStreamJid,activeList(AStreamJid))).keys().toSet();
    QSet<Jid> deny = denyed - FLabeledContacts.value(AStreamJid);
    QSet<Jid> allow = FLabeledContacts.value(AStreamJid) - denyed;
    
    foreach(Jid contactJid, deny) {
      setPrivacyLabel(AStreamJid,contactJid,true); }

    foreach(Jid contactJid, allow) {
      setPrivacyLabel(AStreamJid,contactJid,false); }

    IRosterIndex *rootIndex = FRostersModel->streamRoot(AStreamJid);
    IRosterIndex *groupIndex = FRostersModel->findRosterIndex(RIT_NotInRosterGroup,FRostersModel->notInRosterGroupName(),rootIndex);
    if (groupIndex)
    {
      for (int i=0;i<groupIndex->childCount();i++)
      {
        IRosterIndex *index = groupIndex->child(i);
        if (index->type() == RIT_Contact || index->type()==RIT_Agent)
        {
          IRosterItem ritem;
          ritem.itemJid = index->data(RDR_BareJid).toString();
          if ((denyedStanzas(ritem,privacyList(AStreamJid,activeList(AStreamJid))) & IPrivacyRule::AnyStanza)>0)
            FRostersView->insertIndexLabel(FRosterLabelId,index);
          else
            FRostersView->removeIndexLabel(FRosterLabelId,index);
        }
      }
    }
  }
}

void PrivacyLists::onListAboutToBeChanged(const Jid &AStreamJid, const IPrivacyList &AList)
{
  if (AList.name == activeList(AStreamJid))
  {
    sendOfflinePresences(AStreamJid,AList);
  }
}

void PrivacyLists::onListChanged(const Jid &AStreamJid, const QString &AList)
{
  if (isAutoPrivacy(AStreamJid) && FAutoLists.contains(AList))
  {
    FApplyAutoLists.insert(AStreamJid,activeList(AStreamJid));
    FApplyAutoListsTimer.start();
  }
  else if (AList == activeList(AStreamJid))
  {
    sendOnlinePresences(AStreamJid,privacyList(AStreamJid,AList));
    updatePrivacyLabels(AStreamJid);
  }
}

void PrivacyLists::onActiveListAboutToBeChanged(const Jid &AStreamJid, const QString &AList)
{
  sendOfflinePresences(AStreamJid,privacyList(AStreamJid,AList));
}

void PrivacyLists::onActiveListChanged(const Jid &AStreamJid, const QString &AList)
{
  sendOnlinePresences(AStreamJid,privacyList(AStreamJid,AList));
  updatePrivacyLabels(AStreamJid);
}

void PrivacyLists::onApplyAutoLists()
{
  QList<Jid> streamJids = FApplyAutoLists.keys();
  foreach(Jid streamJid, streamJids)
  {
    IPrivacyList list;
    list.name = FApplyAutoLists.value(streamJid);

    IPrivacyRule selfAllow;
    selfAllow.type = PRIVACY_TYPE_JID;
    selfAllow.value = streamJid.prepared().eBare();
    selfAllow.action = PRIVACY_ACTION_ALLOW;
    selfAllow.stanzas = IPrivacyRule::AnyStanza;
    list.rules.append(selfAllow);

    foreach(QString listName, FAutoLists)
    {
      IPrivacyList autoList = privacyList(streamJid,listName,true);
      list.rules+=autoList.rules;
    }

    if (list.name == PRIVACY_LIST_AUTO_VISIBLE)
    {
      IPrivacyRule visible;
      visible.type = PRIVACY_TYPE_ALWAYS;
      visible.action = PRIVACY_ACTION_ALLOW;
      visible.stanzas = IPrivacyRule::AnyStanza;
      list.rules.append(visible);
    }
    else if (list.name == PRIVACY_LIST_AUTO_INVISIBLE)
    {
      IPrivacyRule invisible;
      invisible.type = PRIVACY_TYPE_ALWAYS;
      invisible.action = PRIVACY_ACTION_DENY;
      invisible.stanzas = IPrivacyRule::PresencesOut;
      list.rules.append(invisible);
    }

    for (int i=0;i<list.rules.count();i++)
      list.rules[i].order = i;

    savePrivacyList(streamJid,list);
  }
  FApplyAutoLists.clear();
}

void PrivacyLists::onStreamOpened(IXmppStream *AXmppStream)
{
  if (FStanzaProcessor)
  {
    int handler = FStanzaProcessor->insertHandler(this,SHC_PRIVACY,IStanzaProcessor::DirectionIn,SHP_DEFAULT,AXmppStream->jid());
    FSHIPrivacy.insert(AXmppStream->jid(),handler);
    
    handler = FStanzaProcessor->insertHandler(this,SHC_ROSTER,IStanzaProcessor::DirectionIn,SHP_DEFAULT-1,AXmppStream->jid());
    FSHIRosterIn.insert(AXmppStream->jid(),handler);

    handler = FStanzaProcessor->insertHandler(this,SHC_ROSTER,IStanzaProcessor::DirectionOut,SHP_DEFAULT-1,AXmppStream->jid());
    FSHIRosterOut.insert(AXmppStream->jid(),handler);

    loadPrivacyLists(AXmppStream->jid());
  }
}

void PrivacyLists::onStreamClosed(IXmppStream *AXmppStream)
{
  if (FEditListsDialogs.contains(AXmppStream->jid()))
    delete FEditListsDialogs.take(AXmppStream->jid());

  FApplyAutoLists.remove(AXmppStream->jid());
  FOfflinePresences.remove(AXmppStream->jid());
  FActiveLists.remove(AXmppStream->jid());
  FDefaultLists.remove(AXmppStream->jid());
  FPrivacyLists.remove(AXmppStream->jid());
  FStreamRequests.remove(AXmppStream->jid());

  updatePrivacyLabels(AXmppStream->jid());

  if (FStanzaProcessor)
  {
    FStanzaProcessor->removeHandler(FSHIPrivacy.take(AXmppStream->jid()));
    FStanzaProcessor->removeHandler(FSHIRosterIn.take(AXmppStream->jid()));
    FStanzaProcessor->removeHandler(FSHIRosterOut.take(AXmppStream->jid()));
  }
}

void PrivacyLists::onRostersViewContextMenu(IRosterIndex *AIndex, Menu *AMenu)
{
  Jid streamJid = AIndex->data(RDR_StreamJid).toString();
  if (isReady(streamJid))
  {
    if (AIndex->type() == RIT_StreamRoot)
    {
      Menu *pmenu = createPrivacyMenu(AMenu);
      createAutoPrivacyStreamActions(streamJid,pmenu);

      if (!isAutoPrivacy(streamJid))
      {
        QList<IPrivacyList> lists = privacyLists(streamJid);
        for (int i=0; i<lists.count(); i++)
          if (FAutoLists.contains(lists.at(i).name))
            lists.removeAt(i--);

        if (!lists.isEmpty())
        {
          createSetActiveMenu(streamJid,lists,pmenu);
          createSetDefaultMenu(streamJid,lists,pmenu);
        }
      }

      Action *action = new Action(AMenu);
      action->setText(tr("Advanced..."));
      action->setData(ADR_STREAM_JID,AIndex->data(RDR_StreamJid));
      connect(action,SIGNAL(triggered(bool)),SLOT(onShowEditListsDialog(bool)));
      pmenu->addAction(action,AG_DEFAULT+400,false);
    } 
    else if (isAutoPrivacy(streamJid))
    {
      if (AIndex->type()==RIT_Contact || AIndex->type()==RIT_Agent)
      {
        Menu *pmenu = createPrivacyMenu(AMenu);
        Jid contactJid = AIndex->data(RDR_BareJid).toString();
        createAutoPrivacyContactActions(streamJid,contactJid,pmenu);
      }
      else if (AIndex->type()==RIT_Group)
      {
        Menu *pmenu = createPrivacyMenu(AMenu);
        QString group = AIndex->data(RDR_Group).toString();
        createAutoPrivacyGroupActions(streamJid,group,pmenu);
      }
    }
  }
}

void PrivacyLists::onRosterIndexCreated(IRosterIndex *AIndex, IRosterIndex * /*AParent*/)
{
  if (FRostersView && (AIndex->type() == RIT_Contact || AIndex->type() == RIT_Agent))
  {
    Jid streamJid = AIndex->data(RDR_StreamJid).toString();
    if (!activeList(streamJid).isEmpty())
    {
      Jid contactJid = AIndex->data(RDR_BareJid).toString();
      IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
      IRosterItem ritem = roster!=NULL ? roster->rosterItem(contactJid) : IRosterItem();
      ritem.itemJid = contactJid;
      if ((denyedStanzas(ritem,privacyList(streamJid,activeList(streamJid))) & IPrivacyRule::AnyStanza)>0)
      {
        if (ritem.isValid)
          FLabeledContacts[streamJid]+=ritem.itemJid;
        FRostersView->insertIndexLabel(FRosterLabelId,AIndex);
      }
    }
  }
}

void PrivacyLists::onRosterLabelToolTips(IRosterIndex *AIndex, int ALabelId, QMultiMap<int,QString> &AToolTips)
{
  if (ALabelId == FRosterLabelId)
  {
    Jid streamJid = AIndex->data(RDR_StreamJid).toString();
    Jid contactJid = AIndex->data(RDR_BareJid).toString();
    IRoster *roster = FRosterPlugin!=NULL ? FRosterPlugin->getRoster(streamJid) : NULL;
    IRosterItem ritem = roster!=NULL ? roster->rosterItem(contactJid) : IRosterItem();
    ritem.itemJid = contactJid;
    int stanzas = denyedStanzas(ritem,privacyList(streamJid,activeList(streamJid)));
    QString toolTip = tr("<b>Privacy settings:</b>") +"<br>";
    toolTip += tr("- queries: %1").arg((stanzas & IPrivacyRule::Queries) >0             ? tr("<b>denyed</b>") : tr("allowed")) + "<br>";
    toolTip += tr("- messages: %1").arg((stanzas & IPrivacyRule::Messages) >0           ? tr("<b>denyed</b>") : tr("allowed")) + "<br>";
    toolTip += tr("- presences in: %1").arg((stanzas & IPrivacyRule::PresencesIn) >0    ? tr("<b>denyed</b>") : tr("allowed")) + "<br>";
    toolTip += tr("- presences out: %1").arg((stanzas & IPrivacyRule::PresencesOut) >0  ? tr("<b>denyed</b>") : tr("allowed"));
    AToolTips.insertMulti(TTO_PRIVACY,toolTip);
  }
}

void PrivacyLists::onShowEditListsDialog(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    showEditListsDialog(streamJid);
  }
}

void PrivacyLists::onSetActiveListByAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    QString alist = action->data(ADR_LISTNAME).toString();
    if (alist != activeList(streamJid))
      setActiveList(streamJid,alist);
  }
}

void PrivacyLists::onSetDefaultListByAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    QString dlist = action->data(ADR_LISTNAME).toString();
    if (dlist != defaultList(streamJid))
      setDefaultList(streamJid,dlist);
  }
}

void PrivacyLists::onSetAutoPrivacyByAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    QString listName = action->data(ADR_LISTNAME).toString();
    setAutoPrivacy(streamJid,listName);
  }
}

void PrivacyLists::onChangeContactAutoListed(bool AInserted)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    Jid contactJid = action->data(ADR_CONTACT_JID).toString();
    QString listName = action->data(ADR_LISTNAME).toString();
    setAutoListed(streamJid,contactJid,listName,AInserted);
  }
}

void PrivacyLists::onChangeGroupAutoListed(bool AInserted)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    QString groupName = action->data(ADR_GROUP_NAME).toString();
    QString listName = action->data(ADR_LISTNAME).toString();
    setAutoListed(streamJid,groupName,listName,AInserted);
  }
}

void PrivacyLists::onChangeOffRosterBlocked(bool ABlocked)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAM_JID).toString();
    setOffRosterBlocked(streamJid,ABlocked);
  }
}

void PrivacyLists::onEditListsDialogDestroyed(const Jid AStreamJid)
{
  FEditListsDialogs.remove(AStreamJid);
}

void PrivacyLists::onMultiUserChatCreated(IMultiUserChat *AMultiChat)
{
  setAutoListed(AMultiChat->streamJid(),AMultiChat->roomJid(),PRIVACY_LIST_CONFERENCES,true);
}

Q_EXPORT_PLUGIN2(PrivacyListsPlugin, PrivacyLists)