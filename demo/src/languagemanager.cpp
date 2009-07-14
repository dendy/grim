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
 * The GNU General Public License is contained in the file COPYING in the
 * packaging of this file. Please review information in this file to ensure
 * the GNU General Public License version 3.0 requirements will be met.
 *
 * Copy of GNU General Public License available at:
 * http://www.gnu.org/copyleft/gpl.html
 *
 *****************************************************************************/

#include "languagemanager.h"

#include <QTranslator>
#include <QCoreApplication>
#include <QDir>
#include <QLocale>




static const char * const LanguageNameInEnglish = QT_TRANSLATE_NOOP( "LanguageManager", "Language Name In English" );
static const char * const LanguageNameInNative  = QT_TRANSLATE_NOOP( "LanguageManager", "Language Name In Native" );

static const QString DefaultLanguageName = QLatin1String( "en" );

static const QString QmDirPath = QLatin1String( ":/lang" );
static const QString QmInfoPrefix = QLatin1String( "info" );
static const QString QmTemplate = QLatin1String( "%1_*.qm" );
static const QString QmLanguageTemplate = QLatin1String( "*_%1.qm" );




static LanguageManager * _instance = 0;




LanguageManager * LanguageManager::instance()
{
	if ( !_instance )
	{
		_instance = new LanguageManager;
		qAddPostRoutine( LanguageManager::_destroyInstance );
	}

	return _instance;
}


void LanguageManager::_destroyInstance()
{
	Q_ASSERT( _instance );
	delete _instance;
	_instance = 0;
}





LanguageManager::LanguageManager()
{
	// lookup qm files under the :/lang dir

	QDir langDir( QmDirPath );
	for ( QListIterator<QFileInfo> it( langDir.entryInfoList(
		QStringList() << QmTemplate.arg( QmInfoPrefix ), QDir::Files | QDir::CaseSensitive ) ); it.hasNext(); )
	{
		const QFileInfo qmFileInfo = it.next();

		QString moduleName;
		QString suffix;
		if ( !_splitFileName( qmFileInfo.completeBaseName(), &moduleName, &suffix ) )
			continue;

		Q_ASSERT( moduleName == QmInfoPrefix );
		Q_ASSERT( !languageInfoForName_.contains( suffix ) );

		QTranslator * infoTranslator = new QTranslator( this );
		if ( !infoTranslator->load( qmFileInfo.filePath() ) )
		{
			delete infoTranslator;
			continue;
		}

		LanguageInfo languageInfo;
		languageInfo.isNull = false;
		languageInfo.name = suffix;
		languageInfo.englishName = infoTranslator->translate( "LanguageManager", LanguageNameInEnglish );
		languageInfo.nativeName = infoTranslator->translate( "LanguageManager", LanguageNameInNative );

		languageInfoForName_[ languageInfo.name ] = languageInfo;
		names_ << languageInfo.name;
	}

	QString defaultName;

	const QLocale currentLocale;
	if ( languageInfoForName_.contains( currentLocale.name() ) )
	{
		defaultName = currentLocale.name();
	}
	else
	{
		const QString localeLangName = currentLocale.name().left( currentLocale.name().indexOf( QLatin1Char( '_' ) ) );
		if ( languageInfoForName_.contains( localeLangName ) )
		{
			defaultName = localeLangName;
		}
		else
		{
			if ( languageInfoForName_.contains( DefaultLanguageName ) )
				defaultName = DefaultLanguageName;
		}
	}

	if ( !defaultName.isNull() )
		setCurrentName( defaultName );
}


LanguageManager::~LanguageManager()
{
	setCurrentName( QString() );

	languageInfoForName_.clear();
	names_.clear();
}


bool LanguageManager::_splitFileName( const QString & fileName, QString * moduleName, QString * suffix ) const
{
	const int slashPos = fileName.indexOf( QLatin1Char( '_' ) );

	if ( slashPos == -1 || slashPos == 0 || slashPos == fileName.length() - 1 )
		return false;

	*moduleName = fileName.left( slashPos );
	*suffix = fileName.mid( slashPos + 1 );

	return true;
}


void LanguageManager::setCurrentName( const QString & name )
{
	if ( name == currentName_ )
		return;

	if ( !currentName_.isNull() )
	{
		LanguageInfo & languageInfo = languageInfoForName_[ currentName_ ];

		for ( QListIterator<QTranslator*> it( languageInfo.translators ); it.hasNext(); )
			QCoreApplication::instance()->removeTranslator( it.next() );

		qDeleteAll( languageInfo.translators );
		languageInfo.translators.clear();
	}

	currentName_ = name;

	if ( !currentName_.isNull() )
	{
		LanguageInfo & languageInfo = languageInfoForName_[ currentName_ ];

		if ( !languageInfo.isTranslatorFileNamesUpdated )
		{
			languageInfo.isTranslatorFileNamesUpdated = true;

			QDir langDir( QmDirPath );
			for ( QListIterator<QFileInfo> it( langDir.entryInfoList(
				QStringList() << QmLanguageTemplate.arg( currentName_ ), QDir::Files | QDir::CaseSensitive ) ); it.hasNext(); )
			{
				const QFileInfo qmFileInfo = it.next();

				QString moduleName;
				QString suffix;
				if ( !_splitFileName( qmFileInfo.completeBaseName(), &moduleName, &suffix ) )
					continue;

				Q_ASSERT( suffix == currentName_ );

				languageInfo.translatorFileNames << qmFileInfo.filePath();
			}
		}

		for ( QStringListIterator fileNameIt( languageInfo.translatorFileNames ); fileNameIt.hasNext(); )
		{
			const QString & fileName = fileNameIt.next();

			QTranslator * moduleTranslator = new QTranslator( this );
			if ( !moduleTranslator->load( fileName ) )
			{
				delete moduleTranslator;
				continue;
			}

			languageInfo.translators << moduleTranslator;

			QCoreApplication::instance()->installTranslator( moduleTranslator );
		}
	}
}
