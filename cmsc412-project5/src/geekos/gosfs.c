/**
 * gosfs.c
 * Author: Eric Norris <erictnorris@gmail.com>
 **/
 
#include <limits.h>
#include <geekos/errno.h>
#include <geekos/kassert.h>
#include <geekos/screen.h>
#include <geekos/malloc.h>
#include <geekos/string.h>
#include <geekos/bitset.h>
#include <geekos/synch.h>
#include <geekos/bufcache.h>
#include <geekos/list.h>
#include <geekos/gosfs.h>
#include <geekos/vfs.h>
#include <geekos/string.h>

static struct Mount_Point_Ops s_gosfsMountPointOps = { &GOSFS_Open,
        &GOSFS_Create_Directory, &GOSFS_Open_Directory, &GOSFS_Stat,
        &GOSFS_Sync, &GOSFS_Delete };

static struct File_Ops s_gosfsFileOps = { &GOSFS_FStat, &GOSFS_Read,
        &GOSFS_Write, &GOSFS_Seek, &GOSFS_Close, 0 /* Read_Entry */
};

static struct File_Ops s_gosfsDirOps = { &GOSFS_FStat, 0, 0, &GOSFS_Seek,
        &GOSFS_Close, &GOSFS_Read_Entry /* Read_Entry */
};

/* Reads in an entire GOSFSBlock (4KB) and copies into dest */
static int readGOSFSBlock(struct Block_Device *dev, int gosfsBlock, void *dest,
        size_t numBytes) {
    char buffer[GOSFS_BLOCK_SIZE], *bufferPtr = buffer;
    int blockIndex = gosfsBlock * (GOSFS_BLOCK_SIZE / SECTOR_SIZE);
    int i, rc;

    for (i = 0; i < (GOSFS_BLOCK_SIZE / SECTOR_SIZE); i++) {
        rc = Block_Read(dev, blockIndex + i, bufferPtr);
        if (rc < 0)
            goto fail;
        bufferPtr += SECTOR_SIZE;
    }
    memcpy(dest, buffer, numBytes);
    return 0;

    fail: return rc;
}

/* Writes an entire GOSFSBlock (4KB) using the src */
static int writeGOSFSBlock(struct Block_Device *dev, int gosfsBlock, void *src,
        size_t numBytes) {
    char buffer[GOSFS_BLOCK_SIZE], *bufferPtr = buffer;
    int blockIndex = gosfsBlock * (GOSFS_BLOCK_SIZE / SECTOR_SIZE);
    int i, rc;

    memcpy(buffer, src, numBytes);
    for (i = 0; i < (GOSFS_BLOCK_SIZE / SECTOR_SIZE); i++) {
        rc = Block_Write(dev, blockIndex + i, bufferPtr);
        if (rc < 0)
            goto fail;
        bufferPtr += SECTOR_SIZE;
    }
    return 0;

    fail: return rc;
}

/* Looks up and returns a pointer to a file in a GOSFSdirectory */
static GOSFSfileNode* lookupFileInDirectory(GOSFSdirectory *ptr, char *file,
        size_t nameLength) {
    uint_t i;
    for (i = 0; i < MAX_FILES_PER_DIR; i++) {
        if (ptr->files[i].isUsed && strncmp(ptr->files[i].name, file,
                nameLength) == 0)
            return &ptr->files[i];
    }
    return 0;
}

/* Looks up the last parent directory for a given path. Malloc()s a directory
 * entry and returns the fileNode pointing to the directory (also Malloc()ed).
 * After returning, the path pointer refers to the last file in the original
 * path (E.G. path = /a/b/c/d.exe, afterwards path = d.exe).
 * NOTE: If the file is in the root directory, the returned fileNode will be
 * equal to NULL, as the root directory has no file node.
 */
static GOSFSfileNode* lookupDirectory(GOSFSinstance *instance,
        GOSFSdirectory **directory, char **path) {
    GOSFSfileNode *filePtr = 0, *fileNode = 0;
    GOSFSdirectory *dir;
    char *nextSlash;
    int rc, offset;

    /* Malloc data for file node and directory */
    fileNode = Malloc(sizeof(GOSFSfileNode));
    dir = Malloc(sizeof(GOSFSdirectory));
    if (fileNode == 0 || dir == 0)
        goto fail;

    /* Point to the root directory and read in contents... */
    readGOSFSBlock(instance->dev, instance->superblock->rootDirPointer, dir,
            sizeof(GOSFSdirectory));

    /* Iterate until there are no remaining slashes */
    while ((nextSlash = strchr(*path + 1, '/'))) {
        /* Ignores leading '/' character */
        *path += 1;
        offset = nextSlash - *path;

        filePtr = lookupFileInDirectory(dir, *path, offset);
        if (filePtr == 0 || !filePtr->isDirectory)
            goto fail;

        /* Copy file node data into fileNode */
        memcpy(fileNode, filePtr, sizeof(GOSFSfileNode));

        /* Read in the directory this file node represents */
        rc = readGOSFSBlock(instance->dev, fileNode->blocks[0], dir,
                sizeof(GOSFSdirectory));
        if (rc < 0)
            goto fail;

        *path = nextSlash;
    }
    if (filePtr == 0) {
        /* The loop never iterated, i.e. the file is in the root directory. */
        Free(fileNode);
        fileNode = 0;
    }
    /* Trims leading '/' character. */
    *path += 1;
    *directory = dir;
    return fileNode;

    fail: if (fileNode)
        Free(fileNode);
    if (dir)
        Free(dir);
    *directory = 0;
    return 0;
}

