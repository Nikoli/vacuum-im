#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "../../definations/accountvaluenames.h"
#include "../../interfaces/iaccountmanager.h"
#include "../../interfaces/ixmppstreams.h"
#include "../../interfaces/isettings.h"

class Account : 
  public QObject,
  public IAccount
{
  Q_OBJECT;
  Q_INTERFACES(IAccount);
public:
  Account(IXmppStreams *AXmppStreams, ISettings *ASettings, const QString &AAccountId, QObject *AParent);
  ~Account();
  virtual QObject *instance() { return this; }
  virtual const QString &accountId() const;
  virtual IXmppStream *xmppStream() const;
  virtual bool isValid() const;
  virtual bool isActive() const;
  virtual void setActive(bool AActive);
  virtual QString name() const;
  virtual void setName(const QString &AName);
  virtual Jid streamJid() const;
  virtual void setStreamJid(const Jid &AJid);
  virtual QString password() const;
  virtual void setPassword(const QString &APassword);
  virtual QString defaultLang() const;
  virtual void setDefaultLang(const QString &ALang);
  //AccountValues
  virtual QByteArray encript(const QString &AValue, const QByteArray &AKey) const;
  virtual QString decript(const QByteArray &AValue, const QByteArray &AKey) const;
  virtual QVariant value(const QString &AName, const QVariant &ADefault=QVariant()) const; 
  virtual void setValue(const QString &AName, const QVariant &AValue);
  virtual void delValue(const QString &AName);
signals:
  virtual void changed(const QString &AName, const QVariant &AValue);
private:
  ISettings *FSettings;
  IXmppStream *FXmppStream;
  IXmppStreams *FXmppStreams;
private:
  QString FAccountId;
};

#endif // ACCOUNT_H
