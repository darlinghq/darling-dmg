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

/* What the file system said when the last darling_dmg_attach on this image failed to mount it (e.g. a FUSE or
 * fusermount error), UTF-8, possibly several lines; NULL if nothing. Valid until the next attach or close. */
const char* darling_dmg_attach_messages(darling_dmg_image* image);

/* Unmounts the volume mounted at mount_point (MNT_FORCE when force is non-zero) and removes the mount point
 * directory. ENOENT if nothing is mounted there; otherwise the error unmount(2) failed with. */
int darling_dmg_detach(const char* mount_point, int force);

/* Licence agreement ("SLA") an image asks to be accepted before it is attached, in one language. */
struct darling_dmg_license {
	const char* language;      /* BCP 47 tag, e.g. "en-US", "de-DE", "ja-JP" */
	int is_default;            /* the language the image names as its default */
	int is_rtf;                /* text is RTF rather than plain text */
	const char* text;          /* UTF-8; line breaks are '\n' */
	const char* language_name; /* the remaining strings are UTF-8, NULL if the image does not provide them */
	const char* agree;
	const char* disagree;
	const char* print;
	const char* save;
	const char* message;       /* the prompt shown with the buttons */
};

/* Number of languages the image's agreement comes in; 0 if it has none (or one that cannot be read). */
size_t darling_dmg_license_count(darling_dmg_image* image);
/* NULL if index is out of range. Valid until darling_dmg_close. */
const struct darling_dmg_license* darling_dmg_license_at(darling_dmg_image* image, size_t index);
/* The agreement in the first of the preferred languages (BCP 47 tags, '_' accepted for '-') the image has, matched
 * by the full tag first and then by language alone; otherwise the default one. NULL if the image has none. */
const struct darling_dmg_license* darling_dmg_license_for_languages(darling_dmg_image* image,
	const char* const* preferred, size_t count);

#ifdef __cplusplus
}
#endif

#endif
