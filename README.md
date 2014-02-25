# darling-dmg

This project allows ordinary users to directly mount OS X disk images under Linux via FUSE.

## Supported file types

* DMG (UDIF) files containing an Apple Disk Image.
* Apple Disk Images containing an HFS+/HFSX file system.
* HFS+/HFSX file systems (incl. file systems embedded within HFS).

As you can see, the list is recursive. That means darling-dmg, can mount DMG files or unpacked DMG files or a single partition carved out of the latter.

Read only access only.

## Usage

    darling-dmg <file-to-mount> <where-to-mount> [FUSE arguments]

### Accessing resource forks

Resource forks are available via xattrs (extended attributes) or prefferably under the name ````/original/filename#..namedfork#rsrc````.

