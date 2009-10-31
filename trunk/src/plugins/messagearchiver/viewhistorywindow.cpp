#include "viewhistorywindow.h"

#include <QListView>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>

#define MINIMUM_DATETIME      QDateTime(QDate(1,1,1),QTime(0,0,0))
#define MAXIMUM_DATETIME      QDateTime::currentDateTime()

#define ADR_GROUP_KIND        Action::DR_Parametr1
#define ADR_SOURCE            Action::DR_Parametr1

#define BIN_WINDOW_STATE      "ArchiveWindowState"
#define BIN_WINDOW_GEOMETRY   "ArchiveWindowGeometry"

//SortFilterProxyModel
SortFilterProxyModel::SortFilterProxyModel(ViewHistoryWindow *AWindow, QObject *AParent) : QSortFilterProxyModel(AParent)
{
  FWindow = AWindow;
  setDynamicSortFilter(true);
  setSortLocaleAware(true);
  setSortCaseSensitivity(Qt::CaseInsensitive);
}

bool SortFilterProxyModel::filterAcceptsRow(int ARow, const QModelIndex &AParent) const
{
  QModelIndex index = sourceModel()->index(ARow,0,AParent);
  int indexType = index.data(HDR_ITEM_TYPE).toInt();
  if (indexType == HIT_HEADER_JID)
  {
    IArchiveHeader header;
    header.with = index.data(HDR_HEADER_WITH).toString();
    header.start = index.data(HDR_HEADER_START).toDateTime();
    header.threadId = index.data(HDR_HEADER_THREAD).toString();
    header.subject = index.data(HDR_HEADER_SUBJECT).toString();
    return FWindow->isHeaderAccepted(header);
  }
  else if (indexType == HIT_GROUP_CONTACT)
  {
    Jid indexWith = index.data(HDR_HEADER_WITH).toString();
    for (int i=0; i<sourceModel()->rowCount(index);i++)
      if (filterAcceptsRow(i,index))
        return true;
    return false;
  }
  else if (indexType == HIT_GROUP_DATE)
  {
    const IArchiveFilter &filter = FWindow->filter();
    QDateTime indexStart = index.data(HDR_DATE_START).toDateTime();
    QDateTime indexEnd = index.data(HDR_DATE_END).toDateTime();
    if ((!filter.start.isValid() || filter.start<indexEnd) && (!filter.end.isValid() || indexStart < filter.end))
      for (int i=0; i<sourceModel()->rowCount(index);i++)
        if (filterAcceptsRow(i,index))
          return true;
    return false;
  }
  return true;
}

//ViewHistoryWindow
ViewHistoryWindow::ViewHistoryWindow(IMessageArchiver *AArchiver, const Jid &AStreamJid, QWidget *AParent) : QMainWindow(AParent)
{
  ui.setupUi(this);
  setAttribute(Qt::WA_DeleteOnClose,true);
  setWindowTitle(tr("View History - %1").arg(AStreamJid.bare()));
  IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(this,MNI_HISTORY_VIEW,0,0,"windowIcon");

  FRoster = NULL;
  FViewWidget = NULL;
  FSettings = NULL;
  FMessageWidgets = NULL;
  FMessageStyles = NULL;
  FCollectionTools = NULL;
  FStatusIcons = NULL;
  FMessagesTools = NULL;

  FArchiver = AArchiver;
  FStreamJid = AStreamJid;
  FGroupKind = GK_CONTACT;
  FSource = AS_AUTO;

  QToolBar *groupsToolBar = this->addToolBar("Groups Tools");
  FCollectionTools = new ToolBarChanger(groupsToolBar);
  ui.wdtCollectionTools->setLayout(new QVBoxLayout);
  ui.wdtCollectionTools->layout()->setMargin(0);
  ui.wdtCollectionTools->layout()->addWidget(FCollectionTools->toolBar());

  QListView *cmbView= new QListView(ui.cmbContact);
  QSortFilterProxyModel *cmbSort = new QSortFilterProxyModel(ui.cmbContact);
  cmbSort->setSortCaseSensitivity(Qt::CaseInsensitive);
  cmbSort->setSortLocaleAware(true);
  cmbSort->setSourceModel(ui.cmbContact->model());
  cmbSort->setSortRole(Qt::DisplayRole);
  cmbView->setModel(cmbSort);
  ui.cmbContact->setView(cmbView);

  ui.lneText->setFocus();

  FModel = new QStandardItemModel(0,3,this);
  FModel->setHorizontalHeaderLabels(QStringList() << tr("With") << tr("Date") << tr("Subject"));
  
  FProxyModel = new SortFilterProxyModel(this);
  FProxyModel->setSourceModel(FModel);
  FProxyModel->setSortRole(HDR_SORT_ROLE);

  ui.trvCollections->setModel(FProxyModel);
  ui.trvCollections->setSortingEnabled(true);
  ui.trvCollections->sortByColumn(1,Qt::DescendingOrder);
  ui.trvCollections->header()->setResizeMode(0,QHeaderView::ResizeToContents);
  ui.trvCollections->header()->setResizeMode(1,QHeaderView::ResizeToContents);
  ui.trvCollections->header()->setResizeMode(2,QHeaderView::Stretch);
  connect(ui.trvCollections->selectionModel(),SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),
    SLOT(onCurrentItemChanged(const QModelIndex &, const QModelIndex &)));
  connect(ui.trvCollections,SIGNAL(customContextMenuRequested(const QPoint &)),SLOT(onItemContextMenuRequested(const QPoint &)));

  FInvalidateTimer.setInterval(0);
  FInvalidateTimer.setSingleShot(true);
  connect(&FInvalidateTimer,SIGNAL(timeout()),SLOT(onInvalidateTimeout()));

  connect(FArchiver->instance(),SIGNAL(archivePrefsChanged(const Jid &, const IArchiveStreamPrefs &)),
    SLOT(onArchivePrefsChanged(const Jid &, const IArchiveStreamPrefs &)));
  connect(FArchiver->instance(),SIGNAL(localCollectionSaved(const Jid &, const IArchiveHeader &)),
    SLOT(onLocalCollectionSaved(const Jid &, const IArchiveHeader &)));
  connect(FArchiver->instance(),SIGNAL(localCollectionRemoved(const Jid &, const IArchiveHeader &)),
    SLOT(onLocalCollectionRemoved(const Jid &, const IArchiveHeader &)));
  connect(FArchiver->instance(),SIGNAL(serverHeadersLoaded(const QString &, const QList<IArchiveHeader> &, const IArchiveResultSet &)),
    SLOT(onServerHeadersLoaded(const QString &, const QList<IArchiveHeader> &, const IArchiveResultSet &)));
  connect(FArchiver->instance(),SIGNAL(serverCollectionLoaded(const QString &, const IArchiveCollection &, const IArchiveResultSet &)),
    SLOT(onServerCollectionLoaded(const QString &, const IArchiveCollection &, const IArchiveResultSet &)));
  connect(FArchiver->instance(),SIGNAL(serverCollectionSaved(const QString &, const IArchiveHeader &)),
    SLOT(onServerCollectionSaved(const QString &, const IArchiveHeader &)));
  connect(FArchiver->instance(),SIGNAL(serverCollectionsRemoved(const QString &, const IArchiveRequest &)),
    SLOT(onServerCollectionsRemoved(const QString &, const IArchiveRequest &)));
  connect(FArchiver->instance(),SIGNAL(requestFailed(const QString &, const QString &)),SLOT(onRequestFailed(const QString &, const QString &)));

  connect(ui.pbtApply,SIGNAL(clicked()),SLOT(onApplyFilterClicked()));
  connect(ui.lneText,SIGNAL(returnPressed()),SLOT(onApplyFilterClicked()));
  connect(ui.cmbContact->lineEdit(),SIGNAL(returnPressed()),SLOT(onApplyFilterClicked()));

  initialize();
  createGroupKindMenu();
  createSourceMenu();
  createHeaderActions();

  FViewOptions.isGroupchat = false;
  setMessageStyle();
}

