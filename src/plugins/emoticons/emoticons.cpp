#include "emoticons.h"

#include <QSet>
#include <QTextBlock>

#define DEFAULT_ICONSET                 "kolobok_dark"

Emoticons::Emoticons()
{
	FMessageWidgets = NULL;
	FMessageProcessor = NULL;
	FOptionsManager = NULL;
}

Emoticons::~Emoticons()
{

}

void Emoticons::pluginInfo(IPluginInfo *APluginInfo)
{
	APluginInfo->name = tr("Emoticons");
	APluginInfo->description = tr("Allows to use your smiley images in messages");
	APluginInfo->version = "1.0";
	APluginInfo->author = "Potapov S.A. aka Lion";
	APluginInfo->homePage = "http://www.vacuum-im.org";
	APluginInfo->dependences.append(MESSAGEWIDGETS_UUID);
}

bool Emoticons::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
	IPlugin *plugin = APluginManager->pluginInterface("IMessageProcessor").value(0,NULL);
	if (plugin)
	{
		FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
	}

	plugin = APluginManager->pluginInterface("IMessageWidgets").value(0,NULL);
	if (plugin)
	{
		FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
		if (FMessageWidgets)
		{
			connect(FMessageWidgets->instance(),SIGNAL(toolBarWidgetCreated(IToolBarWidget *)),SLOT(onToolBarWidgetCreated(IToolBarWidget *)));
			connect(FMessageWidgets->instance(),SIGNAL(editWidgetCreated(IEditWidget *)),SLOT(onEditWidgetCreated(IEditWidget *)));
		}
	}

	plugin = APluginManager->pluginInterface("IOptionsManager").value(0,NULL);
	if (plugin)
	{
		FOptionsManager = qobject_cast<IOptionsManager *>(plugin->instance());
	}

	connect(Options::instance(),SIGNAL(optionsOpened()),SLOT(onOptionsOpened()));
	connect(Options::instance(),SIGNAL(optionsChanged(const OptionsNode &)),SLOT(onOptionsChanged(const OptionsNode &)));

	return FMessageWidgets!=NULL;
}

bool Emoticons::initObjects()
{
	if (FMessageProcessor)
	{
		FMessageProcessor->insertMessageWriter(this,MWO_EMOTICONS);
	}
	return true;
}

bool Emoticons::initSettings()
{
	Options::setDefaultValue(OPV_MESSAGES_EMOTICONS,QStringList() << DEFAULT_ICONSET);

	if (FOptionsManager)
	{
		IOptionsDialogNode dnode = { ONO_EMOTICONS, OPN_EMOTICONS, tr("Emoticons"), tr("Select emoticons iconsets"), MNI_EMOTICONS };
		FOptionsManager->insertOptionsDialogNode(dnode);
		FOptionsManager->insertOptionsHolder(this);
	}
	return true;
}

void Emoticons::writeMessage(int AOrder, Message &AMessage, QTextDocument *ADocument, const QString &ALang)
{
	Q_UNUSED(AMessage);
	Q_UNUSED(ALang);
	if (AOrder == MWO_EMOTICONS)
		replaceImageToText(ADocument);
}

void Emoticons::writeText(int AOrder, Message &AMessage, QTextDocument *ADocument, const QString &ALang)
{
	Q_UNUSED(AMessage);
	Q_UNUSED(ALang);
	if (AOrder == MWO_EMOTICONS)
		replaceTextToImage(ADocument);
}

IOptionsWidget *Emoticons::optionsWidget(const QString &ANodeId, int &AOrder, QWidget *AParent)
{
	if (ANodeId == OPN_EMOTICONS)
	{
		AOrder = OWO_EMOTICONS;
		return new EmoticonsOptions(this,AParent);
	}
	return NULL;
}

QList<QString> Emoticons::activeIconsets() const
{
	QList<QString> iconsets = Options::node(OPV_MESSAGES_EMOTICONS).value().toStringList();
	for (QList<QString>::iterator it = iconsets.begin(); it != iconsets.end(); )
	{
		if (!FStorages.contains(*it))
			it = iconsets.erase(it);
		else
			it++;
	}
	return iconsets;
}

QUrl Emoticons::urlByKey(const QString &AKey) const
{
	return FUrlByKey.value(AKey);
}

