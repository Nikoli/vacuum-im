#include "vcarddialog.h"

#include <QMessageBox>
#include <QFileDialog>

VCardDialog::VCardDialog(IVCardPlugin *AVCardPlugin, const Jid &AStreamJid, const Jid &AContactJid)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose,true);
	setWindowTitle(tr("vCard - %1").arg(AContactJid.full()));
	IconStorage::staticStorage(RSR_STORAGE_MENUICONS)->insertAutoIcon(this,MNI_VCARD,0,0,"windowIcon");

	FContactJid = AContactJid;
	FStreamJid = AStreamJid;
	FVCardPlugin = AVCardPlugin;

	FSaveClisked = false;

	if (FStreamJid && FContactJid)
		ui.btbButtons->setStandardButtons(QDialogButtonBox::Save|QDialogButtonBox::Close);
	else
		ui.btbButtons->setStandardButtons(QDialogButtonBox::Close);
	ui.btbButtons->addButton(tr("Reload"),QDialogButtonBox::ResetRole);
	connect(ui.btbButtons,SIGNAL(clicked(QAbstractButton *)),SLOT(onDialogButtonClicked(QAbstractButton *)));

	FVCard = FVCardPlugin->vcard(FContactJid);
	connect(FVCard->instance(),SIGNAL(vcardUpdated()),SLOT(onVCardUpdated()));
	connect(FVCard->instance(),SIGNAL(vcardPublished()),SLOT(onVCardPublished()));
	connect(FVCard->instance(),SIGNAL(vcardError(const QString &)),SLOT(onVCardError(const QString &)));

	connect(ui.tlbPhotoSave,SIGNAL(clicked()),SLOT(onPhotoSaveClicked()));
	connect(ui.tlbPhotoLoad,SIGNAL(clicked()),SLOT(onPhotoLoadClicked()));
	connect(ui.tlbPhotoClear,SIGNAL(clicked()),SLOT(onPhotoClearClicked()));
	connect(ui.tlbLogoSave,SIGNAL(clicked()),SLOT(onLogoSaveClicked()));
	connect(ui.tlbLogoLoad,SIGNAL(clicked()),SLOT(onLogoLoadClicked()));
	connect(ui.tlbLogoClear,SIGNAL(clicked()),SLOT(onLogoClearClicked()));
	connect(ui.tlbEmailAdd,SIGNAL(clicked()),SLOT(onEmailAddClicked()));
	connect(ui.tlbEmailDelete,SIGNAL(clicked()),SLOT(onEmailDeleteClicked()));
	connect(ui.ltwEmails,SIGNAL(itemActivated(QListWidgetItem *)),SLOT(onEmailItemActivated(QListWidgetItem *)));
	connect(ui.tlbPhoneAdd,SIGNAL(clicked()),SLOT(onPhoneAddClicked()));
	connect(ui.tlbPhoneDelete,SIGNAL(clicked()),SLOT(onPhoneDeleteClicked()));
	connect(ui.ltwPhones,SIGNAL(itemActivated(QListWidgetItem *)),SLOT(onPhoneItemActivated(QListWidgetItem *)));
	
	if (FVCard->isEmpty())
	{
		if (FVCard->update(FStreamJid))
		{
			ui.twtVCard->setEnabled(false);
			ui.btbButtons->setEnabled(false);
		}
		else
		{
			onVCardError(tr("Service unavailable"));
		}
	}

	ui.twtVCard->setCurrentIndex(0);
	updateDialog();
}

VCardDialog::~VCardDialog()
{
	FVCard->unlock();
}

