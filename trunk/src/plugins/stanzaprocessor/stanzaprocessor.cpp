#include "stanzaprocessor.h"

#include <QSet>
#include <QTime>

StanzaProcessor::StanzaProcessor() 
{
  qsrand(QTime::currentTime().msec());
}

StanzaProcessor::~StanzaProcessor()
{

}

void StanzaProcessor::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Managing XMPP stanzas");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Stanza Processor"); 
  APluginInfo->uid = STANZAPROCESSOR_UUID;
  APluginInfo->version = "0.1";
  APluginInfo->dependences.append(XMPPSTREAMS_UUID);
}

bool StanzaProcessor::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->getPlugins("IXmppStreams").value(0,NULL);
  if (plugin)
  {
    FXmppStreams = qobject_cast<IXmppStreams *>(plugin->instance());
    if (FXmppStreams)
    {
      connect(FXmppStreams->instance(), SIGNAL(element(IXmppStream *, const QDomElement &)),
        SLOT(onStreamElement(IXmppStream *, const QDomElement &))); 
      connect(FXmppStreams->instance(), SIGNAL(jidChanged(IXmppStream *, const Jid &)),
        SLOT(onStreamJidChanged(IXmppStream *, const Jid &)));
      connect(FXmppStreams->instance(), SIGNAL(closed(IXmppStream *)),
        SLOT(onStreamClosed(IXmppStream *)));
    }
  }
  return FXmppStreams!=NULL;
}


//IStanzaProcessor
QString StanzaProcessor::newId() const
{
  return QString::number((qrand()<<16)+qrand(),36);
}

bool StanzaProcessor::sendStanzaIn(const Jid &AStreamJid, const Stanza &AStanza)
{
  Stanza stanza(AStanza);
  emit stanzaReceived(AStreamJid, stanza);
  bool acceptedIn = processStanzaIn(AStreamJid,stanza);
  bool acceptedIq = processStanzaRequest(AStreamJid,stanza);
  return acceptedIn || acceptedIq;
}

bool StanzaProcessor::sendStanzaOut(const Jid &AStreamJid, const Stanza &AStanza)
{
  Stanza stanza(AStanza);
  if (processStanzaOut(AStreamJid,stanza))
  {
    IXmppStream *stream = FXmppStreams->getStream(AStreamJid);
    if (stream && stream->sendStanza(stanza)>0)
    {
      emit stanzaSent(AStreamJid, stanza);
      return true;
    }
  }
  return false;
}

bool StanzaProcessor::sendStanzaRequest(IStanzaRequestOwner *AIqOwner, const Jid &AStreamJid, const Stanza &AStanza, int ATimeout)
{
  if (AIqOwner && AStanza.tagName()=="iq" && !AStanza.id().isEmpty() && !FRequests.contains(AStanza.id()))
  {
    if ((AStanza.type() == "set" || AStanza.type() == "get") && sendStanzaOut(AStreamJid,AStanza))
    {
      StanzaRequest request;
      request.streamJid = AStreamJid;
      request.owner = AIqOwner;
      if (ATimeout > 0)
      {
        request.timer = new QTimer();
        request.timer->setSingleShot(true);  
        connect(request.timer,SIGNAL(timeout()),SLOT(onStanzaRequestTimeout()));
        request.timer->start(ATimeout); 
      }
      FRequests.insert(AStanza.id(),request);
      connect(AIqOwner->instance(),SIGNAL(destroyed(QObject *)),SLOT(onStanzaRequestOwnerDestroyed(QObject *)));
      return true;
    }
  }
  return false;
}

QList<int> StanzaProcessor::stanzaHandles() const
{
  return FHandles.keys();
}

IStanzaHandle StanzaProcessor::stanzaHandle(int AHandleId) const
{
  return FHandles.value(AHandleId);
}

int StanzaProcessor::insertStanzaHandle(const IStanzaHandle &AHandle)
{
  if (AHandle.handler!=NULL && !AHandle.conditions.isEmpty())
  {
    int handleId = (qrand()<<16) + qrand();
    for (; handleId==0 || FHandles.contains(handleId); handleId++);
    FHandles.insert(handleId,AHandle);
    FHandleIdByPriority.insertMulti(AHandle.priority,handleId);
    connect(AHandle.handler->instance(),SIGNAL(destroyed(QObject *)),SLOT(onStanzaHandlerDestroyed(QObject *)));
    emit stanzaHandleInserted(handleId,AHandle);
    return handleId;
  }
  return -1;
}

