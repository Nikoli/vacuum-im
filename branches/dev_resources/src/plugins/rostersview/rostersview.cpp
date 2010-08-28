#include "rostersview.h"

#include <QApplication>
#include <QHeaderView>
#include <QToolTip>
#include <QCursor>
#include <QHelpEvent>

RostersView::RostersView(QWidget *AParent) : QTreeView(AParent)
{
  FLabelIdCounter = 1;
  FNotifyId = 1;
  FOptions = 0;

  FRostersModel = NULL;
  FPressedLabel = RLID_DISPLAY;
  FPressedIndex = NULL;

  FContextMenu = new Menu(this);
  FRosterIndexDelegate = new RosterIndexDelegate(this);

  FBlinkShow = true;
  FBlinkTimer.setInterval(500);
  connect(&FBlinkTimer,SIGNAL(timeout()),SLOT(onBlinkTimer()));

  header()->hide();
  header()->setStretchLastSection(false);
  setAutoScroll(false);
  setIndentation(4);
  setRootIsDecorated(false);
  setSelectionMode(NoSelection);
  setContextMenuPolicy(Qt::DefaultContextMenu);
  setItemDelegate(FRosterIndexDelegate);

  connect(this,SIGNAL(labelToolTips(IRosterIndex *, int, QMultiMap<int,QString> &)),
    SLOT(onRosterLabelToolTips(IRosterIndex *, int, QMultiMap<int,QString> &)));
}

RostersView::~RostersView()
{
  removeLabels();
}

void RostersView::setModel(IRostersModel *AModel)
{
  if (FRostersModel != AModel)
  {
    emit modelAboutToBeSeted(AModel);
    
    if (FRostersModel)
    {
      disconnect(FRostersModel,SIGNAL(indexInserted(IRosterIndex *)),this,SLOT(onIndexInserted(IRosterIndex *)));
      disconnect(FRostersModel,SIGNAL(indexDestroyed(IRosterIndex *)),this,SLOT(onIndexDestroyed(IRosterIndex *)));
      removeLabels();
    }

    if (AModel)
    {
      connect(AModel,SIGNAL(indexInserted(IRosterIndex *)),this,SLOT(onIndexInserted(IRosterIndex *)));
      connect(AModel,SIGNAL(indexDestroyed(IRosterIndex *)), SLOT(onIndexDestroyed(IRosterIndex *)));
    }

    FRostersModel = AModel;
    if (!FProxyModels.isEmpty())
      FProxyModels.first()->setSourceModel(AModel);
    setLastModel(AModel);
    emit modelSeted(AModel);
  }
}

bool RostersView::repaintRosterIndex(IRosterIndex *AIndex)
{
  if (FRostersModel)
  {
    QModelIndex modelIndex = mapFromModel(FRostersModel->modelIndexByRosterIndex(AIndex));
    if (modelIndex.isValid())
    {
      QRect rect = visualRect(modelIndex).adjusted(1,1,-1,-1);
      if (!rect.isEmpty())
      {
        viewport()->repaint(rect);
        return true;
      }
    }
  }
  return false;
}

void RostersView::expandIndexParents(IRosterIndex *AIndex)
{
  QModelIndex index = FRostersModel->modelIndexByRosterIndex(AIndex);
  index = mapFromModel(index);
  expandIndexParents(index);
}

void RostersView::expandIndexParents(const QModelIndex &AIndex)
{
  QModelIndex index = AIndex;
  while (index.parent().isValid())
  {
    expand(index.parent());
    index = index.parent();
  }
}

void RostersView::addProxyModel(QAbstractProxyModel *AProxyModel)
{
  if (AProxyModel && !FProxyModels.contains(AProxyModel))
  {
    emit proxyModelAboutToBeAdded(AProxyModel);
    QAbstractProxyModel *lastProxy = lastProxyModel();
    FProxyModels.append(AProxyModel);
    setLastModel(AProxyModel);
    if (lastProxy)
      AProxyModel->setSourceModel(lastProxy);
    else
      AProxyModel->setSourceModel(FRostersModel);
    emit proxyModelAdded(AProxyModel); 
  }
}