void VCardDialog::updateDialog()
{
	bool readOnly = !(FContactJid && FStreamJid);

	ui.lneFullName->setText(FVCard->value(VVN_FULL_NAME));
	ui.lneFullName->setReadOnly(readOnly);
	ui.lneFirstName->setText(FVCard->value(VVN_GIVEN_NAME));
	ui.lneFirstName->setReadOnly(readOnly);
	ui.lneMiddleName->setText(FVCard->value(VVN_MIDDLE_NAME));
	ui.lneMiddleName->setReadOnly(readOnly);
	ui.lneLastName->setText(FVCard->value(VVN_FAMILY_NAME));
	ui.lneLastName->setReadOnly(readOnly);
	ui.lneNickName->setText(FVCard->value(VVN_NICKNAME));
	ui.lneNickName->setReadOnly(readOnly);
	ui.lneJabberId->setText(FContactJid.full());
	ui.lneJabberId->setReadOnly(true);

	QDate birthday = QDate::fromString(FVCard->value(VVN_BIRTHDAY),Qt::ISODate);
	if (!birthday.isValid())
		birthday = QDate::fromString(FVCard->value(VVN_BIRTHDAY),Qt::TextDate);
	if (!birthday.isValid() || birthday<ui.dedBirthday->minimumDate())
		birthday = ui.dedBirthday->minimumDate();
	ui.dedBirthday->setDate(birthday);
	ui.dedBirthday->setReadOnly(readOnly);
	ui.dedBirthday->setEnabled(!readOnly || birthday.isValid());
	ui.dedBirthday->setCalendarPopup(!readOnly);
	ui.cmbGender->lineEdit()->setText(FVCard->value(VVN_GENDER));
	ui.cmbGender->setEnabled(!readOnly);
	ui.lneMarital->setText(FVCard->value(VVN_MARITAL_STATUS));
	ui.lneMarital->setReadOnly(readOnly);
	ui.lneTitle->setText(FVCard->value(VVN_TITLE));
	ui.lneTitle->setReadOnly(readOnly);
	ui.lneDepartment->setText(FVCard->value(VVN_ORG_UNIT));
	ui.lneDepartment->setReadOnly(readOnly);
	ui.lneCompany->setText(FVCard->value(VVN_ORG_NAME));
	ui.lneCompany->setReadOnly(readOnly);
	ui.lneRole->setText(FVCard->value(VVN_ROLE));
	ui.lneRole->setReadOnly(readOnly);
	ui.lneHomePage->setText(FVCard->value(VVN_URL));
	ui.lneHomePage->setReadOnly(readOnly);

	static const QStringList tagHome = QStringList() << "HOME";
	static const QStringList tagWork = QStringList() << "WORK";
	static const QStringList tagsAdres = tagHome+tagWork;
	ui.lneHomeStreet->setText(FVCard->value(VVN_ADR_STREET,tagHome,tagsAdres));
	ui.lneHomeStreet->setReadOnly(readOnly);
	ui.lneHomeCity->setText(FVCard->value(VVN_ADR_CITY,tagHome,tagsAdres));
	ui.lneHomeCity->setReadOnly(readOnly);
	ui.lneHomeState->setText(FVCard->value(VVN_ADR_REGION,tagHome,tagsAdres));
	ui.lneHomeState->setReadOnly(readOnly);
	ui.lneHomeZip->setText(FVCard->value(VVN_ADR_PCODE,tagHome,tagsAdres));
	ui.lneHomeZip->setReadOnly(readOnly);
	ui.lneHomeCountry->setText(FVCard->value(VVN_ADR_COUNTRY,tagHome,tagsAdres));
	ui.lneHomeCountry->setReadOnly(readOnly);
	ui.lneWorkStreet->setText(FVCard->value(VVN_ADR_STREET,tagWork,tagsAdres));
	ui.lneWorkStreet->setReadOnly(readOnly);
	ui.lneWorkCity->setText(FVCard->value(VVN_ADR_CITY,tagWork,tagsAdres));
	ui.lneWorkCity->setReadOnly(readOnly);
	ui.lneWorkState->setText(FVCard->value(VVN_ADR_REGION,tagWork,tagsAdres));
	ui.lneWorkState->setReadOnly(readOnly);
	ui.lneWorkZip->setText(FVCard->value(VVN_ADR_PCODE,tagWork,tagsAdres));
	ui.lneWorkZip->setReadOnly(readOnly);
	ui.lneWorkCountry->setText(FVCard->value(VVN_ADR_COUNTRY,tagWork,tagsAdres));
	ui.lneWorkCountry->setReadOnly(readOnly);

	ui.ltwEmails->clear();
	static const QStringList emailTagList = QStringList() << "HOME" << "WORK" << "INTERNET" << "X400";
	QHash<QString,QStringList> emails = FVCard->values(VVN_EMAIL,emailTagList);
	foreach(QString email, emails.keys())
	{
		QListWidgetItem *listItem = new QListWidgetItem(email,ui.ltwEmails);
		listItem->setData(Qt::UserRole,emails.value(email));
		ui.ltwEmails->addItem(listItem);
	}
	ui.tlbEmailAdd->setVisible(!readOnly);
	ui.tlbEmailDelete->setVisible(!readOnly);

	ui.ltwPhones->clear();
	static const QStringList phoneTagList = QStringList() << "HOME" << "WORK" << "CELL" << "MODEM";
	QHash<QString,QStringList> phones = FVCard->values(VVN_TELEPHONE,phoneTagList);
	foreach(QString phone, phones.keys())
	{
		QListWidgetItem *listItem = new QListWidgetItem(phone,ui.ltwPhones);
		listItem->setData(Qt::UserRole,phones.value(phone));
		ui.ltwPhones->addItem(listItem);
	}
	ui.tlbPhoneAdd->setVisible(!readOnly);
	ui.tlbPhoneDelete->setVisible(!readOnly);

	setLogo(QPixmap::fromImage(FVCard->logoImage()));
	ui.tlbLogoClear->setVisible(!readOnly);
	ui.tlbLogoLoad->setVisible(!readOnly);

	setPhoto(QPixmap::fromImage(FVCard->photoImage()));
	ui.tlbPhotoClear->setVisible(!readOnly);
	ui.tlbPhotoLoad->setVisible(!readOnly);

	ui.tedComments->setPlainText(FVCard->value(VVN_DESCRIPTION));
	ui.tedComments->setReadOnly(readOnly);
}