ViewHistoryWindow::~ViewHistoryWindow()
{
  if (FSettings)
  {
    FSettings->saveBinaryData(BIN_WINDOW_GEOMETRY+FStreamJid.pBare(),saveGeometry());
    FSettings->saveBinaryData(BIN_WINDOW_STATE+FStreamJid.pBare(),saveState());
  }
  clearModel();
  emit windowDestroyed(this);
}

bool ViewHistoryWindow::isHeaderAccepted(const IArchiveHeader &AHeader) const
{
  Jid gateFilter = FArchiver->gateJid(FFilter.with);
  Jid gateWith = FArchiver->gateJid(AHeader.with);
  bool accepted = !gateFilter.isValid() ||  (gateFilter.resource().isEmpty() ? gateFilter && gateWith : gateFilter == gateWith);
  accepted &= (!FFilter.start.isValid() || FFilter.start<=AHeader.start) && (!FFilter.end.isValid() || AHeader.start < FFilter.end);
  accepted &= FFilter.threadId.isEmpty() || FFilter.threadId==AHeader.threadId;
  if (accepted && !FFilter.body.isEmpty() && FFilter.body.indexIn(AHeader.subject) < 0)
  {
    IArchiveCollection collection = FCollections.value(AHeader);
    if (collection.messages.isEmpty() && collection.notes.isEmpty())
      collection = FArchiver->loadLocalCollection(FStreamJid,AHeader);

    accepted = false;
    for (int i=0; !accepted && i<collection.messages.count(); i++)
      accepted = FFilter.body.indexIn(collection.messages.at(i).body())>=0;

    QStringList notes = collection.notes.values();
    for (int i=0; !accepted && i<notes.count(); i++)
      accepted = FFilter.body.indexIn(notes.at(i))>=0;
  }
  return accepted;
}

QList<IArchiveHeader> ViewHistoryWindow::currentHeaders() const
{
  return FCurrentHeaders;
}

QStandardItem *ViewHistoryWindow::findHeaderItem(const IArchiveHeader &AHeader, QStandardItem *AParent) const
{
  int rowCount = AParent ? AParent->rowCount() : FModel->rowCount();
  for (int row=0; row<rowCount; row++)
  {
    QStandardItem *item = AParent ? AParent->child(row,0) : FModel->item(row,0);
    if (item->data(HDR_ITEM_TYPE) == HIT_HEADER_JID)
    {
      Jid with = item->data(HDR_HEADER_WITH).toString();
      QDateTime start = item->data(HDR_HEADER_START).toDateTime();
      if (AHeader.with == with && AHeader.start == start)
        return item;
    }
    item = findHeaderItem(AHeader,item);
    if (item)
      return item;
  }
  return NULL;
}

void ViewHistoryWindow::setGroupKind(int AGroupKind)
{
  foreach (Action *action, FGroupKindMenu->groupActions())
    action->setChecked(action->data(ADR_GROUP_KIND).toInt() == AGroupKind);
  if (FGroupKind != AGroupKind)
  {
    FGroupKind = AGroupKind;
    rebuildModel();
    emit groupKindChanged(AGroupKind);
  }
}

void ViewHistoryWindow::setArchiveSource(int ASource)
{
  foreach (Action *action, FSourceMenu->groupActions())
    action->setChecked(action->data(ADR_SOURCE).toInt() == ASource);

  if (FSource != ASource)
  {
    FSource = ASource;
    reload();
    emit archiveSourceChanged(ASource);
  }
}

void ViewHistoryWindow::setFilter(const IArchiveFilter &AFilter)
{
  FFilter = AFilter;
  processRequests(createRequests(AFilter));
  updateFilterWidgets();
  FInvalidateTimer.start();
  emit filterChanged(AFilter);
}

void ViewHistoryWindow::reload()
{
  clearModel();
  FHeaderRequests.clear();
  FCollectionRequests.clear();
  FRenameRequests.clear();
  FRemoveRequests.clear();
  FRequestList.clear();
  FCollections.clear();
  processRequests(createRequests(FFilter));
  FInvalidateTimer.start();
}