void RostersView::removeProxyModel(QAbstractProxyModel *AProxyModel)
{
  int index = FProxyModels.indexOf(AProxyModel);
  if (index != -1)
  {
    emit proxyModelAboutToBeRemoved(AProxyModel);
    QAbstractProxyModel *befour = FProxyModels.value(index-1,NULL);
    QAbstractProxyModel *after = FProxyModels.value(index+1,NULL);
    if (after == NULL && befour == NULL)
      setLastModel(FRostersModel);
    else if (after == NULL)
      setLastModel(befour);
    else if (befour == NULL)
      after->setSourceModel(FRostersModel);
    else
      after->setSourceModel(befour);
    AProxyModel->setSourceModel(NULL);
    FProxyModels.removeAt(index);
    emit proxyModelRemoved(AProxyModel);
  } 
}

QModelIndex RostersView::mapToModel(const QModelIndex &AProxyIndex) const
{
  QModelIndex index = AProxyIndex;
  if (FProxyModels.count() > 0)
  {
    QList<QAbstractProxyModel *>::const_iterator it = FProxyModels.constEnd();
    do 
    {
      it--;
      index = (*it)->mapToSource(index);
    } while(it != FProxyModels.constBegin());
  }
  return index;
}

QModelIndex RostersView::mapFromModel(const QModelIndex &AModelIndex) const
{
  QModelIndex index = AModelIndex;
  if (FProxyModels.count() > 0)
  {
    QList<QAbstractProxyModel *>::const_iterator it = FProxyModels.constBegin();
    while (it != FProxyModels.constEnd())
    {
      index = (*it)->mapFromSource(index);
      it++;
    }
  }
  return index;
}

QModelIndex RostersView::mapToProxy(QAbstractProxyModel *AProxyModel, const QModelIndex &AModelIndex) const
{
  QModelIndex index = AModelIndex;
  if (FProxyModels.count() > 0)
  {
    QList<QAbstractProxyModel *>::const_iterator it = FProxyModels.constBegin();
    while (it!=FProxyModels.constEnd())
    {
      index = (*it)->mapFromSource(index);
      if ((*it) == AProxyModel)
        return index;
      it++;
    }
  }
  return index;
}

QModelIndex RostersView::mapFromProxy(QAbstractProxyModel *AProxyModel, const QModelIndex &AProxyIndex) const
{
  QModelIndex index = AProxyIndex;
  if (FProxyModels.count() > 0)
  {
    bool doMap = false;
    QList<QAbstractProxyModel *>::const_iterator it = FProxyModels.constEnd();
    do 
    {
      it--;
      if ((*it) == AProxyModel)
        doMap = true;
      if (doMap)
        index = (*it)->mapToSource(index);
    } while(it != FProxyModels.constBegin());
  }
  return index;
}

int RostersView::createIndexLabel(int AOrder, const QVariant &ALabel, int AFlags)
{
  int labelId = 0;
  if (ALabel.isValid())
  {
    labelId = FLabelIdCounter++;
    FIndexLabels.insert(labelId,ALabel);
    FIndexLabelOrders.insert(labelId,AOrder);
    FIndexLabelFlags.insert(labelId,AFlags);
    if (AFlags & IRostersView::LabelBlink)
      appendBlinkLabel(labelId);
  }
  return labelId;
}

void RostersView::updateIndexLabel(int ALabelId, const QVariant &ALabel, int AFlags)
{
  if (ALabel.isValid() && FIndexLabels.contains(ALabelId) && FIndexLabels.value(ALabelId)!=ALabel)
  {
    FIndexLabels[ALabelId] = ALabel;
    FIndexLabelFlags[ALabelId] = AFlags;
    foreach (IRosterIndex *index, FIndexLabelIndexes.value(ALabelId))
    {
      QList<QVariant> ids = index->data(RDR_LabelIds).toList();
      QList<QVariant> labels = index->data(RDR_LabelValues).toList();
      QList<QVariant> flags = index->data(RDR_LabelFlags).toList();
      int i = 0;
      while (ids.at(i).toInt()!=ALabelId) i++;
      labels[i] = ALabel;
      flags[i] = AFlags;
      if (AFlags & IRostersView::LabelBlink)
        appendBlinkLabel(ALabelId);
      else
        removeBlinkLabel(ALabelId);
      index->setData(RDR_LabelValues,labels);
      index->setData(RDR_LabelFlags,flags);
    }
  }
}

