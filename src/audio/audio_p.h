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

#pragma once

#include <QSharedDataPointer>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QList>
#include <QHash>
#include <QEvent>
#include <QBasicTimer>

#include "audiomath.h"
#include "audiobuffer.h"
#include "audiosource.h"
#include "audiocontext.h"

#if defined GRIM_AUDIO_OPENAL_INCLUDE_DIR_PREFIX_AL
#	include <AL/alc.h>
#	include <AL/al.h>
#elif defined GRIM_AUDIO_OPENAL_INCLUDE_DIR_PREFIX_OpenAL
#	include <OpenAL/alc.h>
#	include <OpenAL/al.h>
#else
#	include <alc.h>
#	include <al.h>
#endif




class QPluginLoader;




namespace Grim {




class AudioManager;
class AudioDevice;
class AudioCaptureDevice;
class AudioContext;
class AudioContextPrivate;
class AudioBuffer;
class AudioFormatPlugin;
class AudioSource;
class AudioListener;
class AudioBufferLoader;
class AudioFormatFile;
class AudioBufferQueueItem;
class AudioBufferContent;
class AudioBufferRequest;




class AudioOpenALVector
{
public:
	inline AudioOpenALVector( const AudioVector & vector )
	{
		data[ 0 ] = vector.x();
		data[ 1 ] = vector.y();
		data[ 2 ] = vector.z();
	}

	inline AudioOpenALVector( const AudioVector & vectorOne, const AudioVector & vectorTwo )
	{
		data[ 0 ] = vectorOne.x();
		data[ 1 ] = vectorOne.y();
		data[ 2 ] = vectorOne.z();
		data[ 3 ] = vectorTwo.x();
		data[ 4 ] = vectorTwo.y();
		data[ 5 ] = vectorTwo.z();
	}

	ALfloat data[ 6 ];
};




class AudioContextLockerData : public QSharedData
{
private:
	inline AudioContextLockerData( QReadWriteLock * mutex ) :
		mutex_( mutex )
	{
		mutex_->lockForWrite();
	}

public:
	inline ~AudioContextLockerData()
	{
		mutex_->unlock();
	}

private:
	QReadWriteLock * mutex_;

	friend class AudioManagerPrivate;
};




class AudioContextLocker
{
public:
	QExplicitlySharedDataPointer<AudioContextLockerData> d;
};




class AudioOpenALBuffer
{
public:
	inline AudioOpenALBuffer() : id( 0 )
	{}

	inline bool isNull() const
	{ return id == 0; }

	ALuint id;
	int channels;
	int bitsPerSample;
	int frequency;
	int size;
	qint64 samples;
	bool isSequential;
	qint64 totalSamples;
	qint64 samplesOffset;
};




class AudioManagerPrivate
{
public:
	static AudioManagerPrivate * sharedManagerPrivate();
	static void destroySharedManager();

	static QList<QByteArray> parseAlcString( const ALCchar * string );

	typedef QList<AudioFormatPlugin*> AudioFormatPluginList;

	AudioManagerPrivate( AudioManager * manager );
	~AudioManagerPrivate();

	QList<QByteArray> availableFileFormats() const;
	QList<QString> availableFileFormatExtensions() const;

	AudioFormatFile * createFormatFile( const QString & fileName, const QByteArray & format );

	AudioDevice * createDevice( const QByteArray & deviceName );
	void removeDevice( AudioDevice * audioDevice );

	AudioCaptureDevice * createCaptureDevice( const QByteArray & captureDeviceName,
		int channelsCount, int bitsPerSample, int frequency, int maxSamples );
	void removeCaptureDevice( AudioCaptureDevice * audioCaptureDevice );

	AudioContextLocker lockAudioContext( AudioContext * audioContext );
	AudioContextLocker lockAudioContextForDevice( AudioDevice * audioDevice );

	void unsetCurrentContextIfCurrent( AudioContext * audioContext );

	bool verifyOpenALBuffer( const AudioBufferContent & content ) const;
	AudioOpenALBuffer createOpenALBuffer( const AudioBufferContent & content ) const;
	void destroyOpenALBuffer( const AudioOpenALBuffer & alBuffer ) const;

private:
	AudioFormatFile * _createFormatFileFromPlugins( const AudioFormatPluginList & plugins,
		const QString & fileName, const QByteArray & format );
	void _addAudioFormatPlugin( AudioFormatPlugin * plugin );

private:
	AudioManager * manager_;

