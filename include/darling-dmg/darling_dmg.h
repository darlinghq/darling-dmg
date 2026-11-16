/*
 * libdarling-dmg: attaching disk images in Darling.
 *
 * This is Darling's own private interface, used by hdiutil and DiskImageMounter. It is not modelled on any Apple
 * framework and is not part of the macOS API.
 *
 * Every function that returns int returns 0 on success or an errno value; none of them sets errno.
 */
#ifndef DARLING_DMG_H
#define DARLING_DMG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct darling_dmg_image darling_dmg_image;

/* Opens a disk image for attaching. ENOENT/EACCES/... for a file that cannot be opened,
 * EFTYPE for a file that is not a disk image this library can read. */
int darling_dmg_open(const char* path, darling_dmg_image** out);
void darling_dmg_close(darling_dmg_image* image);

/* The name of the volume in the image, UTF-8. Valid until darling_dmg_close. */
const char* darling_dmg_volume_name(darling_dmg_image* image);

struct darling_dmg_attach_options {
	const char* mount_root;  /* directory the mount point is made in; NULL means "/Volumes" */
	const char* mount_point; /* exact mount point; NULL means one is made in mount_root from the volume name */
};

/* Mounts the image's volume read-only. The volume stays mounted after the calling process exits.
 * The mount point used is written to mount_point (NUL-terminated); ENAMETOOLONG if it does not fit. */
int darling_dmg_attach(darling_dmg_image* image, const struct darling_dmg_attach_options* options,
	char* mount_point, size_t mount_point_size);

/* Unmounts the volume mounted at mount_point (MNT_FORCE when force is non-zero) and removes the mount point
 * directory. ENOENT if nothing is mounted there; otherwise the error unmount(2) failed with. */
int darling_dmg_detach(const char* mount_point, int force);

#ifdef __cplusplus
}
#endif

#endif
