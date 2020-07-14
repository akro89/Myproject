#include "require.h"
#include "abio.h"
#include "lib.h"
#include "vendor.h"
#include "oracle12c_lib.h"


#ifdef __ABIO_AIX
	__uint64 GetRawDeviceFileSizeAIX(char * file, struct abio_file_stat * file_stat)
	{
		struct lv_id lid;
		struct queryvgs * vgs;
		struct queryvg * vg;
		struct querylv * lv;
		
		static struct querylv * lvList;
		static int lvNumber;
		
		char volumeName[MAX_PATH_LENGTH];
		__uint64 fsSize;
		
		int i;
		int j;
		int k;
		
		
		
		fsSize = 0;
		
		// get an information of logical volume
		if (lvNumber == 0)
		{
			// get number of online volume group
			if (lvm_queryvgs(&vgs, 0) == 0)
			{
				// get volume group list
				for (i = 0; i < vgs->num_vgs; i++)
				{
					if (lvm_queryvg(&vgs->vgs[i].vg_id, &vg, NULL) == 0)
					{
						lvNumber += vg->num_lvs;
					}
				}
			}
			
			if (lvNumber > 0)
			{
				// get logical volume list
				if (lvm_queryvgs(&vgs, 0) == 0)
				{
					lvList = (struct querylv *)malloc(sizeof(struct querylv) * lvNumber);
					memset(lvList, 0, sizeof(struct querylv) * lvNumber);
					k = 0;
					
					// get volume group information
					for (i = 0; i < vgs->num_vgs; i++)
					{
						if (lvm_queryvg(&vgs->vgs[i].vg_id, &vg, NULL) == 0)
						{
							// get logical volume information
							lid.vg_id.word1 = vgs->vgs[i].vg_id.word1;
							lid.vg_id.word2 = vgs->vgs[i].vg_id.word2;
							lid.vg_id.word3 = vgs->vgs[i].vg_id.word3;
							lid.vg_id.word4 = vgs->vgs[i].vg_id.word4;
							
							for (j = 0; j < vg->num_lvs; j++)
							{
								lid.minor_num = vg->lvs[j].lv_id.minor_num;
								
								if (lvm_querylv(&lid, &lv, NULL) == 0)
								{
									memcpy(lvList + k, lv, sizeof(struct querylv));
									k++;
								}
							}
						}
					}
				}
			}
		}
		
		// get a raw device file size
		if (lvNumber > 0)
		{
			// get a logical volume name from raw device file path
			for (i = strlen(file) - 1; i > -1; i--)
			{
				if (file[i] == FILE_PATH_DELIMITER)
				{
					break;
				}
			}
			
			memset(volumeName, 0, sizeof(volumeName));
			if (S_ISCHR(file_stat->mode))
			{
				strcpy(volumeName, file + i + 1 + 1);
			}
			else
			{
				strcpy(volumeName, file + i + 1);
			}
			
			
			// find a raw device file in logical volume list
			for (i = 0; i < lvNumber; i++)
			{
				if (!strcmp(lvList[i].lvname, volumeName))
				{
					// get a volume size
					fsSize = 1;
					for (j = 0; j < lvList[i].ppsize; j++)
					{
						fsSize *= 2;
					}
					fsSize *= lvList[i].currentsize;
					
					break;
				}
			}
		}
		
		
		return fsSize;
	}
#elif __ABIO_SOLARIS
	__uint64 GetRawDeviceFileSizeSolaris(char * file, struct abio_file_stat * file_stat)
	{
		FILE * vfp;
		struct vfstab vfs;
		
		va_fd_t fd;
		struct fs fs;
		
		__uint64 fsSize;
		
		
		
		fsSize = 0;
		
		// get filesystem information from filesystem entry
		if ((vfp = va_fopen(NULL, VFSTAB, "r", 1)) != NULL)
		{
			while ((getvfsent(vfp, &vfs)) != -1)
			{
				if (!strcmp(vfs.vfs_special, file))
				{
					// read superblock
					if ((fd = va_open(NULL, vfs.vfs_special, O_RDONLY, 0, 1)) != (va_fd_t)-1)
					{
						va_lseek(fd, (va_offset_t)(1024 * 16), SEEK_SET);
						
						va_read(fd, &fs, sizeof(struct fs), DATA_TYPE_NOT_CHANGE);
						
						fsSize = fs.fs_size;
						
						// solaris의 경우 file system type이 ufs일 경우에만 이 값이 제대로 표시된다.
						// veritas file system이 설치되어서 vxfs일 경우에는 이 값이 표시가 되지 않는다.
						if (fs.fs_fsize != 0)
						{
							fsSize *= fs.fs_fsize;
						}
						
						va_close(fd);
					}
					
					break;
				}
			}
			
			va_fclose(vfp);
		}
		
		
		return fsSize;
	}
#elif __ABIO_HP
	__uint64 GetRawDeviceFileSizeHP(char * file, struct abio_file_stat * file_stat)
	{
		FILE * fsp;
		struct mntent * fsent;
		
		va_fd_t fd;
		struct fs fs;
		
		__uint64 fsSize;
		
		
		
		fsSize = 0;
		
		// get filesystem information from filesystem entry
		if ((fsp = setmntent(MNT_CHECKLIST, "r")) != NULL)
		{
			while ((fsent = getmntent(fsp)) != NULL)
			{
				if (!strcmp(fsent->mnt_fsname, file))
				{
					// read superblock
					if ((fd = va_open(NULL, fsent->mnt_fsname, O_RDONLY, 0, 1)) != (va_fd_t)-1)
					{
						va_lseek(fd, (va_offset_t)8192, SEEK_SET);
						
						va_read(fd, &fs, sizeof(struct fs), DATA_TYPE_NOT_CHANGE);
						
						fsSize = fs.fs_size;
						
						// hp의 경우 file system type이 hfs일 경우에만 이 값이 제대로 표시된다.
						// veritas file system이 설치되어서 vxfs일 경우에는 이 값이 표시가 되지 않는다.
						if (fs.fs_fsize != 0)
						{
							fsSize *= fs.fs_fsize;
						}
						
						va_close(fd);
					}
					
					break;
				}
			}
			
			endmntent(fsp);
		}
		
		
		return fsSize;
	}
#elif __ABIO_LINUX
	__uint64 GetRawDeviceFileSizeLinux(char * file, struct abio_file_stat * file_stat)
	{
		FILE * fsp;
		struct mntent * fsent;
		
		__uint64 fsSize;
		
		
		
		fsSize = 0;
		
		// get filesystem information from filesystem entry
		if ((fsp = setmntent(_PATH_FSTAB, "r")) != NULL)
		{
			while ((fsent = getmntent(fsp)) != NULL)
			{
				if (!strcmp(fsent->mnt_fsname, file))
				{
					// read superblock
					
					break;
				}
			}
			
			endmntent(fsp);
		}
		
		
		return fsSize;
	}
#endif