void StanzaProcessor::removeStanzaHandle(int AHandleId)
{
  if (FHandles.contains(AHandleId))
  {
    IStanzaHandle shandle = FHandles.take(AHandleId);
    FHandleIdByPriority.remove(shandle.priority,AHandleId);
    emit stanzaHandleRemoved(AHandleId, shandle);
  }
}

bool StanzaProcessor::checkStanza(const Stanza &AStanza, const QString &ACondition) const
{
  return checkCondition(AStanza.element(),ACondition);
}

bool StanzaProcessor::checkCondition(const QDomElement &AElem, const QString &ACondition, const int APos) const
{
  static QSet<QChar> delimiters = QSet<QChar>()<<' '<<'/'<<'\\'<<'\t'<<'\n'<<'['<<']'<<'='<<'\''<<'"'<<'@';
  
  QDomElement elem = AElem;
  
  int pos = APos;
  if (pos<ACondition.count() && ACondition[pos] == '/') 
    pos++;

  QString tagName;
  while (pos<ACondition.count() && !delimiters.contains(ACondition[pos]))
    tagName.append(ACondition[pos++]);

  if (!tagName.isEmpty() &&  elem.tagName() != tagName)
    elem = elem.nextSiblingElement(tagName);

  if (elem.isNull())  
    return false; 

  QMultiHash<QString,QString> attributes;
  while (pos<ACondition.count() && ACondition[pos] != '/')
  {
    if (ACondition[pos] == '[')
    {
      pos++;
      QString attrName = "";
      QString attrValue = "";
      while (pos<ACondition.count() && ACondition[pos] != ']')
      {
        if (ACondition[pos] == '@')
        {
          pos++;
          while (pos<ACondition.count() && !delimiters.contains(ACondition[pos]))
            attrName.append(ACondition[pos++]);
        } 
        else if (ACondition[pos] == '"' || ACondition[pos] == '\'')
        {
          QChar end = ACondition[pos++];
          while (pos<ACondition.count() && ACondition[pos] != end)
            attrValue.append(ACondition[pos++]);
          pos++;
        }
        else pos++;
      }
      if (!attrName.isEmpty())
        attributes.insertMulti(attrName,attrValue); 
      pos++;
    } 
    else pos++;
  }

  if (pos < ACondition.count() && !elem.hasChildNodes())
    return false;

  while (!elem.isNull())
  {
    int attr = 0;
    QList<QString> attrNames = attributes.keys();
    while (attr<attrNames.count() && !elem.isNull())
    {
      QString attrName = attrNames.at(attr);
      QList<QString> attrValues = attributes.values(attrName);
      bool attrBlankValue = attrValues.contains("");
      bool elemHasAttr;
      QString elemAttrValue;
      if (elem.hasAttribute(attrName))
      {
        elemHasAttr = true;
        elemAttrValue = elem.attribute(attrName);
      } 
      else if (attrName == "xmlns")
      {
        elemHasAttr = true;
        elemAttrValue = elem.namespaceURI();
      }
      else
        elemHasAttr = false;

      if (!elemHasAttr || (!attrValues.contains(elemAttrValue) && !attrBlankValue))
      {
        elem = elem.nextSiblingElement(tagName);
        attr = 0;
      }
      else attr++;
    }

    if (!elem.isNull() && pos < ACondition.count())
    {
      if (checkCondition(elem.firstChildElement(),ACondition,pos))
        return true;
      else
        elem = elem.nextSiblingElement(tagName); 
    }
    else if (!elem.isNull())
      return true;
  }
  
  return false;
}

bool StanzaProcessor::processStanzaIn(const Jid &AStreamJid, Stanza &AStanza) const
{
  bool hooked = false;
  bool accepted = false;
  QList<int> checkedHandles;

  QMapIterator<int,int> it(FHandleIdByPriority);
  it.toBack();
  while(!hooked && it.hasPrevious())
  {
    it.previous();
    const IStanzaHandle &shandle = FHandles.value(it.value());
    if (shandle.direction == IStanzaHandle::DirectionIn && (shandle.streamJid.isEmpty() || shandle.streamJid==AStreamJid))
    {
      for (int i = 0; i<shandle.conditions.count(); i++)
      {
        if (checkCondition(AStanza.element(), shandle.conditions.at(i)))
        {
          hooked = shandle.handler->stanzaEdit(it.value(),AStreamJid,AStanza,accepted);
          checkedHandles.append(it.value()); 
          break;
        }
      }
    }
  }

  for (int i = 0; !hooked && i<checkedHandles.count(); i++)
  {
    int shandleId = checkedHandles.at(i);
    const IStanzaHandle &shandle = FHandles.value(shandleId);
    hooked = shandle.handler->stanzaRead(shandleId,AStreamJid,AStanza,accepted);
  } 

  return accepted;
}

