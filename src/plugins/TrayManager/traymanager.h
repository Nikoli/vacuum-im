#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QMap>
#include <QSystemTrayIcon>
#include "../../interfaces/ipluginmanager.h"
#include "../../interfaces/itraymanager.h"

class TrayManager : 
  public QObject,
  public IPlugin,
  public ITrayManager
{
  Q_OBJECT;
  Q_INTERFACES(IPlugin ITrayManager);

public:
  TrayManager();
  ~TrayManager();

  virtual QObject *instance() { return this; }

  //IPlugin
  virtual QUuid pluginUuid() const { return TRAYMANAGER_UUID; }
  virtual void pluginInfo(PluginInfo *APluginInfo);
  virtual bool initConnections(IPluginManager * /*APluginManager*/, int &/*AInitOrder*/) { return true; }
  virtual bool initObjects() { return true; }
  virtual bool initSettings() { return true; }
  virtual bool startPlugin();

  //ITrayManager
  virtual QIcon icon() const { return FTrayIcon.icon(); }
  virtual QString toolTip() const { return FTrayIcon.toolTip(); }
  virtual QIcon baseIcon() const { return FBaseIcon; }
  virtual void setBaseIcon(const QIcon &AIcon);
  virtual int appendNotify(const QIcon &AIcon, const QString &AToolTip);
  virtual void updateNotify(int ANotifyId, const QIcon &AIcon, const QString &AToolTip);
  virtual void removeNotify(int ANotifyId);
signals:
  virtual void contextMenu(int ANotifyId, Menu *AMenu);
  virtual void toolTips(int ANotifyId, QMultiMap<int,QString> &AToolTips);
  virtual void notifyAdded(int ANotifyId);
  virtual void notifyActivated(int ANotifyId);
  virtual void notifyRemoved(int ANotifyId);
protected slots:
  void onActivated(QSystemTrayIcon::ActivationReason AReason);
private:
  Menu *FContextMenu;
private:
  static int FNextNotifyId;
  int FCurNotifyId;
  struct NotifyItem {
    QIcon icon;
    QString toolTip;
  };
  QSystemTrayIcon FTrayIcon;
  QIcon FBaseIcon;
  QMap<int,NotifyItem *> FNotifyItems;
};

#endif // TRAYMANAGER_H
