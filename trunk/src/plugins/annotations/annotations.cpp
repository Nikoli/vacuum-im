#include "annotations.h"

#define PST_ANNOTATIONS       "storage"
#define PSN_ANNOTATIONS       "storage:rosternotes"

#define ADR_STREAMJID         Action::DR_StreamJid
#define ADR_CONTACTJID        Action::DR_Parametr1

Annotations::Annotations()
{
  FPrivateStorage = NULL;
  FRosterPlugin = NULL;
  FRostersViewPlugin = NULL;
}

Annotations::~Annotations()
{

}

void Annotations::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Plugin for making annotations about roster items and other entities");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Annotations"); 
  APluginInfo->uid = ANNOTATIONS_UUID;
  APluginInfo->version = "0.1";
  APluginInfo->dependences.append(PRIVATESTORAGE_UUID);
}

bool Annotations::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->getPlugins("IPrivateStorage").value(0,NULL);
  if (plugin)
  {
    FPrivateStorage = qobject_cast<IPrivateStorage *>(plugin->instance());
    if (FPrivateStorage)
    {
      connect(FPrivateStorage->instance(),SIGNAL(storageOpened(const Jid &)),SLOT(onPrivateStorageOpened(const Jid &)));
      connect(FPrivateStorage->instance(),SIGNAL(dataSaved(const QString &, const Jid &, const QDomElement &)),
        SLOT(onPrivateDataSaved(const QString &, const Jid &, const QDomElement &)));
      connect(FPrivateStorage->instance(),SIGNAL(dataLoaded(const QString &, const Jid &, const QDomElement &)),
        SLOT(onPrivateDataLoaded(const QString &, const Jid &, const QDomElement &)));
      connect(FPrivateStorage->instance(),SIGNAL(dataError(const QString &, const QString &)),
        SLOT(onPrivateDataError(const QString &, const QString &)));
      connect(FPrivateStorage->instance(),SIGNAL(storageClosed(const Jid &)),SLOT(onPrivateStorageClosed(const Jid &)));
    }
  }

  plugin = APluginManager->getPlugins("IRosterPlugin").value(0,NULL);
  if (plugin)
  {
    FRosterPlugin = qobject_cast<IRosterPlugin *>(plugin->instance());
    if (FRosterPlugin)
    {
      connect(FRosterPlugin->instance(),SIGNAL(rosterItemRemoved(IRoster *, const IRosterItem &)),
        SLOT(onRosterItemRemoved(IRoster *, const IRosterItem &)));
    }
  }

  plugin = APluginManager->getPlugins("IRostersViewPlugin").value(0,NULL);
  if (plugin)
    FRostersViewPlugin = qobject_cast<IRostersViewPlugin *>(plugin->instance());

  return FPrivateStorage!=NULL;
}

bool Annotations::initObjects()
{
  if (FRostersViewPlugin)
  {
    IRostersView *rostersView = FRostersViewPlugin->rostersView();
    connect(rostersView,SIGNAL(contextMenu(IRosterIndex *, Menu *)),SLOT(onRostersViewContextMenu(IRosterIndex *, Menu *)));
    connect(rostersView,SIGNAL(labelToolTips(IRosterIndex *, int , QMultiMap<int,QString> &)),
      SLOT(onRosterLabelToolTips(IRosterIndex *, int , QMultiMap<int,QString> &)));
  }
  return true;
}

bool Annotations::isEnabled(const Jid &AStreamJid) const
{
  return FAnnotations.contains(AStreamJid);
}

QList<Jid> Annotations::annotations(const Jid &AStreamJid) const
{
  return FAnnotations.value(AStreamJid).keys();
}

QString Annotations::annotation(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FAnnotations.value(AStreamJid).value(AContactJid.bare()).note;
}

QDateTime Annotations::annotationCreateDate(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FAnnotations.value(AStreamJid).value(AContactJid.bare()).created.toLocal();
}

QDateTime Annotations::annotationModifyDate(const Jid &AStreamJid, const Jid &AContactJid) const
{
  return FAnnotations.value(AStreamJid).value(AContactJid.bare()).modified.toLocal();
}

void Annotations::setAnnotation(const Jid &AStreamJid, const Jid &AContactJid, const QString &ANote)
{
  if (isEnabled(AStreamJid))
  {
    if (!ANote.isEmpty())
    {
      Annotation &item = FAnnotations[AStreamJid][AContactJid.bare()];
      item.modified = DateTime(QDateTime::currentDateTime());
      if (!item.created.isValid())
        item.created = item.modified;
      item.note = ANote;
    }
    else
    {
      FAnnotations[AStreamJid].remove(AContactJid.bare());
    }
    emit annotationModified(AStreamJid,AContactJid);
  }
}

bool Annotations::loadAnnotations(const Jid &AStreamJid)
{
  if (FPrivateStorage && !FLoadRequests.contains(AStreamJid))
  {
    QString id = FPrivateStorage->loadData(AStreamJid,PST_ANNOTATIONS,PSN_ANNOTATIONS);
    if (!id.isEmpty())
    {
      FLoadRequests.insert(AStreamJid,id);
      return true;
    }
  }
  return false;
}