void ViewHistoryWindow::initialize()
{
  IPluginManager *manager = FArchiver->pluginManager();
  
  IPlugin *plugin = manager->pluginInterface("IRosterPlugin").value(0);
  if (plugin)
  {
    FRoster = qobject_cast<IRosterPlugin *>(plugin->instance())->getRoster(FStreamJid);
    if (FRoster)
      connect(FRoster->xmppStream()->instance(),SIGNAL(closed()),SLOT(onStreamClosed()));
  }

  plugin = manager->pluginInterface("IMessageWidgets").value(0);
  if (plugin)
  {
    FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
    if (FMessageWidgets)
    {
      FViewWidget = FMessageWidgets->newViewWidget(FStreamJid,FStreamJid);
      FMessagesTools = FMessageWidgets->newToolBarWidget(NULL,FViewWidget,NULL,NULL);
      ui.wdtMessages->setLayout(new QVBoxLayout);
      ui.wdtMessages->layout()->setMargin(0);
      ui.wdtMessages->layout()->addWidget(FMessagesTools->instance());
      ui.wdtMessages->layout()->addWidget(FViewWidget->instance());
    }
  }

  plugin = manager->pluginInterface("IMessageStyles").value(0,NULL);
  if (plugin)
    FMessageStyles = qobject_cast<IMessageStyles *>(plugin->instance());

  plugin = manager->pluginInterface("ISettingsPlugin").value(0);
  if (plugin)
  {
    FSettings = qobject_cast<ISettingsPlugin *>(plugin->instance())->settingsForPlugin(MESSAGEARCHIVER_UUID);
    if (FSettings)
    {
      restoreGeometry(FSettings->loadBinaryData(BIN_WINDOW_GEOMETRY+FStreamJid.pBare()));
      restoreState(FSettings->loadBinaryData(BIN_WINDOW_STATE+FStreamJid.pBare()));
    }
  }

  plugin = manager->pluginInterface("IStatusIcons").value(0);
  if (plugin)
    FStatusIcons = qobject_cast<IStatusIcons *>(plugin->instance());
}

QList<IArchiveHeader> ViewHistoryWindow::indexHeaders(const QModelIndex &AIndex) const
{
  QList<IArchiveHeader> headers;
  for (int row=0; row<ui.trvCollections->model()->rowCount(AIndex); row++)
  {
    QModelIndex index = ui.trvCollections->model()->index(row,0,AIndex);
    headers += indexHeaders(index);
  }

  if (AIndex.data(HDR_ITEM_TYPE).toInt() == HIT_HEADER_JID)
  {
    IArchiveHeader header;
    header.with = AIndex.data(HDR_HEADER_WITH).toString();
    header.start = AIndex.data(HDR_HEADER_START).toDateTime();
    headers.append(header);
  }

  return headers;
}

QList<IArchiveRequest> ViewHistoryWindow::createRequests(const IArchiveFilter &AFilter) const
{
  QList<IArchiveRequest> requests;
  
  IArchiveRequest request;
  request.with = AFilter.with;
  request.start = AFilter.start.isValid() ? AFilter.start : MINIMUM_DATETIME;
  request.end = AFilter.end.isValid() ? AFilter.end : MAXIMUM_DATETIME;
  requests.append(request);

  foreach(IArchiveRequest oldReq, FRequestList)
  {
    if (!oldReq.with.isValid() || oldReq.with==AFilter.with)
    {
      for(int i=0; i<requests.count(); i++)
      {
        IArchiveRequest &newReq = requests[i];
        if (newReq.start < oldReq.start && oldReq.end < newReq.end)
        {
          IArchiveRequest newReq2 = newReq;
          newReq2.start = oldReq.end;
          requests.append(newReq2);
          newReq.end = oldReq.start;
        }
        else if (oldReq.start <= newReq.start && newReq.end <= oldReq.end)
        {
          requests.removeAt(i--);
        }
        else if (oldReq.start <= newReq.start && oldReq.end < newReq.end && newReq.start < oldReq.end)
        {
          newReq.start = oldReq.end;
        }
        else if (newReq.start < oldReq.start && oldReq.start < newReq.end && newReq.end <= oldReq.end)
        {
          newReq.end = oldReq.start;
        }
      }
    }
  }
  return requests;
}

void ViewHistoryWindow::divideRequests(const QList<IArchiveRequest> &ARequests, QList<IArchiveRequest> &ALocal, 
                                       QList<IArchiveRequest> &AServer) const
{
  QDateTime replPoint = FArchiver->replicationPoint(FStreamJid);
  if (FSource == AS_LOCAL_ARCHIVE || !FArchiver->isSupported(FStreamJid))
  {
    ALocal = ARequests;
  }
  else if (FSource == AS_SERVER_ARCHIVE)
  {
    AServer = ARequests;
  }
  else if (!replPoint.isValid())
  {
    ALocal = ARequests;
    AServer = ARequests;
  }
  else foreach (IArchiveRequest request, ARequests)
  {
    if (!replPoint.isValid() || request.end <= replPoint)
    {
      ALocal.append(request);
    }
    else if (replPoint <= request.start)
    {
      AServer.append(request);
    }
    else
    {
      IArchiveRequest serverRequest = request;
      IArchiveRequest localRequest = request;
      serverRequest.start = replPoint;
      localRequest.end = replPoint;
      AServer.append(serverRequest);
      ALocal.append(localRequest);
    }
  }
}

bool ViewHistoryWindow::loadServerHeaders(const IArchiveRequest &ARequest, const QString &AAfter)
{
  QString requestId = FArchiver->loadServerHeaders(FStreamJid,ARequest,AAfter);
  if (!requestId.isEmpty())
  {
    FHeaderRequests.insert(requestId,ARequest);
    return true;
  }
  return false;
}

bool ViewHistoryWindow::loadServerCollection(const IArchiveHeader &AHeader, const QString &AAfter)
{
  QString requestId = FArchiver->loadServerCollection(FStreamJid,AHeader,AAfter);
  if (!requestId.isEmpty())
  {
    FCollectionRequests.insert(requestId,AHeader);
    return true;
  }
  return false;
}

QString ViewHistoryWindow::contactName(const Jid &AContactJid, bool ABare) const
{
  QString name = FArchiver->gateNick(FStreamJid,AContactJid);
  if (name==AContactJid.node() || name==AContactJid.domain())
    return ABare ? AContactJid.bare() : AContactJid.full();
  else
    return !ABare && !AContactJid.resource().isEmpty() ? name+"/"+AContactJid.resource() : name;
}

QStandardItem *ViewHistoryWindow::findChildItem(int ARole, const QVariant &AValue, QStandardItem *AParent) const
{
  int rowCount = AParent!=NULL ? AParent->rowCount() : FModel->rowCount();
  for (int i=0; i<rowCount; i++)
  {
    QStandardItem *childItem = AParent!=NULL ? AParent->child(i,0) : FModel->item(i,0);
    if (childItem->data(ARole)==AValue)
      return childItem;
  }
  return NULL;
}