void VCardDialog::updateVCard()
{
	FVCard->clear();

	FVCard->setValueForTags(VVN_FULL_NAME,ui.lneFullName->text());
	FVCard->setValueForTags(VVN_GIVEN_NAME,ui.lneFirstName->text());
	FVCard->setValueForTags(VVN_MIDDLE_NAME,ui.lneMiddleName->text());
	FVCard->setValueForTags(VVN_FAMILY_NAME,ui.lneLastName->text());
	FVCard->setValueForTags(VVN_NICKNAME,ui.lneNickName->text());

	if (ui.dedBirthday->date() > ui.dedBirthday->minimumDate())
		FVCard->setValueForTags(VVN_BIRTHDAY,ui.dedBirthday->date().toString(Qt::ISODate));
	else
		FVCard->setValueForTags(VVN_BIRTHDAY,"");
	FVCard->setValueForTags(VVN_GENDER,ui.cmbGender->currentText());
	FVCard->setValueForTags(VVN_MARITAL_STATUS,ui.lneMarital->text());
	FVCard->setValueForTags(VVN_TITLE,ui.lneTitle->text());
	FVCard->setValueForTags(VVN_ORG_UNIT,ui.lneDepartment->text());
	FVCard->setValueForTags(VVN_ORG_NAME,ui.lneCompany->text());
	FVCard->setValueForTags(VVN_ROLE,ui.lneRole->text());
	FVCard->setValueForTags(VVN_URL,ui.lneHomePage->text());

	static const QStringList adresTags = QStringList() << "WORK" << "HOME";
	static const QStringList tagHome = QStringList() << "HOME";
	static const QStringList tagWork = QStringList() << "WORK";
	FVCard->setValueForTags(VVN_ADR_STREET,ui.lneHomeStreet->text(),tagHome,adresTags);
	FVCard->setValueForTags(VVN_ADR_CITY,ui.lneHomeCity->text(),tagHome,adresTags);
	FVCard->setValueForTags(VVN_ADR_REGION,ui.lneHomeState->text(),tagHome,adresTags);
	FVCard->setValueForTags(VVN_ADR_PCODE,ui.lneHomeZip->text(),tagHome,adresTags);
	FVCard->setValueForTags(VVN_ADR_COUNTRY,ui.lneHomeCountry->text(),tagHome,adresTags);
	FVCard->setValueForTags(VVN_ADR_STREET,ui.lneWorkStreet->text(),tagWork,adresTags);
	FVCard->setValueForTags(VVN_ADR_CITY,ui.lneWorkCity->text(),tagWork,adresTags);
	FVCard->setValueForTags(VVN_ADR_REGION,ui.lneWorkState->text(),tagWork,adresTags);
	FVCard->setValueForTags(VVN_ADR_PCODE,ui.lneWorkZip->text(),tagWork,adresTags);
	FVCard->setValueForTags(VVN_ADR_COUNTRY,ui.lneWorkCountry->text(),tagWork,adresTags);

	static const QStringList emailTagList = QStringList() << "HOME" << "WORK" << "INTERNET" << "X400";
	for (int i = 0; i<ui.ltwEmails->count(); i++)
	{
		QListWidgetItem *listItem = ui.ltwEmails->item(i);
		FVCard->setTagsForValue(VVN_EMAIL,listItem->text(),listItem->data(Qt::UserRole).toStringList(),emailTagList);
	}

	static const QStringList phoneTagList = QStringList() << "HOME" << "WORK" << "CELL" << "MODEM";
	for (int i = 0; i<ui.ltwPhones->count(); i++)
	{
		QListWidgetItem *listItem = ui.ltwPhones->item(i);
		FVCard->setTagsForValue(VVN_TELEPHONE,listItem->text(),listItem->data(Qt::UserRole).toStringList(),phoneTagList);
	}

	if (!FLogo.isNull())
		FVCard->setLogoImage(FLogo.toImage());
	if (!FPhoto.isNull())
		FVCard->setPhotoImage(FPhoto.toImage());

	FVCard->setValueForTags(VVN_DESCRIPTION,ui.tedComments->toPlainText());
}

