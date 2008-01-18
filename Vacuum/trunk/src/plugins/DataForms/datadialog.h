#ifndef DATADIALOG_H
#define DATADIALOG_H

#include "../../interfaces/idataforms.h"
#include "ui_datadialog.h"

class DataDialog : 
  public IDataDialog
{
  Q_OBJECT;
  Q_INTERFACES(IDataDialog);
public:
  DataDialog(IDataForm *ADataForm, QWidget *AParent = NULL);
  ~DataDialog();
  virtual IDataForm *dataForm() const { return FDataForm; }
  virtual ToolBarChanger *toolBarChanged() const { return FToolBarChanger; }
  virtual QDialogButtonBox *dialogButtons() const { return ui.dbbDialogButtons; }
  virtual void showPage(int APage);
  virtual void showPrevPage();
  virtual void showNextPage();
signals:
  virtual void currentPageChanged(int APage);
protected:
  void updateDialog();
protected slots:
  void onPrevPageClicked();
  void onNextPageClicked();
private:
  Ui::DataDialogClass ui;
private:
  IDataForm *FDataForm;
private:
  ToolBarChanger *FToolBarChanger;
};

#endif // DATADIALOG_H
