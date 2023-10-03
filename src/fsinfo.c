/* fsinfo.c - Get file system information */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2023, L. Abramovich <leo.clifm@outlook.com>
 * All rights reserved.

 * CliFM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CliFM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
*/

#include "helpers.h"

#if defined(LINUX_FSINFO)
# include <stdio.h> /* fclose(3), fgets(3) */
# include <string.h> /* strnlen(3), strncmp(3) */
# include <mntent.h> /* xxxmntent(), used by get_remote_fs_name() */
# include <sys/statfs.h> /* statfs(2) */
# include <sys/sysmacros.h> /* major() and minor(), used by get_dev_name() */
# include "linuxfs.h" /* FS_MAGIC macros for file system types */
# include "aux.h" /* open_fread() */
#elif defined(HAVE_STATFS)
# include <sys/mount.h> /* statfs(2) */
#endif /* __linux__ */

#if defined(LINUX_FSINFO)
/* Given an ext file system, tell whether it is version 2, 3, or 4.
 * Returns a pointer to a constant string with the proper name. If none is
 * found a generic "ext2/3/4" is returned.
 *
 * This function just checks information gathered at startup, which is way
 * faster than performing the whole thing each time it is needed. However,
 * File systems mounted in the current session won't be checked here. */
static char *
get_ext_fs_type(const char *file)
{
	char *type = "ext2/3/4";

	if (!ext_mnt || !file)
		return type;

	size_t mnt_longest = 0;
	int i, index = -1;
	for (i = 0; ext_mnt[i].mnt_point; i++) {
		char *ptr = strstr(file, ext_mnt[i].mnt_point);
		if (!ptr || ptr != file)
			continue;

		size_t l = strlen(ext_mnt[i].mnt_point);
		if (l > mnt_longest) {
			mnt_longest = l;
			index = i;
		}
	}

	if (index == -1)
		return type;

	switch (ext_mnt[index].type) {
	case EXT2_FSTYPE: type = "ext2"; break;
	case EXT3_FSTYPE: type = "ext3"; break;
	case EXT4_FSTYPE: type = "ext4"; break;
	default: type = "ext?"; break;
	}

	return type;
}

/* Return a pointer to a constant string with the name of the file system
 * to which the file FILE belongs. REMOTE is set to one if the corresponding
 * file system is a remote one. */