	QList<QPluginLoader*> pluginLoaders_;

	QReadWriteLock fileFormatsMutex_;
	QList<QByteArray> availableFileFormats_;
	QList<QString> availableFileFormatExtensions_;
	QList<AudioFormatPlugin*> audioFormatPlugins_;
	QHash<QByteArray,AudioFormatPluginList> audioFormatPluginsForName_;
	QHash<QString,AudioFormatPluginList> audioFormatPluginsForExtension_;

	// resolved via ALC_ENUMERATION_EXT
	QList<QByteArray> availableDeviceNames_;
	QByteArray defaultDeviceName_;

	QList<QByteArray> availableCaptureDeviceNames_;
	QByteArray defaultCaptureDeviceName_;

	// resolved via ALC_ENUMERATE_ALL_EXT
	QList<QByteArray> availableAllDeviceNames_;
	QByteArray defaultAllDeviceName_;

	QReadWriteLock currentAudioContextsMutex_;
	AudioContext * currentAudioContext_;

	QReadWriteLock audioDevicesMutex_;
	QList<AudioDevice*> audioDevices_;

	QReadWriteLock audioCaptureDevicesMutex_;
	QList<AudioCaptureDevice*> audioCaptureDevices_;

	friend class AudioManager;
};




class AudioDevicePrivate : public QObject
{
	Q_OBJECT

public:
	AudioDevicePrivate( const QByteArray & name, ALCdevice * alcDevice );
	~AudioDevicePrivate();

	AudioContextLocker lock();

	AudioContext * createContext( int frequency, int refreshInterval, int sync, int monoSources, int stereoSources );
	void removeContext( AudioContext * audioContext );

	AudioBuffer createBuffer( const QString & fileName, const QByteArray & format, AudioBuffer::Policy policy );
	AudioBuffer createBuffer( const AudioBufferData & audioBufferData, AudioContext * audioContext );
	void removeBuffer( const AudioBuffer & audioBuffer );

	void loadBuffer( AudioBuffer audioBuffer, AudioBufferRequest * audioBufferRequest, bool isPrioritized );
	void increaseLoadPriority( int requestId );
	void cancelLoadRequest( int requestId );

private slots:
	void _requestFinished( int requestId, void * resultp );

private:
	void _bufferFinishedForSource( AudioSource * audioSource );

private:
	AudioDevice * audioDevice_;

	QByteArray name_;
	ALCdevice * alcDevice_;

	QList<QByteArray> extensionNames_;
	ALCint versionMajor_;
	ALCint versionMinor_;

	QReadWriteLock audioContextsMutex_;
	QList<AudioContext*> audioContexts_;

	QReadWriteLock audioBuffersMutex_;
	QList<AudioBuffer> audioBuffers_;

	AudioBufferLoader * bufferLoader_;

	QReadWriteLock audioBufferRequestsMutex_;
	struct RequestItem
	{
		RequestItem() :
			audioBuffer( 0 ), request( 0 )
		{}

		RequestItem( const AudioBuffer & _audioBuffer, AudioBufferRequest * _request ) :
			audioBuffer( _audioBuffer ), request( _request )
		{}

		AudioBuffer audioBuffer;
		AudioBufferRequest * request;
	};
	QHash<int,RequestItem> requestItemForId_;

	friend class AudioDevice;
	friend class AudioContextPrivate;
	friend class AudioManagerPrivate;
};




class AudioCaptureDevicePrivate : public QObject
{
	Q_OBJECT

public:
	AudioCaptureDevicePrivate( const QByteArray & name, ALCdevice * alcCaptureDevice,
		ALCenum alFormat, int channelsCount, int frequency, int bitsPerSample, int maxSamples );
	~AudioCaptureDevicePrivate();

	void setCaptureInterval( int interval );

	AudioBufferData captureBufferData();

	void start();
	void stop();

protected:
	void timerEvent( QTimerEvent * e );

private:
	AudioBufferData _processSamples();

private:
	AudioCaptureDevice * audioCaptureDevice_;

	QByteArray name_;
	ALCdevice * alcCaptureDevice_;

	ALCenum alFormat_;
	int channelsCount_;
	int frequency_;
	int bitsPerSample_;
	int maxSamples_;
	int bytesPerSample_;