void VCardDialog::setPhoto(const QPixmap &APhoto)
{
	FPhoto = APhoto;
	ui.pmfPhoto->setPixmap(FPhoto);
	ui.tlbPhotoSave->setVisible(!FPhoto.isNull());
}

void VCardDialog::setLogo(const QPixmap &ALogo)
{
	FLogo = ALogo;
	ui.pmfLogo->setPixmap(FLogo);
	ui.tlbLogoSave->setVisible(!FLogo.isNull());
}

void VCardDialog::onVCardUpdated()
{
	ui.btbButtons->setEnabled(true);
	ui.twtVCard->setEnabled(true);
	updateDialog();
}

void VCardDialog::onVCardPublished()
{
	if (!FSaveClisked)
	{
		ui.btbButtons->setEnabled(true);
		ui.twtVCard->setEnabled(true);
	}
	else
	{
		accept();
	}
}

void VCardDialog::onVCardError(const QString &AError)
{
	QMessageBox::critical(this,tr("vCard error"),tr("vCard request or publish failed.<br>%1").arg(Qt::escape(AError)));

	if (!FSaveClisked)
		deleteLater();

	FSaveClisked = false;
	ui.twtVCard->setEnabled(true);
	ui.btbButtons->setEnabled(true);
}

void VCardDialog::onUpdateDialogTimeout()
{
	updateDialog();
}

void VCardDialog::onPhotoSaveClicked()
{
	if (!FPhoto.isNull())
	{
		QString filename = QFileDialog::getSaveFileName(this,tr("Save image"),"",tr("Image Files (*.png *.jpg *.bmp *.gif)"));
		if (!filename.isEmpty())
			FPhoto.save(filename);
	}
}

void VCardDialog::onPhotoLoadClicked()
{
	QString filename = QFileDialog::getOpenFileName(this,tr("Open image"),"",tr("Image Files (*.png *.jpg *.bmp *.gif)"));
	if (!filename.isEmpty())
	{
		QImage image(filename);
		if (!image.isNull())
			setPhoto(QPixmap::fromImage(image));
	}
}

void VCardDialog::onPhotoClearClicked()
{
	setPhoto(QPixmap());
}

void VCardDialog::onLogoSaveClicked()
{
	if (!FLogo.isNull())
	{
		QString filename = QFileDialog::getSaveFileName(this,tr("Save image"),"",tr("Image Files (*.png *.jpg *.bmp *.gif)"));
		if (!filename.isEmpty())
			FLogo.save(filename);
	}
}

void VCardDialog::onLogoLoadClicked()
{
	QString filename = QFileDialog::getOpenFileName(this,tr("Open image"),"",tr("Image Files (*.png *.jpg *.bmp *.gif)"));
	if (!filename.isEmpty())
	{
		QImage image(filename);
		if (!image.isNull())
			setLogo(QPixmap::fromImage(image));
	}
}

