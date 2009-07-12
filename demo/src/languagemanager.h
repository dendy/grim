/******************************************************************************
 *
 * Grim - Game engine library
 * Copyright (C) 2009 Daniel Levin (dendy.ua@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3.0 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The GNU General Public License is contained in the file LICENSE.GPL3 in the
 * packaging of this file. Please review information in this file to ensure
 * the GNU General Public License version 3.0 requirements will be met.
 *
 * Copy of GNU General Public License available at:
 * http://www.gnu.org/copyleft/gpl.html
 *
 *****************************************************************************/

#pragma once

#include <QObject>
#include <QStringList>
#include <QHash>




class QTranslator;




class LanguageInfo
{
public:
	LanguageInfo() :
		isNull( true ), isTranslatorFileNamesUpdated( false )
	{
	}

	bool isNull;

	QString name;

	QString englishName;
	QString nativeName;

	bool isTranslatorFileNamesUpdated;
	QStringList translatorFileNames;
	QList<QTranslator*> translators;
};




class LanguageManager : public QObject
{
public:
	static LanguageManager * instance();

	QStringList names() const;
	LanguageInfo languageInfoForName( const QString & name ) const;

	QString currentName() const;
	void setCurrentName( const QString & name );

private:
	LanguageManager();
	~LanguageManager();

	static void _destroyInstance();

	bool _splitFileName( const QString & fileName, QString * moduleName, QString * suffix ) const;

private:
	QStringList names_;
	QHash<QString,LanguageInfo> languageInfoForName_;

	QString currentName_;
};




inline QStringList LanguageManager::names() const
{ return names_; }

inline LanguageInfo LanguageManager::languageInfoForName( const QString & name ) const
{ return languageInfoForName_.value( name ); }

inline QString LanguageManager::currentName() const
{ return currentName_; }