	int captureInterval_;
	QBasicTimer timer_;

	friend class AudioCaptureDevice;
};




class AudioContextPrivate : public QObject
{
	Q_OBJECT

public:
	enum EventType
	{
		EventType_FinishedAudioSources = QEvent::User + 1
	};

	AudioContextPrivate( AudioDevice * audioDevice, ALCcontext * alcContext );
	~AudioContextPrivate();

	AudioContextLocker lock();

	void setProcessing( bool set );

	AudioContext::DistanceModel distanceModel() const;
	void setDistanceModel( AudioContext::DistanceModel distanceModel );

	float dopplerFactor() const;
	void setDopplerFactor( float factor );

	float speedOfSound() const;
	void setSpeedOfSound( float speed );

	AudioListener * defaultListener() const;
	AudioListener * currentListener() const;
	AudioListener * createListener();

	AudioSource * createSource();

	void addSourceForFinishedBuffer( AudioSource * audioSource );
	void removeSourceForFinishedBuffer( AudioSource * audioSource );

	void addActiveSource( AudioSource * audioSource );
	void removeActiveSource( AudioSource * audioSource );

protected:
	bool event( QEvent * e );
	void timerEvent( QTimerEvent * e );

private:
	void _updateActiveAudioSourcesTimer();
	void _processActiveAudioSources();

private:
	AudioContext * audioContext_;

	AudioDevice * audioDevice_;
	ALCcontext * alcContext_;

	bool isProcessing_;

	AudioContext::DistanceModel distanceModel_;
	float dopplerFactor_;
	float speedOfSound_;

	AudioListener * defaultAudioListener_;
	AudioListener * currentAudioListener_;
	QList<AudioListener*> audioListeners_;

	QList<AudioSource*> audioSources_;

	QReadWriteLock finishedAudioSourcesMutex_;
	QList<AudioSource*> finishedAudioSources_;
	bool finishedAudioSourcesPosted_;

	// for checking from AudioListener and AudioSource destructors to
	// omit twice removing from lists
	AudioSource * currentDestructingAudioSource_;
	AudioListener * currentDestructingAudioListener_;

	// currently processing audio sources
	QBasicTimer activeAudioSourcesTimer_;
	QList<AudioSource*> activeAudioSources_;

	friend class AudioDevicePrivate;
	friend class AudioContext;
	friend class AudioManagerPrivate;
	friend class AudioListenerPrivate;
	friend class AudioBufferPrivate;
	friend class AudioSourcePrivate;
};




class AudioBufferContent
{
public:
	int channels;
	int bitsPerSample;
	int frequency;
	bool isSequential;
	qint64 totalSamples;
	qint64 samplesOffset;
	qint64 samples;
	QByteArray data;
};




class AudioBufferRequest
{
public:
	AudioBufferRequest() :
		requestId( 0 ), isActive( false ), hasError( false ), isProcessed( false ),
		file( 0 ), sampleOffset( -1 ), sampleCount( -1 )
	{}

	int requestId;
	bool isActive;
	bool hasError;
	bool isProcessed;

	AudioBufferContent content;
	AudioOpenALBuffer alBuffer;
	AudioFormatFile * file;
	qint64 sampleOffset;
	qint64 sampleCount;
};




class AudioBufferStatic
{
public:
	AudioBufferRequest request;

	QList<AudioSource*> audioSources;
};




class AudioBufferQueueItem
{
public:
	AudioBufferRequest request;

	AudioSource * audioSource;
};




class AudioBufferQueue
{
public:
	QList<AudioBufferQueueItem> items;
};




class AudioBufferPrivate : public QSharedData
{
public:
	AudioBufferPrivate( AudioDevice * audioDevice,
		const QString & fileName, const QByteArray & format, AudioBuffer::Policy policy );
	AudioBufferPrivate( AudioDevice * audioDevice,
		const AudioOpenALBuffer & alBuffer );
	~AudioBufferPrivate();

	void tryRemoveSelfFromDevice();

	bool isStreaming() const;

	// called from AudioSource to destroy file and request
	void clearQueueItemForSource( AudioSource * audioSource, bool deleteFile );

	AudioBufferRequest * requestForSource( AudioSource * audioSource );
	AudioSource * sourceForRequestId( int requestId );

	AudioBufferStatic * toStatic() const;
	AudioBufferQueue * toQueue() const;

