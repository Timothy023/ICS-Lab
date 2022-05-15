/*
Filesystem Lab disigned and implemented by Liang Junkai,RUC
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fuse.h>
#include <errno.h>
#include <stdbool.h>
#include "disk.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define DIRMODE (S_IFDIR|0755)
#define REGMODE (S_IFREG|0644)

#define idSuperBlock	0
#define idBitmapStart	1
#define idBitmapEnd		2
#define idInodeStart	3
#define idInodeEnd		799
#define idBlockStart	800
#define idBlockEnd		65535

#define maxBitmap		1024
#define MaxFileName		128

#define InodePerBlock		((BLOCK_SIZE - 8) / sizeof(struct INODE))
#define FilenamePerBlock	((BLOCK_SIZE - 8) / sizeof(struct FILENAME))
#define PointerPerBlock		(BLOCK_SIZE / sizeof(unsigned int))

struct SUPERBLOCK {
	unsigned int numInodes;
	unsigned int numDateBlocks;
	unsigned int idRootInode;
};

struct INODE {
	mode_t mode;
	off_t size;
	time_t AccessTime;
	time_t CreateTime;
	time_t ModifiedTime;
	unsigned int numBlocks;
	unsigned int DirectPointer[12];
	unsigned int IndirectPointer[2];
};

struct FILENAME {
	char filename[MaxFileName];
	unsigned int position;
};

unsigned int bitmap[maxBitmap]; // First: 0~32767; Second: 32768~65535

void GetSuperBlock(struct SUPERBLOCK *sb) {
	char buf[BLOCK_SIZE] = {'\0'};
	disk_read(idSuperBlock, buf);
	(*sb) = *((struct SUPERBLOCK *)buf);
}

void WriteSuperBlock(struct SUPERBLOCK *sb) {
	char buf[BLOCK_SIZE] = {'\0'};
	memcpy(buf, sb, sizeof(struct SUPERBLOCK));
	disk_write(idSuperBlock, buf);
}

void ChangeBlockStatus(unsigned int idBlock) {
	struct SUPERBLOCK sb;
	GetSuperBlock(&sb);

	int pos = idBitmapStart;
	char buf[BLOCK_SIZE] = {'\0'};
	if (idBlock >= 32768) {
		pos += 1;
		idBlock -= 32768;
	}
	disk_read(pos, buf);
	memcpy(bitmap, buf, BLOCK_SIZE);
	int px = idBlock / 32;
	int py = idBlock % 32;
	if ((bitmap[px] & (((unsigned int) 1) << py)) == 0) sb.numDateBlocks -= 1;
	else sb.numDateBlocks += 1;
	bitmap[px] ^= ((unsigned int) 1) << py;
	memcpy(buf, bitmap, BLOCK_SIZE);
	disk_write(pos, buf);

	WriteSuperBlock(&sb);
}

int GetFreeDataBlock() {
	char buf[BLOCK_SIZE] = {'\0'};
	const unsigned int isFull = (unsigned int)(-1);
	for (int pos = idBitmapStart; pos <= idBitmapEnd; ++pos) {
		disk_read(pos, buf);
		memcpy(bitmap, buf, BLOCK_SIZE);
		int offset = (pos - idBitmapStart) * 32768;
		int st = (!(pos - idBitmapStart)) * idBlockStart;
		for (int i = st; i < 1024; ++i) {
			if (bitmap[i] != isFull) {
				for (int j = 0; j < 32; ++j) {
					if ((bitmap[i] & (1 << j)) == 0)  {
						return i * 32 + j + offset;
					}
				}
			}
		}
	}
	return -1;
}

int GetFreeInode() {
	char buf[BLOCK_SIZE] = {'\0'};
	unsigned long long isFull = (1ll << InodePerBlock) - 1;
	for (int i = idInodeStart; i <= idInodeEnd; ++i) {
		disk_read(i, buf);
		unsigned long long InodeBitmap = *((unsigned long long *)buf);
		if (InodeBitmap == isFull) continue;
		for (int j = 0; j < InodePerBlock; ++j) {
			if ((InodeBitmap & (1ll << j)) == 0) {
				return (i - idInodeStart) * InodePerBlock + j;
			}
		}
	}
	return -1;
}

void ReadInode(unsigned int idInode, struct INODE *inode) {
	int idBlock = idInodeStart + idInode / InodePerBlock;
	int off = idInode % InodePerBlock;
	char buf[BLOCK_SIZE] = {'\0'};
	disk_read(idBlock, buf);
	memcpy(inode, buf + 8 + off * sizeof(struct INODE), sizeof(struct INODE));
}

void WriteInode(unsigned int idInode, const struct INODE *inode) {
	struct SUPERBLOCK sb;
	GetSuperBlock(&sb);

	int idBlock = idInodeStart + idInode / InodePerBlock;
	int off = idInode % InodePerBlock;
	char buf[BLOCK_SIZE] = {'\0'};
	disk_read(idBlock, buf);

	unsigned long long InodeBitmap = *((unsigned long long *)buf);
	if ((InodeBitmap & (1ll << off)) == 0) sb.numInodes -= 1;
	InodeBitmap |= (1ll << off);
	memcpy(buf, (void *)(&InodeBitmap), sizeof(unsigned long long));
	memcpy(buf + 8 + off * sizeof(struct INODE), inode, sizeof(struct INODE));
	disk_write(idBlock, buf);

	WriteSuperBlock(&sb);
}

bool Path2Inode(const char *path, struct INODE *inode, unsigned int *id) {
	char buf[BLOCK_SIZE] = {'\0'};
	char fn[MaxFileName] = {'\0'};
	int len = strlen(path);
	struct SUPERBLOCK sb;
	disk_read(idSuperBlock, buf);
	sb = *((struct SUPERBLOCK *)buf);
	unsigned int nowidInode = sb.idRootInode;
	struct INODE nowInode;
	ReadInode(nowidInode, &nowInode);
	bool flag = true;

	for (int i = ((path[0] == '/') ? 1 : 0), tmp; i < len; ++i) {
		tmp = 0;
		for (tmp = 0; path[i] != '/' && i < len; ++i, ++tmp) {
			fn[tmp] = path[i];
		}
		fn[tmp] = '\0';

		if (nowInode.mode != DIRMODE) return false;

		flag = false;
		for (int j = 0; j < 12 && !flag; ++j) {
			unsigned int idBlock = nowInode.DirectPointer[j];
			if (idBlock == 0) continue;
			disk_read(idBlock, buf);
			unsigned long long tmp = *((unsigned long long *)buf);
			// printf ("test in path to inode: %d %d %d %d %llu\n", i, j, idBlock, flag, tmp);
			for (int k = 0; k < FilenamePerBlock; ++k) {
				if ((tmp & (1ll << k)) == 0) continue;
				struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + k * sizeof(struct FILENAME)));
				// printf ("test details: %s %s %d %d %ld %d\n", fn, nxtfile.filename, i, j, FilenamePerBlock, k);
				if (strcmp(fn, nxtfile.filename) == 0) {
					flag = true;
					nowidInode = nxtfile.position;
					ReadInode(nowidInode, &nowInode);
					printf ("test nowidInode: %d\n", nowidInode);
					break;
				}
			}
		}
		for (int j = 0; j < 2 && !flag; ++j) {
			unsigned int idBlock = nowInode.IndirectPointer[j];
			if (idBlock == 0) continue;
			disk_read(idBlock, buf);
			for (int k = 0; k < PointerPerBlock && !flag; ++k) {
				unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
				if (nxtidBlock == 0) continue;
				char buf1[BLOCK_SIZE] = {'\0'};
				disk_read(nxtidBlock, buf1);
				unsigned long long tmp = *((unsigned long long *)buf1);
				for (int p = 0; p < FilenamePerBlock; ++p) {
					if ((tmp & (1ll << p)) == 0) continue;
					struct FILENAME nxtfile = *((struct FILENAME *)(buf1 + 8 + p * sizeof(struct FILENAME)));
					if (strcmp(fn, nxtfile.filename) == 0) {
						if (i == len || (i == len - 1 && path[i] == '/')) {
							flag = true;
							nowidInode = nxtfile.position;
							ReadInode(nowidInode, &nowInode);
							break;
						}
					}
				}
			}
		}
	}
	printf ("Out path to inode: %d\n", flag);
	if (flag) {
		(*inode) = nowInode;
		(*id) = nowidInode;
		return true;
	}
	else return false;
}

void GetFatherPath(const char *path, char *FatherPath, char *DirName) {
	int len = strlen(path);
	int lstid = 0;
	for (int i = 0; i < len; ++i) {
		FatherPath[i] = path[i];
		if (path[i] == '/') lstid = i;
	}
	if (lstid == 0) FatherPath[1] = '\0';
	else FatherPath[lstid] = '\0';
	for (int i = lstid + 1; i < len; ++i) {
		DirName[i - lstid - 1] = path[i];
	}
}

void DeleteInode(int idInode) {
	struct SUPERBLOCK sb;
	GetSuperBlock(&sb);
	sb.numInodes += 1;
	WriteSuperBlock(&sb);

	char buf[BLOCK_SIZE] = {'\0'};

	struct INODE inode;
	ReadInode(idInode, &inode);
	for (int i = 0; i < 12; ++i) {
		if (inode.DirectPointer[i] == 0) continue;
		ChangeBlockStatus(inode.DirectPointer[i]);
	}
	for (int j = 0; j < 2; ++j) {
		unsigned int idBlock = inode.IndirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			else ChangeBlockStatus(nxtidBlock);
		}
		ChangeBlockStatus(idBlock);
	}

	int idBlock = idInodeStart + idInode / InodePerBlock;
	int off = idInode % InodePerBlock;
	disk_read(idBlock, buf);

	unsigned long long InodeBitmap = *((unsigned long long *)buf);
	InodeBitmap ^= (1ll << off);
	memcpy(buf, (void *)(&InodeBitmap), sizeof(unsigned long long));
	disk_write(idBlock, buf);
}

//Format the virtual block device in the following function
int mkfs() {
	char buf[BLOCK_SIZE] = {'\0'};
	struct SUPERBLOCK sb;
	sb.numDateBlocks = 65536;
	sb.numInodes = (idInodeEnd - idInodeStart + 1) * InodePerBlock;
	sb.idRootInode = GetFreeInode();
	memcpy(buf, (void *)(&sb), sizeof(struct SUPERBLOCK));

	struct INODE root;
	root.mode = DIRMODE;
	root.AccessTime = time(NULL);
	root.CreateTime = time(NULL);
	root.ModifiedTime = time(NULL);
	root.numBlocks = 0;
	memset(root.DirectPointer, 0, sizeof(root.DirectPointer));
	memset(root.IndirectPointer, 0, sizeof(root.IndirectPointer));

	WriteInode(sb.idRootInode, &root);

	return 0;
}

//Filesystem operations that you need to implement
int fs_getattr (const char *path, struct stat *attr) {
	printf("Getattr is called:%s\n",path);
	struct INODE inode;
	unsigned int id;
	bool isExist = Path2Inode(path, &inode, &id);
	printf ("test in getattr: %s %d\n", path, isExist);
	if (isExist) {
		attr->st_mode = inode.mode;
		attr->st_nlink = 1;
		attr->st_uid = getuid();
		attr->st_gid = getgid();
		attr->st_size = inode.size;
		attr->st_atime = inode.AccessTime;
		attr->st_mtime = inode.ModifiedTime;
		attr->st_ctime = inode.CreateTime;
		return 0;
	}
	else {
		printf ("getattr fail: %s\n", path);
		return -ENOENT;
	}
}

int fs_readdir (const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	printf("Readdir is called:%s\n", path);
	unsigned int id;
	struct INODE inode;
	bool isExist = Path2Inode(path, &inode, &id);
	if (isExist == false) {
		printf ("readdir fail: %s\n", path);
		return -ENOENT;
	}

	char buf[BLOCK_SIZE] = {'\0'};

	for (int j = 0; j < 12; ++j) {
		unsigned int idBlock = inode.DirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) continue;
			struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + k * sizeof(struct FILENAME)));
			filler(buffer, nxtfile.filename, NULL, 0);
			printf ("test file name: %s\n", nxtfile.filename);
		}
	}
	for (int j = 0; j < 2; ++j) {
		unsigned int idBlock = inode.IndirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			char buf1[BLOCK_SIZE] = {'\0'};
			disk_read(nxtidBlock, buf1);
			unsigned long long tmp = *((unsigned long long *)buf1);
			for (int p = 0; p < FilenamePerBlock; ++p) {
				if ((tmp & (1ll << p)) == 0) continue;
				struct FILENAME nxtfile = *((struct FILENAME *)(buf1 + 8 + p * sizeof(struct FILENAME)));
				filler(buffer, nxtfile.filename, NULL, 0);
			}
		}
	}
	return 0;
}

int fs_read (const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("Read is called:%s\n",path);

	char buf[BLOCK_SIZE] = {'\0'};
	unsigned int idInode;
	struct INODE inode;
	bool isExist = Path2Inode(path, &inode, &idInode);
	if (!isExist) return -ENOENT;

	size_t st = offset, en = offset + size - 1;
	unsigned idst = st / BLOCK_SIZE, iden = en / BLOCK_SIZE;
	int nowid = 0;
	int haveRead = 0, haveScan = 0;
	int rst, ren;
	for (int i = 0; i < 12; ++i, nowid++, haveScan += BLOCK_SIZE) {
		if (nowid < idst || iden < nowid) continue;
		if (nowid == idst) rst = st % BLOCK_SIZE;
		else rst = 0;
		int maxn = BLOCK_SIZE;
		if (haveScan + BLOCK_SIZE > inode.size) {
			maxn = inode.size - haveScan;
		}
		if (nowid == iden) ren = min(en % BLOCK_SIZE, maxn);
		else ren = maxn;

		unsigned int idBlock = inode.DirectPointer[i];
		if (idBlock == 0) break;
		disk_read(idBlock, buf);
		memcpy(buffer + haveRead, buf + rst, ren - rst + 1);

		char tmp[40960] = {'\0'};
		memcpy(tmp, buf + rst, ren - rst + 1);
		tmp[ren - rst + 1] = '\0';
		printf ("test read buffer: [%s]\n", tmp);

		haveRead += ren - rst;
	}
	for (int j = 0; j < 2; ++j) {
		unsigned int idBlock = inode.IndirectPointer[j];
		if (idBlock == 0) break;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k, ++nowid, haveScan += BLOCK_SIZE) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			if (nowid < idst || iden < nowid) continue;

			if (nowid == idst) rst = st % BLOCK_SIZE;
			else rst = 0;
			int maxn = BLOCK_SIZE;
			if (haveScan + BLOCK_SIZE > inode.size) {
				maxn = inode.size - haveScan;
			}
			if (nowid == iden) ren = min(en % BLOCK_SIZE, maxn);
			else ren = maxn;

			char buf1[BLOCK_SIZE] = {'\0'};
			disk_read(nxtidBlock, buf1);
			memcpy(buffer + haveRead, buf1 + rst, ren - rst + 1);
			haveRead += ren - rst + 1;
		}
	}

	inode.AccessTime = time(NULL);
	WriteInode(idInode, &inode);
	printf("Finish read\n");
	buf[0] = '\0';
	memcpy(buffer + haveRead, buf, 1);

	return haveRead;
}

int fs_mknod (const char *path, mode_t mode, dev_t dev) {
	printf("Mknod is called:%s\n",path);
	if (path[0] == '/' && strlen(path) == 1)
		return -ENOSPC;

	char FatherPath[1024] = {'\0'};
	char DirName[MaxFileName] = {'\0'};
	char buf[BLOCK_SIZE] = {'\0'};
	GetFatherPath(path, FatherPath, DirName);
	
	struct INODE FatherInode;
	unsigned int FatherId;
	bool isExist = Path2Inode(FatherPath, &FatherInode, &FatherId);
	printf ("test FatherPath: %s  %s  %d  %d\n", FatherPath, DirName, FatherId, isExist);
	if (isExist == false) return -ENOSPC;

	unsigned int idInode = GetFreeInode();
	struct INODE DirInode;
	DirInode.mode = REGMODE;
	DirInode.AccessTime = time(NULL);
	DirInode.CreateTime = time(NULL);
	DirInode.ModifiedTime = time(NULL);
	DirInode.numBlocks = 0;
	memset(DirInode.DirectPointer, 0, sizeof(DirInode.DirectPointer));
	memset(DirInode.IndirectPointer, 0, sizeof(DirInode.IndirectPointer));

	bool flag = false;
	FatherInode.numBlocks += 1;
	FatherInode.ModifiedTime = time(NULL);
	FatherInode.CreateTime = time(NULL);
	for (int i = 0; i < 12 && !flag; ++i) {
		unsigned int idBlock;
		idBlock = FatherInode.DirectPointer[i];
		if (idBlock == 0) {
			idBlock = GetFreeDataBlock();
			FatherInode.DirectPointer[i] = idBlock;
			memset(buf, 0, sizeof(buf));
			ChangeBlockStatus(idBlock);
		}
		else disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) {
				struct FILENAME file;
				memcpy(file.filename, DirName, sizeof(DirName));
				file.position = idInode;
				tmp |= (1ll << k);
				memcpy(buf, &tmp, sizeof(unsigned long long));
				memcpy(buf + 8 + k * sizeof(struct FILENAME), &file, sizeof(struct FILENAME));
				flag = true;
				disk_write(idBlock, buf);
				printf ("test in set inode: %d %d %d\n", idBlock, i, k);
				break;
			}
		}
	}
	if (!flag) {
		for (int i = 0; i < 2 && !flag; ++i) {
			int idBlock = FatherInode.IndirectPointer[i];
			if (idBlock == 0) {
				idBlock = GetFreeDataBlock();
				if (idBlock == -1) continue;
				FatherInode.IndirectPointer[i] = idBlock;
				memset(buf, 0, sizeof(buf));
				ChangeBlockStatus(idBlock);
			}
			else disk_read(idBlock, buf);
			for (int k = 0; k < PointerPerBlock && !flag; ++k) {
				char buf1[BLOCK_SIZE] = {'\0'};
				unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
				if (nxtidBlock == 0) {
					nxtidBlock = GetFreeDataBlock();
					if (nxtidBlock == -1) continue;
					memset(buf1, 0, sizeof(buf1));
					ChangeBlockStatus(nxtidBlock);
					memcpy(buf + k * 4, &nxtidBlock, sizeof(unsigned int));
				}
				else disk_read(nxtidBlock, buf1);
				unsigned long long tmp = *((unsigned long long *)buf1);
				for (int p = 0; p < FilenamePerBlock; ++p) {
					if ((tmp & (1ll << p)) == 0) {
						struct FILENAME file;
						memcpy(file.filename, DirName, sizeof(DirName));
						file.position = idInode;
						tmp |= (1ll << p);
						memcpy(buf1, &tmp, sizeof(unsigned long long));
						memcpy(buf1 + 8 + p * sizeof(struct FILENAME), &file, sizeof(struct FILENAME));
						flag = true;
						disk_write(idBlock, buf);
						disk_write(nxtidBlock, buf1);

						printf ("test in set inode: %d %d %d\n", idBlock, i, p);
						break;
					}
				}
			}
		}
	}
	if (!flag) return -ENOSPC;
	printf ("at the end of mknod: %d %d\n", FatherId, FatherInode.DirectPointer[0]);
	WriteInode(FatherId, &FatherInode);
	WriteInode(idInode, &DirInode);
	return 0;
}

int fs_mkdir (const char *path, mode_t mode) {
	printf("Mkdir is called:%s\n",path);

	if (path[0] == '/' && strlen(path) == 1)
		return -ENOENT;

	char FatherPath[1024] = {'\0'};
	char DirName[MaxFileName] = {'\0'};
	char buf[BLOCK_SIZE] = {'\0'};
	GetFatherPath(path, FatherPath, DirName);
	
	struct INODE FatherInode;
	unsigned int FatherId;
	bool isExist = Path2Inode(FatherPath, &FatherInode, &FatherId);
	printf ("test FatherPath: %s  %s  %d  %d\n", FatherPath, DirName, FatherId, isExist);
	if (isExist == false) return -ENOENT;

	unsigned int idInode = GetFreeInode();
	struct INODE DirInode;
	DirInode.mode = DIRMODE;
	DirInode.AccessTime = time(NULL);
	DirInode.CreateTime = time(NULL);
	DirInode.ModifiedTime = time(NULL);
	DirInode.numBlocks = 0;
	memset(DirInode.DirectPointer, 0, sizeof(DirInode.DirectPointer));
	memset(DirInode.IndirectPointer, 0, sizeof(DirInode.IndirectPointer));

	bool flag = false;
	FatherInode.numBlocks += 1;
	FatherInode.ModifiedTime = time(NULL);
	FatherInode.CreateTime = time(NULL);
	for (int i = 0; i < 12 && !flag; ++i) {
		unsigned int idBlock;
		idBlock = FatherInode.DirectPointer[i];
		if (idBlock == 0) {
			idBlock = GetFreeDataBlock();
			if (idBlock == -1) continue;
			FatherInode.DirectPointer[i] = idBlock;
			memset(buf, 0, sizeof(buf));
			ChangeBlockStatus(idBlock);
		}
		else disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) {
				struct FILENAME file;
				memcpy(file.filename, DirName, sizeof(DirName));
				file.position = idInode;
				tmp |= (1ll << k);
				memcpy(buf, &tmp, sizeof(unsigned long long));
				memcpy(buf + 8 + k * sizeof(struct FILENAME), &file, sizeof(struct FILENAME));
				flag = true;
				disk_write(idBlock, buf);
				printf ("test in set inode: %d %d %d\n", idBlock, i, k);
				break;
			}
		}
	}
	if (!flag) {
		for (int i = 0; i < 2 && !flag; ++i) {
			int idBlock = FatherInode.IndirectPointer[i];
			if (idBlock == 0) {
				idBlock = GetFreeDataBlock();
				if (idBlock == -1) continue;
				FatherInode.IndirectPointer[i] = idBlock;
				memset(buf, 0, sizeof(buf));
				ChangeBlockStatus(idBlock);
			}
			else disk_read(idBlock, buf);
			for (int k = 0; k < PointerPerBlock && !flag; ++k) {
				char buf1[BLOCK_SIZE] = {'\0'};
				unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
				if (nxtidBlock == 0) {
					nxtidBlock = GetFreeDataBlock();
					if (nxtidBlock == -1) continue;
					memset(buf1, 0, sizeof(buf1));
					ChangeBlockStatus(nxtidBlock);
					memcpy(buf + k * 4, &nxtidBlock, sizeof(unsigned int));
				}
				else disk_read(nxtidBlock, buf1);
				unsigned long long tmp = *((unsigned long long *)buf1);
				for (int p = 0; p < FilenamePerBlock; ++p) {
					if ((tmp & (1ll << p)) == 0) {
						struct FILENAME file;
						memcpy(file.filename, DirName, sizeof(DirName));
						file.position = idInode;
						tmp |= (1ll << p);
						memcpy(buf1, &tmp, sizeof(unsigned long long));
						memcpy(buf1 + 8 + p * sizeof(struct FILENAME), &file, sizeof(struct FILENAME));
						flag = true;
						disk_write(idBlock, buf);
						disk_write(nxtidBlock, buf1);

						printf ("test in set inode: %d %d %d\n", idBlock, i, p);
						break;
					}
				}
			}
		}
	}
	if (!flag) return -ENOSPC;
	printf ("at the end of mkdir: %d %d %d\n", FatherId, FatherInode.DirectPointer[0], FatherInode.IndirectPointer[0]);
	WriteInode(FatherId, &FatherInode);
	WriteInode(idInode, &DirInode);
	return 0;
}

int fs_rmdir (const char *path) {
	printf("Rmdir is called:%s\n",path);

	if (path[0] == '/' && strlen(path) == 1)
		return -ENOENT;

	char FatherPath[1024] = {'\0'};
	char DirName[MaxFileName] = {'\0'};
	char buf[BLOCK_SIZE] = {'\0'};
	GetFatherPath(path, FatherPath, DirName);
	
	struct INODE FatherInode;
	unsigned int FatherId;
	bool isExist = Path2Inode(FatherPath, &FatherInode, &FatherId);
	printf ("test FatherPath: %s  %s  %d  %d\n", FatherPath, DirName, FatherId, isExist);
	if (isExist == false) return -ENOENT;

	FatherInode.ModifiedTime = time(NULL);
	FatherInode.CreateTime = time(NULL);

	bool flag = false;
	for (int j = 0; j < 12 && !flag; ++j) {
		unsigned int idBlock = FatherInode.DirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) continue;
			struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + k * sizeof(struct FILENAME)));
			if (strcmp(DirName, nxtfile.filename) == 0) {
				flag = true;
				tmp ^= (1ll << k);
				memcpy(buf, &tmp, sizeof(unsigned long long));
				disk_write(idBlock, buf);
				DeleteInode(nxtfile.position);
				break;
			}
		}
	}
	for (int j = 0; j < 2 && !flag; ++j) {
		unsigned int idBlock = FatherInode.IndirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			disk_read(nxtidBlock, buf);
			unsigned long long tmp = *((unsigned long long *)buf);
			for (int p = 0; p < FilenamePerBlock; ++p) {
				if ((tmp & (1ll << p)) == 0) continue;
				struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + p * sizeof(struct FILENAME)));
				if (strcmp(DirName, nxtfile.filename) == 0) {
					flag = true;
					tmp ^= (1ll << k);
					memcpy(buf, &tmp, sizeof(unsigned long long));
					disk_write(idBlock, buf);
					DeleteInode(nxtfile.position);
					break;
				}
			}
		}
	}

	return 0;
}

int fs_unlink (const char *path) {
	printf("Unlink is callded:%s\n",path);

	if (path[0] == '/' && strlen(path) == 1)
		return -ENOENT;

	char FatherPath[1024] = {'\0'};
	char DirName[MaxFileName] = {'\0'};
	char buf[BLOCK_SIZE] = {'\0'};
	GetFatherPath(path, FatherPath, DirName);
	
	struct INODE FatherInode;
	unsigned int FatherId;
	bool isExist = Path2Inode(FatherPath, &FatherInode, &FatherId);
	printf ("test FatherPath: %s  %s  %d  %d\n", FatherPath, DirName, FatherId, isExist);
	if (isExist == false) return -ENOENT;

	FatherInode.ModifiedTime = time(NULL);
	FatherInode.CreateTime = time(NULL);

	bool flag = false;
	for (int j = 0; j < 12 && !flag; ++j) {
		unsigned int idBlock = FatherInode.DirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) continue;
			struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + k * sizeof(struct FILENAME)));
			if (strcmp(DirName, nxtfile.filename) == 0) {
				flag = true;
				tmp ^= (1ll << k);
				memcpy(buf, &tmp, sizeof(unsigned long long));
				disk_write(idBlock, buf);

				DeleteInode(nxtfile.position);
				break;
			}
		}
	}
	for (int j = 0; j < 2 && !flag; ++j) {
		unsigned int idBlock = FatherInode.IndirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			disk_read(nxtidBlock, buf);
			unsigned long long tmp = *((unsigned long long *)buf);
			for (int p = 0; p < FilenamePerBlock; ++p) {
				if ((tmp & (1ll << p)) == 0) continue;
				struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + p * sizeof(struct FILENAME)));
				if (strcmp(DirName, nxtfile.filename) == 0) {
					flag = true;
					tmp ^= (1ll << k);
					memcpy(buf, &tmp, sizeof(unsigned long long));
					disk_write(idBlock, buf);
					DeleteInode(nxtfile.position);
					break;
				}
			}
		}
	}

	return 0;
}

int fs_rename (const char *oldpath, const char *newpath) {
	printf("Rename is called:%s\n",newpath);

	unsigned int idInode;
	struct INODE inode;
	bool isExist = Path2Inode(oldpath, &inode, &idInode);
	if (!isExist) return -ENOSPC;

	char buf[BLOCK_SIZE] = {'\0'};

	char oldFatherPath[1024] = {'\0'};
	char oldDirName[MaxFileName] = {'\0'};
	GetFatherPath(oldpath, oldFatherPath, oldDirName);
	struct INODE oldFatherInode;
	unsigned int oldFatherId;
	isExist = Path2Inode(oldFatherPath, &oldFatherInode, &oldFatherId);
	if (!isExist) return -ENOSPC;

	bool flag = false;
	for (int j = 0; j < 12 && !flag; ++j) {
		unsigned int idBlock = oldFatherInode.DirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) continue;
			struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + k * sizeof(struct FILENAME)));
			if (strcmp(oldDirName, nxtfile.filename) == 0) {
				flag = true;
				tmp ^= (1ll << k);
				memcpy(buf, &tmp, sizeof(unsigned long long));
				disk_write(idBlock, buf);
				break;
			}
		}
	}
	for (int j = 0; j < 2 && !flag; ++j) {
		unsigned int idBlock = oldFatherInode.IndirectPointer[j];
		if (idBlock == 0) continue;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			disk_read(nxtidBlock, buf);
			unsigned long long tmp = *((unsigned long long *)buf);
			for (int p = 0; p < FilenamePerBlock; ++p) {
				if ((tmp & (1ll << p)) == 0) continue;
				struct FILENAME nxtfile = *((struct FILENAME *)(buf + 8 + p * sizeof(struct FILENAME)));
				if (strcmp(oldDirName, nxtfile.filename) == 0) {
					flag = true;
					tmp ^= (1ll << k);
					memcpy(buf, &tmp, sizeof(unsigned long long));
					disk_write(idBlock, buf);
					break;
				}
			}
		}
	}



	char newFatherPath[1024] = {'\0'};
	char newDirName[MaxFileName] = {'\0'};
	GetFatherPath(newpath, newFatherPath, newDirName);
	struct INODE newFatherInode;
	unsigned int newFatherId;
	isExist = Path2Inode(newFatherPath, &newFatherInode, &newFatherId);
	if (!isExist) return -ENOSPC;

	flag = false;
	for (int i = 0; i < 12 && !flag; ++i) {
		unsigned int idBlock;
		idBlock = newFatherInode.DirectPointer[i];
		if (idBlock == 0) {
			idBlock = GetFreeDataBlock();
			if (idBlock == -1) continue;
			newFatherInode.DirectPointer[i] = idBlock;
			memset(buf, 0, sizeof(buf));
			ChangeBlockStatus(idBlock);
		}
		else disk_read(idBlock, buf);
		unsigned long long tmp = *((unsigned long long *)buf);
		for (int k = 0; k < FilenamePerBlock; ++k) {
			if ((tmp & (1ll << k)) == 0) {
				struct FILENAME file;
				memcpy(file.filename, newDirName, sizeof(newDirName));
				file.position = idInode;
				tmp |= (1ll << k);
				memcpy(buf, &tmp, sizeof(unsigned long long));
				memcpy(buf + 8 + k * sizeof(struct FILENAME), &file, sizeof(struct FILENAME));
				flag = true;
				disk_write(idBlock, buf);
				break;
			}
		}
	}
	WriteInode(newFatherId, &newFatherInode);

	return 0;
}

int fs_truncate (const char *path, off_t size) {
	printf("Truncate is called: %s %ld\n", path, size);

	struct INODE inode;
	unsigned int id;
	bool isExist = Path2Inode(path, &inode, &id);
	if (!isExist) return -ENOENT;

	inode.CreateTime = time(NULL);
	off_t delta = 0;
	inode.size = size;
	if (size > inode.numBlocks * BLOCK_SIZE) {
		delta = size - inode.numBlocks * BLOCK_SIZE;
	}
	while (delta > 0) {
		if (delta <= BLOCK_SIZE) delta = 0;
		else delta -= BLOCK_SIZE;
		bool flag = false;
		for (int i = 0; i < 12 && !flag; ++i) {
			unsigned int idBlock;
			idBlock = inode.DirectPointer[i];
			if (idBlock == 0) {
				flag = true;
				idBlock = GetFreeDataBlock();
				if (idBlock == -1) return -ENOSPC;
				inode.DirectPointer[i] = idBlock;
				ChangeBlockStatus(idBlock);
				inode.numBlocks += 1;
				break;
			}
		}
		char buf[BLOCK_SIZE] = {'\0'};
		for (int i = 0; i < 2 && !flag; ++i) {
			int idBlock = inode.IndirectPointer[i];
			if (idBlock == 0) {
				idBlock = GetFreeDataBlock();
				if (idBlock == -1) continue;
				inode.IndirectPointer[i] = idBlock;
				memset(buf, 0, sizeof(buf));
				ChangeBlockStatus(idBlock);
			}
			else disk_read(idBlock, buf);
			for (int k = 0; k < PointerPerBlock && !flag; ++k) {
				char buf1[BLOCK_SIZE] = {'\0'};
				unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
				if (nxtidBlock == 0) {
					nxtidBlock = GetFreeDataBlock();
					if (nxtidBlock == -1) continue;
					memset(buf1, 0, sizeof(buf1));
					ChangeBlockStatus(nxtidBlock);
					memcpy(buf + k * 4, &nxtidBlock, sizeof(unsigned int));
					inode.numBlocks += 1;
					flag = true;
					disk_write(idBlock, buf);
					disk_write(nxtidBlock, buf1);
					break;
				}
			}
		}
	}
	WriteInode(id, &inode);
	printf ("Finish fs_truncate\n");
	return 0;
}

int fs_write (const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("Write is called:%s\n",path);

	char buf[BLOCK_SIZE] = {'\0'};
	unsigned int idInode;
	struct INODE inode;
	bool isExist = Path2Inode(path, &inode, &idInode);
	if (!isExist) return -ENOENT;

	if (fi->flags == O_APPEND) offset = inode.size;
	int rv = fs_truncate(path, max(offset + size, inode.size));
	if (rv != 0) return 0;

	isExist = Path2Inode(path, &inode, &idInode);
	if (!isExist) return -ENOENT;

	size_t st = offset, en = offset + size - 1;
	unsigned idst = st / BLOCK_SIZE, iden = en / BLOCK_SIZE;
	int nowid = 0;
	int haveWrite = 0, haveScan = 0;
	int rst, ren;
	for (int i = 0; i < 12; ++i, nowid++, haveScan += BLOCK_SIZE) {
		if (nowid < idst || iden < nowid) continue;
		if (nowid == idst) rst = st % BLOCK_SIZE;
		else rst = 0;
		int maxn = BLOCK_SIZE;
		if (haveScan + BLOCK_SIZE > inode.size) {
			maxn = inode.size - haveScan;
		}
		if (nowid == iden) ren = min(en % BLOCK_SIZE, maxn);
		else ren = maxn;

		unsigned int idBlock = inode.DirectPointer[i];
		if (idBlock == 0) break;
		disk_read(idBlock, buf);
		memcpy(buf + rst, buffer + haveWrite, ren - rst + 1);

		char tmp[40960] = {'\0'};
		memcpy(tmp, buffer + haveWrite, ren - rst + 1);
		tmp[ren - rst + 1] = '\0';
		printf ("test write buffer: [%s]\n", tmp);
		printf ("test write parameter: %ld %ld %d %d %d %d %ld\n", st, en, rst, ren, ren - rst + 1, haveWrite, size);

		disk_write(idBlock, buf);
		haveWrite += ren - rst + 1;
	}
	for (int j = 0; j < 2; ++j) {
		unsigned int idBlock = inode.IndirectPointer[j];
		if (idBlock == 0) break;
		disk_read(idBlock, buf);
		for (int k = 0; k < PointerPerBlock; ++k, ++nowid, haveScan += BLOCK_SIZE) {
			unsigned int nxtidBlock = *((unsigned int *)(buf + k * 4));
			if (nxtidBlock == 0) continue;
			if (nowid < idst || iden < nowid) continue;

			if (nowid == idst) rst = st % BLOCK_SIZE;
			else rst = 0;
			int maxn = BLOCK_SIZE;
			if (haveScan + BLOCK_SIZE > inode.size) {
				maxn = inode.size - haveScan;
			}
			if (nowid == iden) ren = min(en % BLOCK_SIZE, maxn);
			else ren = maxn;

			char buf1[BLOCK_SIZE] = {'\0'};
			disk_read(nxtidBlock, buf1);
			memcpy(buf1 + rst, buffer + haveWrite, ren - rst + 1);
			disk_write(nxtidBlock, buf1);
			haveWrite += ren - rst + 1;
		}
	}

	inode.ModifiedTime = time(NULL);
	inode.CreateTime = time(NULL);
	WriteInode(idInode, &inode);
	printf("Finish fs_write\n");

	return haveWrite;
}

int fs_utime (const char *path, struct utimbuf *buffer) {
	printf("Utime is called:%s\n",path);

	struct INODE inode;
	unsigned int id;
	bool isExist = Path2Inode(path, &inode, &id);
	if (!isExist) return -ENOENT;

	inode.AccessTime = buffer->actime;
	inode.ModifiedTime = buffer->modtime;

	WriteInode(id, &inode);

	return 0;
}

int fs_statfs (const char *path, struct statvfs *stat) {
	printf("Statfs is called:%s\n",path);
	
	struct SUPERBLOCK sb;
	GetSuperBlock(&sb);

	stat->f_bsize = BLOCK_SIZE;
	stat->f_blocks = BLOCK_NUM;
	stat->f_bfree = sb.numDateBlocks;
	stat->f_bavail = stat->f_bfree;
	stat->f_files = idInodeEnd - idInodeStart + 1;
	stat->f_ffree = sb.numInodes;
	stat->f_favail = stat->f_ffree;
	stat->f_namemax = MaxFileName;

	return 0;
}

int fs_open (const char *path, struct fuse_file_info *fi) {
	printf("Open is called:%s\n",path);

	return 0;
}

//Functions you don't actually need to modify
int fs_release (const char *path, struct fuse_file_info *fi)
{
	printf("Release is called:%s\n",path);
	return 0;
}

int fs_opendir (const char *path, struct fuse_file_info *fi)
{
	printf("Opendir is called:%s\n",path);
	return 0;
}

int fs_releasedir (const char * path, struct fuse_file_info *fi)
{
	printf("Releasedir is called:%s\n",path);
	return 0;
}

static struct fuse_operations fs_operations = {
	.getattr    = fs_getattr,
	.readdir    = fs_readdir,
	.read       = fs_read,
	.mkdir      = fs_mkdir,
	.rmdir      = fs_rmdir,
	.unlink     = fs_unlink,
	.rename     = fs_rename,
	.truncate   = fs_truncate,
	.utime      = fs_utime,
	.mknod      = fs_mknod,
	.write      = fs_write,
	.statfs     = fs_statfs,
	.open       = fs_open,
	.release    = fs_release,
	.opendir    = fs_opendir,
	.releasedir = fs_releasedir
};

int main(int argc, char *argv[]) {
	if(disk_init())
		{
		printf("Can't open virtual disk!\n");
		return -1;
		}
	if(mkfs())
		{
		printf("Mkfs failed!\n");
		return -2;
		}
	printf ("test size of superblock: %lu\n", sizeof(struct SUPERBLOCK));
	printf ("test size of inode: %lu\n", sizeof(struct INODE));
	printf ("test file per block %ld\n", FilenamePerBlock);
    return fuse_main(argc, argv, &fs_operations, NULL);
}