void RostersView::insertIndexLabel(int ALabelId, IRosterIndex *AIndex)
{
  if (AIndex && FIndexLabels.contains(ALabelId) && !FIndexLabelIndexes.value(ALabelId).contains(AIndex))
  {
    QList<QVariant> ids = AIndex->data(RDR_LabelIds).toList();
    QList<QVariant> labels = AIndex->data(RDR_LabelValues).toList();
    QList<QVariant> orders = AIndex->data(RDR_LabelOrders).toList();
    QList<QVariant> flags = AIndex->data(RDR_LabelFlags).toList();
    int i = 0;
    int order = FIndexLabelOrders.value(ALabelId);
    while (i<orders.count() && orders.at(i).toInt() < order) i++;
    ids.insert(i,ALabelId);
    orders.insert(i,order);
    labels.insert(i,FIndexLabels.value(ALabelId)); 
    flags.insert(i,FIndexLabelFlags.value(ALabelId));
    FIndexLabelIndexes[ALabelId] += AIndex;
    if (FIndexLabelFlags.value(ALabelId) && EnsureVisible > 0)
      expandIndexParents(AIndex);
    AIndex->setData(RDR_LabelIds,ids);
    AIndex->setData(RDR_LabelValues,labels);
    AIndex->setData(RDR_LabelFlags,flags);
    AIndex->setData(RDR_LabelOrders,orders);
  }
}

void RostersView::removeIndexLabel(int ALabelId, IRosterIndex *AIndex)
{
  if (AIndex && FIndexLabels.contains(ALabelId) && FIndexLabelIndexes.value(ALabelId).contains(AIndex))
  {
    QList<QVariant> ids = AIndex->data(RDR_LabelIds).toList();
    QList<QVariant> labels = AIndex->data(RDR_LabelValues).toList();
    QList<QVariant> orders = AIndex->data(RDR_LabelOrders).toList();
    QList<QVariant> flags = AIndex->data(RDR_LabelFlags).toList();
    int i = 0;
    while (i<ids.count() && ids.at(i).toInt()!=ALabelId) i++;
    ids.removeAt(i);
    orders.removeAt(i);
    labels.removeAt(i);
    flags.removeAt(i);
    FIndexLabelIndexes[ALabelId] -= AIndex;
    if (FIndexLabelIndexes[ALabelId].isEmpty())
      FIndexLabelIndexes.remove(ALabelId);
    AIndex->setData(RDR_LabelOrders,orders);
    AIndex->setData(RDR_LabelFlags,flags);
    AIndex->setData(RDR_LabelValues,labels);
    AIndex->setData(RDR_LabelIds,ids);
  }
}

void RostersView::destroyIndexLabel(int ALabelId)
{
  if (FIndexLabels.contains(ALabelId))
  {
    removeBlinkLabel(ALabelId);
    foreach (IRosterIndex *index, FIndexLabelIndexes.value(ALabelId))
      removeIndexLabel(ALabelId,index);
    FIndexLabels.remove(ALabelId);
    FIndexLabelOrders.remove(ALabelId);
    FIndexLabelFlags.remove(ALabelId);
    FIndexLabelIndexes.remove(ALabelId);
  }
}

int RostersView::labelAt(const QPoint &APoint, const QModelIndex &AIndex) const
{
  if (itemDelegate(AIndex) != FRosterIndexDelegate)
    return RLID_DISPLAY;

  return FRosterIndexDelegate->labelAt(APoint,indexOption(AIndex),AIndex);
}

QRect RostersView::labelRect(int ALabeld, const QModelIndex &AIndex) const
{
  if (itemDelegate(AIndex) != FRosterIndexDelegate)
    return QRect();

  return FRosterIndexDelegate->labelRect(ALabeld,indexOption(AIndex),AIndex);
}

