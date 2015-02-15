#ifndef CONNECTIONOPTIONSWIDGET_H
#define CONNECTIONOPTIONSWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <interfaces/iconnectionmanager.h>
#include <interfaces/iaccountmanager.h>
#include <interfaces/ioptionsmanager.h>
#include "connectionmanager.h"
#include "ui_connectionoptionswidget.h"

class ConnectionManager;

class ConnectionOptionsWidget :
	public QWidget,
	public IOptionsDialogWidget
{
	Q_OBJECT;
	Q_INTERFACES(IOptionsDialogWidget);
public:
	ConnectionOptionsWidget(IConnectionManager *AManager, const OptionsNode &ANode, QWidget *AParent);
	~ConnectionOptionsWidget();
	virtual QWidget* instance() { return this; }
public slots:
	void apply();
	void reset();
signals:
	void modified();
	void childApply();
	void childReset();
protected:
	void setPluginById(const QString &APluginId);
protected slots:
	void onComboConnectionsChanged(int AIndex);
private:
	IConnectionManager *FManager;
private:
	Ui::ConnectionOptionsWidgetClass ui;
private:
	QString FPluginId;
	OptionsNode FOptions;
	QVBoxLayout *FPluginLayout;
	IOptionsDialogWidget *FPluginSettings;
};

#endif // CONNECTIONOPTIONSWIDGET_H
