#ifndef IROSTERSVIEW_H
#define IROSTERSVIEW_H

#include <QTreeView>
#include <QAbstractProxyModel>
#include "../../interfaces/irostersmodel.h"
#include "../../utils/menu.h"

#define ROSTERSVIEW_UUID "{BDD12B32-9C88-4e3c-9B36-2DCB5075288F}"

class IRostersClickHooker
{
public:
  virtual bool rosterIndexClicked(IRosterIndex *AIndex, int AHookerId) =0;
};

class IRostersView :
  virtual public QTreeView
{
public:
  enum Option {
    ShowOfflineContacts           =1,
    ShowOnlineFirst               =2,
    ShowFooterText                =4,
    ShowResource                  =8
  };
  enum LabelFlags {
    LabelBlink                    =1
  };
public:
  virtual QObject *instance() = 0;
  virtual void setModel(IRostersModel *AModel) =0; 
  virtual IRostersModel *rostersModel() const =0;
  //--ProxyModels
  virtual void addProxyModel(QAbstractProxyModel *AProxyModel) =0;
  virtual QList<QAbstractProxyModel *> proxyModels() const =0;
  virtual QAbstractProxyModel *lastProxyModel() const =0;
  virtual void removeProxyModel(QAbstractProxyModel *AProxyModel) =0;
  virtual QModelIndex mapToModel(const QModelIndex &AProxyIndex) =0;
  virtual QModelIndex mapFromModel(const QModelIndex &AModelIndex) =0;
  virtual QModelIndex mapToProxy(QAbstractProxyModel *AProxyModel, const QModelIndex &AModelIndex) =0;
  virtual QModelIndex mapFromProxy(QAbstractProxyModel *AProxyModel, const QModelIndex &AProxyIndex) =0;
  //--IndexLabel
  virtual int createIndexLabel(int AOrder, const QVariant &ALabel, int AFlags = 0) =0;
  virtual void updateIndexLabel(int ALabelId, const QVariant &ALabel, int AFlags = 0) =0;
  virtual void insertIndexLabel(int ALabelId, IRosterIndex *AIndex) =0;
  virtual void removeIndexLabel(int ALabelId, IRosterIndex *AIndex) =0;
  virtual void destroyIndexLabel(int ALabelId) =0;
  virtual int labelAt(const QPoint &APoint, const QModelIndex &AIndex) const =0;
  virtual QRect labelRect(int ALabeld, const QModelIndex &AIndex) const =0;
  //--ClickHookers
  virtual int createClickHooker(IRostersClickHooker *AHooker, int APriority, bool AAutoRemove = false) =0;
  virtual void insertClickHooker(int AHookerId, IRosterIndex *AIndex) =0;
  virtual void removeClickHooker(int AHookerId, IRosterIndex *AIndex) =0;
  virtual void destroyClickHooker(int AHookerId) =0;
  //--FooterText
  virtual void insertFooterText(int AOrderAndId, const QString &AText, IRosterIndex *AIndex) =0;
  virtual void removeFooterText(int AOrderAndId, IRosterIndex *AIndex) =0;
signals:
  virtual void modelAboutToBeSeted(IRostersModel *) =0;
  virtual void modelSeted(IRostersModel *) =0;
  virtual void proxyModelAboutToBeAdded(QAbstractProxyModel *) =0;
  virtual void proxyModelAdded(QAbstractProxyModel *) =0;
  virtual void proxyModelAboutToBeRemoved(QAbstractProxyModel *) =0;
  virtual void proxyModelRemoved(QAbstractProxyModel *) =0;
  virtual void contextMenu(IRosterIndex *, Menu *) =0;
  virtual void toolTips(IRosterIndex *, QMultiMap<int,QString> &AToolTips) =0;
  virtual void labelContextMenu(IRosterIndex *, int ALabelId, Menu *) =0;
  virtual void labelToolTips(IRosterIndex *, int ALabelId, QMultiMap<int,QString> &AToolTips) =0;
  virtual void labelClicked(IRosterIndex *, int ALabelId) =0;
  virtual void labelDoubleClicked(IRosterIndex *, int ALabelId, bool &AAccepted) =0;
};


class IRostersViewPlugin
{
public:
  virtual QObject *instance() = 0;
  virtual IRostersView *rostersView() =0;
  virtual bool checkOption(IRostersView::Option AOption) const =0;
  virtual void setOption(IRostersView::Option AOption, bool AValue) =0;
  virtual void restoreExpandState(const QModelIndex &AParent = QModelIndex()) =0;
public slots:
  virtual void setOptionByAction(bool) =0;
signals:
  virtual void viewCreated(IRostersView *) =0;
  virtual void viewDestroyed(IRostersView *) =0;
  virtual void optionChanged(IRostersView::Option AOption, bool AValue) =0;
};

Q_DECLARE_INTERFACE(IRostersClickHooker,"Vacuum.Plugin.IRostersClickHooker/1.0");
Q_DECLARE_INTERFACE(IRostersView,"Vacuum.Plugin.IRostersView/1.0");
Q_DECLARE_INTERFACE(IRostersViewPlugin,"Vacuum.Plugin.IRostersViewPlugin/1.0");

#endif