#ifndef ISETTINGS_H
#define ISETTINGS_H

#include <QDomDocument>
#include <QUuid>
#include <QVariant>
#include <QHash>
#include <QWidget>
#include <QIcon>

class IOptionsHolder {
public:
  virtual QObject *instance() =0;
  virtual QWidget *optionsWidget(const QString &ANode, int &AOrder) const =0;
public slots:
  virtual void applyOptions() =0;
};

class ISettings {
public:
  virtual QObject* instance() =0;
  virtual QVariant valueNS(const QString &AName, const QString &ANameNS, 
    const QVariant &ADefault=QVariant()) const =0;
  virtual QVariant value(const QString &AName, const QVariant &ADefault=QVariant()) const =0;
  virtual QHash<QString,QVariant> values(const QString &AName) const =0;
  virtual ISettings &setValueNS(const QString &AName, const QString &ANameNS, 
    const QVariant &AValue) =0;
  virtual ISettings &setValue(const QString &AName, const QVariant &AValue) =0;
  virtual ISettings &delValueNS(const QString &AName, const QString &ANameNS) =0;
  virtual ISettings &delValue(const QString &AName) =0;
  virtual ISettings &delNS(const QString &ANameNS) =0;
signals:
  virtual void opened()=0;
  virtual void closed()=0;
};

class ISettingsPlugin {
public:
  virtual QObject *instance() =0;
  virtual ISettings *newSettings(const QUuid &, QObject *)=0;
  virtual QString fileName() const =0;
  virtual bool setFileName(const QString &) =0;
  virtual bool saveSettings() =0;
  virtual QDomDocument document() const=0;
  virtual QString profile() const =0;
  virtual QDomElement setProfile(const QString &) =0;
  virtual QDomElement getProfile(const QString &) =0;
  virtual QDomElement getPluginNode(const QUuid &) =0;
  virtual void openOptionsNode(const QString &ANode, const QString &AName, 
    const QString &ADescription, const QIcon &AIcon) =0;
public slots:
  virtual void openOptionsDialog(const QString &ANode = "") =0;
signals:
  virtual void profileOpened()=0;
  virtual void profileClosed()=0;
  virtual void optionsDialogAccepted() =0;
  virtual void optionsDialogRejected() =0;
};

Q_DECLARE_INTERFACE(ISettings,"Vacuum.Plugin.ISettings/1.0")
Q_DECLARE_INTERFACE(ISettingsPlugin,"Vacuum.Plugin.ISettingsPlugin/1.0")

#endif
