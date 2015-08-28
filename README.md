MegaFuse
========

This is a linux client for the MEGA cloud storage provider.
It is based on FUSE and it allows to mount the remote cloud drive on the local filesystem.
Once mounted, all linux program will see the cloud drive as a normal folder.

This software is based on FUSE.

The files are downloaded on the fly when requested and then cached to speedup processing.
The downloader will assign a higher priority to the requested chunk, and prefetch the remaining data.
This allows also fast streaming of video files without prior encoding.

To compile on OS X, you need additional packages, I suggest using [Homebrew](http://brew.sh)
After setting up Homebrew, install additional package by the following command:

	brew install cryptopp curl berkeley-db freeimage readline

then install OSXFuse from [here](http://osxfuse.github.io)
OSXFuse need further configuration:
Modify a value in both /usr/local/include/fuse/fuse.h & /usr/local/include/osxfuse/fuse/fuse.h:
#define FUSE_USE_VERSION 21 > 26 (change from 21 to 26)

to compile:

	make

it will create an executable file called MegaFuse.
If you're lazy, here is my [compiled binary](https://mega.nz/#!gEwgiYiJ!oybWCZxyGG24dn2Ri3fkcy7q675HXml0xJzNJKn66D8)

Please edit your config file "megafuse.conf" before running, you have to change at least your username and password.
The mountpoint must be an empty directory.
Optionally you can add this tool to /etc/fstab but this is untested,yet.
to run,just

	./MegaFuse

you can pass additional options to the fuse module via the command line option -f. example:
	
	./MegaFuse -f -o allow_other -o uid=1000
	
you can specify the location of the conf file with the command line option -c, by default the program will search the file "megafuse.conf" in the current path

	./MegaFuse -c /home/user/megafuse.conf
	
for the full list of options, launch the program with the option -h

after an abnormal termination you might need to clear the mountpoint:
	
	$ fusermount -u $MOUNTPOINT
	or # umount $MOUNTPOINT

This project is forked from [here](https://github.com/Saoneth/MegaFuse), which is forked from the [original author](https://github.com/matteoserva/MegaFuse)
I modified for OS X usage and add cache folder configuration.
The original author is currently accepting donations via paypal at the address of his main project

	http://ygopro.it/web/modules.php?name=Donations&op=make

FAQ
========
	Q: Is this a sync application?
	A: No.
	Traditionally on linux the filesystem and the sync tool are separate programs.
	MegaFuse allows you to access the remote filesystem,
	then you can use most of the sync apps that are already available on your linux distribution to sync any folder you want.
-
	Q: Why there is no GUI?
	A: Operations performed through mega are done synchronously.
	It's the operating system itself that alerts you about the result of an operation.
	For example, if you copy a file to the MEGA folder, the copy operation completes when the file has been successfully uploaded.
   	Of course a GUI can be added but it would be an additional application written on top of MegaFuse.
-
	Q: Why it is so fast?
	A: A cache is kept to speed up operations.
	Files are split in chunks during the download, reads can be completed as soon as enough data is available.
-
	Q: How do I open a media file for streaming? Can I jump to a specific point of the file?
	A: The downloader is smart enough to assign a higher priority to the requested portion of the file.
   	Just open the audio/video file with your favourite player. I suggest VLC.
   	You can do all the operations supported by your player as if the file was saved on your hard drive.
-
	Q: How do I access a shared file from another account?
	A: Import the file into your account, then it will be available from MegaFuse