bool Annotations::saveAnnotations(const Jid &AStreamJid)
{
  if (isEnabled(AStreamJid) && !FSaveRequests.contains(AStreamJid))
  {
    QDomDocument doc;
    QDomElement storage = doc.appendChild(doc.createElementNS(PSN_ANNOTATIONS,PST_ANNOTATIONS)).toElement();

    const QHash<Jid, Annotation> &items = FAnnotations.value(AStreamJid);
    QHash<Jid, Annotation>::const_iterator it = items.constBegin();
    while (it != items.constEnd())
    {
      QDomElement elem = storage.appendChild(doc.createElement("note")).toElement();
      elem.setAttribute("jid",it.key().eBare());
      elem.setAttribute("cdate",it.value().created.toX85UTC());
      elem.setAttribute("mdate",it.value().modified.toX85UTC());
      elem.appendChild(doc.createTextNode(it.value().note));
      it++;
    }

    QString id = FPrivateStorage->saveData(AStreamJid,doc.documentElement());
    if (!id.isEmpty())
    {
      FSaveRequests.insert(AStreamJid,id);
      return true;
    }
  }
  return false;
}

void Annotations::onPrivateStorageOpened(const Jid &AStreamJid)
{
  loadAnnotations(AStreamJid);
}

void Annotations::onPrivateDataSaved(const QString &AId, const Jid &AStreamJid, const QDomElement &/*AElement*/)
{
  if (FSaveRequests.value(AStreamJid) == AId)
  {
    FSaveRequests.remove(AStreamJid);
    emit annotationsSaved(AStreamJid);
  }
}

void Annotations::onPrivateDataLoaded(const QString &AId, const Jid &AStreamJid, const QDomElement &AElement)
{
  if (FLoadRequests.value(AStreamJid) == AId)
  {
    FLoadRequests.remove(AStreamJid);

    QHash<Jid, Annotation> &items = FAnnotations[AStreamJid];
    items.clear();

    QDomElement elem = AElement.firstChildElement("note");
    while (!elem.isNull())
    {
      Jid itemJid = elem.attribute("jid");
      if (itemJid.isValid() && !elem.text().isEmpty())
      {
        Annotation item;
        item.created.setDateTime(elem.attribute("cdate"));
        item.modified.setDateTime(elem.attribute("mdate"));
        item.note = elem.text();
        items.insert(itemJid.bare(),item);
      }
      elem= elem.nextSiblingElement("note");
    }
    emit annotationsLoaded(AStreamJid);
  }
}

void Annotations::onPrivateDataError(const QString &AId, const QString &AError)
{
  if (FLoadRequests.values().contains(AId))
  {
    Jid streamJid = FLoadRequests.key(AId);
    FLoadRequests.remove(streamJid);
    emit annotationsError(streamJid, AError);
  }
  else if (FSaveRequests.values().contains(AId))
  {
    Jid streamJid = FSaveRequests.key(AId);
    FSaveRequests.remove(streamJid);
    emit annotationsError(streamJid, AError);
  }
}

void Annotations::onPrivateStorageClosed(const Jid &AStreamJid)
{
  qDeleteAll(FEditDialogs.take(AStreamJid));
  FLoadRequests.remove(AStreamJid);
  FSaveRequests.remove(AStreamJid);
  FAnnotations.remove(AStreamJid);
}

void Annotations::onRosterItemRemoved(IRoster *ARoster, const IRosterItem &ARosterItem)
{
  if (isEnabled(ARoster->streamJid()))
  {
    if (!annotation(ARoster->streamJid(),ARosterItem.itemJid).isEmpty())
    {
      setAnnotation(ARoster->streamJid(),ARosterItem.itemJid,"");
      saveAnnotations(ARoster->streamJid());
    }
  }
}

void Annotations::onRostersViewContextMenu(IRosterIndex *AIndex, Menu *AMenu)
{
  Jid streamJid = AIndex->data(RDR_StreamJid).toString();
  Jid contactJid = AIndex->data(RDR_BareJid).toString();
  if (isEnabled(streamJid) && contactJid.isValid())
  {
    Action *action = new Action(AMenu);
    action->setText(tr("Annotation"));
    action->setIcon(RSR_STORAGE_MENUICONS,MNI_ANNOTATIONS);
    action->setData(ADR_STREAMJID,streamJid.full());
    action->setData(ADR_CONTACTJID,contactJid.bare());
    connect(action,SIGNAL(triggered(bool)),SLOT(onEditNoteActionTriggered(bool)));
    AMenu->addAction(action,AG_ANNOTATIONS,true);
  }
}

void Annotations::onRosterLabelToolTips(IRosterIndex *AIndex, int ALabelId, QMultiMap<int,QString> &AToolTips)
{
  if (ALabelId == RLID_DISPLAY)
  {
    QString note = annotation(AIndex->data(RDR_StreamJid).toString(),AIndex->data(RDR_BareJid).toString());
    if (!note.isEmpty())
    {
      QString toolTip = "<hr>"+Qt::escape(note).trimmed().replace("\n","<br>");
      AToolTips.insert(TTO_ANNOTATIONS,toolTip);
    }
  }
}

void Annotations::onEditNoteActionTriggered(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    Jid streamJid = action->data(ADR_STREAMJID).toString();
    Jid contactJid = action->data(ADR_CONTACTJID).toString();
    EditNoteDialog *dialog = FEditDialogs.value(streamJid).value(contactJid);
    if (!dialog)
    {
      dialog = new EditNoteDialog(this,streamJid,contactJid);
      FEditDialogs[streamJid].insert(contactJid,dialog);
      connect(dialog,SIGNAL(dialogDestroyed()),SLOT(onEditNoteDialogDestroyed()));
    }
    dialog->show();
  }
}

void Annotations::onEditNoteDialogDestroyed()
{
  EditNoteDialog *dialog = qobject_cast<EditNoteDialog *>(sender());
  if (dialog)
    FEditDialogs[dialog->streamJid()].remove(dialog->contactJid());
}

Q_EXPORT_PLUGIN2(AnnotationsPlugin, Annotations)
