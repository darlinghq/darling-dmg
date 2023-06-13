# darling-dmg

<a href="http://teamcity.dolezel.info/viewType.html?buildTypeId=DarlingDmg_Build&guest=1">
<img src="http://teamcity.dolezel.info/app/rest/builds/buildType:(id:DarlingDmg_Build)/statusIcon"/>
</a>

This project allows ordinary users to directly mount OS X disk images under Linux via FUSE. darling-dmg is part of Darling - http://www.darlinghq.org

Without darling-dmg, the only way to do this would be to manually extract the DMG file, become root and mount the HFS+ filesystem as root. This is slow, wasteful and may even crash your system. The project's author has seen the Linux HFS+ implementation cause kernel crashes.

## Supported file types

* DMG (UDIF) files containing an Apple Disk Image.
* Apple Disk Images containing an HFS+/HFSX file system.
* HFS+/HFSX file systems (incl. file systems embedded within HFS).

This means, darling-dmg can mount DMG files or unpacked DMG files or a single partition carved out of the latter.

Read only access only.

## Build Requirements

`darling-dmg` requires a C++11-capable compiler, CMake >3.10 and `make` alongside its dependencies. Below are common ways to install them.

On Fedora (and derivatives):

```bash
sudo dnf install fuse-devel bzip2-devel libicu-devel libxml2-devel openssl-devel zlib-devel pkgconf
```

On Debian (and derivatives):

```bash
sudo apt-get install libfuse-dev libbz2-dev libicu-dev libxml2-dev libssl-dev libz-dev pkg-config
```

On Alpine Linux:

```bash
sudo apk add fuse-dev bzip2-dev icu-dev libxml2-dev openssl-dev zlib-dev pkgconf
```

## Usage

```
darling-dmg <file-to-mount> <where-to-mount> [FUSE arguments]
```

### Accessing resource forks

Resource forks are available via xattrs (extended attributes) or preferably under the name ````/original/filename#..namedfork#rsrc````.

### Reusability

Some people have had success with using darling-dmg as a library for their own use.