int RostersView::appendNotify(IRosterIndexList AIndexes, int AOrder, const QIcon &AIcon, const QString &AToolTip, int AFlags)
{
  if (!AIndexes.isEmpty() && !AIcon.isNull())
  {
    int notifyId = FNotifyId++;
    NotifyItem notifyItem;
    notifyItem.notifyId = notifyId;
    notifyItem.order = AOrder;
    notifyItem.icon = AIcon;
    notifyItem.toolTip = AToolTip;
    notifyItem.flags = AFlags;
    notifyItem.indexes = AIndexes;
    FNotifyItems.insert(notifyId,notifyItem);

    foreach(IRosterIndex *index, AIndexes)
    {
      int labelId;
      QHash<int, int> &indexOrderLabel = FNotifyIndexOrderLabel[index];
      if (!indexOrderLabel.contains(AOrder))
      {
        labelId = createIndexLabel(AOrder,AIcon,AFlags);
        insertIndexLabel(labelId,index);
        indexOrderLabel.insert(AOrder,labelId);
      }
      else
      {
        labelId = indexOrderLabel.value(AOrder);
        updateIndexLabel(labelId,AIcon,AFlags);
        insertIndexLabel(labelId,index);
      }
      FNotifyLabelItems[labelId].prepend(notifyId);
    }
    return notifyId;
  }
  return 0;
}

QList<int> RostersView::indexNotifies(IRosterIndex *AIndex, int AOrder) const
{
  int labelId = FNotifyIndexOrderLabel.value(AIndex).value(AOrder,0);
  return FNotifyLabelItems.value(labelId);
}

void RostersView::updateNotify(int ANotifyId, const QIcon &AIcon, const QString &AToolTip, int AFlags)
{
  if (FNotifyItems.contains(ANotifyId))
  {
    NotifyItem &notifyItem = FNotifyItems[ANotifyId];
    notifyItem.icon = AIcon;
    notifyItem.toolTip = AToolTip;
    notifyItem.flags = AFlags;

    foreach(IRosterIndex *index, notifyItem.indexes)
    {
      int labelId = FNotifyIndexOrderLabel[index].value(notifyItem.order);
      if (FNotifyLabelItems[labelId].first() == ANotifyId)
        updateIndexLabel(labelId,AIcon,AFlags);
    }
  }
}

void RostersView::removeNotify(int ANotifyId)
{
  if (FNotifyItems.contains(ANotifyId))
  {
    NotifyItem &notifyItem = FNotifyItems[ANotifyId];
    foreach(IRosterIndex *index, notifyItem.indexes)
    {
      int labelId = FNotifyIndexOrderLabel[index].value(notifyItem.order);
      QList<int> &labelItems = FNotifyLabelItems[labelId];
      labelItems.removeAt(labelItems.indexOf(ANotifyId));
      if (!labelItems.isEmpty())
      {
        NotifyItem &firstNotifyItem = FNotifyItems[labelItems.first()];
        updateIndexLabel(labelId,firstNotifyItem.icon,firstNotifyItem.flags);
      }
      else
        removeIndexLabel(labelId,index);
    }
    FNotifyItems.remove(ANotifyId);
  }
}

void RostersView::insertClickHooker(int AOrder, IRostersClickHooker *AHooker)
{
  FClickHookers.insertMulti(AOrder,AHooker);
}

void RostersView::removeClickHooker(int AOrder, IRostersClickHooker *AHooker)
{
  FClickHookers.remove(AOrder,AHooker);
}

void RostersView::insertFooterText(int AOrderAndId, const QVariant &AValue, IRosterIndex *AIndex)
{
  if (!AValue.isNull())
  {
    QString footerId = intId2StringId(AOrderAndId);
    QMap<QString,QVariant> footerMap = AIndex->data(RDR_FooterText).toMap();
    footerMap.insert(footerId, AValue);
    AIndex->setData(RDR_FooterText,footerMap);
  } 
  else 
    removeFooterText(AOrderAndId,AIndex);
}

void RostersView::removeFooterText(int AOrderAndId, IRosterIndex *AIndex)
{
  QString footerId = intId2StringId(AOrderAndId);
  QMap<QString,QVariant> footerMap = AIndex->data(RDR_FooterText).toMap();
  if (footerMap.contains(footerId))
  {
    footerMap.remove(footerId);
    if (!footerMap.isEmpty())
      AIndex->setData(RDR_FooterText,footerMap);
    else
      AIndex->setData(RDR_FooterText,QVariant());
  }
}

