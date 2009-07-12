
This application demonstrates abilites of the Grim Audio and Archive modules.

It allows to mount archives into virtual file system and gives ability to any Qt class
to work with archive as with normal directory unless it uses standard Qt file system classes:
QFile, QDir, QFileInfo and QDirIterator.

By default archives are mounted into temporary directory with the name similar to target archive file name.
Archives can be mounted whereever you want, for example into: zip:/myarchive.
Temporary directory is used just to make QFileSystemModel happy.
QFileSystemModel provider user friendly frontend for sirfing over file systems.
And unfortunately currently it works only with existed normal file systems.


To mount archive right-click on it to popup menu and select "Mount".
It will appear at side bar to the left of file system view.
Now you can sirf your archive as normal directory.

To unmount right-click either on side bar's entry or archive or directory where archive is mounted
and select "Unmount".


At startup default audio device will be created.
Under Linux this will be "ALSA Software" is it present.
You can safely switch audio device from the list of available devices.
All opened audio tracks will be recreated for the selected audio device.


Right-clicking on audio file with appropriate extension popups menu that allows you to create player.
This also creates entry for the track in the side bar to the left of file system view, under the mounted archives.
Then you can play your track with audio widget, embedded into main window.
Note that if have selected "Separate Windows" under the "View" menu - separate player widgets will be
created for each loaded track.
Regardless of layout mode all tracks can be played at once.

Double-clicking on audio track in file system or in side bar raises player for that track in the "Separate Windows" layout mode
or makes this track current into single embedded player inside main window in the "Single View" layout mode.


The cool thing you can test is playing audio tracks right from compressed archives!
Go on, pack something into ZIP and mount it.
This not uses hacks like creating temporary file where audio will be extracked first.
Audio module knows nothing about archives - it works with the audio track file inside archive like with all other files.
Note that if archive contents are compressing - you usually can't change playback position because file stream is sequential.
If archive contents are not compressed - you can change playback position like as for common random-access files from file system.


One more cool thing - is "Archive test". You will find it under the "Windows" menu.
It creates the specified number of separate threads and starts reading random files from the mounted archives.
Note that mounted archive file system is completely thread-safe.
You can safely unmount it while threads are reading files from it.


You can even play any number of audio tracks from the mounted archive while separate theads aggresively reading files
from the same archive!


Language switching is there for demonstration only.