	void loadSelf( AudioBufferRequest * request, bool isPrioritized );

	void attachSource( AudioSource * audioSource );
	void detachSource( AudioSource * audioSource );

private:
	void _init();
	void _clearStaticData();
	void _clearQueueItem( AudioBufferQueueItem * queueItem, bool deleteFile );

private:
	AudioDevice * audioDevice_;
	QString fileName_;
	QByteArray format_;
	AudioBuffer::Policy policy_;

	bool removingFromAudioDeviceFlag_;

	QReadWriteLock mutex_;

	void * data_;

	friend class AudioBuffer;
	friend class AudioDevicePrivate;
	friend class AudioSourcePrivate;
	friend class AudioContextPrivate;
};




class AudioListenerPrivate
{
public:
	AudioListenerPrivate( AudioContext * audioContext );
	~AudioListenerPrivate();

	bool isCurrent() const;
	void setCurrent();

	float gain() const;
	void setGain( float gain );

	AudioVector position() const;
	void setPosition( const AudioVector & position );

	AudioVector velocity() const;
	void setVelocity( const AudioVector & velocity );

	AudioVector orientationAt() const;
	AudioVector orientationUp() const;
	void setOrientation( const AudioVector & at, const AudioVector & up );

private:
	void _applyGain();
	void _applyPosition();
	void _applyVelocity();
	void _applyOrientation();
	void _applyAll();

private:
	AudioListener * audioListener_;
	AudioContext * audioContext_;

	float gain_;
	AudioVector position_;
	AudioVector velocity_;
	AudioVector orientationAt_;
	AudioVector orientationUp_;

	friend class AudioListener;
};




class AudioSourcePrivate
{
public:
	AudioSourcePrivate( AudioContext * audioContext );
	~AudioSourcePrivate();

	AudioSource::State state() const;

	AudioBuffer buffer() const;
	void setBuffer( const AudioBuffer & buffer );

	bool isSequential() const;

	int channelsCount() const;
	int bitsPerSample() const;
	float frequency() const;

	qint64 totalSamples() const;

	qint64 currentSampleOffset() const;
	void setCurrentSampleOffset( qint64 sampleOffset );

	bool isLooping() const;
	void setLooping( bool set );

	float gain() const;
	void setGain( float gain );

	float minGain() const;
	void setMinGain( float gain );

	float maxGain() const;
	void setMaxGain( float gain );

	AudioVector position() const;
	void setPosition( const AudioVector & position );

	AudioVector velocity() const;
	void setVelocity( const AudioVector & velocity );

	bool isRelativeToListener() const;
	void setRelativeToListener( bool set );

	float pitch() const;
	void setPitch( float pitch );

	AudioVector direction() const;
	void setDirection( const AudioVector & direction );

	float innerConeAngle() const;
	void setInnerConeAngle( float angle );

	float outerConeAngle() const;
	void setOuterConeAngle( float angle );

	float outerConeGain() const;
	void setOuterConeGain( float gain );

	float referenceDistance() const;
	void setReferenceDistance( float distance );

	float rolloffFactor() const;
	void setRolloffFactor( float factor );

	float maxDistance() const;
	void setMaxDistance( float distance );

	void play();
	void pause();
	void stop();

	void setStaticOpenALBuffer( AudioBufferRequest * request );
	void setQueueOpenALBuffer( AudioBufferRequest * request );

	void stopSelf();
	void deinitializeSelf();
	void processSelf();

private:
	void _applyLooping();
	void _applyGain();
	void _applyMinGain();
	void _applyMaxGain();
	void _applyPosition();
	void _applyVelocity();
	void _applyRelativeToListener();
	void _applyPitch();
	void _applyDirection();
	void _applyInnerConeAngle();
	void _applyOuterConeAngle();
	void _applyOuterConeGain();
	void _applyReferenceDistance();
	void _applyRolloffFactor();
	void _applyMaxDistance();
	void _applyAll();

	void _setInitialized( bool set );
	void _setState( AudioSource::State state );
	void _setActive( bool set );

	bool _isEmpty() const;
	bool _isOffsetInBounds( qint64 offset, bool includingCurrentRequest ) const;
	qint64 _calculateCurrentSampleOffset() const;
	bool _isAtEnd( AudioBufferRequest * request ) const;

	void _requestMore( AudioBufferRequest * request, bool requestZeroSamples = false );