QStandardItem *ViewHistoryWindow::createSortItem(const QVariant &ASortData)
{
  QStandardItem *item = new QStandardItem;
  item->setData(HIT_SORT_ITEM, HDR_ITEM_TYPE);
  item->setData(ASortData,HDR_SORT_ROLE);
  return item;
}

QStandardItem *ViewHistoryWindow::createCustomItem(int AType, const QVariant &ADiaplay)
{
  QStandardItem *item = new QStandardItem;
  item->setData(AType, HDR_ITEM_TYPE);
  item->setData(FStreamJid.full(),HDR_STREAM_JID);
  item->setData(ADiaplay, HDR_SORT_ROLE);
  item->setData(ADiaplay, Qt::DisplayRole);
  return item;
}

QStandardItem *ViewHistoryWindow::createDateGroup(const IArchiveHeader &AHeader, QStandardItem *AParent)
{
  IconStorage *storage = IconStorage::staticStorage(RSR_STORAGE_MENUICONS);

  QDateTime year(QDate(AHeader.start.date().year(),1,1),QTime(0,0,0));
  QStandardItem *yearItem = findChildItem(HDR_DATE_START,year,AParent);
  if (!yearItem)
  {
    yearItem = createCustomItem(HIT_GROUP_DATE,year.date().year());
    yearItem->setData(year,HDR_DATE_START);
    yearItem->setData(year.addYears(1).addSecs(-1),HDR_DATE_END);
    yearItem->setData(year,HDR_SORT_ROLE);
    yearItem->setIcon(storage->getIcon(MNI_HISTORY_DATE));
    QList<QStandardItem *> items = QList<QStandardItem *>() << yearItem << createSortItem(year) << createSortItem(year);
    AParent!=NULL ? AParent->appendRow(items) : FModel->appendRow(items);
    emit itemCreated(yearItem);
  }

  QDateTime month(QDate(year.date().year(),AHeader.start.date().month(),1),QTime(0,0,0));
  QStandardItem *monthItem = findChildItem(HDR_DATE_START,month,yearItem);
  if (!monthItem)
  {
    monthItem = createCustomItem(HIT_GROUP_DATE,QDate::longMonthName(month.date().month()));
    monthItem->setData(month,HDR_DATE_START);
    monthItem->setData(month.addMonths(1).addSecs(-1),HDR_DATE_END);
    monthItem->setData(month,HDR_SORT_ROLE);
    monthItem->setIcon(storage->getIcon(MNI_HISTORY_DATE));
    yearItem->appendRow(QList<QStandardItem *>() << monthItem << createSortItem(month) << createSortItem(month));
    emit itemCreated(monthItem);
  }

  QDateTime day(QDate(year.date().year(),month.date().month(),AHeader.start.date().day()),QTime(0,0,0));
  QStandardItem *dayItem = findChildItem(HDR_DATE_START,day,monthItem);
  if (!dayItem)
  {
    dayItem = createCustomItem(HIT_GROUP_DATE,day.date());
    dayItem->setData(day,HDR_DATE_START);
    dayItem->setData(day.addDays(1).addSecs(-1),HDR_DATE_END);
    dayItem->setData(day,HDR_SORT_ROLE);
    dayItem->setIcon(storage->getIcon(MNI_HISTORY_DATE));
    monthItem->appendRow(QList<QStandardItem *>() << dayItem << createSortItem(day) << createSortItem(day));
    emit itemCreated(dayItem);
  }

  return dayItem;
}

QStandardItem *ViewHistoryWindow::createContactGroup(const IArchiveHeader &AHeader, QStandardItem *AParent)
{
  Jid gateWith = FArchiver->gateJid(AHeader.with);
  QStandardItem *groupItem = findChildItem(HDR_HEADER_WITH,gateWith.prepared().eBare(),AParent);
  if (!groupItem)
  {
    QString name = contactName(AHeader.with,true);
    groupItem = createCustomItem(HIT_GROUP_CONTACT,name);
    groupItem->setData(gateWith.prepared().eBare(),HDR_HEADER_WITH);
    groupItem->setToolTip(AHeader.with.bare());
    if (FStatusIcons)
      groupItem->setIcon(FStatusIcons->iconByJidStatus(AHeader.with,IPresence::Online,SUBSCRIPTION_BOTH,false));
    QList<QStandardItem *> items = QList<QStandardItem *>() << groupItem << createSortItem(AHeader.start) << createSortItem(name);
    AParent!=NULL ? AParent->appendRow(items) : FModel->appendRow(items);
    emit itemCreated(groupItem);
  }
  else
  {
    QStandardItem *sortDateItem = AParent ? AParent->child(groupItem->row(),1) : FModel->item(groupItem->row(),1);
    if (sortDateItem && sortDateItem->data(HDR_SORT_ROLE).toDateTime()<AHeader.start)
      sortDateItem->setData(AHeader.start,HDR_SORT_ROLE);
  }
  return groupItem;
}

QStandardItem *ViewHistoryWindow::createHeaderParent(const IArchiveHeader &AHeader, QStandardItem *AParent)
{
  if (AParent == NULL)
  {
    switch(FGroupKind)
    {
    case GK_DATE :
    case GK_DATE_CONTACT :
      return createHeaderParent(AHeader,createDateGroup(AHeader,AParent));
    case GK_CONTACT :
    case GK_CONTACT_DATE : 
      return createHeaderParent(AHeader,createContactGroup(AHeader,AParent));
    }
  }
  else if (AParent->data(HDR_ITEM_TYPE).toInt() == HIT_GROUP_CONTACT)
  {
    switch(FGroupKind)
    {
    case GK_CONTACT_DATE : 
      return createHeaderParent(AHeader,createDateGroup(AHeader,AParent));
    }
  }
  else if (AParent->data(HDR_ITEM_TYPE).toInt() == HIT_GROUP_DATE)
  {
    switch(FGroupKind)
    {
    case GK_DATE_CONTACT : 
      return createHeaderParent(AHeader,createContactGroup(AHeader,AParent));
    }
  }
  return AParent;
}