char *
get_fs_type_name(const char *file, int *remote)
{
	struct statfs a;
	if (statfs(file, &a) == -1)
		return "?";

	switch (a.f_type) {
	case T_AAFS_MAGIC: return "aafs";
	case T_ACFS_MAGIC: *remote = 1; return "acfs";
	case T_ADFS_MAGIC: return "adfs";
	case T_AFFS_MAGIC: return "affs";
	case T_AFS_FS_MAGIC: *remote = 1; return "k-afs";
	case T_AFS_MAGIC: *remote = 1; return "afs";
	case T_ANON_INODE_FS_MAGIC: return "anon-inode-fs";
	case T_AUFS_MAGIC: *remote = 1; return "aufs";
	case T_AUTOFS_MAGIC: return "autofs";
	case T_BALLONFS_MAGIC: return "ballon-kvm-fs";
	case T_BEFS_MAGIC: return "befs";
	case T_BDEVFS_MAGIC: return "bdevfs";
	case T_BFS_MAGIC: return "bfs";
	case T_BINDERFS_MAGIC: return "binderfs";
	case T_BINFMTFS_MAGIC: return "binfmt-misc";
	case T_BPF_FS_MAGIC: return "bps-fs";
	case T_BTRFS_MAGIC: return "btrfs";
	case T_BTRFS_TEST_MAGIC: return "btrfs-test";
	case T_CEPH_MAGIC: *remote = 1; return "ceph";
	case T_CGROUP_MAGIC: return "cgroupfs";
	case T_CGROUP2_MAGIC: return "cgroup2fs";
	case T_CIFS_MAGIC: *remote = 1; return "cifs";
	case T_CODA_MAGIC: *remote = 1; return "coda";
	case T_COH_MAGIC: return "coh";
	case T_CONFIGFS_MAGIC: return "configfs";
	case T_CRAMFS_MAGIC: return "cramfs";
	case T_CRAMFS_MAGIC_WEND: return "cramfs-wend";
	case T_DAXFS_MAGIC: return "daxfs";
	case T_DEBUGFS_MAGIC: return "debugfs";
	case T_DEVFS_MAGIC: return "devfs"; // Linux 2.6.17 and earlier
	case T_DEVMEM_MAGIC: return "devmem";
	case T_DEVPTS_MAGIC: return "devpts";
	case T_DMA_BUF_MAGIC: return "dma-buf-fs";
	case T_ECRYPTFS_MAGIC: return "ecryptfs";
	case T_EFIVARFS_MAGIC: return "efivarfs";
	case T_EFS_MAGIC: return "efs";
	case T_EROFS_MAGIC_V1: return "erofs";
	case T_EXFAT_MAGIC: return "exfat";
	case T_EXT_MAGIC: return "ext"; /* Linux 2.0 and earlier */
	case T_EXT2_OLD_MAGIC: return "ext2";
/*	case T_EXT2_MAGIC: // ext2/3/4 have the same magic number
	case T_EXT3_MAGIC: */
	case T_EXT4_MAGIC: return get_ext_fs_type(file);
	case T_F2FS_MAGIC: return "f2fs";
	case T_FAT_MAGIC: return "fat";
	case T_FHGFS_MAGIC: *remote = 1; return "fhgfs";
	case T_FUSE_MAGIC: *remote = 1; return "fuseblk";
	case T_FUSECTL_MAGIC: *remote = 1; return "fusectl";
	case T_FUTEXFS_MAGIC: return "futexfs";
	case T_GFS2_MAGIC: *remote = 1; return "gfs/gfs2";
	case T_GPFS_MAGIC: *remote = 1; return "gpfs";
	case T_HFS_MAGIC: return "hfs";
	case T_HFS_PLUS_MAGIC: return "hfs+";
	case T_HFSX_MAGIC: return "hfsx";
	case T_HOSTFS_MAGIC: return "hostfs";
	case T_HPFS_MAGIC: return "hpfs";
	case T_HUGETLBFS_MAGIC: return "hugetlbfs";
	case T_IBRIX_MAGIC: *remote = 1; return "ibrix";
	case T_INOTIFYFS_MAGIC: return "inotifyfs";
	case T_ISOFS_MAGIC: return "isofs";
	case T_ISOFS_R_WIN_MAGIC: return "isofs";
	case T_ISOFS_WIN_MAGIC: return "isofs";
	case T_JFFS_MAGIC: return "jffs";
	case T_JFFS2_MAGIC: return "jffs2";
	case T_JFS_MAGIC: return "jfs";
	case T_LOGFS_MAGIC: return "logfs";
	case T_LUSTRE_MAGIC: *remote = 1; return "lustre";
	case T_M1FS_MAGIC: return "m1fs";
	case T_MINIX_MAGIC: return "minix";
	case T_MINIX_MAGIC2: return "minix (30 char.)";
	case T_MINIX2_MAGIC: return "minix v2";
	case T_MINIX2_MAGIC2: return "minix v2 (30 char.)";
	case T_MINIX3_MAGIC: return "minix3";
	case T_MQUEUE_MAGIC: return "mqueue";
	case T_MSDOS_MAGIC: return "msdos";
	case T_MTD_INODE_FS_MAGIC: return "inodefs";
	case T_NCP_MAGIC: *remote = 1; return "novell";
	case T_NFS_MAGIC: *remote = 1; return "nfs";
	case T_NFSD_MAGIC: *remote = 1; return "nfsd";
	case T_NILFS_MAGIC: return "nilfs";
	case T_NSFS_MAGIC: return "nsfs";
	case T_NTFS_MAGIC: return "ntfs";
	case T_OCFS2_MAGIC: *remote = 1; return "ocfs2";
	case T_OPENPROM_MAGIC: return "openprom";
	case T_OVERLAYFS_MAGIC: *remote = 1; return "overlayfs";
	case T_PANFS_MAGIC: *remote = 1; return "panfs";
	case T_PIPEFS_MAGIC: *remote = 1; return "pipefs";
	case T_PPC_CMM_FS_MAGIC: return "ppc-cmm-fs";
	case T_PRL_FS_MAGIC: *remote = 1; return "prl_fs";
	case T_PROC_MAGIC: return "procfs";
	case T_PSTOREFS_MAGIC: return "pstorefs";
	case T_QNX4_MAGIC: return "qnx4";
	case T_QNX6_MAGIC: return "qnx6";
	case T_RAMFS_MAGIC: return "ramfs";
	case T_RDTGROUP_MAGIC: return "rdt";
	case T_REISERFS_MAGIC: return "reiserfs";
	case T_RPC_PIPEFS_MAGIC: return "rpc-pipefs";
	case T_SDCARDFS_MAGIC: return "sdcardfs";
	case T_SECRETMEM_MAGIC: return "secretmem";
	case T_SECURITYFS_MAGIC: return "securityfs";
	case T_SELINUX_MAGIC: return "selinux";
	case T_SMACK_MAGIC: return "smackfs";
	case T_SMB_MAGIC: *remote = 1; return "smb";
	case T_SMB2_MAGIC: *remote = 1; return "smb2";
	case T_SNFS_MAGIC: *remote = 1; return "snfs";
	case T_SOCKFS_MAGIC: return "sockfs";
	case T_SQUASHFS_MAGIC: return "squashfs";
	case T_STACK_END_MAGIC: return "stack-end";
	case T_SYSFS_MAGIC: return "sysfs";
	case T_SYSV2_MAGIC: return "sysv2";
	case T_SYSV4_MAGIC: return "sysv4";
	case T_TMPFS_MAGIC: return "tmpfs";
	case T_TRACEFS_MAGIC: return "tracefs";
	case T_UBIFS_MAGIC: return "ubifs";
	case T_UDF_MAGIC: return "udf";
	case T_UFS_MAGIC: return "ufs";
	case T_USBDEVICE_MAGIC: return "usbdevfs";
	case T_V9FS_MAGIC: return "v9fs";
	case T_VBOXSF_MAGIC: *remote = 1; return "vboxsf";
	case T_VMHGFS_MAGIC: *remote = 1; return "vmhgfs";
	case T_VXFS_MAGIC: *remote = 1; return "vxfs";
	case T_VZFS_MAGIC: return "vzfs";
	case T_WSLFS_MAGIC: return "wslfs";
	case T_XENFS_MAGIC: return "xenfs";
	case T_XENIX_MAGIC: return "xenix";
	case T_XFS_MAGIC: return "xfs";
	case T_XIA_MAGIC: return "xia"; /* Linux 2.0 and earlier */
	case T_Z3FOLD_MAGIC: return "z3fold";
	case T_ZFS_MAGIC: return "zfs";
	case T_ZONEFS_MAGIC: return "zonefs";
	case T_ZSMALLOCFS_MAGIC: return "zsmallocfs";
	default: return "unknown";
	}
}

