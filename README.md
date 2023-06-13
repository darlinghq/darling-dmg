# darling-dmg

This project allows ordinary users to directly mount OS X disk images under Linux via FUSE. darling-dmg is part of Darling - http://www.darlinghq.org

Without darling-dmg, the only way to do this would be to manually extract the DMG file, become root and mount the HFS+ filesystem as root. This is slow, wasteful and may even crash your system. The project's author has seen the Linux HFS+ implementation cause kernel crashes.

## Supported file types

* DMG (UDIF) files containing an Apple Disk Image.
* Apple Disk Images containing an HFS+/HFSX file system.
* HFS+/HFSX file systems (incl. file systems embedded within HFS).

This means, darling-dmg can mount DMG files or unpacked DMG files or a single partition carved out of the latter.

Read only access only.

## Build Requirements

| Dependency | Required version     | Notes                              |
|------------|----------------------|------------------------------------|
| GCC/Clang  | >5 (GCC), >3 (Clang) | Compiler with C++11 support        |
| CMake      | 3.10                 | Build system                       |
| pkg-config |                      | Library-agnostic package detection |
| OpenSSL    |                      | Base64 decoding                    |
| Bzip2      |                      | Decompression                      |
| Zlib       |                      | Decompression                      |
| FUSE       | 2.x (not 3.x)        | Userspace filesystem support       |
| libicu     |                      | Unicode support                    |
| libxml2    |                      | XML (property list) parsing        |

`darling-dmg` requires a C++11-capable compiler, CMake >3.10 and `make` alongside the remaining dependencies mentioned above. Below are common ways to install library dependencies.

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