QStandardItem *ViewHistoryWindow::createHeaderItem(const IArchiveHeader &AHeader)
{
  QStandardItem *parentItem = createHeaderParent(AHeader,NULL);
  
  QString name = contactName(AHeader.with);
  QStandardItem *itemJid = createCustomItem(HIT_HEADER_JID,name);
  itemJid->setData(AHeader.with.prepared().eFull(), HDR_HEADER_WITH);
  itemJid->setData(AHeader.start,                   HDR_HEADER_START);
  itemJid->setData(AHeader.subject,                 HDR_HEADER_SUBJECT);
  itemJid->setData(AHeader.threadId,                HDR_HEADER_THREAD);
  itemJid->setData(AHeader.version,                 HDR_HEADER_VERSION);
  itemJid->setToolTip(AHeader.with.full());
  if (FStatusIcons)
    itemJid->setIcon(FStatusIcons->iconByJidStatus(AHeader.with,IPresence::Online,SUBSCRIPTION_BOTH,false));
  QStandardItem *itemDate = createCustomItem(HIT_HEADER_DATE,AHeader.start);
  QStandardItem *itemSubject = createCustomItem(HIT_HEADER_SUBJECT,AHeader.subject);
  itemSubject->setToolTip(AHeader.subject);

  if (parentItem)
    parentItem->appendRow(QList<QStandardItem *>() << itemJid << itemDate << itemSubject);
  else
    FModel->appendRow(QList<QStandardItem *>() << itemJid << itemDate << itemSubject);
  emit itemCreated(itemJid);

  FInvalidateTimer.start();
  return itemJid;
}

void ViewHistoryWindow::updateHeaderItem(const IArchiveHeader &AHeader)
{
  QStandardItem *headerItem = findHeaderItem(AHeader);
  if (headerItem)
  {
    headerItem->setData(AHeader.subject,HDR_HEADER_SUBJECT);
    headerItem->setData(AHeader.threadId,HDR_HEADER_THREAD);
    headerItem->setData(AHeader.version,HDR_HEADER_VERSION);
    if (headerItem->parent())
      headerItem->parent()->child(headerItem->row(),2)->setText(AHeader.subject);
    else
      FModel->item(headerItem->row(),2)->setText(AHeader.subject);
    IArchiveCollection collection = FCollections.take(AHeader);
    collection.header = AHeader;
    FCollections.insert(AHeader,collection);
  }
}

void ViewHistoryWindow::removeCustomItem(QStandardItem *AItem)
{
  if (AItem)
  {
    while (AItem->rowCount()>0)
      removeCustomItem(AItem->child(0,0));
    emit itemDestroyed(AItem);
    AItem->parent()!=NULL ? AItem->parent()->removeRow(AItem->row()) : qDeleteAll(FModel->takeRow(AItem->row()));
    FInvalidateTimer.start();
  }
}

void ViewHistoryWindow::setViewOptions(const IArchiveCollection &ACollection)
{
  FViewOptions.isGroupchat = false;
  for (int i=0; !FViewOptions.isGroupchat && i<ACollection.messages.count();i++)
    FViewOptions.isGroupchat = ACollection.messages.at(i).type()==Message::GroupChat;

  if (FMessageStyles && !FViewOptions.isGroupchat)
  {
    FViewOptions.selfName = Qt::escape(FMessageStyles->userName(FStreamJid));
    FViewOptions.selfAvatar = FMessageStyles->userAvatar(FStreamJid);
    if (!ACollection.header.with.resource().isEmpty() && ACollection.header.with.pDomain().startsWith("conference."))
      FViewOptions.contactName = Qt::escape(ACollection.header.with.resource());
    else
      FViewOptions.contactName = Qt::escape(FArchiver->gateNick(FStreamJid,ACollection.header.with));
    FViewOptions.contactAvatar = FMessageStyles->userAvatar(ACollection.header.with);
  }
}

void ViewHistoryWindow::setMessageStyle()
{
  if (FMessageStyles)
  {
    IMessageStyleOptions soptions = FMessageStyles->styleOptions(FViewOptions.isGroupchat ? Message::GroupChat : Message::Chat);
    IMessageStyle *style = FMessageStyles->styleForOptions(soptions);
    if (style != FViewWidget->messageStyle())
      FViewWidget->setMessageStyle(style,soptions);
    else if (style)
      style->changeOptions(FViewWidget->styleWidget(),soptions);
    ui.lblCollectionInfo->setText(tr("No conversation selected"));
  }
}

void ViewHistoryWindow::showNotification(const QString &AMessage)
{
  IMessageContentOptions options;
  options.kind = IMessageContentOptions::Status;
  options.direction = IMessageContentOptions::DirectionIn;
  options.time = QDateTime::currentDateTime();
  options.timeFormat = FMessageStyles!=NULL ? FMessageStyles->timeFormat(options.time) : QString::null;
  FViewWidget->appendText(AMessage,options);
}

void ViewHistoryWindow::processRequests(const QList<IArchiveRequest> &ARequests)
{
  QList<IArchiveRequest> localRequests;
  QList<IArchiveRequest> serverRequests;
  divideRequests(ARequests,localRequests,serverRequests);

  foreach(IArchiveRequest request, localRequests)
  {
    FRequestList.append(request);
    processHeaders(FArchiver->loadLocalHeaders(FStreamJid,request));
  }

  foreach(IArchiveRequest request, serverRequests)
  {
    loadServerHeaders(request);
  }
}

void ViewHistoryWindow::processHeaders(const QList<IArchiveHeader> &AHeaders)
{
  foreach(IArchiveHeader header, AHeaders)
  {
    if (!FCollections.contains(header))
    {
      IArchiveCollection collection;
      collection.header = header;
      FCollections.insert(header,collection);
      createHeaderItem(header);
      insertFilterWith(header);
    }
  }
}