QString Emoticons::keyByUrl(const QUrl &AUrl) const
{
	return FKeyByUrl.value(AUrl.toString());
}

void Emoticons::createIconsetUrls()
{
	FUrlByKey.clear();
   FKeyByUrl.clear();
	foreach(QString substorage, Options::node(OPV_MESSAGES_EMOTICONS).value().toStringList())
	{
		IconStorage *storage = FStorages.value(substorage);
		if (storage)
		{
         QHash<QString, QString> fileFirstKey;
         foreach(QString key, storage->fileFirstKeys())
            fileFirstKey.insert(storage->fileFullName(key), key);

			foreach(QString key, storage->fileKeys())
			{
				if (!FUrlByKey.contains(key)) 
            {
               QString file = storage->fileFullName(key);
               QUrl url = QUrl::fromLocalFile(file);
               FUrlByKey.insert(key,url);
               FKeyByUrl.insert(url.toString(),fileFirstKey.value(file));
            }
			}
		}
	}
}

void Emoticons::replaceTextToImage(QTextDocument *ADocument) const
{
	static const QRegExp regexp("\\S+");
	for (QTextCursor cursor = ADocument->find(regexp); !cursor.isNull();  cursor = ADocument->find(regexp,cursor))
	{
		QUrl url = FUrlByKey.value(cursor.selectedText());
		if (!url.isEmpty())
		{
			ADocument->addResource(QTextDocument::ImageResource,url,QImage(url.toLocalFile()));
			cursor.insertImage(url.toString());
		}
	}
}

void Emoticons::replaceImageToText(QTextDocument *ADocument) const
{
	static const QString imageChar = QString(QChar::ObjectReplacementCharacter);
	for (QTextCursor cursor = ADocument->find(imageChar); !cursor.isNull();  cursor = ADocument->find(imageChar,cursor))
	{
		if (cursor.charFormat().isImageFormat())
		{
			QString key = FKeyByUrl.value(cursor.charFormat().toImageFormat().name());
			if (!key.isEmpty())
			{
				cursor.insertText(key);
				cursor.insertText(" ");
			}
		}
	}
}

SelectIconMenu *Emoticons::createSelectIconMenu(const QString &ASubStorage, QWidget *AParent)
{
	SelectIconMenu *menu = new SelectIconMenu(ASubStorage, AParent);
	connect(menu->instance(),SIGNAL(iconSelected(const QString &, const QString &)), SLOT(onIconSelected(const QString &, const QString &)));
	connect(menu->instance(),SIGNAL(destroyed(QObject *)),SLOT(onSelectIconMenuDestroyed(QObject *)));
	return menu;
}

void Emoticons::insertSelectIconMenu(const QString &ASubStorage)
{
	foreach(IToolBarWidget *widget, FToolBarsWidgets)
	{
		SelectIconMenu *menu = createSelectIconMenu(ASubStorage,widget->instance());
		FToolBarWidgetByMenu.insert(menu,widget);
		QToolButton *button = widget->toolBarChanger()->insertAction(menu->menuAction(),TBG_MWTBW_EMOTICONS);
		button->setToolButtonStyle(Qt::ToolButtonIconOnly);
		button->setPopupMode(QToolButton::InstantPopup);
	}
}

void Emoticons::removeSelectIconMenu(const QString &ASubStorage)
{
	QMap<SelectIconMenu *,IToolBarWidget *>::iterator it = FToolBarWidgetByMenu.begin();
	while (it != FToolBarWidgetByMenu.end())
	{
		SelectIconMenu *menu = it.key();
		IToolBarWidget *widget = it.value();
		if (menu->iconset() == ASubStorage)
		{
			widget->toolBarChanger()->removeItem(widget->toolBarChanger()->actionHandle(menu->menuAction()));
			it = FToolBarWidgetByMenu.erase(it);
			delete menu;
		}
		else
			it++;
	}
}

void Emoticons::onToolBarWidgetCreated(IToolBarWidget *AWidget)
{
	if (AWidget->editWidget() != NULL)
	{
		FToolBarsWidgets.append(AWidget);
		foreach(QString substorage, activeIconsets())
		{
			SelectIconMenu *menu = createSelectIconMenu(substorage,AWidget->instance());
			FToolBarWidgetByMenu.insert(menu,AWidget);
			QToolButton *button = AWidget->toolBarChanger()->insertAction(menu->menuAction(),TBG_MWTBW_EMOTICONS);
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			button->setPopupMode(QToolButton::InstantPopup);
		}
		connect(AWidget->instance(),SIGNAL(destroyed(QObject *)),SLOT(onToolBarWidgetDestroyed(QObject *)));
	}
}

