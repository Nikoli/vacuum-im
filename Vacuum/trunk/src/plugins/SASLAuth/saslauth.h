#ifndef SASLAUTH_H
#define SASLAUTH_H

#include <QObject>
#include <QObjectCleanupHandler>
#include "../../definations/namespaces.h"
#include "../../interfaces/ipluginmanager.h"
#include "../../interfaces/ixmppstreams.h"

#define SASLAUTH_UUID "{E583F155-BE87-4919-8769-5C87088F0F57}"

class SASLAuth :
  public QObject,
  public IStreamFeature
{
  Q_OBJECT;
  Q_INTERFACES(IStreamFeature);

public:
  SASLAuth(IXmppStream *AXmppStream);
  ~SASLAuth();

  virtual QObject *instance() { return this; }
  virtual QString name() const { return "mechanisms"; }
  virtual QString nsURI() const { return NS_FEATURE_SASL; }
  virtual IXmppStream *xmppStream() const { return FXmppStream; }
  virtual bool start(const QDomElement &AElem); 
  virtual bool needHook(Direction ADirection) const;
  virtual bool hookData(QByteArray *, Direction) { return false; }
  virtual bool hookElement(QDomElement *AElem, Direction ADirection);
signals:
  virtual void finished(bool); 
  virtual void error(const QString &);
protected slots:
  void onStreamClosed(IXmppStream *);
private: 
  IXmppStream *FXmppStream;
private:
  bool FNeedHook;
  QString FMechanism;
  qint8 chlNumber;
  QString realm, nonce, cnonce, qop, uri;
};


class SASLAuthPlugin : 
  public QObject,
  public IPlugin,
  public IStreamFeaturePlugin
{
  Q_OBJECT
  Q_INTERFACES(IPlugin IStreamFeaturePlugin)

public:
  SASLAuthPlugin();
  ~SASLAuthPlugin();

  //IPlugin
  virtual QObject *instance() { return this; }
  virtual QUuid pluginUuid() const { return SASLAUTH_UUID; }
  virtual void pluginInfo(PluginInfo *APluginInfo);
  virtual bool initConnections(IPluginManager *APluginManager, int &AInitOrder);
  virtual bool initObjects();
  virtual bool initSettings() { return true; }
  virtual bool startPlugin() { return true; }

  //IStreamFeaturePlugin
  virtual IStreamFeature *addFeature(IXmppStream *AXmppStream);
  virtual IStreamFeature *getFeature(const Jid &AStreamJid) const;
  virtual void removeFeature(IXmppStream *AXmppStream);
signals:
  virtual void featureAdded(IStreamFeature *);
  virtual void featureRemoved(IStreamFeature *);
protected slots:
  void onStreamAdded(IXmppStream *AXmppStream);
  void onStreamRemoved(IXmppStream *AXmppStream);
  void onSASLAuthDestroyed(QObject *AObject);
private:
  QList<SASLAuth *> FFeatures;
  QObjectCleanupHandler FCleanupHandler;
};

#endif // SASLAUTH_H