bool StanzaProcessor::processStanzaOut(const Jid &AStreamJid, Stanza &AStanza) const
{
  bool hooked = false;
  bool accepted = false;
  QList<int> checkedHandlers;

  QMapIterator<int,int> it(FHandleIdByPriority);
  while(!hooked && it.hasNext())
  {
    it.next();
    const IStanzaHandle &shandle = FHandles.value(it.value());
    if (shandle.direction == IStanzaHandle::DirectionOut && (shandle.streamJid.isEmpty() || shandle.streamJid==AStreamJid))
    {
      for (int i = 0; i<shandle.conditions.count(); i++)
      {
        if (checkCondition(AStanza.element(), shandle.conditions.at(i)))
        {
          hooked = shandle.handler->stanzaEdit(it.value(),AStreamJid,AStanza,accepted);
          checkedHandlers.append(it.value()); 
          break;
        }
      }
    }
  }

  for (int i = 0; !hooked && i<checkedHandlers.count(); i++)
  {
    int shandleId = checkedHandlers.at(i);
    const IStanzaHandle &shandle = FHandles.value(shandleId);
    hooked = shandle.handler->stanzaRead(shandleId,AStreamJid,AStanza,accepted);
  } 

  return !hooked;
}

bool StanzaProcessor::processStanzaRequest(const Jid &AStreamJid, const Stanza &AStanza) 
{
  if (AStanza.tagName()=="iq" && FRequests.contains(AStanza.id()) && (AStanza.type()=="result" || AStanza.type()=="error"))
  {
    const StanzaRequest &request = FRequests.value(AStanza.id());
    request.owner->stanzaRequestResult(AStreamJid,AStanza);  
    removeStanzaRequest(AStanza.id());
    return true;
  }
  return false;
}

void StanzaProcessor::removeStanzaRequest(const QString &AStanzaId)
{
  StanzaRequest request = FRequests.take(AStanzaId);
  delete request.timer;
}

void StanzaProcessor::onStreamElement(IXmppStream *AXmppStream, const QDomElement &AElem)
{
  Stanza stanza(AElem);

  if (stanza.from().isEmpty())
    stanza.setFrom(AXmppStream->jid().eFull());
  stanza.setTo(AXmppStream->jid().eFull());

  if (!sendStanzaIn(AXmppStream->jid(),stanza))
    if (stanza.canReplyError())
      sendStanzaOut(AXmppStream->jid(), stanza.replyError("service-unavailable")); 
}

void StanzaProcessor::onStreamJidChanged(IXmppStream *AXmppStream, const Jid &ABefour)
{
  foreach(int shandleId, FHandles.keys())
    if (FHandles.value(shandleId).streamJid == ABefour)
      FHandles[shandleId].streamJid = AXmppStream->jid();
}

void StanzaProcessor::onStreamClosed(IXmppStream *AStream)
{
  foreach(QString stanzaId, FRequests.keys())
  {
    const StanzaRequest &request = FRequests.value(stanzaId);
    if (request.streamJid == AStream->jid())
    {
      request.owner->stanzaRequestTimeout(request.streamJid, stanzaId);
      removeStanzaRequest(stanzaId);
    }
  }
}

void StanzaProcessor::onStanzaRequestTimeout()
{
  QTimer *timer = qobject_cast<QTimer *>(sender());
  if (timer != NULL)
  {
    foreach(QString stanzaId, FRequests.keys())
    {
      const StanzaRequest &request = FRequests.value(stanzaId);
      if (request.timer == timer)
      {
        request.owner->stanzaRequestTimeout(request.streamJid, stanzaId);
        removeStanzaRequest(stanzaId);
        break;
      }
    }
  }
}

void StanzaProcessor::onStanzaRequestOwnerDestroyed(QObject *AOwner)
{
  foreach(QString stanzaId, FRequests.keys())
    if ((QObject *)FRequests.value(stanzaId).owner == AOwner)
      removeStanzaRequest(stanzaId);
}

void StanzaProcessor::onStanzaHandlerDestroyed(QObject *AHandler)
{
  foreach (int shandleId, FHandles.keys())
    if ((QObject *)FHandles.value(shandleId).handler == AHandler)
      removeStanzaHandle(shandleId);
}

Q_EXPORT_PLUGIN2(StanzaProcessorPlugin, StanzaProcessor)
