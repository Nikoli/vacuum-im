#ifndef INDEXDATAHOLDER_H
#define INDEXDATAHOLDER_H

#include "../../interfaces/irostersmodel.h"
#include "../../interfaces/irostersview.h"
#include "../../utils/skin.h"

class IndexDataHolder : 
  public QObject,
  public IRosterIndexDataHolder
{
  Q_OBJECT;
  Q_INTERFACES(IRosterIndexDataHolder);

public:
  IndexDataHolder(QObject *AParent);
  ~IndexDataHolder();

  //IRosterIndexDataHolder
  virtual QObject *instance() { return this; }
  virtual bool setData(IRosterIndex *AIndex, int ARole, const QVariant &AValue);
  virtual QVariant data(const IRosterIndex *AIndex, int ARole) const;
  virtual QList<int> roles() const;
public:
  void clear();
  bool checkOption(IRostersView::Option AOption) const;
  void setOption(IRostersView::Option AOption, bool AValue);
signals:
  virtual void dataChanged(IRosterIndex *AIndex, int ARole);
protected:
  QString toolTipText(const IRosterIndex *AIndex) const;
private:
  int FOptions;
  SkinIconset FStatusIconset;
  QHash<const IRosterIndex *,QHash<int,QVariant> > FData;
};

#endif // INDEXDATAHOLDER_H
