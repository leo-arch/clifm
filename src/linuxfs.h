/* linuxfs.h - Macros for file system magic numbers */

/*
 * This file is part of CliFM
 *
 * Copyright (C) 2016-2024, L. Abramovich <leo.clifm@outlook.com>
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

#ifndef LINUXFS_H
#define LINUXFS_H

/* These macros are taken from linux/magic.h, statfs(2), and coreutils stat.c
 * (see https://github.com/coreutils/coreutils/blob/master/src/stat.c). */

#define T_AAFS_MAGIC           0x5a3c69f0
#define T_ACFS_MAGIC           0x61636673 /* remote */
#define T_ADFS_MAGIC           0xadf5
#define T_AFFS_MAGIC           0xadff
#define T_AFS_FS_MAGIC         0x6b414653 /* k-afs: remote */
#define T_AFS_MAGIC            0x5346414f /* remote */
#define T_ANON_INODE_FS_MAGIC  0x09041934
#define T_AUFS_MAGIC           0x61756673 /* remote */
#define T_AUTOFS_MAGIC         0x0187
#define T_BALLONFS_MAGIC       0x13661366
#define T_BDEVFS_MAGIC         0x62646576
#define T_BEFS_MAGIC           0x42465331
#define T_BFS_MAGIC            0x1badface
#define T_BINDERFS_MAGIC       0x6c6f6f70
#define T_BINFMTFS_MAGIC       0x42494e4d
#define T_BPF_FS_MAGIC         0xcafe4a11
#define T_BTRFS_MAGIC          0x9123683e
#define T_BTRFS_TEST_MAGIC     0x73727279
#define T_CEPH_MAGIC	       0x00c36400 /* remote */
#define T_CGROUP_MAGIC         0x27e0eb
#define T_CGROUP2_MAGIC        0x63677270
#define T_CIFS_MAGIC           0xff534D42 /* remote */
#define T_CODA_MAGIC	       0x73757245 /* remote */
#define T_COH_MAGIC            0x012ff7b7
#define T_CONFIGFS_MAGIC       0x62656570
#define T_CRAMFS_MAGIC		   0x28cd3d45
#define T_CRAMFS_MAGIC_WEND	   0x453dcd28
#define T_DAXFS_MAGIC          0x64646178
#define T_DEBUGFS_MAGIC        0x64626720
#define T_DEVMEM_MAGIC         0x454d444d
#define T_DEVFS_MAGIC          0x1373 /* Linux 2.6.17 and earlier */
#define T_DEVPTS_MAGIC         0x1cd1
#define T_DMA_BUF_MAGIC        0x444d4142
#define T_ECRYPTFS_MAGIC       0xf15f
#define T_EFIVARFS_MAGIC       0xde5e81e4
#define T_EFS_MAGIC	           0x414a53
#define T_EROFS_MAGIC_V1       0xe0f5e1e2
#define T_EXFAT_MAGIC          0x2011bab0
#define T_EXT_MAGIC            0x137d /* Linux 2.0 and earlier */
#define T_EXT2_OLD_MAGIC       0xef51 /* Old ext2 */
/*#define T_EXT2_MAGIC           0xef53
#define T_EXT3_MAGIC           0xef53 */
#define T_EXT4_MAGIC           0xef53
#define T_F2FS_MAGIC           0xf2f52010
#define T_FAT_MAGIC            0x4006
#define T_FHGFS_MAGIC          0x19830326 /* remote */
#define T_FUSE_MAGIC           0x65735546 /* remote */
#define T_FUSECTL_MAGIC        0x65735543 /* remote */
#define T_FUTEXFS_MAGIC        0xbad1dea
#define T_GFS2_MAGIC           0x01161970 /* remote */
#define T_GPFS_MAGIC           0x47504653 /* remote */
#define T_HFS_MAGIC            0x4244
#define T_HFS_PLUS_MAGIC       0x482b
#define T_HFSX_MAGIC           0x4858
#define T_HOSTFS_MAGIC         0x00c0ffee
#define T_HPFS_MAGIC           0xf995e849
#define T_HUGETLBFS_MAGIC      0x958458f6
#define T_IBRIX_MAGIC          0x013111a8 /* remote */
#define T_INOTIFYFS_MAGIC      0x2bad1dea
#define T_ISOFS_MAGIC          0x9660
#define T_ISOFS_R_WIN_MAGIC    0x4004 /* ISOFS_R_WIN */
#define T_ISOFS_WIN_MAGIC      0x4000 /* ISOFS_WIN */
#define T_JFFS_MAGIC           0x07c0
#define T_JFFS2_MAGIC          0x72b6
#define T_JFS_MAGIC            0x3153464a
#define T_LOGFS_MAGIC          0xc97e8168
#define T_LUSTRE_MAGIC         0x0bd00bd0 /* remote */
#define T_M1FS_MAGIC           0x5346314d
#define T_MINIX_MAGIC          0x137f /* minix v1 fs, 14 char names */
#define T_MINIX_MAGIC2         0x138f /* minix v1 fs, 30 char names */
#define T_MINIX2_MAGIC         0x2468 /* minix v2 fs, 14 char names */
#define T_MINIX2_MAGIC2        0x2478 /* minix v2 fs, 30 char names */
#define T_MINIX3_MAGIC         0x4d5a /* minix v3 fs, 60 char names */
#define T_MQUEUE_MAGIC         0x19800202
#define T_MSDOS_MAGIC          0x4d44
#define T_MTD_INODE_FS_MAGIC   0x11307854
#define T_NCP_MAGIC            0x564c /* remote */
#define T_NFS_MAGIC            0x6969 /* remote */
#define T_NFSD_MAGIC           0x6e667364 /* remote */
#define T_NILFS_MAGIC          0x3434
#define T_NSFS_MAGIC           0x6e736673
#define T_NTFS_MAGIC           0x5346544e
#define T_OCFS2_MAGIC          0x7461636f /* remote */
#define T_OPENPROM_MAGIC       0x9fa1
#define T_OVERLAYFS_MAGIC      0x794c7630 /* remote */
#define T_PANFS_MAGIC          0xaad7aaea /* remote */
#define T_PID_FS_MAGIC         0x50494446
#define T_PIPEFS_MAGIC         0x50495045 /* remote */
#define T_PPC_CMM_FS_MAGIC     0xc7571590
#define T_PRL_FS_MAGIC         0x7c7c6673 /* remote */
#define T_PROC_MAGIC           0x9fa0
#define T_PSTOREFS_MAGIC       0x6165676c
#define T_QNX4_MAGIC           0x002f
#define T_QNX6_MAGIC           0x68191122
#define T_RAMFS_MAGIC	       0x858458f6
#define T_RDTGROUP_MAGIC       0x7655821
#define T_REISERFS_MAGIC       0x52654973
#define T_RPC_PIPEFS_MAGIC     0x67596969
#define T_SDCARDFS_MAGIC       0x5dca2df5
#define T_SECRETMEM_MAGIC	   0x5345434d
#define T_SECURITYFS_MAGIC	   0x73636673
#define T_SELINUX_MAGIC		   0xf97cff8c
#define T_SMACK_MAGIC	       0x43415d53
#define T_SMB_MAGIC            0x517b /* remote */
#define T_SMB2_MAGIC           0xfe534d42 /* remote */
#define T_SNFS_MAGIC           0xbeefdead /* remote */
#define T_SOCKFS_MAGIC         0x534f434b
#define T_SQUASHFS_MAGIC	   0x73717368
#define T_STACK_END_MAGIC      0x57ac6e9d
#define T_SYSV2_MAGIC          0x012ff7b6
#define T_SYSV4_MAGIC          0x012ff7b5
#define T_SYSFS_MAGIC          0x62656572
#define T_TMPFS_MAGIC          0x01021994
#define T_TRACEFS_MAGIC        0x74726163
#define T_UBIFS_MAGIC          0x24051905
#define T_UDF_MAGIC            0x15013346
#define T_UFS_MAGIC            0x00011954
#define T_USBDEVICE_MAGIC      0x9fa2
#define T_V9FS_MAGIC           0x01021997
#define T_VBOXSF_MAGIC         0x786f4256 /* remote */
#define T_VMHGFS_MAGIC         0xbacbacbc /* remote */
#define T_VXFS_MAGIC           0xa501fcf5 /* remote */
#define T_VZFS_MAGIC           0x565a4653
#define T_WSLFS_MAGIC          0x53464846
#define T_XENFS_MAGIC          0xabba1974
#define T_XENIX_MAGIC          0x012ff7b4
#define T_XIA_MAGIC            0x012fd16d /* Linux 2.0 and earlier */
#define T_XFS_MAGIC            0x58465342
#define T_Z3FOLD_MAGIC         0x0033
#define T_ZFS_MAGIC            0x2fc12fc1
#define T_ZONEFS_MAGIC         0x5a4f4653
#define T_ZSMALLOCFS_MAGIC     0x58295829

#endif /* LINUXFS_H */