/* Return a pointer to the name of the remote file system to which the file
 * FILE belongs (ex: "//192.168.10.27/share"). */
char *
get_remote_fs_name(const char *file)
{
	FILE *fp = setmntent(_PATH_MOUNTED, "r");
	if (!fp)
		return DEV_NO_NAME;

	size_t mnt_longest = 0;
	static char name[PATH_MAX]; *name = '\0';
	struct mntent *ent;

	while ((ent = getmntent(fp)) != NULL) {
		char *ptr = strstr(file, ent->mnt_dir);
		if (!ptr || ptr != file)
			continue;

		size_t l = strlen(ent->mnt_dir);
		if (l > mnt_longest) {
			mnt_longest = l;
			xstrsncpy(name, ent->mnt_fsname, sizeof(name));
		}
	}

	endmntent(fp);

	if (!*name)
		return DEV_NO_NAME;

	return name;
}

/* Return a pointer to the name of the block device whose ID is DEV. */
char *
get_dev_name(const dev_t dev)
{
	int maj = (int)major(dev);
	if (maj == 0) /* special devices (tmp, dev, sys, proc, etc) */
		return DEV_NO_NAME;

	int min = (int)minor(dev);

	char dev_path[PATH_MAX];
	snprintf(dev_path, sizeof(dev_path),
		"/sys/dev/block/%d:%d/uevent", maj, min);

	int fd;
	FILE *fp = open_fread(dev_path, &fd);
	if (!fp)
		return DEV_NO_NAME;

	static char devname[NAME_MAX];
	*devname = DEV_NO_NAME[0]; devname[1] = '\0';

	char line[NAME_MAX];
	while (fgets(line, (int)sizeof(line), fp)) {
		if (*line != 'D' || strncmp(line + 1, "EVNAME=", 7) != 0)
			continue;

		size_t l = strnlen(line, sizeof(line));
		if (l > 0 && line[l - 1] == '\n')
			line[l - 1] = '\0';

		snprintf(devname, sizeof(devname), "/dev/%s", line + 8);
		break;
	}

	fclose(fp);
	return devname;
}

#elif defined(HAVE_STATFS)
/* Update DEVNAME and DEVTYPE to make it point to the device name and device
 * type of the file system wo which the file FILE belongs. */
void
get_dev_info(const char *file, char **devname, char **devtype)
{
	static struct statfs a;
	if (!file || !*file || statfs(file, &a) == -1) {
		*devname = DEV_NO_NAME;
		*devtype = DEV_NO_NAME;
		return;
	}

	*devname = a.f_mntfromname;
	*devtype = a.f_fstypename;
}

#else
void *_skip_me_fsinfo;
#endif /* __linux__ */