bool RostersView::checkOption(IRostersView::Option AOption) const
{
  return (FOptions & AOption) > 0;
}

void RostersView::setOption(IRostersView::Option AOption, bool AValue)
{
  AValue ? FOptions |= AOption : FOptions &= ~AOption;
  if (AOption == IRostersView::ShowStatusText)
    updateStatusText();
}

QStyleOptionViewItemV2 RostersView::indexOption(const QModelIndex &AIndex) const
{
  QStyleOptionViewItemV2 option = viewOptions();
  option.initFrom(this);
  option.rect = visualRect(AIndex);
  option.showDecorationSelected |= selectionBehavior() & SelectRows;
  option.state |= isExpanded(AIndex) ? QStyle::State_Open : QStyle::State_None;
  if (hasFocus() && currentIndex() == AIndex)
    option.state |= QStyle::State_HasFocus;
  if (selectedIndexes().contains(AIndex))
    option.state |= QStyle::State_Selected;
  if ((AIndex.flags() & Qt::ItemIsEnabled) == 0)
    option.state &= ~QStyle::State_Enabled;
  if (indexAt(viewport()->mapFromGlobal(QCursor::pos())) == AIndex)
    option.state |= QStyle::State_MouseOver;
  return option;
}

void RostersView::appendBlinkLabel(int ALabelId)
{
  FBlinkLabels+=ALabelId;
  if (!FBlinkTimer.isActive())
    FBlinkTimer.start();
}

void RostersView::removeBlinkLabel(int ALabelId)
{
  FBlinkLabels-=ALabelId;
  if (FBlinkLabels.isEmpty() && FBlinkTimer.isActive())
    FBlinkTimer.stop();
}

QString RostersView::intId2StringId(int AIntId)
{
  return QString("%1").arg(AIntId,10,10,QLatin1Char('0'));
}

void RostersView::removeLabels()
{
  QList<int> labels = FIndexLabels.keys();
  foreach (int label, labels)
  {
    QSet<IRosterIndex *> indexes = FIndexLabelIndexes.value(label);
    foreach(IRosterIndex *index, indexes)
      removeIndexLabel(label,index);
  }
}

void RostersView::setLastModel(QAbstractItemModel *AModel)
{
  emit lastModelAboutToBeChanged(AModel);
  QTreeView::setModel(AModel);
  emit lastModelChanged(AModel);
}

void RostersView::updateStatusText(IRosterIndex *AIndex)
{
  const static QList<int> statusTypes = QList<int>() << RIT_StreamRoot << RIT_Contact << RIT_Agent;
  
  IRosterIndexList indexes;
  if (AIndex == NULL)
  {
    QMultiHash<int,QVariant> findData;
    foreach(int type, statusTypes)
      findData.insert(RDR_Type,type);
    indexes = FRostersModel!=NULL ? FRostersModel->rootIndex()->findChild(findData,true) : IRosterIndexList();
  }
  else if (statusTypes.contains(AIndex->type()))
    indexes.append(AIndex);

  bool show = checkOption(IRostersView::ShowStatusText);
  foreach(IRosterIndex *index, indexes)
  {
    if (show)
      insertFooterText(FTO_STATUS,RDR_Status,index);
    else
      removeFooterText(FTO_STATUS,index);
  }
}

void RostersView::drawBranches(QPainter * /*APainter*/, const QRect &/*ARect*/, const QModelIndex &/*AIndex*/) const
{

}

void RostersView::rowsAboutToBeRemoved(const QModelIndex &AParent, int AStart, int AEnd)
{
  emit indexAboutToBeRemoved(AParent,AStart,AEnd);
  QTreeView::rowsAboutToBeRemoved(AParent,AStart,AEnd);
}

void RostersView::rowsInserted(const QModelIndex &AParent, int AStart, int AEnd)
{
  QTreeView::rowsInserted(AParent,AStart,AEnd);
  emit indexInserted(AParent,AStart,AEnd);
}