/* Creates and returns a pointer to a file created in a given directory,
 * changes are immmediately written to disk. */
static GOSFSfileNode* createFileInDirectory(GOSFSinstance *instance,
        GOSFSfileNode *dirFileNode, GOSFSdirectory *dir, char *name) {
    GOSFSfileNode *newFile;
    int i, rc, offset = -1, directoryBlock =
            instance->superblock->rootDirPointer;

    /* Make sure name is not too long */
    if (strlen(name) >= sizeof(dir->files[0].name))
        goto fail;

    /* First find a free spot in the directory */
    for (i = 0; i < MAX_FILES_PER_DIR; i++) {
        if (!dir->files[i].isUsed) {
            offset = i;
            break;
        }
    }
    if (offset == -1)
        /* No free space in directory */
        goto fail;

    /* Now set up the file node at that point */
    newFile = &dir->files[i];
    memset(newFile, '\0', sizeof(GOSFSfileNode));
    newFile->isUsed = true;
    memcpy(newFile->name, name, strlen(name));

    /* Figure out directory block index */
    if (dirFileNode)
        directoryBlock = dirFileNode->blocks[0];

    /* Attempt to write out this revised directory */
    rc = writeGOSFSBlock(instance->dev, directoryBlock, dir,
            sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;
    return newFile;

    fail: return 0;
}

/*
 * Format a drive with GOSFS.
 */
int GOSFS_Format(struct Block_Device *blockDev) {
    GOSFSsuperblock *superblock;
    GOSFSdirectory *rootDir;
    int rc;

    /* Allocate space for superblock */
    superblock = Malloc(sizeof(GOSFSsuperblock));
    rootDir = Malloc(sizeof(GOSFSdirectory));
    if (superblock == 0 || rootDir == 0)
        goto memfail;

    /* Zero out superblock and root directory for new filesystem */
    memset(superblock, '\0', sizeof(GOSFSsuperblock));
    memset(rootDir, '\0', sizeof(GOSFSdirectory));

    /* Set magic number and calculate disk size */
    superblock->magic = GOSFS_MAGIC;
    superblock->size = blockDev->ops->Get_Num_Blocks(blockDev) * SECTOR_SIZE;

    /* Write the root directory to block 1 (following the superblock */
    superblock->rootDirPointer = 1;
    rc = writeGOSFSBlock(blockDev, 1, rootDir, sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;

    /* Mark the superblock (0) and root dir block as taken */
    Set_Bit(superblock->freeBlocks, 0);
    Set_Bit(superblock->freeBlocks, 1);

    /* Write out the superblock */
    rc = writeGOSFSBlock(blockDev, GOSFS_SUPERBLOCK, superblock,
            sizeof(GOSFSsuperblock));
    if (rc < 0)
        goto fail;

    /* Free allocated structures */
    Free(superblock);
    Free(rootDir);
    return 0;

    memfail: rc = ENOMEM;
    goto fail;

    fail: if (superblock)
        Free(superblock);
    if (rootDir)
        Free(rootDir);
    return rc;
}

/*
 * Mount GOSFS. Return 0 on success, return < 0 on failure.
 * - Check that the magic number is correct.
 */
int GOSFS_Mount(struct Mount_Point *mountPoint) {
    GOSFSinstance *instance;
    GOSFSsuperblock *superblock;
    struct Block_Device *blockDev = mountPoint->dev;
    int rc;

    /* Allocate space in memory for the superblock */
    superblock = Malloc(sizeof(GOSFSsuperblock));
    if (superblock == 0)
        goto memfail;

    /* Read in the superblock (block index = 0) */
    rc = readGOSFSBlock(blockDev, GOSFS_SUPERBLOCK, superblock,
            sizeof(GOSFSsuperblock));
    if (rc < 0)
        goto fail;

    /* Check the magic number */
    if (superblock->magic != GOSFS_MAGIC) {
        Print("Bad magic number (%x) for GOSFS filesystem\n",
                superblock->magic);
        goto invalidfs;
    }

    /* Allocate space in memory for the filesystem data */
    instance = Malloc(sizeof(GOSFSinstance));
    if (instance == 0)
        goto memfail;

    /* Success - fill in mount point ops, filesystem data */
    mountPoint->ops = &s_gosfsMountPointOps;
    instance->superblock = superblock;
    instance->dev = blockDev;
    mountPoint->fsData = instance;
    return 0;

    memfail: rc = ENOMEM;
    goto fail;

    invalidfs: rc = EINVALIDFS;
    goto fail;

    fail: if (superblock != 0)
        Free(superblock);
    return rc;
}

/*
 * Get metadata for given File. Called with a file descriptor.
 */
int GOSFS_FStat(struct File *file, struct VFS_File_Stat *stat) {
    GOSFSfileNode *node = &((GOSFSptr *) file->fsData)->node;
    stat->isDirectory = node->isDirectory;
    stat->isSetuid = node->isSetUid;
    stat->size = node->size;
    memcpy(&stat->acls, &node->acls, sizeof(node->acls));

    return 0;
}

/*
 * Open a file with the given name and mode.
 * Return > 0 on success, < 0 on failure (e.g. does not exist).
 */
int GOSFS_Open(struct Mount_Point *mountPoint, const char *path, int mode,
        struct File **pFile) {
    GOSFSinstance *instance = (GOSFSinstance *) mountPoint->fsData;
    GOSFSdirectory *dir = 0;
    GOSFSfileNode *dirFileNode = 0, *fileNode = 0;
    GOSFSptr *cached;
    struct File *file;
    char *filePath = path;
    int rc;

    cached = Malloc(sizeof(GOSFSptr));
    if (cached == 0)
        goto memfail;

    dirFileNode = lookupDirectory(instance, &dir, &filePath);
    if (dirFileNode == 0 && dir == 0) {
        /* Complete failure. */
        rc = EACCESS;
        goto fail;
    }

    /* Now lookup desired file in the directory. */
    fileNode = lookupFileInDirectory(dir, filePath, strlen(filePath));
    if (fileNode == 0) {
        /* File does not exist. */
        if (mode & O_CREATE) {
            /* Create the file in this directory. */
            fileNode = createFileInDirectory(instance, dirFileNode, dir,
                    filePath);
            if (fileNode == 0) {
                rc = EACCESS;
                goto fail;
            }
        } else {
            rc = EACCESS;
            goto fail;
        }
    }

    /* Fill in GOSFSptr fields */
    if (dirFileNode)
        /* Non-root directory */
        cached->blockNum = dirFileNode->blocks[0];
    else
        /* Root directory */
        cached->blockNum = instance->superblock->rootDirPointer;

    /* Calculate offset via ptr arithmetic */
    cached->offset = fileNode - dir->files;

    /* Copy over file node */
    memcpy(&cached->node, fileNode, sizeof(GOSFSfileNode));

    /* Allocate file for VFS */
    file = Allocate_File(&s_gosfsFileOps, 0, fileNode->size, cached, mode,
            mountPoint);
    if (file == 0)
        goto memfail;

    /* Success! */
    *pFile = file;
    if (dirFileNode)
        Free(dirFileNode);
    Free(dir);
    return 0;

    memfail: rc = ENOMEM;
    goto fail;

    fail: if (dirFileNode)
        Free(dirFileNode);
    if (dir)
        Free(dir);
    if (cached)
        Free(cached);
    return rc;
}

#define INDIRECT_BLOCK                  8
#define DOUBLE_INDIRECT_BLOCK           9
#define INDIRECT_BLOCK_START            8
#define DOUBLE_INDIRECT_BLOCK_START     (INDIRECT_BLOCK_START + (GOSFS_BLOCK_SIZE / sizeof(int)))
#define MAX_BLOCK                       (DOUBLE_INDIRECT_BLOCK_START + ((GOSFS_BLOCK_SIZE / sizeof(int))^2))

/* Returns the actual disk block a "virtual" block number refers to, e.g. block
 * nine refers to the second block from the indirect block list.
 */
static int getPhysicalBlockNum(GOSFSinstance *instance, GOSFSfileNode *node,
        int block) {
    int *indirectBlock = 0;
    int *doubleIndirectBlock = 0;
    int rc;

    if (block < INDIRECT_BLOCK_START) {
        rc = node->blocks[block];
    } else if (block < DOUBLE_INDIRECT_BLOCK_START) {
        /* Allocate space for indirect block */
        indirectBlock = Malloc(GOSFS_BLOCK_SIZE);

        if (!indirectBlock)
            goto memfail;

        /* Read in indirect block */
        rc = readGOSFSBlock(instance->dev, node->blocks[INDIRECT_BLOCK],
                indirectBlock, GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;

        /* Return actual block */
        rc = indirectBlock[block - INDIRECT_BLOCK_START];
    } else if (block < MAX_BLOCK) {
        /* Allocate space for indirect and double indirect block */
        indirectBlock = Malloc(GOSFS_BLOCK_SIZE);
        doubleIndirectBlock = Malloc(GOSFS_BLOCK_SIZE);

        if (!indirectBlock || !doubleIndirectBlock)
            goto memfail;

        /* Read in indirect block */
        rc = readGOSFSBlock(instance->dev, node->blocks[INDIRECT_BLOCK],
                indirectBlock, GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;

        /* Now find block for double indirect block */
        rc = indirectBlock[(block - DOUBLE_INDIRECT_BLOCK_START)
                / (GOSFS_BLOCK_SIZE / sizeof(int))];

        /* Read in double indirect block */
        rc = readGOSFSBlock(instance->dev, rc, doubleIndirectBlock,
                GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;

        /* Return actual block */
        rc = doubleIndirectBlock[(block - DOUBLE_INDIRECT_BLOCK_START)
                % (GOSFS_BLOCK_SIZE / sizeof(int))];
    } else {
        /* This would be a triple indirect and above */
        rc = EINVALID;
    }
    if (indirectBlock)
        Free(indirectBlock);
    if (doubleIndirectBlock)
        Free(doubleIndirectBlock);
    return rc;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (indirectBlock)
        Free(indirectBlock);
    if (doubleIndirectBlock)
        Free(doubleIndirectBlock);
    return rc;
}

/*
 * Read data from current position in file.
 * Return > 0 on success, < 0 on failure.
 */
int GOSFS_Read(struct File *file, void *buf, ulong_t numBytes) {
    GOSFSinstance *instance = (GOSFSinstance *) file->mountPoint->fsData;
    GOSFSptr *filePtr = (GOSFSptr *) file->fsData;
    ulong_t start = file->filePos;
    ulong_t end = file->filePos + numBytes;
    ulong_t vblock, seekPos, offset, length;
    int rc, physBlock;
    char *blockBuf = 0;

    /* Its okay to attempt to read past the end of the file */
    if (end > file->endPos) {
        numBytes = file->endPos - file->filePos;
        end = file->endPos;
    }

    /* Copied from pfat.c, its just not possible to read more than INT_MAX */
    if (numBytes > INT_MAX) {
        rc = EINVALID;
        goto fail;
    }

    /* Obvious conditions */
    if (start >= file->endPos || end < start || !(file->mode & O_READ)) {
        rc = EINVALID;
        goto fail;
    }

    /* Malloc data for the temporary block buffer */
    blockBuf = Malloc(GOSFS_BLOCK_SIZE * sizeof(char));
    if (!blockBuf)
        goto memfail;

    /* vblock represents the "virtual" block address */

    seekPos = start;
    while (numBytes) {
        vblock = seekPos / GOSFS_BLOCK_SIZE;
        offset = seekPos % GOSFS_BLOCK_SIZE;
        /* Calculate the number of bytes to be read */
        length = GOSFS_BLOCK_SIZE - offset;
        if (length > numBytes)
            length = numBytes;

        /* Read data into buffer from block */
        physBlock = getPhysicalBlockNum(instance, &filePtr->node, vblock);
        if (physBlock < 0)
            goto fail;
        rc = readGOSFSBlock(instance->dev, physBlock, blockBuf,
                GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;

        /* Copy data into user buffer */
        memcpy(buf, blockBuf + offset, length);

        /* Move forward in file */
        numBytes -= length;
        buf += length;
        seekPos += length;
    }

    /* Now we need to update the file seek position */
    file->filePos = seekPos;

    /* Free the temporary buffer */
    Free(blockBuf);

    /* Return number of bytes read */
    return seekPos - start;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (blockBuf)
        Free(blockBuf);
    return rc;
}

/* Grows a file by one block (from the end position) and, if necessary,
 * allocates indirect or double indirect blocks to contain this
 * new block.
 */
static int growFile(struct File *file) {
    GOSFSinstance *instance = (GOSFSinstance *) file->mountPoint->fsData;
    GOSFSsuperblock *superblock = instance->superblock;
    GOSFSptr *filePtr = (GOSFSptr *) file->fsData;
    GOSFSfileNode *node = &filePtr->node;
    GOSFSdirectory *dir;
    ulong_t nextBlock = file->endPos / GOSFS_BLOCK_SIZE;
    char *buf;
    int *indirect = 0, *doubleIndirect = 0;
    int rc, newBlock;

    buf = Malloc(GOSFS_BLOCK_SIZE);
    indirect = Malloc(GOSFS_BLOCK_SIZE);
    doubleIndirect = Malloc(GOSFS_BLOCK_SIZE);
    dir = Malloc(sizeof(GOSFSdirectory));
    if (!buf || !indirect || !doubleIndirect || !dir)
        goto memfail;

    if (nextBlock < INDIRECT_BLOCK_START) {
        /* This is a simple direct block */
        rc = Find_First_Free_Bit(superblock->freeBlocks, superblock->size);
        if (rc < 0)
            goto fail;

        node->blocks[nextBlock] = rc;
        newBlock = rc;
    } else if (nextBlock < DOUBLE_INDIRECT_BLOCK_START) {
        /* Check for indirect block existence */
        if (node->blocks[INDIRECT_BLOCK] == 0) {
            /* Need to create indirect block */
            rc = Find_First_Free_Bit(superblock->freeBlocks, superblock->size);
            if (rc < 0)
                goto fail;

            /* Update (cached) file node */
            node->blocks[INDIRECT_BLOCK] = rc;
            newBlock = rc;

            /* Write out zero-filled block */
            memset(buf, '\0', GOSFS_BLOCK_SIZE);
            rc
                    = writeGOSFSBlock(instance->dev, newBlock, buf,
                            GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;

            /* Claim bit */
            Set_Bit(superblock->freeBlocks, newBlock);

            /* "Read" indirect block by copying it to array */
            memcpy(indirect, buf, GOSFS_BLOCK_SIZE);
        } else {
            /* Read in indirect block */
            rc = readGOSFSBlock(instance->dev, node->blocks[INDIRECT_BLOCK],
                    indirect, GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;
        }

        rc = Find_First_Free_Bit(superblock->freeBlocks, superblock->size);
        if (rc < 0)
            goto fail;

        /* Update block array */
        indirect[nextBlock - INDIRECT_BLOCK_START] = rc;
        newBlock = rc;

        /* Write out updated block array to disk */
        rc = writeGOSFSBlock(instance->dev, node->blocks[INDIRECT_BLOCK],
                indirect, GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;
    } else if (nextBlock < MAX_BLOCK) {
        /* Check for indirect block existence */
        if (node->blocks[DOUBLE_INDIRECT_BLOCK] == 0) {
            /* Need to create indirect block */
            rc = Find_First_Free_Bit(superblock->freeBlocks, superblock->size);
            if (rc < 0)
                goto fail;

            /* Update (cached) file node */
            node->blocks[DOUBLE_INDIRECT_BLOCK] = rc;
            newBlock = rc;

            /* Write out zero-filled block */
            memset(buf, '\0', GOSFS_BLOCK_SIZE);
            rc
                    = writeGOSFSBlock(instance->dev, newBlock, buf,
                            GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;

            /* Claim bit */
            Set_Bit(superblock->freeBlocks, newBlock);

            /* "Read" indirect block by copying it to array */
            memcpy(indirect, buf, GOSFS_BLOCK_SIZE);
        } else {
            /* Read in indirect block */
            rc = readGOSFSBlock(instance->dev,
                    node->blocks[DOUBLE_INDIRECT_BLOCK], indirect,
                    GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;
        }

        /* Check for double indirect block existence */
        if (indirect[(nextBlock - DOUBLE_INDIRECT_BLOCK_START)
                / (GOSFS_BLOCK_SIZE / sizeof(int))] == 0) {
            /* Need to create second indirect block */
            rc = Find_First_Free_Bit(superblock->freeBlocks, superblock->size);
            if (rc < 0)
                goto fail;

            /* Update indirect block */
            indirect[(nextBlock - DOUBLE_INDIRECT_BLOCK_START)
                    / (GOSFS_BLOCK_SIZE / sizeof(int))] = rc;
            newBlock = rc;

            Set_Bit(superblock->freeBlocks, newBlock);

            /* Write out zero-filled block */
            memset(buf, '\0', GOSFS_BLOCK_SIZE);
            rc
                    = writeGOSFSBlock(instance->dev, newBlock, buf,
                            GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;

            /* Write out updated indirect block */
            rc = writeGOSFSBlock(instance->dev,
                    node->blocks[DOUBLE_INDIRECT_BLOCK], indirect,
                    GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;

            /* "Read" double indirect block by copying array */
            memcpy(doubleIndirect, buf, GOSFS_BLOCK_SIZE);
        } else {
            /* Read in double indirect block */
            rc = indirect[(nextBlock - DOUBLE_INDIRECT_BLOCK_START)
                    / (GOSFS_BLOCK_SIZE / sizeof(int))];
            rc = readGOSFSBlock(instance->dev, rc, doubleIndirect,
                    GOSFS_BLOCK_SIZE);
            if (rc < 0)
                goto fail;
        }

        /* Now finally we can allocate a block for the double indirect layer */
        rc = Find_First_Free_Bit(superblock->freeBlocks, superblock->size);
        if (rc < 0)
            goto fail;

        doubleIndirect[(nextBlock - DOUBLE_INDIRECT_BLOCK_START)
                % (GOSFS_BLOCK_SIZE / sizeof(int))] = rc;
        newBlock = rc;

        /* Write out updated double indirect block */
        rc = indirect[(nextBlock - DOUBLE_INDIRECT_BLOCK_START)
                / (GOSFS_BLOCK_SIZE / sizeof(int))];
        rc = writeGOSFSBlock(instance->dev, rc, doubleIndirect,
                GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;
    } else {
        /* This would be triple indirect and above */
        rc = EINVALID;
        goto fail;
    }

    /* Now update file node on disk */
    rc = readGOSFSBlock(instance->dev, filePtr->blockNum, dir,
            sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;
    memcpy(&dir->files[filePtr->offset], node, sizeof(GOSFSfileNode));
    rc = writeGOSFSBlock(instance->dev, filePtr->blockNum, dir,
            sizeof(GOSFSdirectory));

    Set_Bit(superblock->freeBlocks, newBlock);

    /* Now update superblock */
    rc = writeGOSFSBlock(instance->dev, GOSFS_SUPERBLOCK, superblock,
            sizeof(GOSFSsuperblock));
    if (rc < 0)
        goto fail;

    Free(buf);
    Free(indirect);
    Free(doubleIndirect);
    Free(dir);
    return newBlock;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (buf)
        Free(buf);
    if (indirect)
        Free(indirect);
    if (doubleIndirect)
        Free(doubleIndirect);
    if (dir)
        Free(dir);
    return rc;
}

/*
 * Write data to current position in file.
 * Return > 0 on success, < 0 on failure.
 */
int GOSFS_Write(struct File *file, void *buf, ulong_t numBytes) {
    GOSFSinstance *instance = (GOSFSinstance *) file->mountPoint->fsData;
    GOSFSptr *filePtr = (GOSFSptr *) file->fsData;
    GOSFSdirectory *dir;
    ulong_t start = file->filePos, endPos = file->endPos;
    ulong_t seekPos, vblock, offset, length;
    int rc, physBlock;
    char *blockBuf;

    /* Do we even have write access? Is numBytes valid? */
    if (!(file->mode & O_WRITE) || numBytes > INT_MAX
            || filePtr->node.isDirectory)
        return EINVALID;

    blockBuf = Malloc(GOSFS_BLOCK_SIZE);
    if (!blockBuf)
        goto memfail;

    seekPos = start;
    while (numBytes) {
        vblock = seekPos / GOSFS_BLOCK_SIZE;
        offset = seekPos % GOSFS_BLOCK_SIZE;
        length = GOSFS_BLOCK_SIZE - offset;
        if (numBytes < length)
            length = numBytes;

        /* Do we need to allocate a new block? */
        if (offset == 0 && seekPos >= file->endPos) {
            /* We should grow the file */
            physBlock = growFile(file);
            if (physBlock < 0)
                goto fail;
        } else {
            physBlock = getPhysicalBlockNum(instance, &filePtr->node, vblock);
        }

        /* Next read in block into buffer */
        rc = readGOSFSBlock(instance->dev, physBlock, blockBuf,
                GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;

        /* Memcpy() over to replace buffer data */
        memcpy(blockBuf + offset, buf, length);

        /* Write out buffer back to disk */
        rc = writeGOSFSBlock(instance->dev, physBlock, blockBuf,
                GOSFS_BLOCK_SIZE);
        if (rc < 0)
            goto fail;

        /* Increment position */
        seekPos += length;
        buf += length;
        numBytes -= length;
        if (seekPos > file->endPos)
            file->endPos = seekPos;
    }

    /* Update the file position */
    file->filePos = seekPos;

    /* We may need to update the file size */
    if (endPos < file->endPos) {
        filePtr->node.size = file->endPos;

        dir = (GOSFSdirectory *) blockBuf;
        /* Now update file node on disk */
        rc = readGOSFSBlock(instance->dev, filePtr->blockNum, dir,
                sizeof(GOSFSdirectory));
        if (rc < 0)
            goto fail;
        memcpy(&dir->files[filePtr->offset], &filePtr->node,
                sizeof(GOSFSfileNode));
        rc = writeGOSFSBlock(instance->dev, filePtr->blockNum, dir,
                sizeof(GOSFSdirectory));
    }

    Free(blockBuf);

    return seekPos - start;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (blockBuf)
        Free(blockBuf);
    return rc;
}

/*
 * Get metadata for given file. Need to find the file from the given path.
 */
int GOSFS_Stat(struct Mount_Point *mountPoint, const char *path,
        struct VFS_File_Stat *stat) {
    GOSFSinstance *instance = (GOSFSinstance *) mountPoint->fsData;
    GOSFSfileNode *node;
    struct File *file;
    int rc;

    /* Locate and psuedo-open the file */
    rc = GOSFS_Open(mountPoint, path, 0, &file);
    if (rc < 0)
        return rc;

    /* Fill in the stat fields */
    node = &((GOSFSptr *) file->fsData)->node;
    stat->isDirectory = node->isDirectory;
    stat->isSetuid = node->isSetUid;
    stat->size = node->size;
    memcpy(&stat->acls, &node->acls, sizeof(node->acls));

    /* Free up resources */
    GOSFS_Close(file);
    Free(file);

    return 0;
}

/*
 * Synchronize the filesystem data with the disk
 * (i.e., flush out all buffered filesystem data).
 */
int GOSFS_Sync(struct Mount_Point *mountPoint) {
    /* No-op */
    return 0;
}

/*
 * Close a file.
 */
int GOSFS_Close(struct File *file) {
    Free(file->fsData);
    return 0;
}

/*
 * Create a directory named by given path.
 */
int GOSFS_Create_Directory(struct Mount_Point *mountPoint, const char *path) {
    GOSFSinstance *instance = (GOSFSinstance *) mountPoint->fsData;
    GOSFSfileNode *filePtr = 0;
    GOSFSdirectory *dir, *newDir;
    char *file = path, *nextSlash;
    int rc, offset, i;
    uint_t parentBlock;

    /* Malloc data for file node and directory */
    newDir = Malloc(sizeof(GOSFSdirectory));
    dir = Malloc(sizeof(GOSFSdirectory));
    if (dir == 0 || newDir == 0)
        goto memfail;

    /* Point to the root directory and read in contents... */
    parentBlock = instance->superblock->rootDirPointer;
    readGOSFSBlock(instance->dev, parentBlock, dir, sizeof(GOSFSdirectory));

    /* Iterate until there are no remaining slashes */
    while ((nextSlash = strchr(file + 1, '/'))) {
        /* Ignores leading '/' character */
        file += 1;
        offset = nextSlash - file;

        filePtr = lookupFileInDirectory(dir, file, offset);
        if (!filePtr) {
            /* Need to create a directory */
            /* Make sure name is not too long */
            if (offset >= sizeof(dir->files[0].name))
                goto fail;

            /* First find a free spot in the directory */
            for (i = 0; i < MAX_FILES_PER_DIR; i++) {
                if (!dir->files[i].isUsed) {
                    offset = i;
                    break;
                }
            }
            if (offset == -1)
                /* No free space in directory */
                goto fail;

            /* Now set up the file node at that point */
            filePtr = &dir->files[i];
            memset(filePtr, '\0', sizeof(GOSFSfileNode));
            filePtr->isUsed = true;
            filePtr->isDirectory = true;
            memcpy(filePtr->name, file, offset);

            /* Allocate an empty block for this directory */
            rc = Find_First_Free_Bit(instance->superblock->freeBlocks,
                    instance->superblock->size);
            if (rc < 0)
                goto fail;

            Set_Bit(instance->superblock->freeBlocks, rc);

            /* Clear out this new directory */
            memset(newDir, '\0', sizeof(GOSFSdirectory));

            filePtr->blocks[0] = rc;
            rc = writeGOSFSBlock(instance->dev, rc, newDir,
                    sizeof(GOSFSdirectory));
            if (rc < 0)
                goto fail;

            /* Now write out the updated parent directory to disk */
            rc = writeGOSFSBlock(instance->dev, parentBlock, dir,
                    sizeof(GOSFSdirectory));
            if (rc < 0)
                goto fail;
        }
        if (!filePtr->isDirectory)
            goto fail;

        /* Read in the directory this file node represents */
        parentBlock = filePtr->blocks[0];
        rc = readGOSFSBlock(instance->dev, parentBlock, dir,
                sizeof(GOSFSdirectory));
        if (rc < 0)
            goto fail;

        file = nextSlash;
    }
    file += 1;

    /* Make sure this doesn't exist */
    filePtr = lookupFileInDirectory(dir, file, strlen(file));
    if (filePtr)
        goto fail;

    /* Create the final directory */
    /* Make sure name is not too long */
    if (strlen(file) >= sizeof(dir->files[0].name))
        goto fail;

    /* First find a free spot in the directory */
    for (i = 0; i < MAX_FILES_PER_DIR; i++) {
        if (!dir->files[i].isUsed) {
            offset = i;
            break;
        }
    }
    if (offset == -1)
        /* No free space in directory */
        goto fail;

    /* Now set up the file node at that point */
    filePtr = &dir->files[i];
    memset(filePtr, '\0', sizeof(GOSFSfileNode));
    filePtr->isUsed = true;
    filePtr->isDirectory = true;
    memcpy(filePtr->name, file, strlen(file));

    /* Allocate an empty block for this directory */
    rc = Find_First_Free_Bit(instance->superblock->freeBlocks,
            instance->superblock->size);
    if (rc < 0)
        goto fail;

    Set_Bit(instance->superblock->freeBlocks, rc);

    /* Clear out this new directory */
    memset(newDir, '\0', sizeof(GOSFSdirectory));

    filePtr->blocks[0] = rc;
    rc = writeGOSFSBlock(instance->dev, rc, newDir, sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;

    /* Now write out the updated directory to disk */
    rc = writeGOSFSBlock(instance->dev, parentBlock, dir,
            sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;

    /* Write out the updated superblock */
    rc = writeGOSFSBlock(instance->dev, GOSFS_SUPERBLOCK, instance->superblock,
            sizeof(GOSFSsuperblock));
    if (rc < 0)
        goto fail;

    Free(dir);
    return 0;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (dir)
        Free(dir);
    return rc;
}

/*
 * Open a directory named by given path.
 */
int GOSFS_Open_Directory(struct Mount_Point *mountPoint, const char *path,
        struct File **pDir) {
    /* Psuedo-open the "file" */
    int rc = GOSFS_Open(mountPoint, path, 0, pDir);
    if (rc >= 0) {
        /* Change the "file" info to match a directory */
        (*pDir)->endPos = MAX_FILES_PER_DIR;
        (*pDir)->ops = &s_gosfsDirOps;
    }
    return rc;
}

/*
 * Seek to a position in file. Should not seek beyond the end of the file.
 */
int GOSFS_Seek(struct File *file, ulong_t pos) {
    if (pos > file->endPos)
        return -1;

    file->filePos = pos;

    return 0;
}

/*
 * Delete the given file.
 * Return > 0 on success, < 0 on failure.
 */
int GOSFS_Delete(struct Mount_Point *mountPoint, const char *path) {
    GOSFSinstance *instance = (GOSFSinstance *) mountPoint->fsData;
    GOSFSsuperblock *superblock = instance->superblock;
    struct File *file = 0;
    GOSFSdirectory *dir = 0;
    GOSFSptr *filePtr;
    ulong_t vblock, endBlock;
    int physBlock, rc, i;

    /* Psuedo-open the file */
    rc = GOSFS_Open(mountPoint, path, 0, &file);
    if (rc < 0)
        goto fail;

    dir = Malloc(sizeof(GOSFSdirectory));
    if (!dir)
        goto memfail;

    filePtr = (GOSFSptr *) file->fsData;

    /* Is this a directory? */
    if (filePtr->node.isDirectory) {
        /* Read in directory contents */
        rc = readGOSFSBlock(instance->dev, filePtr->node.blocks[0], dir,
                sizeof(GOSFSdirectory));
        if (rc < 0)
            goto fail;

        /* Confirm this is empty */
        rc = -1;
        for (i = 0; i < MAX_FILES_PER_DIR; i++) {
            if (dir->files[i].isUsed) {
                rc = i;
                break;
            }
        }
        if (rc != -1) {
            /* This directory isn't empty! */
            rc = EINVALID;
            goto fail;
        }

        /* Free single block */
        Clear_Bit(superblock->freeBlocks, filePtr->node.blocks[0]);
    } else {
        /* We need to reclaim all of the blocks used by this file */
        vblock = 0;
        endBlock = (file->endPos + GOSFS_BLOCK_SIZE - 1) / GOSFS_BLOCK_SIZE;
        while (vblock < endBlock) {
            physBlock = getPhysicalBlockNum(instance, &filePtr->node, vblock);
            Clear_Bit(superblock->freeBlocks, physBlock);
            vblock++;
        }
    }

    /* We should also clear out the file node in the directory */
    memset(&filePtr->node, '\0', sizeof(GOSFSfileNode));
    rc = readGOSFSBlock(instance->dev, filePtr->blockNum, dir,
            sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;
    memcpy(&dir->files[filePtr->offset], &filePtr->node, sizeof(GOSFSfileNode));
    rc = writeGOSFSBlock(instance->dev, filePtr->blockNum, dir,
            sizeof(GOSFSdirectory));

    /* Now we update the superblock */
    rc = writeGOSFSBlock(instance->dev, GOSFS_SUPERBLOCK, superblock,
            sizeof(GOSFSsuperblock));
    if (rc < 0)
        goto fail;

    GOSFS_Close(file);
    Free(file);
    Free(dir);
    return 1;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (file) {
        GOSFS_Close(file);
        Free(file);
    }
    if (dir)
        Free(dir);
    return rc;
}

/*
 * Read a directory entry from an open directory.
 * Return > 0 on success, < 0 on failure.
 */
int GOSFS_Read_Entry(struct File *dir, struct VFS_Dir_Entry *entry) {
    GOSFSinstance *instance = (GOSFSinstance *) dir->mountPoint->fsData;
    GOSFSptr *filePtr = (GOSFSptr *) dir->fsData;
    GOSFSdirectory *directory = 0;
    GOSFSfileNode *node;
    ulong_t index = dir->filePos;
    int rc;

    directory = Malloc(sizeof(GOSFSdirectory));
    if (!dir)
        goto memfail;

    /* Read in the directory */
    rc = readGOSFSBlock(instance->dev, filePtr->node.blocks[0], directory,
            sizeof(GOSFSdirectory));
    if (rc < 0)
        goto fail;

    /* Starting at index, find position of next entry */
    for (; index < MAX_FILES_PER_DIR; index++) {
        if (directory->files[index].isUsed) {
            node = &directory->files[index];
            memcpy(entry->name, node->name, sizeof(node->name));
            entry->stats.isDirectory = node->isDirectory;
            entry->stats.isSetuid = node->isSetUid;
            entry->stats.size = node->size;
            memcpy(&entry->stats.acls, &node->acls, sizeof(node->acls));
            dir->filePos += 1;
            return 0;
        }
    }
    rc = ENOTFOUND;
    goto fail;

    memfail: rc = ENOMEM;
    goto fail;
    fail: if (dir)
        Free(dir);
    return rc;
}

static struct Filesystem_Ops s_gosfsFilesystemOps = { &GOSFS_Format,
        &GOSFS_Mount, };

/* ----------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

void Init_GOSFS(void) {
    Register_Filesystem("gosfs", &s_gosfsFilesystemOps);
}