void ViewHistoryWindow::processCollection(const IArchiveCollection &ACollection, bool AAppend)
{
  if (FViewWidget && FCurrentHeaders.contains(ACollection.header))
  {
    if (!AAppend)
    {
      setViewOptions(ACollection);
      setMessageStyle();
      FViewWidget->setContactJid(ACollection.header.with);
      QString infoString = tr("Conversation with <b>%1</b> started at %2").arg(Qt::escape(contactName(ACollection.header.with))).arg(ACollection.header.start.toString());
      if (!ACollection.header.subject.isEmpty())
        infoString += "<br><i>"+tr("Subject: %1").arg(Qt::escape(ACollection.header.subject))+"</i>";
      ui.lblCollectionInfo->setText(infoString);
    }

    IMessageContentOptions options;
    options.kind = IMessageContentOptions::Status;
    options.direction = IMessageContentOptions::DirectionIn;
    options.noScroll = true;

    options.senderId = FStreamJid.full();
    options.senderName = FViewOptions.selfName;
    QMultiMap<QDateTime,QString>::const_iterator it = ACollection.notes.constBegin();
    while (it!=ACollection.notes.constEnd())
    {
      options.time = it.key();
      options.timeFormat = FMessageStyles!=NULL ? FMessageStyles->timeFormat(options.time) : QString::null;
      FViewWidget->appendText(it.value(),options);
      it++;
    }

    options.kind = IMessageContentOptions::Message;
    foreach(Message message, ACollection.messages)
    {
      options.type = 0;
      options.time = message.dateTime();
      options.timeFormat = FMessageStyles!=NULL ? FMessageStyles->timeFormat(options.time) : QString::null;
      Jid senderJid = !message.from().isEmpty() ? message.from() : FStreamJid;

      if (FViewOptions.isGroupchat)
      {
        options.direction = IMessageContentOptions::DirectionIn;
        options.type |= IMessageContentOptions::Groupchat;
        options.senderName = Qt::escape(senderJid.resource());
        options.senderId = options.senderName;
      }
      else if (ACollection.header.with == senderJid)
      {
        options.direction = IMessageContentOptions::DirectionIn;
        options.senderId = senderJid.full();
        options.senderName = FViewOptions.contactName;
        options.senderAvatar = FViewOptions.contactAvatar;
        options.senderColor = "blue";
      }
      else
      {
        options.direction = IMessageContentOptions::DirectionOut;
        options.senderId = senderJid.full();
        options.senderName = FViewOptions.selfName;
        options.senderAvatar = FViewOptions.selfAvatar;
        options.senderColor = "red";
      }

      FViewWidget->appendMessage(message,options);
    }
  }
}

void ViewHistoryWindow::insertFilterWith(const IArchiveHeader &AHeader)
{
  Jid gateWith = FArchiver->gateJid(AHeader.with);
  QString name = contactName(AHeader.with,true);
  if (!name.isEmpty() && !gateWith.isEmpty())
  {
    int index = ui.cmbContact->findData(gateWith.pBare());
    if (index < 0)
    {
      QIcon icon;
      if (FStatusIcons)
        icon = FStatusIcons->iconByJidStatus(AHeader.with,IPresence::Online,SUBSCRIPTION_BOTH,false);
      ui.cmbContact->addItem(icon, name, gateWith.pBare());
      updateFilterWidgets();
    }
    else
      ui.cmbContact->setItemText(index,name);
    ui.cmbContact->model()->sort(0);
  }
}

void ViewHistoryWindow::updateFilterWidgets()
{
  int index = ui.cmbContact->findData(FFilter.with.pFull());
  if (index<0)
  {
    ui.cmbContact->setCurrentIndex(-1);
    ui.cmbContact->setEditText(FFilter.with.full());
  }
  else
    ui.cmbContact->setCurrentIndex(index);
  ui.dedStart->setDate(FFilter.start.isValid() ? FFilter.start.date() : MINIMUM_DATETIME.date());
  ui.dedEnd->setDate(FFilter.end.isValid() ? FFilter.end.date() : MAXIMUM_DATETIME.date());
  ui.lneText->setText(FFilter.body.pattern());
}

void ViewHistoryWindow::clearModel()
{
  while (FModel->rowCount()>0)
    removeCustomItem(FModel->item(0,0));
}

void ViewHistoryWindow::rebuildModel()
{
  clearModel();
  foreach(IArchiveHeader header, FCollections.keys()) {
    createHeaderItem(header); }
}

void ViewHistoryWindow::createGroupKindMenu()
{
  FGroupKindMenu = new Menu(this);
  FGroupKindMenu->setTitle(tr("Groups"));
  FGroupKindMenu->setToolTip(tr("Grouping type"));
  FGroupKindMenu->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY_GROUPS);

  Action *action = new Action(FGroupKindMenu);
  action->setCheckable(true);
  action->setText(tr("No groups"));
  action->setData(ADR_GROUP_KIND,GK_NO_GROUPS);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupKindByAction(bool)));
  FGroupKindMenu->addAction(action);

  action = new Action(FGroupKindMenu);
  action->setCheckable(true);
  action->setText(tr("Date"));
  action->setData(ADR_GROUP_KIND,GK_DATE);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupKindByAction(bool)));
  FGroupKindMenu->addAction(action);

  action = new Action(FGroupKindMenu);
  action->setCheckable(true);
  action->setChecked(true);
  action->setText(tr("Contact"));
  action->setData(ADR_GROUP_KIND,GK_CONTACT);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupKindByAction(bool)));
  FGroupKindMenu->addAction(action);

  action = new Action(FGroupKindMenu);
  action->setCheckable(true);
  action->setText(tr("Date and Contact"));
  action->setData(ADR_GROUP_KIND,GK_DATE_CONTACT);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupKindByAction(bool)));
  FGroupKindMenu->addAction(action);
  
  action = new Action(FGroupKindMenu);
  action->setCheckable(true);
  action->setText(tr("Contact and Date"));
  action->setData(ADR_GROUP_KIND,GK_CONTACT_DATE);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeGroupKindByAction(bool)));
  FGroupKindMenu->addAction(action);

  action = new Action(FGroupKindMenu);
  action->setText(tr("Expand All"));
  connect(action,SIGNAL(triggered()),ui.trvCollections,SLOT(expandAll()));
  FGroupKindMenu->addAction(action,AG_DEFAULT+100);

  action = new Action(FGroupKindMenu);
  action->setText(tr("Collapse All"));
  connect(action,SIGNAL(triggered()),ui.trvCollections,SLOT(collapseAll()));
  FGroupKindMenu->addAction(action,AG_DEFAULT+100);

  QToolButton *button = FCollectionTools->addToolButton(FGroupKindMenu->menuAction(),TBG_MAVHG_ARCHIVE_GROUPING,false);
  button->setPopupMode(QToolButton::InstantPopup);
}