void VCardDialog::onLogoClearClicked()
{
	setLogo(QPixmap());
}

void VCardDialog::onEmailAddClicked()
{
	static QStringList emailTagList = QStringList() << "HOME" << "WORK" << "INTERNET" << "X400";
	EditItemDialog dialog("",QStringList(),emailTagList,this);
	dialog.setLabelText(tr("EMail:"));
	if (dialog.exec() == QDialog::Accepted && !dialog.value().isEmpty()
	    && ui.ltwEmails->findItems(dialog.value(),Qt::MatchFixedString).isEmpty())
	{
		QListWidgetItem *item = new QListWidgetItem(dialog.value(),ui.ltwEmails);
		item->setData(Qt::UserRole,dialog.tags());
		ui.ltwEmails->addItem(item);
	}
}

void VCardDialog::onEmailDeleteClicked()
{
	QListWidgetItem *item = ui.ltwEmails->takeItem(ui.ltwEmails->currentRow());
	delete item;
}

void VCardDialog::onEmailItemActivated(QListWidgetItem *AItem)
{
	if (FStreamJid && FContactJid)
	{
		static QStringList emailTagList = QStringList() << "HOME" << "WORK" << "INTERNET" << "X400";
		EditItemDialog dialog(AItem->text(),AItem->data(Qt::UserRole).toStringList(),emailTagList,this);
		dialog.setLabelText(tr("EMail:"));
		if (dialog.exec() == QDialog::Accepted)
		{
			AItem->setText(dialog.value());
			AItem->setData(Qt::UserRole,dialog.tags());
		}
	}
}

void VCardDialog::onPhoneAddClicked()
{
	static QStringList phoneTagList = QStringList() << "HOME" << "WORK" << "CELL" << "MODEM";
	EditItemDialog dialog("",QStringList(),phoneTagList,this);
	dialog.setLabelText(tr("Phone:"));
	if (dialog.exec() == QDialog::Accepted && !dialog.value().isEmpty()
	    && ui.ltwPhones->findItems(dialog.value(),Qt::MatchFixedString).isEmpty())
	{
		QListWidgetItem *item = new QListWidgetItem(dialog.value(),ui.ltwPhones);
		item->setData(Qt::UserRole,dialog.tags());
		ui.ltwPhones->addItem(item);
	}
}

void VCardDialog::onPhoneDeleteClicked()
{
	QListWidgetItem *item = ui.ltwPhones->takeItem(ui.ltwPhones->currentRow());
	delete item;
}

void VCardDialog::onPhoneItemActivated(QListWidgetItem *AItem)
{
	if (FStreamJid && FContactJid)
	{
		static QStringList phoneTagList = QStringList() << "HOME" << "WORK" << "CELL" << "MODEM";
		EditItemDialog dialog(AItem->text(),AItem->data(Qt::UserRole).toStringList(),phoneTagList,this);
		dialog.setLabelText(tr("Phone:"));
		if (dialog.exec() == QDialog::Accepted)
		{
			AItem->setText(dialog.value());
			AItem->setData(Qt::UserRole,dialog.tags());
		}
	}
}

void VCardDialog::onDialogButtonClicked(QAbstractButton *AButton)
{
	if (ui.btbButtons->standardButton(AButton) == QDialogButtonBox::Close)
	{
		close();
	}
	else if (ui.btbButtons->standardButton(AButton) == QDialogButtonBox::Save)
	{
		updateVCard();
		if (FVCard->publish(FStreamJid))
		{
			ui.btbButtons->setEnabled(false);
			ui.twtVCard->setEnabled(false);
			FSaveClisked = true;
		}
		else
		{
			QMessageBox::warning(this,tr("vCard error"),tr("Failed to publish vCard"));
		}
	}
	else if (ui.btbButtons->buttonRole(AButton) == QDialogButtonBox::ResetRole)
	{
		if (FVCard->update(FStreamJid))
		{
			ui.btbButtons->setEnabled(false);
			ui.twtVCard->setEnabled(false);
		}
		else
		{
			QMessageBox::warning(this,tr("vCard error"),tr("Failed to update vCard"));
		}
	}
}
