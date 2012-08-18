/*
* aspellchecker.h
*
* Copyright (C) 2006  Remko Troncon
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* You can also redistribute and/or modify this program under the
* terms of the Psi License, specified in the accompanied COPYING
* file, as published by the Psi Project; either dated January 1st,
* 2005, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*/

#ifndef ASPELLCHECKER_H
#define ASPELLCHECKER_H

#include <QList>
#include <QString>

#include "spellbackend.h"

struct AspellConfig;
struct AspellSpeller;

class ASpellChecker : 
	public SpellBackend
{
public:
	ASpellChecker();
	~ASpellChecker();
	virtual bool available() const;
	virtual bool writable() const;
	virtual QString actuallLang();
	virtual void setLang(const QString &ALang);
	virtual QList<QString> dictionaries();
	virtual void setCustomDictPath(const QString &APath);
	virtual void setPersonalDictPath(const QString &APath);
	virtual bool isCorrect(const QString &AWord);
	virtual bool canAdd(const QString &AWord);
	virtual bool add(const QString &AWord);
	virtual QList<QString> suggestions(const QString &AWord);
private:
	void loadPersonalDict();
	void savePersonalDict(const QString &AWord); 
private:
	QString FActualLang;
	AspellConfig *FConfig;
	AspellSpeller *FSpeller;
	QString FPersonalDictPath; 
};

#endif