void ViewHistoryWindow::createSourceMenu()
{
  FSourceMenu = new Menu(this);
  FSourceMenu->setTitle(tr("Source"));
  FSourceMenu->setToolTip(tr("History source"));
  FSourceMenu->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY_SOURCE);

  Action *action = new Action(FSourceMenu);
  action->setCheckable(true);
  action->setChecked(true);
  action->setText(tr("Auto select"));
  action->setData(ADR_SOURCE,AS_AUTO);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeSourceByAction(bool)));
  FSourceMenu->addAction(action,AG_DEFAULT-1);

  action = new Action(FSourceMenu);
  action->setCheckable(true);
  action->setText(tr("Local archive"));
  action->setData(ADR_SOURCE,AS_LOCAL_ARCHIVE);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeSourceByAction(bool)));
  FSourceMenu->addAction(action);

  action = new Action(FSourceMenu);
  action->setCheckable(true);
  action->setText(tr("Server archive"));
  action->setData(ADR_SOURCE,AS_SERVER_ARCHIVE);
  connect(action,SIGNAL(triggered(bool)),SLOT(onChangeSourceByAction(bool)));
  FSourceMenu->addAction(action);

  QToolButton *button = FCollectionTools->addToolButton(FSourceMenu->menuAction(),TBG_MAVHG_ARCHIVE_GROUPING,false);
  button->setPopupMode(QToolButton::InstantPopup);

  FSourceMenu->setEnabled(FArchiver->isSupported(FStreamJid));
}

void ViewHistoryWindow::createHeaderActions()
{
  FFilterBy = new Action(FCollectionTools->toolBar());
  FFilterBy->setText(tr("Filter"));
  FFilterBy->setIcon(RSR_STORAGE_MENUICONS,MNI_HISRORY_FILTER);
  FFilterBy->setEnabled(false);
  connect(FFilterBy,SIGNAL(triggered(bool)),SLOT(onHeaderActionTriggered(bool)));
  FCollectionTools->addAction(FFilterBy,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);

  FRename = new Action(FCollectionTools->toolBar());
  FRename->setText(tr("Rename"));
  FRename->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY_RENAME);
  FRename->setEnabled(false);
  connect(FRename,SIGNAL(triggered(bool)),SLOT(onHeaderActionTriggered(bool)));
  FCollectionTools->addAction(FRename,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);

  FRemove = new Action(FCollectionTools->toolBar());
  FRemove->setText(tr("Remove"));
  FRemove->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY_REMOVE);
  connect(FRemove,SIGNAL(triggered(bool)),SLOT(onHeaderActionTriggered(bool)));
  FCollectionTools->addAction(FRemove,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);

  FReload = new Action(FCollectionTools->toolBar());
  FReload->setText(tr("Reload"));
  FReload->setIcon(RSR_STORAGE_MENUICONS,MNI_HISTORY_RELOAD);
  connect(FReload,SIGNAL(triggered(bool)),SLOT(onHeaderActionTriggered(bool)));
  FCollectionTools->addAction(FReload,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);
}

void ViewHistoryWindow::onLocalCollectionSaved(const Jid &AStreamJid, const IArchiveHeader &AHeader)
{
  if (AStreamJid == FStreamJid)
  {
    if (FCollections.contains(AHeader))
      updateHeaderItem(AHeader);
    else
      processHeaders(QList<IArchiveHeader>() << AHeader);
  }
}

void ViewHistoryWindow::onLocalCollectionRemoved(const Jid &AStreamJid, const IArchiveHeader &AHeader)
{
  if (AStreamJid == FStreamJid)
  {
    FCollections.remove(AHeader);
    removeCustomItem(findHeaderItem(AHeader));
  }
}

void ViewHistoryWindow::onServerHeadersLoaded(const QString &AId, const QList<IArchiveHeader> &AHeaders, const IArchiveResultSet &AResult)
{
  if (FHeaderRequests.contains(AId))
  {
    IArchiveRequest request = FHeaderRequests.take(AId);
    if (AResult.index == 0)
      FRequestList.append(request);
    if (!AResult.last.isEmpty() && AResult.index+AHeaders.count()<AResult.count)
      loadServerHeaders(request,AResult.last);
    processHeaders(AHeaders);
  }
}

void ViewHistoryWindow::onServerCollectionLoaded(const QString &AId, const IArchiveCollection &ACollection, const IArchiveResultSet &AResult)
{
  if (FCollectionRequests.contains(AId))
  {
    IArchiveCollection &collection = FCollections[ACollection.header];
    collection.messages += ACollection.messages;
    collection.notes += ACollection.notes;

    if (FCurrentHeaders.contains(ACollection.header))
      processCollection(ACollection,AResult.index>0);

    if (!AResult.last.isEmpty() && AResult.index+ACollection.messages.count()+ACollection.notes.count()<AResult.count)
      loadServerCollection(ACollection.header,AResult.last);

    FCollectionRequests.remove(AId);
  }
}

void ViewHistoryWindow::onServerCollectionSaved(const QString &AId, const IArchiveHeader &AHeader)
{
  if (FRenameRequests.contains(AId))
  {
    updateHeaderItem(AHeader);
    FRenameRequests.remove(AId);
  }
}

void ViewHistoryWindow::onServerCollectionsRemoved(const QString &AId, const IArchiveRequest &/*ARequest*/)
{
  if (FRemoveRequests.contains(AId))
  {
    IArchiveHeader header = FRemoveRequests.take(AId);
    QStandardItem *item = findHeaderItem(header);
    if (item)
      removeCustomItem(item);
    FCollections.remove(header);
  }
}

void ViewHistoryWindow::onRequestFailed(const QString &AId, const QString &AError)
{
  if (FHeaderRequests.contains(AId))
  {
    FHeaderRequests.remove(AId);
  }
  else if (FCollectionRequests.contains(AId))
  {
    IArchiveHeader header = FCollectionRequests.take(AId);

    IArchiveCollection &collection = FCollections[header];
    collection.messages.clear();
    collection.notes.clear();

    if (FCurrentHeaders.contains(header))
      showNotification(tr("Message loading failed: %1").arg(AError));
  }
  else if (FRenameRequests.contains(AId))
  {
    FRenameRequests.remove(AId);
  }
  else if (FRemoveRequests.contains(AId))
  {
    FRemoveRequests.remove(AId);
  }
}

