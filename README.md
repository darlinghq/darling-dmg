# darling-dmg

This project allows ordinary users to directly mount OS X disk images under Linux via FUSE. darling-dmg is part of Darling - http://www.darlinghq.org

Without darling-dmg, the only way to do this would be to manually extract the DMG file, become root and mount the HFS+ filesystem as root. This is slow, wasteful and may even crash your system. Author has seen the Linux HFS+ implementation cause kernel crashes.

## Supported file types

* DMG (UDIF) files containing an Apple Disk Image.
* Apple Disk Images containing an HFS+/HFSX file system.
* HFS+/HFSX file systems (incl. file systems embedded within HFS).

As you can see, the list is recursive. That means darling-dmg, can mount DMG files or unpacked DMG files or a single partition carved out of the latter.

Read only access only.

## Build Requirements

You need the development packages for following libraries: fuse, icu, openssl, zlib, bzip2.

## Usage

    darling-dmg <file-to-mount> <where-to-mount> [FUSE arguments]

### Accessing resource forks

Resource forks are available via xattrs (extended attributes) or prefferably under the name ````/original/filename#..namedfork#rsrc````.

### Reusability

Some people have had success with using darling-dmg as a library for their own use.