	void _seekToDesiredSampleOffset();

	void _initializeStatic();
	void _deinitializeStatic();
	void _playStatic();
	void _pauseStatic();
	void _stopStatic();
	void _seekStatic();

	void _initializeQueue( AudioBufferRequest * request );
	void _deinitializeQueue();
	void _playQueue();
	void _pauseQueue();
	void _unqueueQueue();
	void _stopQueue();
	void _clearQueue();
	void _seekQueue();

private:
	AudioSource * audioSource_;
	AudioContext * audioContext_;

	bool areSignalsBlocked_;

	bool inDestructor_;

	ALuint alSourceId_;

	bool isLooping_;

	float gain_;
	float minGain_;
	float maxGain_;
	AudioVector position_;
	AudioVector velocity_;
	bool isRelativeToListener_;
	float pitch_;
	AudioVector direction_;
	float innerConeAngle_;
	float outerConeAngle_;
	float outerConeGain_;
	float referenceDistance_;
	float rolloffFactor_;
	float maxDistance_;

	AudioSource::State state_;
	AudioBuffer audioBuffer_;
	bool isActive_;

	bool isInitialized_;
	bool isSequential_;
	int channelsCount_;
	int bitsPerSample_;
	float frequency_;
	qint64 totalSamples_;
	qint64 firstSampleOffset_;
	qint64 lastSampleOffset_;
	qint64 currentSampleOffset_;
	qint64 desiredSampleOffset_;
	QList<AudioOpenALBuffer> alBuffers_;

	friend class AudioSource;
	friend class AudioDevicePrivate;
	friend class AudioContextPrivate;
	friend class AudioBufferPrivate;
};




inline AudioContext::DistanceModel AudioContextPrivate::distanceModel() const
{ return distanceModel_; }

inline float AudioContextPrivate::dopplerFactor() const
{ return dopplerFactor_; }

inline float AudioContextPrivate::speedOfSound() const
{ return speedOfSound_; }




inline bool AudioBufferPrivate::isStreaming() const
{ return policy_ & AudioBuffer::Streaming; }

inline AudioBufferStatic * AudioBufferPrivate::toStatic() const
{ return static_cast<AudioBufferStatic*>( data_ ); }

inline AudioBufferQueue * AudioBufferPrivate::toQueue() const
{ return static_cast<AudioBufferQueue*>( data_ ); }




inline float AudioListenerPrivate::gain() const
{ return gain_; }

inline AudioVector AudioListenerPrivate::position() const
{ return position_; }

inline AudioVector AudioListenerPrivate::velocity() const
{ return velocity_; }

inline AudioVector AudioListenerPrivate::orientationAt() const
{ return orientationAt_; }

inline AudioVector AudioListenerPrivate::orientationUp() const
{ return orientationUp_; }




inline AudioSource::State AudioSourcePrivate::state() const
{ return state_; }

inline bool AudioSourcePrivate::isSequential() const
{ return isSequential_; }

inline bool AudioSourcePrivate::isLooping() const
{ return isLooping_; }

inline float AudioSourcePrivate::gain() const
{ return gain_; }

inline float AudioSourcePrivate::minGain() const
{ return minGain_; }

inline float AudioSourcePrivate::maxGain() const
{ return maxGain_; }

inline AudioVector AudioSourcePrivate::position() const
{ return position_; }

inline AudioVector AudioSourcePrivate::velocity() const
{ return velocity_; }

inline bool AudioSourcePrivate::isRelativeToListener() const
{ return isRelativeToListener_; }

inline float AudioSourcePrivate::pitch() const
{ return pitch_; }

inline AudioVector AudioSourcePrivate::direction() const
{ return direction_; }

inline float AudioSourcePrivate::innerConeAngle() const
{ return innerConeAngle_; }

inline float AudioSourcePrivate::outerConeAngle() const
{ return outerConeAngle_; }

inline float AudioSourcePrivate::outerConeGain() const
{ return outerConeGain_; }

inline float AudioSourcePrivate::referenceDistance() const
{ return referenceDistance_; }

inline float AudioSourcePrivate::rolloffFactor() const
{ return rolloffFactor_; }

inline float AudioSourcePrivate::maxDistance() const
{ return maxDistance_; }

inline bool AudioSourcePrivate::_isEmpty() const
{ return totalSamples_ == 0; }




} // namespace Grim
