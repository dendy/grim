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

#include "archive.h"
#include "archive_p.h"

#include "archivemanager.h"




namespace Grim {




/**
 * \class Archive
 *
 * \ingroup archive_module
 *
 * \brief The Archive class represents single archive file.
 *
 * The Archive class is used to mount archive file into Qt file system.
 * Current implementation supports only ZIP archives, based on PKWARE .ZIP File Format Specification Version: 6.3.2
 *
 * Class itself has no methods for accessing archive content, instead standard Qt classes should be used,
 * such as QFile, QDir, QFileInfo and QDirIterator.
 * If you want to access archive content from different API's wrappers for standard Qt classes should be created separately.
 *
 * Archive is opened with open(), after what specified mount point will be able to be opened like normal directory.
 * If mount point was not specified archive file itself will be used as mount point.
 * Note that this hides archive file itself from accessing it as file and turns it into directory.
 * You can switch this behavior at runtime with setTreatAsDir() method.
 *
 * Example:
 *
 * \code
 * Archive archive( "archive.zip" );
 * archive.open( Archive::ReadOnly );
 * ...
 * QStringList files = QDir( "archive.zip" ).entryList(); // lists top level contents of archive
 * ...
 * QFile file( "archive.zip/file.txt" );
 * file.open( QIODevice::ReadOnly );
 * QByteArray fileContents = file.readAll();              // reading contents of file
 * \endcode
 *
 * \b Thread-Safety
 *
 * The Archive class is not thread-safe itself, but it contents are.
 * This means that QFile, QDir, QFileInfo anf QDirIterator instances can be creates at any time in any thread, pointing to
 * contents inside archive. This not blocks Archive's thread itself, nor Archive blocks file instances from different threads.
 * Archive instance can be created, destroyed and updated on the fly, even if file instances from different threads reads
 * contents from archive right now. In this case those instances will receive errors on standard operations, for example if
 * file disappeared from archive.
 *
 * If you want to lock archive contents, preventing from write access to archive file from outside - clear DontLock flag,
 * that can be passed to open() method. This ensures that archive will stay locked until close() method will be called
 * explicitly or due to archive destruction.
 *
 * If you want to observe archive contents on the fly - pass DontLock flag into open() method.
 * After this any external application can change this archive and results can be observed with normal Qt file system classes.
 *
 * Due to non-blocking policy of Archive design it is updated separately in different thread.
 * This means that right after opening it with open() method it returns instantly and turns into Initializing state.
 * If some file will try to open something from this archive while it is in Initializing state - it blocks until archive
 * will not updates self.
 *
 * You can block archive, waiting for this delay manually by passing Block flag into open() method.
 * In this case open() method blocks until initialization step will not be passed.
 */


/**
 * \enum Archive::OpenMode
 *
 * This enum is a collection of bit OR'ed flags that controls how archive should be opened.
 */
/**\var Archive::OpenMode Archive::NotOpen
 * For read-only. Indicates that archive is not opened.
 */
/**\var Archive::OpenMode Archive::ReadOnly
 * Open archive for reading. All contents inside archive will be readable.
 */
/**\var Archive::OpenMode Archive::WriteOnly
 * Open archive for writing. All contents inside archive will be writable.
 */
/**\var Archive::OpenMode Archive::ReadWrite
 * Open archive for both reading and writing. All contents inside archive will be readable and writable.
 */
/**\var Archive::OpenMode Archive::DontLock
 * By default archive is opened in locked mode, which means that it will be not able to change outside of application.
 * Passing DontLock flag to open() changes this behavior, allowing external applications to change archive while it is opened.
 */
/**\var Archive::OpenMode Archive::Block
 * Passing Block flag to open() will block current thread until archive contents will not be parsed and initialized.
 * This allows to omit blocking operations on files later.
 * Normally you should not use this flag.
 * See stateChanged() signal for alternate asynchronous way to know when archive will be initialized.
 */


/**
 * \enum Archive::State
 *
 * This read-only enum tells in which state archive currently exists.
 *
 * \sa stateChanged()
 */
/**\var Archive::State Archive::State_Idle
 * Archive is not opened.
 */
/**\var Archive::State Archive::State_Initializing
 * Archive is opened, but not initialized his contents yet.
 */
/**\var Archive::State Archive::State_Ready
 * Archive is initialized already, which means file operations will not be blocked.
 */


/**
 * \enum Archive::Type
 *
 * This enum specifies archive format.
 */
/**\var Archive::Type Archive::Type_Unknown
 * Unknown archive format.
 */
/**\var Archive::Type Archive::Type_Zip
 * PKWARE ZIP archive format.
 */


/**
 * Constructs archive instance with no file name.
 * \a parent is passed to the QObject constructor.
 * Use setFileName() to specify concrete archive file.
 *
 * \sa setFileName()
 */

Archive::Archive( QObject * parent ) :
	QObject( parent ),
	d_( new ArchivePrivate( this ) )
{
}


/**
 * Constructs archive instance with the give \a fileName.
 * \a parent is passed to the QObject constructor.
 */

Archive::Archive( const QString & fileName, QObject * parent ) :
	QObject( parent ),
	d_( new ArchivePrivate( this ) )
{
	setFileName( fileName );
}


/**
 * Closes archive file and destroys archive instance.
 *
 * \sa close()
 */

Archive::~Archive()
{
	delete d_;
}


/**
 * \fn Archive::stateChanged( int state )
 *
 * This signal is emitted if state of broken flag is changed with the current archive state in \a state variable.
 *
 * \sa state(), isBroken()
 */


/**
 * Returns current archive state.
 *
 * \sa State, stateChanges()
 */

Archive::State Archive::state() const
{
	return d_->state_;
}


/**
 * Returns whether this archive is readable or not.
 * This flag can be changed spontaneously at runtime to \c true or \a false if
 * DoctLock flag was passed to the open().
 *
 * \sa stateChanged()
 */

bool Archive::isBroken() const
{
	return d_->isBroken_;
}


/**
 * Mounts archive into file system.
 *
 * If mount point was specified - it will be used, otherwise archive file itself will be used as mount point,
 * which means that file will turned into directory.
 *
 * If mount point was already used by other archive - opening will fail.
 *
 * \a openMode must contain at least one of ReadOnly or WriteOnly flags, or both.
 *
 * If DontLock flag was specified then archive file will be closed spontaneously at runtime, when all Qt file instances
 * releases files from archive. This allows external application to change the archive. In this case archive contents
 * will be updated automatically.
 *
 * If Block flag was specified than open() call blocks until archive contents will not be updated.
 * Use this to ensure that later file operations will not block later spontaneously.
 *
 * \b Note: Current implementation supports only ReadOnly flag, prohibiting opening archive for write.
 *
 * Call actualMountPoint() to obtain path where archive contents were actually mounted.
 *
 * Returns \c true if mounting was successful, otherwise returns \c false.
 *
 * \sa close(), actualMountPoint()
 */

bool Archive::open( OpenMode openMode )
{
	return d_->open( openMode );
}


/**
 * Closes archive and releases mount point from file system.
 *
 * If Qt file instances from any accessed files inside archive - they will be released instantly with normal error codes.
 * For examples QFile::seek() will always returns false and QFile::read() will always returns -1.
 * Opening new files will fail.
 * Listing directories inside will return zero results.
 * And so on.
 *
 * Spontaneous archive closing while files was opened from different threads is absolutely normal and do not blocks that threads
 * or produces errors.
 *
 * \sa open()
 */

void Archive::close()
{
	return d_->close();
}


/**
 * Returns open mode that was passed to open() method or NotOpen if file is closed.
 *
 * \sa open()
 */

Grim::Archive::OpenMode Archive::openMode() const
{
	return d_->openMode_;
}


/**
 * Returns file name this archive operates on.
 *
 * \sa setFileName()
 */

QString Archive::fileName() const
{
	return d_->fileName_;
}


/**
 * Changes file name this archive will operate on to the given \a fileName.
 *
 * Changing file name while archive is opened is prohibited.
 *
 * If mount point was not specified this file name will be used as mount point, turning archive file into directory.
 *
 * \sa fileName(), setMountPoint()
 */

void Archive::setFileName( const QString & fileName )
{
	d_->setFileName( fileName );
}


/**
 * Returns mount point in the file system this archive will be mounted to or null string if mount point was not specified.
 *
 * \sa setMountPoint()
 */

QString Archive::mountPoint() const
{
	return d_->mountPoint_;
}


/**
 * Set that archive contents will be mounted to file system at \a mountPoint.
 * Pass null string if you want mount point to be where archive file exists itself.
 *
 * Changing mount point while archive is opened id prohibited.
 *
 * \sa mountPoint()
 */

void Archive::setMountPoint( const QString & mountPoint )
{
	d_->setMountPoint( mountPoint );
}


/**
 * Returns path where archive will be actually mounted in file system.
 * This would be either one of mountPoint() or fileName(), depending whether mount point was set
 * explicitly with setMountPoint() ot not.
 *
 * \sa mountPoint(), fileName()
 */

QString Archive::actualMountPoint() const
{
	return d_->actualMountPoint();
}


/**
 * Returns whether mount point directory should be maintained by this Archive instance or not.
 *
 * By default archive mount point treated as directory.
 *
 * \sa setTrearAsDir()
 */

bool Archive::treatAsDir() const
{
	return d_->treatAsDir();
}


/**
 * If \a set is \c true that this archive's mount point will be visible as directory for Qt classes,
 * otherwise it will be visible as file.
 * This has sense only if archive contents are mounted instead archive file itself and allows
 * to switch between file and directory modes while archive is opened.
 *
 * \sa treatAsDir()
 */

void Archive::setTreatAsDir( bool set )
{
	d_->setTreatAsDir( set );
}


/**
 * Returns archive global comment.
*/

QString Archive::globalComment() const
{
	return d_->globalComment();
}




} // namespace Grim