bool RostersView::viewportEvent(QEvent *AEvent)
{
  switch(AEvent->type())
  {
  case QEvent::ToolTip:
    {
      QHelpEvent *helpEvent = static_cast<QHelpEvent *>(AEvent); 
      QModelIndex viewIndex = indexAt(helpEvent->pos());
      if (viewIndex.isValid())
      {
        QMultiMap<int,QString> toolTipsMap;
        const int labelId = labelAt(helpEvent->pos(),viewIndex);
        
        QModelIndex modelIndex = mapToModel(viewIndex);
        IRosterIndex *index = static_cast<IRosterIndex *>(modelIndex.internalPointer());

        emit labelToolTips(index,labelId,toolTipsMap);

        if (labelId!=RLID_DISPLAY && toolTipsMap.isEmpty())
          emit labelToolTips(index,RLID_DISPLAY,toolTipsMap);

        QStringList toolTips = toolTipsMap.values();
        QToolTip::showText(helpEvent->globalPos(),toolTips.join("<br>"),this);
        return true;
      }
    }
  default:
    return QTreeView::viewportEvent(AEvent);
  }
}

void RostersView::resizeEvent(QResizeEvent *AEvent)
{
  header()->resizeSection(0,AEvent->size().width());
  QTreeView::resizeEvent(AEvent);
}

void RostersView::contextMenuEvent(QContextMenuEvent *AEvent)
{
  QModelIndex modelIndex = indexAt(AEvent->pos());
  if (modelIndex.isValid())
  {
    const int labelId = labelAt(AEvent->pos(),modelIndex);

    modelIndex = mapToModel(modelIndex);
    IRosterIndex *index = static_cast<IRosterIndex *>(modelIndex.internalPointer());

    FContextMenu->clear();
    if (FNotifyLabelItems.contains(labelId))
      emit notifyContextMenu(index,FNotifyLabelItems.value(labelId).first(),FContextMenu);
    else if (labelId > RLID_DISPLAY)
      emit labelContextMenu(index,labelId,FContextMenu);
    if (FContextMenu->isEmpty())
      emit contextMenu(index,FContextMenu);
    if (!FContextMenu->isEmpty())
      FContextMenu->popup(AEvent->globalPos());
  }
}

void RostersView::mouseDoubleClickEvent(QMouseEvent *AEvent)
{
  bool accepted = false;
  if (viewport()->rect().contains(AEvent->pos()))
  {
    QModelIndex viewIndex = indexAt(AEvent->pos());
    if (viewIndex.isValid())
    {
      QModelIndex modelIndex = mapToModel(viewIndex);
      IRosterIndex *index = static_cast<IRosterIndex *>(modelIndex.internalPointer());
      
      const int labelId = labelAt(AEvent->pos(),viewIndex);

      if (!FNotifyLabelItems.contains(labelId))
      {
        emit labelDoubleClicked(index,labelId,accepted);

        QMultiMap<int,IRostersClickHooker *>::iterator it = FClickHookers.begin();
        while (!accepted && it!=FClickHookers.end())
        {
          accepted = it.value()->rosterIndexClicked(index,it.key());
          it++;
        }
      }
      else
        emit notifyActivated(index,FNotifyLabelItems.value(labelId).first());
    }
  }
  if (!accepted)
    QTreeView::mouseDoubleClickEvent(AEvent);
}

void RostersView::mousePressEvent(QMouseEvent *AEvent)
{
  if (AEvent->button()==Qt::LeftButton && viewport()->rect().contains(AEvent->pos()))
  {
    QModelIndex viewIndex = indexAt(AEvent->pos());
    if (viewIndex.isValid())
    {
      const int labelId = labelAt(AEvent->pos(),viewIndex);
      QModelIndex modelIndex = mapToModel(viewIndex);
      FPressedIndex = static_cast<IRosterIndex *>(modelIndex.internalPointer());
      FPressedLabel = labelId;
      if (labelId == RLID_INDICATORBRANCH)
        setExpanded(viewIndex,!isExpanded(viewIndex));
    }
  }
  QTreeView::mousePressEvent(AEvent);
}

