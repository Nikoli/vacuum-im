#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>

#include "../../interfaces/isettings.h"

class Settings : 
  public QObject,
  public ISettings
{
  Q_OBJECT;
  Q_INTERFACES(ISettings);

public:
  Settings(const QUuid &AUuid, ISettingsPlugin *ASettingsPlugin, QObject *parent);
  ~Settings();

  //ISettings
  virtual QObject *instance() { return this; }
  virtual QByteArray encript(const QString &AValue, const QByteArray &AKey) const;
  virtual QString decript(const QByteArray &AValue, const QByteArray &AKey) const;
  virtual QVariant valueNS(const QString &AName, const QString &ANameNS, 
    const QVariant &ADefault=QVariant()) const;
  virtual QVariant value(const QString &AName, const QVariant &ADefault=QVariant()) const;
  virtual QHash<QString,QVariant> values(const QString &AName) const;
  virtual ISettings &setValueNS(const QString &AName, const QString &ANameNS, 
    const QVariant &AValue);
  virtual ISettings &setValue(const QString &AName, const QVariant &AValue);
  virtual ISettings &delValueNS(const QString &AName, const QString &ANameNS);
  virtual ISettings &delValue(const QString &AName);
  virtual ISettings &delNS(const QString &ANameNS);
signals:
  virtual void opened();
  virtual void closed();
protected:
  QDomElement getElement(const QString &AName, const QString &ANameNS, bool ACreate) const;
  void delNSRecurse(const QString &ANameNS, QDomElement elem);
  static QString variantToString(const QVariant &AVariant);
  static QVariant stringToVariant(const QString &AString, QVariant::Type AType, const QVariant &ADefault);
private slots:
  virtual void onProfileOpened(const QString &AProfile = QString());
  virtual void onProfileClosed(const QString &AProfile = QString());
private:
  ISettingsPlugin *FSettingsPlugin;
private:
  QUuid FUuid;
  QDomElement FSettings;
  bool FSettingsOpened;
};

#endif // CONFIGURATOR_H
