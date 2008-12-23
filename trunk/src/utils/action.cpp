#include <QtDebug>
#include "action.h"

int Action::FNewRole = Action::DR_UserDefined + 1;

Action::Action(QObject *AParent)
  : QAction(AParent)
{
  FMenu = NULL;
  FIconset = NULL;
}

Action::~Action()
{
  emit actionDestroyed(this);
}

void Action::setIcon(const QIcon &AIcon)
{
  if (FIconset)
  {
    disconnect(FIconset,SIGNAL(iconsetChanged()),this,SLOT(onIconsetChanged()));
    FIconName.clear();
    FIconset = NULL;
  }
  QAction::setIcon(AIcon);
}

void Action::setIcon(const QString &AIconsetFile, const QString &AIconName)
{
  FIconName = AIconName;
  if (FIconset)
    disconnect(FIconset,SIGNAL(iconsetChanged()),this,SLOT(onIconsetChanged()));
  FIconset = Skin::getSkinIconset(AIconsetFile);
  connect(FIconset,SIGNAL(iconsetChanged()),SLOT(onIconsetChanged()));
  QAction::setIcon(FIconset->iconByName(AIconName));
}

void Action::setMenu(Menu *AMenu)
{
  if (FMenu)
  {
    disconnect(FMenu,SIGNAL(menuDestroyed(Menu *)),this,SLOT(onMenuDestroyed(Menu *)));
    if (FMenu != AMenu && FMenu->parent()==this)
      delete FMenu;
  }

  if (AMenu)
  {
    if (parent() == AMenu)
    {
      setIcon(AMenu->icon());
      setText(AMenu->title());
      setToolTip(AMenu->toolTip());
      setWhatsThis(AMenu->whatsThis());
    }
    connect(AMenu,SIGNAL(menuDestroyed(Menu *)),SLOT(onMenuDestroyed(Menu *)));
  }
  QAction::setMenu(AMenu);  //� 4.3.0 �������� icon, text � �.�. ��������� Menu �� ��������� Action
  FMenu = AMenu;
}

void Action::setData(int ARole, const QVariant &AData)
{
  if (AData.isValid())
    FData.insert(ARole,AData);
  else
    FData.remove(ARole);
}

void Action::setData(const QHash<int,QVariant> &AData)
{
  FData.unite(AData);
}

QVariant Action::data(int ARole) const
{
  return FData.value(ARole);
}

int Action::newRole()
{
  FNewRole++;
  return FNewRole;
}

void Action::onMenuDestroyed(Menu *AMenu)
{
  if (AMenu == FMenu)
    FMenu = NULL;
}

void Action::onIconsetChanged()
{
  QAction::setIcon(FIconset->iconByName(FIconName));
}