void RostersView::mouseReleaseEvent(QMouseEvent *AEvent)
{
  if (AEvent->button()==Qt::LeftButton && viewport()->rect().contains(AEvent->pos()))
  {
    QModelIndex viewIndex = indexAt(AEvent->pos());
    if (viewIndex.isValid())
    {
      const int labelId = labelAt(AEvent->pos(),viewIndex);
      QModelIndex modelIndex = mapToModel(viewIndex);
      IRosterIndex *index = static_cast<IRosterIndex *>(modelIndex.internalPointer());
      if (FPressedIndex == index && FPressedLabel == labelId)
      {
        if (FNotifyLabelItems.contains(labelId))
          emit notifyActivated(index,FNotifyLabelItems.value(labelId).first());
        else
          emit labelClicked(index,labelId);
      }
    }
  }
  FPressedLabel = RLID_DISPLAY;
  FPressedIndex = NULL;
  QTreeView::mouseReleaseEvent(AEvent);
}

void RostersView::onRosterLabelToolTips(IRosterIndex *AIndex, int ALabelId, QMultiMap<int,QString> &AToolTips)
{
  if (ALabelId == RLID_DISPLAY)
  {
    QString name = AIndex->data(RDR_Name).toString();
    if (!name.isEmpty())
      AToolTips.insert(TTO_CONTACT_NAME,name);

    QString jid = AIndex->data(RDR_Jid).toString();
    if (!jid.isEmpty())
      AToolTips.insert(TTO_CONTACT_JID,jid);

    QString status = AIndex->data(RDR_Status).toString();
    if (!status.isEmpty())
      AToolTips.insert(TTO_CONTACT_STATUS,tr("Status: %1").arg(status));

    QString priority = AIndex->data(RDR_Priority).toString();
    if (!priority.isEmpty())
      AToolTips.insert(TTO_CONTACT_PRIORITY,tr("Priority: %1").arg(priority));

    QString ask = AIndex->data(RDR_Ask).toString();
    QString subscription = AIndex->data(RDR_Subscription).toString();
    if (!subscription.isEmpty())
      AToolTips.insert(TTO_CONTACT_SUBSCRIPTION,tr("Subscription: %1 %2").arg(subscription).arg(ask));
  }
  else if (FNotifyLabelItems.contains(ALabelId))
  {
    NotifyItem &notifyItem = FNotifyItems[FNotifyLabelItems.value(ALabelId).first()];
    if (!notifyItem.toolTip.isEmpty())
      AToolTips.insert(TTO_ROSTERSVIEW_NOTIFY,notifyItem.toolTip);
  }
}

void RostersView::onIndexInserted(IRosterIndex *AIndex)
{
  updateStatusText(AIndex);
}

void RostersView::onIndexDestroyed(IRosterIndex *AIndex)
{
  if (FNotifyIndexOrderLabel.contains(AIndex))
  {
    QList<int> labels = FNotifyIndexOrderLabel[AIndex].values();
    foreach(int labelId, labels)
    {
      QList<int> notifyIds = FNotifyLabelItems.take(labelId);
      foreach(int notifyId, notifyIds)
      {
        NotifyItem &notifyItem = FNotifyItems[notifyId];
        if (notifyItem.indexes.count() == 1)
        {
          emit notifyRemovedByIndex(AIndex,notifyId);
          removeNotify(notifyId);
        }
        else
          notifyItem.indexes.removeAt(notifyItem.indexes.indexOf(AIndex));
      }
      destroyIndexLabel(labelId);
    }
    FNotifyIndexOrderLabel.remove(AIndex);
  }

  QHash<int, QSet<IRosterIndex *> >::iterator it = FIndexLabelIndexes.begin();
  while (it!=FIndexLabelIndexes.end())
  {
    if (it.value().contains(AIndex))
      it.value() -= AIndex;

    if (it.value().isEmpty())
      it = FIndexLabelIndexes.erase(it);
    else
      it++;
  }
}

void RostersView::onBlinkTimer()
{
  FBlinkShow = !FBlinkShow;
  FRosterIndexDelegate->setShowBlinkLabels(FBlinkShow);
  foreach(int labelId,FBlinkLabels)
    foreach(IRosterIndex *index, FIndexLabelIndexes.value(labelId))
      repaintRosterIndex(index);
}