void ViewHistoryWindow::onCurrentItemChanged(const QModelIndex &ACurrent, const QModelIndex &ABefour)
{
  if (ACurrent.parent()!=ABefour.parent() || ACurrent.row()!=ABefour.row())
  {
    QModelIndex index = ACurrent.column()==0 ? ACurrent : ui.trvCollections->model()->index(ACurrent.row(),0,ACurrent.parent());
    FCurrentHeaders = indexHeaders(index);

    if (FViewWidget)
    {
      if (index.data(HDR_ITEM_TYPE).toInt() == HIT_HEADER_JID)
      {
        const IArchiveHeader &header = FCurrentHeaders.at(0);
        IArchiveCollection collection = FCollections.value(header);
        if (collection.messages.isEmpty() && collection.notes.isEmpty())
        {
          if (FArchiver->hasLocalCollection(FStreamJid,header))
          {
            collection = FArchiver->loadLocalCollection(FStreamJid,header);
            FCollections.insert(collection.header,collection);
            processCollection(collection);
          }
          else if (FCollectionRequests.key(header).isEmpty())
          {
            setMessageStyle();
            if (loadServerCollection(header))
              showNotification(tr("Loading messages from server..."));
            else
              showNotification(tr("Messages request failed."));
          }
        }
        else
        {
          processCollection(collection);
        }
        FFilterBy->setEnabled(true);
        FRename->setEnabled(true);
      }
      else
      {
        setMessageStyle();
        FFilterBy->setEnabled(false);
        FRename->setEnabled(false);
      }
    }
    QStandardItem *current = FModel->itemFromIndex(FProxyModel->mapToSource(ACurrent));
    QStandardItem *befour = FModel->itemFromIndex(FProxyModel->mapToSource(ABefour));
    emit currentItemChanged(current,befour);
  }
}

void ViewHistoryWindow::onItemContextMenuRequested(const QPoint &APos)
{
  QStandardItem *item =  FModel->itemFromIndex(FProxyModel->mapToSource(ui.trvCollections->indexAt(APos)));
  if (item)
  {
    Menu *menu = new Menu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose,true);

    if (FFilterBy->isEnabled())
      menu->addAction(FFilterBy,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);
    if (FRename->isEnabled())
      menu->addAction(FRename,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);
    if (FRemove->isEnabled())
      menu->addAction(FRemove,TBG_MAVHG_ARCHIVE_DEFACTIONS,false);

    emit itemContextMenu(item,menu);
    menu->isEmpty() ? delete menu : menu->popup(ui.trvCollections->viewport()->mapToGlobal(APos));
  }
}

void ViewHistoryWindow::onApplyFilterClicked()
{
  if (ui.dedStart->date() <= ui.dedEnd->date())
  {
    IArchiveFilter filter = FFilter;
    int index = ui.cmbContact->currentIndex();
    if (index>=0 && ui.cmbContact->itemText(index)==ui.cmbContact->currentText())
      filter.with = ui.cmbContact->itemData(ui.cmbContact->currentIndex()).toString();
    else
      filter.with = ui.cmbContact->currentText();
    filter.start = ui.dedStart->dateTime();
    filter.end = ui.dedEnd->dateTime();
    filter.body.setPattern(ui.lneText->text());
    filter.body.setCaseSensitivity(Qt::CaseInsensitive);
    setFilter(filter);
  }
  else
    ui.dedEnd->setDate(ui.dedStart->date());
}

void ViewHistoryWindow::onInvalidateTimeout()
{
  FProxyModel->invalidate();
  QModelIndex index = ui.trvCollections->selectionModel()->currentIndex();
  onCurrentItemChanged(index,index);
}

void ViewHistoryWindow::onChangeGroupKindByAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    int groupKind = action->data(ADR_GROUP_KIND).toInt();
    setGroupKind(groupKind);
  }
}

void ViewHistoryWindow::onChangeSourceByAction(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action)
  {
    int source = action->data(ADR_SOURCE).toInt();
    setArchiveSource(source);
  }
}

void ViewHistoryWindow::onHeaderActionTriggered(bool)
{
  Action *action = qobject_cast<Action *>(sender());
  if (action == FFilterBy)
  {
    IArchiveFilter filter = FFilter;
    filter.with = FCurrentHeaders.at(0).with;
    setFilter(filter);
  }
  else if (action == FRename)
  {
    bool ok;
    QString subject = QInputDialog::getText(this,tr("Enter new collection subject"),tr("Subject:"),QLineEdit::Normal,FCurrentHeaders.at(0).subject,&ok);
    if (ok)
    {
      if (FArchiver->isSupported(FStreamJid))
      {
        IArchiveCollection collection;
        collection.header = FCurrentHeaders.at(0);
        collection.header.subject = subject;
        QString requestId = FArchiver->saveServerCollection(FStreamJid,collection);
        if (!requestId.isEmpty())
          FRenameRequests.insert(requestId,collection.header);
      }
      if (FArchiver->hasLocalCollection(FStreamJid,FCurrentHeaders.at(0)))
      {
        IArchiveCollection collection = FArchiver->loadLocalCollection(FStreamJid,FCurrentHeaders.at(0));
        collection.header.subject = subject;
        collection.header.version++;
        FArchiver->saveLocalCollection(FStreamJid,collection,false);
      }
    }
  }
  else if (action == FRemove)
  {
    if (!FCurrentHeaders.isEmpty())
    {
      QString title = FCurrentHeaders.count() > 1 ? tr("Remove collection") : tr("Remove collections");
      QString message = FCurrentHeaders.count() > 1 ? 
        tr("Do you really want to remove %1 collections with messages?").arg(FCurrentHeaders.count()) 
        : tr("Do you really want to remove this collection with messages?");
      if (QMessageBox::question(this,title, message, QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
      {
        QList<IArchiveHeader> headers = FCurrentHeaders;
        foreach(IArchiveHeader header, headers)
        {
          if (FArchiver->isSupported(FStreamJid))
          {
            IArchiveRequest request;
            request.with = header.with;
            request.start = header.start;
            if (request.with.isValid() && request.start.isValid())
            {
              QString requestId = FArchiver->removeServerCollections(FStreamJid,request);
              if (!requestId.isEmpty())
                FRemoveRequests.insert(requestId,header);
            }
          }
          FArchiver->removeLocalCollection(FStreamJid,header);
        }
      }
    }
  }
  else if (action == FReload)
  {
    reload();
  }
}

void ViewHistoryWindow::onArchivePrefsChanged(const Jid &AStreamJid, const IArchiveStreamPrefs &/*APrefs*/)
{
  if (AStreamJid == FStreamJid)
    FSourceMenu->setEnabled(FArchiver->isSupported(FStreamJid));
}

void ViewHistoryWindow::onStreamClosed()
{
  IXmppStream *xmppStream = qobject_cast<IXmppStream *>(sender());
  if (xmppStream && xmppStream->streamJid()==FStreamJid)
    FSourceMenu->setEnabled(false);
}