void Emoticons::onToolBarWidgetDestroyed(QObject *AObject)
{
	QList<IToolBarWidget *>::iterator it = FToolBarsWidgets.begin();
	while (it != FToolBarsWidgets.end())
	{
		if (qobject_cast<QObject *>((*it)->instance()) == AObject)
			it = FToolBarsWidgets.erase(it);
		else
			it++;
	}
}

void Emoticons::onEditWidgetCreated(IEditWidget *AEditWidget)
{
	connect(AEditWidget->textEdit()->document(),SIGNAL(contentsChange(int,int,int)),SLOT(onEditWidgetContentsChanged(int,int,int)));
}

void Emoticons::onEditWidgetContentsChanged(int APosition, int ARemoved, int AAdded)
{
	Q_UNUSED(ARemoved);
	if (AAdded>0)
	{
		QTextDocument *doc = qobject_cast<QTextDocument *>(sender());
		QList<QUrl> urlList = FUrlByKey.values();
		QTextBlock block = doc->findBlock(APosition);
		while (block.isValid() && block.position()<=APosition+AAdded)
		{
			for (QTextBlock::iterator it = block.begin(); !it.atEnd(); it++)
			{
				QTextFragment fragment = it.fragment();
				if (fragment.charFormat().isImageFormat())
				{
					QUrl url = fragment.charFormat().toImageFormat().name();
					if (doc->resource(QTextDocument::ImageResource,url).isNull())
					{
						if (urlList.contains(url))
						{
							doc->addResource(QTextDocument::ImageResource,url,QImage(url.toLocalFile()));
							doc->markContentsDirty(fragment.position(),fragment.length());
						}
					}
				}
			}
			block = block.next();
		}
	}
}

void Emoticons::onIconSelected(const QString &ASubStorage, const QString &AIconKey)
{
	Q_UNUSED(ASubStorage);
	SelectIconMenu *menu = qobject_cast<SelectIconMenu *>(sender());
	if (FToolBarWidgetByMenu.contains(menu))
	{
		IEditWidget *widget = FToolBarWidgetByMenu.value(menu)->editWidget();
		if (widget)
		{
			QUrl url = FUrlByKey.value(AIconKey);
			if (!url.isEmpty())
			{
				QTextEdit *editor = widget->textEdit();
				editor->document()->addResource(QTextDocument::ImageResource,url,QImage(url.toLocalFile()));
				editor->textCursor().beginEditBlock();
				editor->textCursor().insertImage(url.toString());
				editor->textCursor().insertText(" ");
				editor->textCursor().endEditBlock();
				editor->setFocus();
			}
		}
	}
}

void Emoticons::onSelectIconMenuDestroyed(QObject *AObject)
{
	foreach(SelectIconMenu *menu, FToolBarWidgetByMenu.keys())
		if (qobject_cast<QObject *>(menu) == AObject)
			FToolBarWidgetByMenu.remove(menu);
}

void Emoticons::onOptionsOpened()
{
	onOptionsChanged(Options::node(OPV_MESSAGES_EMOTICONS));
}

void Emoticons::onOptionsChanged(const OptionsNode &ANode)
{
	if (ANode.path() == OPV_MESSAGES_EMOTICONS)
	{
		QList<QString> oldStorages = FStorages.keys();
		QList<QString> availStorages = IconStorage::availSubStorages(RSR_STORAGE_EMOTICONS);

		foreach(QString substorage, Options::node(OPV_MESSAGES_EMOTICONS).value().toStringList())
		{
			if (availStorages.contains(substorage))
			{
				if (!FStorages.contains(substorage))
				{
					FStorages.insert(substorage, new IconStorage(RSR_STORAGE_EMOTICONS,substorage,this));
					insertSelectIconMenu(substorage);
				}
				oldStorages.removeAll(substorage);
			}
		}

		foreach (QString substorage, oldStorages)
		{
			removeSelectIconMenu(substorage);
			delete FStorages.take(substorage);
		}

		createIconsetUrls();
	}
}
Q_EXPORT_PLUGIN2(plg_emoticons, Emoticons)