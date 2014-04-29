
#include <xmem.h>
#include <RTClib.h>
#include <masstorage.h>

// forced on.
#ifndef _USE_LFN
#define _USE_LFN 1
#endif

#include <PCpartition/PCPartition.h>
#include <FAT/FAT.h>

#include <xmemUSBFS.h>
#include <xmemUSB.h>

#ifdef __AVR__
// flush, eof, truncate, tell, close

typedef struct {
        uint8_t op;
        uint8_t fd;
} __attribute__((packed)) fss_opfd;

// read, write

typedef struct {
        uint8_t op;
        uint8_t fd;
        uint16_t amount;
        uint8_t data;
} __attribute__((packed)) fss_opfd16;

// lseek

typedef struct {
        uint8_t op;
        uint8_t fd;
        uint32_t amount;
        uint8_t data;
} __attribute__((packed)) fss_opfd32dat;

// chmod, mkdir

typedef struct {
        uint8_t op;
        uint8_t mode;
        uint8_t drive;
        uint8_t colon;
        uint8_t path;
} __attribute__((packed)) fss_op8drcol;

typedef struct {
        uint8_t op;
        uint8_t drive;
        uint8_t colon;
        uint8_t path;
} __attribute__((packed)) fss_opdrcol;

typedef struct {
        uint8_t op;
        uint32_t data;
        uint8_t drive;
        uint8_t colon;
        uint8_t path;
} __attribute__((packed)) fss_op32drcol;

typedef struct {
        uint8_t op;
        uint8_t object;
        uint8_t mode;
        uint8_t drive;
        uint8_t colon;
        uint8_t path;
} __attribute__((packed)) fss_op88drcol;

/* common reply type */
typedef struct {
        FRESULT res;
        uint8_t dat;
} __attribute__((packed)) fss_r8;

//    * f_sync - Flush cached data
#define PIPE_FLUSH      0x00

//    * f_close - Close a file
#define PIPE_CLOSE      0x01

//    * f_read/f_readdir - Read file or a directory entry
#define PIPE_READ       0x02

//    * f_write - Write file
#define PIPE_WRITE      0x03

//    * f_lseek - Move read/write pointer, Expand file size
#define PIPE_LSEEK      0x04

//    * f_tell - Get the current read/write pointer
#define PIPE_TELL       0x05

//    * f_eof - Test for end-of-file on a file
#define PIPE_EOF        0x06

//    * f_truncate - Truncate file size
#define PIPE_TRUNC      0x07

//    * f_size - Get size of a file
#define PIPE_SIZE       0x08

//    * f_open/f_opendir - Open/Create a file or open a directory
#define PIPE_OPEN       0x09

//    * Sync -- Flushes all caches
#define PIPE_SYNC       0x0A

//    * f_unlink - Remove a file or directory
#define PIPE_UNLINK     0x0B

//    f_stat - Get file status
#define PIPE_STAT       0x0C

//    f_rename - Rename/Move a file or directory
#define PIPE_RENAME     0x0D

//    f_mkdir - Create a directory
#define PIPE_MKDIR      0x0E

//    f_chmod - Change attribute
#define PIPE_CHMOD      0x0F

//    f_utime - Change timestamp
#define PIPE_UTIME      0x10

//    f_getfree - Get free clusters
#define PIPE_FREE       0x11

//    * Path -> volume
#define PIPE_VOLUME     0x12

//    * Path/volume label
#define PIPE_MOUNT_LBL  0x13

//    * Path/volume label
#define PIPE_MOUNT_CNT  0x14
#endif
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#ifdef __AVR__
// fs errno...
uint8_t fs_err[USE_MULTIPLE_APP_API];
// NOTE: memory stream transaction pipes must be on the AVR RAM, therefore, global
// They are also static so that they are not reachable from other modules
static memory_stream to_usb_fs_task;
static memory_stream from_usb_fs_task;
#else
// fs errno...
uint8_t fs_err;
#endif
// fs handles...
static FIL *fhandles[_FS_LOCK];
static DIR *dhandles[_FS_LOCK];
// internal stuff
static PFAT *Fats[_VOLUMES];
static storage_t *sto[_VOLUMES];
static boolean last_ready[MAX_USB_MS_DRIVERS][MASS_MAX_SUPPORTED_LUN];
static boolean wasnull[MAX_USB_MS_DRIVERS];

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


namespace GenericFileSystem {

        /**
         * Set up transaction pipes
         *
         * @return 1 for success
         */
        int Setup(void) {
#ifdef __AVR__
                // initialize memory transaction pipes
                xmem::memory_init(&to_usb_fs_task);
                xmem::memory_init(&from_usb_fs_task);
#endif
                return 1;
        }

#ifdef __AVR__

        /**
         * Called from USB polling task. Warning: Huge ugly block of code!
         *
         * @return Zero = no messages, 1 = message processed
         */
        int process_fs_pipe(void) {

                if(xmem::memory_ready(&to_usb_fs_task)) {
                        DIRINFO *dptr;
                        uint8_t *ptr;
                        UINT bc;
                        FRESULT rc = FR_INVALID_PARAMETER; // default is an error
                        uint8_t *message = NULL;
                        FILINFO finfo;
                        uint16_t rs = sizeof (fss_r8);
                        fss_r8 *reply = (fss_r8 *)xmem::safe_malloc(rs); // all regular replies
#if _USE_LFN
                        char lfn[_MAX_LFN + 1];
                        finfo.lfname = lfn;
                        finfo.lfsize = _MAX_LFN;
#endif

                        // get message, process it
                        int len = xmem::memory_recv(&message, &to_usb_fs_task);
                        uint8_t i;
                        ptr = message;
                        if(len) {
                                uint8_t d = *ptr;
                                ptr++;
                                i = *ptr;
                                switch(d) {

                                        case PIPE_FLUSH:
                                        case PIPE_CLOSE:
                                        case PIPE_READ:
                                        case PIPE_WRITE:
                                        case PIPE_LSEEK:
                                        case PIPE_TELL:
                                        case PIPE_EOF:
                                        case PIPE_TRUNC:
                                        case PIPE_SIZE:
                                        {
                                                if(i < 1) {
                                                        break;
                                                }
                                                i--;
                                                ptr++;
                                                if(i < _FS_LOCK) {
                                                        if(fhandles[i]->fs != NULL) {

                                                                switch(d) {

                                                                                ///////////////////////////////////////

                                                                        case PIPE_FLUSH:
                                                                        {
                                                                                rc = FR_OK;
                                                                                f_sync(fhandles[i]);
                                                                                break;
                                                                        }

                                                                                ///////////////////////////////////////

                                                                        case PIPE_CLOSE: // close
                                                                        {
                                                                                rc = f_close(fhandles[i]);
                                                                                fhandles[i]->fs = NULL;
                                                                                break;
                                                                        }

                                                                                ///////////////////////////////////////

                                                                        case PIPE_READ: // read bytes
                                                                        {
                                                                                uint16_t *r_p = (uint16_t *)ptr;
                                                                                rs = *r_p + sizeof (fss_r8) + sizeof (uint16_t);
                                                                                free(reply);
                                                                                reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                                                ptr = &reply->dat;
                                                                                ptr += sizeof (uint16_t);
                                                                                rc = f_read(fhandles[i], ptr, *r_p, &bc);
                                                                                if(bc != rs) rc = FR_EOF;
                                                                                ptr -= sizeof (uint16_t);
                                                                                r_p = (uint16_t *)ptr;
                                                                                *r_p = bc;
                                                                                break;
                                                                        }

                                                                                ///////////////////////////////////////

                                                                        case PIPE_WRITE: // write bytes
                                                                        {
                                                                                fss_opfd16 *m = (fss_opfd16 *)message;
                                                                                rc = f_write(fhandles[i], &(m->data), m->amount, &bc);
                                                                                rs = sizeof (fss_r8) + sizeof (uint16_t);
                                                                                free(reply);
                                                                                reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                                                ptr = &reply->dat;
                                                                                uint16_t *c_p = (uint16_t *)ptr;
                                                                                *c_p = bc;
                                                                                break;
                                                                        }
                                                                                ///////////////////////////////////////

                                                                        case PIPE_LSEEK: // seek to 32 bit position (lseek)
                                                                        {
                                                                                DWORD *offset = (DWORD *)ptr;
                                                                                ptr += 4;
                                                                                FBYTE whence = *ptr;
                                                                                rc = f_clseek(fhandles[i], *offset, whence);
                                                                                break;
                                                                        }

                                                                                ///////////////////////////////////////

                                                                        case PIPE_TELL:
                                                                        {
                                                                                rc = FR_OK;
                                                                                rs = sizeof (fss_r8) + sizeof (uint32_t);
                                                                                free(reply);
                                                                                reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                                                // f_tell is a macro...
                                                                                uint32_t *f_z = (uint32_t *) & reply->dat;
                                                                                *f_z = f_tell(fhandles[i]);
                                                                                break;
                                                                        }

                                                                                ///////////////////////////////////////

                                                                        case PIPE_EOF:
                                                                                rc = FR_OK;
                                                                                reply->dat = f_eof(fhandles[i]);
                                                                                break;

                                                                                ///////////////////////////////////////

                                                                        case PIPE_TRUNC:
                                                                                rc = f_truncate(fhandles[i]);
                                                                                break;

                                                                                ///////////////////////////////////////

                                                                        case PIPE_SIZE:
                                                                                rc = FR_OK;
                                                                                rs = 5;
                                                                                rs = sizeof (fss_r8) + sizeof (uint32_t);
                                                                                reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                                                // f_size is a macro...
                                                                                uint32_t *f_z = (uint32_t *) & reply->dat;
                                                                                *f_z = f_size(fhandles[i]);
                                                                                break;
                                                                }
                                                        }
                                                } else {
                                                        i -= _FS_LOCK;
                                                        switch(d) {

                                                                case PIPE_CLOSE:
                                                                {
                                                                        dhandles[i]->fs = NULL;
                                                                        rc = FR_OK;
                                                                        break;
                                                                }

                                                                case PIPE_READ: // read bytes
                                                                {
                                                                        // read dir...
                                                                        if(dhandles[i]->fs != NULL) {
                                                                                rc = f_readdir(dhandles[i], &finfo);
                                                                                if(rc == FR_OK) {
                                                                                        if(finfo.fname[0]) {
                                                                                                free(reply);
                                                                                                rs = sizeof (fss_r8) + sizeof (DIRINFO);
                                                                                                reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                                                                ptr = (uint8_t *) & reply->dat;
                                                                                                dptr = (DIRINFO *)ptr;
                                                                                                dptr->fsize = finfo.fsize;
                                                                                                dptr->fdate = finfo.fdate;
                                                                                                dptr->ftime = finfo.ftime;
                                                                                                dptr->fattrib = finfo.fattrib;
                                                                                                strcpy((char *)dptr->fname, (char *)finfo.fname);
#if _USE_LFN
                                                                                                if(finfo.lfsize) {
                                                                                                        strcpy((char *)dptr->lfname, (char *)finfo.lfname);
                                                                                                } else {
                                                                                                        dptr->lfname[0] = 0x00;
                                                                                                }
#else
                                                                                                dptr->lfname[0] = 0x00;
#endif
                                                                                        } else {
                                                                                                rc = FR_EOF;
                                                                                        }
                                                                                }
                                                                        }
                                                                        break;
                                                                }
                                                        }
                                                }
                                                break;
                                        }
                                                ///////////////////////////////////////
                                                ///////////////////////////////////////
                                                ///////////////////////////////////////
                                                ///////////////////////////////////////
                                                ///////////////////////////////////////
                                                ///////////////////////////////////////

                                        case PIPE_OPEN: // open, return a handle
                                        {
                                                switch(*ptr) {
                                                        case 'f': // file
                                                                // mode r
                                                                // mode w/W
                                                                // mode x/X (r/w)
                                                                // W and X always create the file.
                                                        {
                                                                ptr++;
                                                                char *m = (char *)ptr;
                                                                ptr++;
                                                                char *name = (char *)ptr;
                                                                for(i = 0; i < _FS_LOCK; i++) {
                                                                        if(fhandles[i]->fs == NULL) break;
                                                                }
                                                                if(i < _FS_LOCK) {
                                                                        reply->dat = i + 1;
                                                                        switch(*m) {
                                                                                case 'r':
                                                                                        rc = f_open(fhandles[i], name, FA_READ);
                                                                                        break;
                                                                                case 'w':
                                                                                        rc = f_open(fhandles[i], name, FA_WRITE | FA_CREATE_ALWAYS);
                                                                                        break;
                                                                                case 'x':
                                                                                        rc = f_open(fhandles[i], name, FA_READ | FA_WRITE);
                                                                                        break;
                                                                                case 'W':
                                                                                        rc = f_open(fhandles[i], name, FA_WRITE);
                                                                                        break;
                                                                                case 'X':
                                                                                        rc = f_open(fhandles[i], name, FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
                                                                                        break;
                                                                        }
                                                                        if(rc != FR_OK) {
                                                                                reply->dat = 0;
                                                                                fhandles[i]->fs = NULL;
                                                                        }
                                                                }
                                                                break;
                                                        }
                                                        case 'd': // dir, mode is ignored, of course
                                                        {
                                                                ptr++;
                                                                for(i = 0; i < _FS_LOCK; i++) {
                                                                        if(dhandles[i]->fs == NULL) break;
                                                                }
                                                                if(i < _FS_LOCK) {
                                                                        reply->dat = 1 + i + _FS_LOCK;
                                                                        rc = f_opendir(dhandles[i], (TCHAR *)ptr);
                                                                        if(rc != FR_OK) {
                                                                                dhandles[i]->fs = NULL;
                                                                                reply->dat = 0;
                                                                        }
                                                                }
                                                                break;
                                                        }
                                                }

                                                break;
                                        }

                                                ///////////////////////////////////////

                                        case PIPE_SYNC:
                                        {
                                                for(i = 0; i < _FS_LOCK; i++) {
                                                        if(fhandles[i]->fs != NULL) {
                                                                f_sync(fhandles[i]);
                                                        }
                                                }
                                                for(i = 0; i < _VOLUMES; i++) {
                                                        if(Fats[i] != NULL) {
                                                                f_sync_fs(Fats[i]->ffs);
                                                                commit_fs(Fats[i]->ffs);
                                                        }
                                                }
                                                rc = FR_OK;
                                                break;
                                        }

                                                ///////////////////////////////////////

                                        case PIPE_UNLINK: // unlink
                                                rc = f_unlink((char *)ptr);
                                                break;

                                                ///////////////////////////////////////

                                        case PIPE_STAT:
                                        {
                                                FILINFO finfo;
                                                rc = f_stat((TCHAR *)ptr, &finfo);
                                                if(rc == FR_OK) {
                                                        rs = sizeof (FILINFO) + sizeof (fss_r8);
                                                        free(reply);
                                                        reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                        ptr = &reply->dat;
                                                        memcpy(ptr, &finfo, sizeof (FILINFO));
                                                }
                                                break;
                                        }

                                                ///////////////////////////////////////


                                        case PIPE_RENAME: // rename
                                        {
                                                char *path_old = (char *)ptr;
                                                char *path_new = path_old + 1 + strlen(path_old);
                                                rc = f_rename(path_old, path_new);
                                                break;
                                        }

                                                ///////////////////////////////////////

                                        case PIPE_MKDIR:
                                                bc = *ptr;
                                                ptr++;
                                                rc = f_mkdir((TCHAR *)ptr);
                                                if((rc == FR_OK) && bc) {
                                                        rc = f_chmod((TCHAR *)ptr, bc, AM_RDO | AM_ARC | AM_SYS | AM_HID);
                                                }
                                                break;

                                                ///////////////////////////////////////

                                        case PIPE_CHMOD:
                                                bc = *ptr;
                                                ptr++;
                                                rc = f_chmod((TCHAR *)ptr, bc, AM_RDO | AM_ARC | AM_SYS | AM_HID);
                                                break;

                                                ///////////////////////////////////////

                                        case PIPE_UTIME:
                                        {
                                                FILINFO finfo;
                                                uint32_t *fdate = (uint32_t *)ptr;
                                                finfo.fdate = *fdate;
                                                rc = f_utime((TCHAR *)ptr, &finfo);
                                                break;
                                        }

                                                ///////////////////////////////////////

                                        case PIPE_FREE:
                                        {
                                                FATFS *fs;
                                                uint32_t blksfree; // free blocks (AKA clusters)
                                                uint64_t bytesfree = 0llu;
                                                uint64_t *bfptr;
                                                rc = f_getfree((TCHAR *)ptr, &blksfree, &fs);
                                                if(rc == FR_OK) {
                                                        bytesfree = blksfree; // blocks
                                                        bytesfree *= fs->csize; // sectors
                                                        bytesfree *= fs->pfat->storage->SectorSize; // bytes
                                                        rs = sizeof (fss_r8) + sizeof (uint64_t);
                                                        free(reply);
                                                        reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                        bfptr = (uint64_t *) & reply->dat;
                                                        *bfptr = bytesfree;
                                                }
                                                break;
                                        }
                                                ///////////////////////////////////////


                                        case PIPE_VOLUME: // path -> volume
                                        {
                                                rc = FR_NO_PATH;
                                                reply->dat = _VOLUMES;
                                                // Check for exact match
                                                for(uint8_t x = 0; x < _VOLUMES; x++) {
                                                        if(Fats[x]) {
                                                                if(!strcasecmp((char *)Fats[x]->label, (char *)ptr)) {
                                                                        rc = FR_OK;
                                                                        reply->dat = Fats[x]->volmap; // should be equal to x
                                                                        break;
                                                                }
                                                        }
                                                }
                                                break;
                                        }

                                                ///////////////////////////////////////

                                        case PIPE_MOUNT_LBL: // get mount name
                                        {
                                                rc = FR_NO_PATH;
                                                reply->dat = 0x00;
                                                if(Fats[i]) {
                                                        rc = FR_OK;
                                                        rs = sizeof (fss_r8) + (strlen((char *)(Fats[i]->label)));
                                                        free(reply);
                                                        reply = (fss_r8 *)xmem::safe_malloc(rs);
                                                        ptr = (uint8_t *) & reply->dat;
                                                        strcpy((char *)ptr, (char *)(Fats[i]->label));
                                                }
                                                break;
                                        }
                                        case PIPE_MOUNT_CNT:
                                        {
                                                reply->dat = 0;
                                                rc = FR_OK;
                                                for(uint8_t x = 0; x < _VOLUMES; x++) {
                                                        if(Fats[x]) {
                                                                reply->dat++;
                                                        }
                                                }
                                                break;
                                        }


                                        default: // Huh?
                                                break;
                                }
                        }
                        free(message);
                        reply->res = rc;
                        xmem::memory_send((uint8_t *)reply, rs, &from_usb_fs_task);
                        free(reply);
                        return 1;
                }
                return 0;
                //else xmem::Yield();

        }
#endif

        /**
         * USB polling. This method checks the USB state, and watches for media
         * insertion and removal. It also automatically mounts newly
         * discovered media, and clears mounts for removed media.
         * On the AVR with XMEM2, it also processes the command queue requests.
         *
         * @param current_state current USB driver state
         * @param last_state last known USB driver state
         * @return Zero if no message was passed, else One
         */
        int Poll(uint8_t current_state, uint8_t last_state) {
                // check storage function pointers
                if(current_state != last_state) {
                        if(current_state != USB_STATE_RUNNING) {
                                for(int i = 0; i < MAX_USB_MS_DRIVERS; i++) {
                                        wasnull[i] = true;
                                        for(int j = 0; j < MASS_MAX_SUPPORTED_LUN; j++) {
                                                last_ready[i][j] = false;
                                        }
                                }
                                for(int i = 0; i < _VOLUMES; i++) {
                                        if(Fats[i] != NULL) {
                                                delete Fats[i];
                                                Fats[i] = NULL;
                                        }
                                        if(sto[i]->private_data != NULL) {
                                                delete sto[i]->private_data;
                                                sto[i]->private_data = NULL;
                                        }
                                }
                        }
                }

                // Check all drivers, and all luns...
                if(current_state == USB_STATE_RUNNING) {
                        //PCPartition *PT;
                        for(int B = 0; B < MAX_USB_MS_DRIVERS; B++) {
                                if(UHS_USB_BulkOnly[B] && UHS_USB_BulkOnly[B]->GetAddress()) {
                                        wasnull[B] = false;
                                        // Build a list.
                                        int ML = UHS_USB_BulkOnly[B]->GetbMaxLUN();
                                        ML++;
                                        for(int LUN = 0; LUN < ML; LUN++) {
                                                boolean OK = UHS_USB_BulkOnly[B]->LUNIsGood(LUN);
                                                // C++ can't compare boolean to a boolean -- stupidity++;
                                                if((last_ready[B][LUN] && !OK) || (!last_ready[B][LUN] && OK)) { // state changed
                                                        last_ready[B][LUN] = OK; //!last_ready[B][LUN];
                                                        if(OK) { // media inserted
                                                                int nm = (int)f_next_mount();
                                                                if(nm < _VOLUMES) {
                                                                        sto[nm]->private_data = new pvt_t;
                                                                        ((pvt_t *)sto[nm]->private_data)->lun = LUN;
                                                                        ((pvt_t *)sto[nm]->private_data)->B = B;
                                                                        sto[nm]->Initialize = *UHS_USB_BulkOnly_Initialize;
                                                                        sto[nm]->Reads = *UHS_USB_BulkOnly_Read;
                                                                        sto[nm]->Writes = *UHS_USB_BulkOnly_Write;
                                                                        sto[nm]->Status = *UHS_USB_BulkOnly_Status;
                                                                        sto[nm]->Commit = *UHS_USB_BulkOnly_Commit;
                                                                        sto[nm]->TotalSectors = UHS_USB_BulkOnly[B]->GetCapacity(LUN);
                                                                        sto[nm]->SectorSize = UHS_USB_BulkOnly[B]->GetSectorSize(LUN);
                                                                        PCPartition *PT = new PCPartition;
                                                                        if(!PT->Init(sto[nm])) {
                                                                                delete sto[nm]->private_data;
                                                                                sto[nm]->private_data = NULL;
                                                                                // partitions exist
                                                                                part_t *apart;
                                                                                for(int j = 0; j < 4; j++) {
                                                                                        nm = f_next_mount();
                                                                                        if(nm < _VOLUMES) {
                                                                                                apart = PT->GetPart(j);
                                                                                                if(apart != NULL && apart->type != 0x00) {
                                                                                                        sto[nm]->private_data = new pvt_t;
                                                                                                        ((pvt_t *)sto[nm]->private_data)->lun = LUN;
                                                                                                        ((pvt_t *)sto[nm]->private_data)->B = B;
                                                                                                        sto[nm]->Reads = *UHS_USB_BulkOnly_Read;
                                                                                                        sto[nm]->Writes = *UHS_USB_BulkOnly_Write;
                                                                                                        sto[nm]->Status = *UHS_USB_BulkOnly_Status;
                                                                                                        sto[nm]->Initialize = *UHS_USB_BulkOnly_Initialize;
                                                                                                        sto[nm]->Commit = *UHS_USB_BulkOnly_Commit;
                                                                                                        sto[nm]->TotalSectors = UHS_USB_BulkOnly[B]->GetCapacity(LUN);
                                                                                                        sto[nm]->SectorSize = UHS_USB_BulkOnly[B]->GetSectorSize(LUN);
                                                                                                        Fats[nm] = new PFAT(sto[nm], nm, apart->firstSector);
                                                                                                        if(Fats[nm]->MountStatus()) {
                                                                                                                delete Fats[nm];
                                                                                                                Fats[nm] = NULL;
                                                                                                                delete sto[nm]->private_data;
                                                                                                                sto[nm]->private_data = NULL;
                                                                                                        }
                                                                                                }
                                                                                        }
                                                                                }
                                                                        } else {
                                                                                // try superblock
                                                                                Fats[nm] = new PFAT(sto[nm], nm, 0);
                                                                                if(Fats[nm]->MountStatus()) {
                                                                                        delete Fats[nm];
                                                                                        Fats[nm] = NULL;
                                                                                        delete sto[nm]->private_data;
                                                                                        sto[nm]->private_data = NULL;
                                                                                }
                                                                        }
                                                                        delete PT;
                                                                }
                                                        } else { // media removed
                                                                for(int k = 0; k < _VOLUMES; k++) {
                                                                        if(Fats[k] != NULL) {
                                                                                if(((pvt_t *)sto[k]->private_data)->B == B && ((pvt_t *)sto[k]->private_data)->lun == LUN) {
                                                                                        delete Fats[k]; // this handles umount
                                                                                        Fats[k] = NULL;
                                                                                        delete sto[k]->private_data;
                                                                                        sto[k]->private_data = NULL;
                                                                                }
                                                                        }
                                                                }
                                                        }
                                                }
                                        }
                                } else if(!wasnull[B]) { // Bulk is null, was it null before?
                                        wasnull[B] = true;
                                        for(int k = 0; k < _VOLUMES; k++) {
                                                if(Fats[k] != NULL) {
                                                        if(((pvt_t *)sto[k]->private_data)->B == B) {
                                                                delete Fats[k]; // this handles umount
                                                                Fats[k] = NULL;
                                                                delete sto[k]->private_data;
                                                                sto[k]->private_data = NULL;
                                                        }
                                                }
                                        }

                                        for(int j = 0; j < MASS_MAX_SUPPORTED_LUN; j++) {
                                                last_ready[B][j] = false;
                                        }

                                }
                        }
                }
#ifdef __AVR__
                return (process_fs_pipe());
#endif
        }

        /**
         *
         * @return 1 on success
         */
        int Init(void) {
                Init_Generic_Storage();
                for(int i = 0; i < _FS_LOCK; i++) {
                        fhandles[i] = new FIL;
                        dhandles[i] = new DIR;
                        fhandles[i]->fs = NULL;
                        dhandles[i]->fs = NULL;
                }

                for(int i = 0; i < MAX_USB_MS_DRIVERS; i++) {
                        wasnull[i] = true;
                        for(int j = 0; j < MASS_MAX_SUPPORTED_LUN; j++) {
                                last_ready[i][j] = false;
                        }
                }

                for(int i = 0; i < _VOLUMES; i++) {
                        sto[i] = new storage_t;
                        sto[i]->private_data = NULL;
                        Fats[i] = NULL;
                }
                return 1;
        }

        /**
         * Storage module interface.
         *
         * @param oper 0=Setup, 1=Tnit, 2=Poll
         * @param pargA current USB driver state
         * @param pargB last known USB driver state
         * @return Two means error. Zero if no message was passed and may be an error. One is success.
         */
        int USB_Module_Call(uint8_t oper, uint8_t pargA, uint8_t pargB) {
                switch(oper) {
                        case 0:
                                return (Setup()); // 1 == success, 0 == error
                                break;
                        case 1:
                                return (Init()); // 1 == success, 0 == error
                                break;
                        case 2:
                                return (Poll(pargA, pargB)); // 1 == message passed, 0 == no messages
                                break;
                        default:
                                return 2; // error!
                                break;
                }
        }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Utilities
extern "C" {

        /**
         * Return a pointer to path name with mount point stripped, unless
         * the mount point is '/'
         *
         * @param path
         * @param vol
         * @return pointer to path name
         */
        const char *_fs_util_trunkpath(const char *path, uint8_t vol) {

                const char *pathtrunc = path;
                char *s = fs_mount_lbl(vol);
                if(s != NULL) {
                        pathtrunc += strlen(s);
                        free(s);
                }
                return pathtrunc;
        }

        /**
         * Convert a path to a volume number.
         *
         * @param path
         * @return volume number, or _VOLUMES on error
         *
         */
        uint8_t _fs_util_vol(const char *path) {
                uint8_t *ptr;
                char *fname;
                uint8_t vol = _VOLUMES;

                if((strlen(path) < 1) || *path != '/' || strstr(path, "/./") || strstr(path, "/../") || strstr(path, "//")) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_INVALID_NAME;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = FR_INVALID_NAME;
#endif
                } else {
#ifdef __AVR__
                        fname = (char *)xmem::safe_malloc(strlen(path) + 1);
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fname = (char *)malloc(strlen(path) + 1);
#endif
                        *fname = 0x00;
                        strcat(fname, path);
                        ptr = (uint8_t *)fname;
                        ptr++; // skip first '/'
                        while(*ptr) {
                                if(*ptr == '/') {
                                        break;
                                }
                                ptr++;
                        }
                        if(*ptr) {
                                *ptr = 0x00;
                                vol = fs_ready((char *)fname);
                        }
                        // Could be a sub-dir in the path, so try just the root.
                        if(vol == _VOLUMES) {
                                // root of "/"
                                vol = fs_ready("/");
                        }
                        free(fname);
                }
                return vol;
        }

#if defined(CORE_TEENSY) && defined(__arm__)

        /**
         * Return a pointer to path name suitable for FATfs
         *
         * @param path
         * @param vol
         * @return pointer to new path name, must be freed by caller
         */
        const char *_fs_util_FATpath(const char *path, uint8_t vol) {
                char *FATpath = (char *)malloc(3 + strlen(path));
                char *ptr = FATpath;
                *ptr = '0' + vol;
                ptr++;
                *ptr = ':';
                ptr++;
                *ptr = 0;
                strcat(FATpath, path);
                const char *rv = FATpath;
                return rv;
        }
#endif
        // functions

        /**
         * Check if mount point is ready
         *
         * @param path mount point path
         * @return volume number, or _VOLUMES on error
         */
        uint8_t fs_ready(const char *path) {
                // path -> volume number
#ifdef __AVR__
                fss_r8 *reply;
#endif
                uint8_t vol;
#ifdef __AVR__
                uint8_t rc;
                uint16_t ltr = strlen(path) + 2;
                fss_opfd *message = (fss_opfd *)xmem::safe_malloc(ltr);
                message->op = PIPE_VOLUME;
                message->fd = 0x00;
                strcat((char *)&message->fd, path);
                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                vol = reply->dat;
                free(reply);
                if(rc) {
                        return _VOLUMES; // error
                }
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        vol = _VOLUMES;
                        fs_err = FR_NO_PATH;
                        for(uint8_t x = 0; x < _VOLUMES; x++) {
                                if(Fats[x]) {
                                        if(!strcasecmp((char *)Fats[x]->label, path)) {
                                                fs_err = FR_OK;
                                                vol = Fats[x]->volmap; // should be equal to x
                                                break;
                                        }
                                }
                        }
                }
#endif
                return vol;
        }

        uint8_t fs_opendir(const char *path) {

                uint8_t fd;
                uint8_t rc;
                const char *pathtrunc;
#ifdef __AVR__
                uint8_t *ptr;
                fss_r8 *reply;
                char *message;
#endif
                uint8_t vol = _fs_util_vol(path);

                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = FR_NO_PATH;
#endif
                        return 0;
                }


                pathtrunc = _fs_util_trunkpath(path, vol);
#ifdef __AVR__
                uint16_t ltr = strlen(pathtrunc) + 5;
                message = (char *)xmem::safe_malloc(ltr);
                ptr = (uint8_t *)message;
                *ptr = PIPE_OPEN; // open
                ptr++;
                *ptr = 'd'; // Directory
                ptr++;
                // volume number
                *ptr = '0' + vol;
                ptr++;
                *ptr = ':';
                ptr++;
                *ptr = 0x00;
                strcat((char *)ptr, (char *)pathtrunc);
                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                fd = reply->dat;
                free(reply);
                if(rc) {
                        return 0; // error
                }
#elif defined(CORE_TEENSY) && defined(__arm__)
                fd = 0;
                int i;

                USB_ISR_PROTECTED_CALL() {
                        for(i = 0; i < _FS_LOCK; i++) {
                                if(dhandles[i]->fs == NULL) break;
                        }
                        if(i < _FS_LOCK) {
                                fd = 1 + i + _FS_LOCK;
                                const char *ptr = _fs_util_FATpath(pathtrunc, vol);
                                rc = f_opendir(dhandles[i], (TCHAR *)ptr);
                                free((void *)ptr);
                                if(rc != FR_OK) {
                                        dhandles[i]->fs = NULL;
                                        fd = 0;
                                        fs_err = FR_NO_PATH;
                                }
                        }
                }
#endif
                return fd;
        }

        /**
         * Open a file. An absolute, fully resolved canonical path is required!
         * Relative paths and links are _not_ supported.
         *
         * @param path
         * @param mode one of rRwWxX
         * @return returns file handle number, or 0 on error
         */
        uint8_t fs_open(const char *path, const char *mode) {
#ifdef __AVR__

                fss_r8 *reply;
#endif
                uint8_t fd;
                uint8_t rc;
                const char *pathtrunc;

                uint8_t vol = _fs_util_vol(path);

                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = FR_NO_PATH;
#endif
                        return 0;
                }

                pathtrunc = _fs_util_trunkpath(path, vol);
#ifdef __AVR__

                uint16_t lpt = strlen(pathtrunc) + sizeof (fss_op88drcol);
                fss_op88drcol *message = (fss_op88drcol *)xmem::safe_malloc(lpt);
                message->op = PIPE_OPEN;
                message->object = 'f';
                message->mode = *mode;
                message->drive = '0' + vol;
                message->colon = ':';
                message->path = 0x00;

                strcat((char *)&message->path, (char *)pathtrunc);

                xmem::memory_send((uint8_t*)message, lpt, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                fd = reply->dat;
                free(reply);
                if(rc) {
                        return 0; // error
                }
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd = 0;
                int i;

                USB_ISR_PROTECTED_CALL() {
                        for(i = 0; i < _FS_LOCK; i++) {
                                if(fhandles[i]->fs == NULL) break;
                        }
                        if(i < _FS_LOCK) {
                                fd = i + 1;
                                const char *name = _fs_util_FATpath(pathtrunc, vol);
                                switch(*mode) {
                                        case 'r':
                                                rc = f_open(fhandles[i], name, FA_READ);
                                                break;
                                        case 'w':
                                                rc = f_open(fhandles[i], name, FA_WRITE | FA_CREATE_ALWAYS);
                                                break;
                                        case 'x':
                                                rc = f_open(fhandles[i], name, FA_READ | FA_WRITE);
                                                break;
                                        case 'W':
                                                rc = f_open(fhandles[i], name, FA_WRITE);
                                                break;
                                        case 'X':
                                                rc = f_open(fhandles[i], name, FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
                                                break;
                                }
                                free((void *)name);
                                if(rc != FR_OK) {
                                        fhandles[i]->fs = NULL;
                                        fd = 0;
                                        fs_err = rc;
                                }
                        }

                }
#endif
                return fd;
        }

        /**
         * Close file
         * @param fd file handle to close
         *
         * @return FRESULT, 0 = success
         */
        int fs_close(uint8_t fd) {
                uint8_t rc;
#ifdef __AVR__
                fss_r8 *reply;
                fss_opfd message = {
                        PIPE_CLOSE,
                        fd
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)
                fd--;

                USB_ISR_PROTECTED_CALL() {
                        if(fd < _FS_LOCK && fhandles[fd]->fs != NULL) {
                                fs_err = f_close(fhandles[fd]);
                                fhandles[fd]->fs = NULL;
                        } else fs_err = FR_INVALID_PARAMETER;
                }
#endif
                return fs_err;
        }

        /**
         * Close directory
         *
         * @param dh directory handle to close
         * @return FRESULT, 0 = success
         */
        int fs_closedir(uint8_t dh) {
                dh -= _FS_LOCK + 1;
                if(dh < _FS_LOCK) {
                        dhandles[dh]->fs = NULL;
                        fs_err = FR_OK;
                } else {
                        fs_err = FR_INVALID_PARAMETER;
                }

        }

        int fs_readdir(uint8_t fd, DIRINFO *data) {
                uint8_t rc;
#ifdef __AVR__
                fss_r8 *reply;

                fss_opfd message = {
                        PIPE_READ,
                        fd
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                if(rc == FR_OK) {
                        memcpy(data, &reply->dat, sizeof (DIRINFO));
                }
                free(reply);
                return rc;
#elif defined(CORE_TEENSY) && defined(__arm__)
                fd -= _FS_LOCK + 1;
                fs_err = FR_EOF;
                if(fd < _FS_LOCK && dhandles[fd]->fs != NULL) {
                        FILINFO finfo;
#if _USE_LFN
                        char lfn[_MAX_LFN + 1];
                        finfo.lfsize = _MAX_LFN;
#else
                        char lfn[1];
                        finfo.lfsize = 0;
#endif
                        finfo.lfname = lfn;
                        rc = f_readdir(dhandles[fd], &finfo);
                        if(rc == FR_OK) {
                                if(finfo.fname[0]) {
                                        fs_err = FR_OK;
                                        data->fsize = finfo.fsize;
                                        data->fdate = finfo.fdate;
                                        data->ftime = finfo.ftime;
                                        data->fattrib = finfo.fattrib;
                                        strcpy((char *)data->fname, (char *)finfo.fname);
                                        data->lfname[0] = 0x00;
#if _USE_LFN
                                        if(finfo.lfsize) {
                                                strcat((char *)data->lfname, (char *)finfo.lfname);
                                        }
#endif
                                }
                        }
                }
                return fs_err;
#endif
        }

        /**
         * Flush all files that have pending write data to disk.
         *
         * @return FRESULT, 0 = success
         */
        uint8_t fs_sync(void) {
                uint8_t rc;
#ifdef __AVR__
                fss_r8 *reply;
                uint8_t message;
                message = PIPE_SYNC;
                xmem::memory_send(&message, 1U, &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        for(int i = 0; i < _FS_LOCK; i++) {
                                if(fhandles[i]->fs != NULL) {
                                        f_sync(fhandles[i]);
                                }
                        }
                        for(int i = 0; i < _VOLUMES; i++) {
                                if(Fats[i] != NULL) {
                                        f_sync_fs(Fats[i]->ffs);
                                        commit_fs(Fats[i]->ffs);
                                }
                        }
                        rc = FR_OK;
                        fs_err = rc;
                }
#endif
                return rc;
        }

        /**
         * Flush pending writes to disk for a specific file.
         *
         * @param fd file handle
         * @return FRESULT, 0 = success
         */
        uint8_t fs_flush(uint8_t fd) {
                uint8_t rc;
#ifdef __AVR__
                fss_opfd message = {
                        PIPE_FLUSH,
                        fd
                };
                fss_r8 *reply;

                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if(fd < _FS_LOCK && fhandles[fd]->fs != NULL) {
                                rc = FR_OK;
                                f_sync(fhandles[fd]);
                        }
                        fs_err = rc;
                }
#endif
                return rc;
        }

        /**
         * Check to see if we are at the end of the file.
         *
         * @param fd file handle
         * @return 0 = not eof, 1 = eof
         */
        uint8_t fs_eof(uint8_t fd) {
                uint8_t rc;
#ifdef __AVR__
                fss_r8 *reply;
                fss_opfd message = {
                        PIPE_EOF,
                        fd
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if(fd < _FS_LOCK && fhandles[fd]->fs != NULL) {
                                rc = f_eof(fhandles[fd]);
                        }
                        fs_err = rc;
                }
#endif
                return rc;
        }

        /**
         * Truncate file to current position.
         *
         * @param fd file handle
         * @return FRESULT, 0 = success
         */
        uint8_t fs_truncate(uint8_t fd) {
                uint8_t rc;
#ifdef __AVR__
                fss_r8 *reply;
                fss_opfd message = {
                        PIPE_TRUNC,
                        fd
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if(fd < _FS_LOCK && fhandles[fd]->fs != NULL) {
                                rc = f_truncate(fhandles[fd]);
                        }
                        fs_err = rc;

                }
#endif
                return rc;
        }

        /**
         * Get current position in file
         *
         * @param fd file handle
         * @return current position in file, 0xfffffffflu == error
         */
        unsigned long fs_tell(uint8_t fd) {
                uint8_t rc;
                unsigned long offset = 0xfffffffflu;
#ifdef __AVR__
                fss_r8 *reply;
                uint8_t *ptr;
                fss_opfd message = {
                        PIPE_TELL,
                        fd
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                if(!rc) {
                        ptr = &reply->dat;
                        uint32_t *o_p = (uint32_t *)ptr;
                        offset = *o_p;
                }
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if(fd < _FS_LOCK && fhandles[fd]->fs != NULL) {
                                rc = FR_OK;
                                // f_tell is a macro...
                                offset = f_tell(fhandles[fd]);
                        }
                        fs_err = rc;

                }
#endif
                return offset;
        }

        /**
         * Seek to position in file.
         *      SEEK_SET The offset is set to offset bytes.
         *      SEEK_CUR The offset is set to its current location plus offset bytes.
         *      SEEK_END The offset is set to the size of the file plus offset bytes.
         *
         * @param fd file handle
         * @param offset
         * @param whence one of SEEK_SET, SEEK_CUR, SEEK_END
         * @return FRESULT, 0 = success
         */
        uint8_t fs_lseek(uint8_t fd, unsigned long offset, int whence) {
                uint8_t rc;
#ifdef __AVR__
                fss_r8 *reply;

                fss_opfd32dat message = {
                        PIPE_LSEEK,
                        fd,
                        offset,
                        whence
                };

                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd32dat), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if(fd < _FS_LOCK && fhandles[fd]->fs != NULL) {
                                rc = f_clseek(fhandles[fd], offset, whence);
                        }
                        fs_err = rc;
                }
#endif
                return rc;
        }

        /**
         * Read from file
         *
         * @param fd file handle
         * @param data pointer to a buffer large enough to hold requested amount
         * @param amount
         * @return amount actually read in, less than 0 is an error
         */
        int fs_read(uint8_t fd, void *data, uint16_t amount) {
                uint8_t rc;
                int count = 0;
#ifdef __AVR__
                uint8_t *ptr;
                fss_r8 *reply;
                fss_opfd16 message = {
                        PIPE_READ,
                        fd,
                        amount
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd16), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                if(rc == FR_EOF || rc == FR_OK) {
                        ptr = &reply->dat;
                        uint16_t *r_p = (uint16_t *)ptr;
                        count = *r_p;
                        ptr += sizeof (uint16_t);
                        memcpy(data, ptr, count);
                }
                if(rc) {
                        if(!count) count = -1;
                }
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if((fd < _VOLUMES) && (fhandles[fd]->fs != NULL)) {
                                UINT bc;
                                rc = f_read(fhandles[fd], data, amount, &bc);
                                if(bc != amount) rc = FR_EOF;
                                count = bc;
                                if(rc) {
                                        if(!count) count = -1;
                                }
                        }
                        fs_err = rc;

                }
#endif
                return count;
        }

        /**
         * Write to file
         *
         * @param fd file handle
         * @param data pointer to a buffer
         * @param amount
         * @return amount of data actually written, less than 0 is an error.
         */
        int fs_write(uint8_t fd, const void *data, uint16_t amount) {
                uint8_t rc;
                int count = 0;
#ifdef __AVR__
                fss_r8 *reply;
                uint16_t *w_p;

                fss_opfd16 *message;
                uint16_t ltr = sizeof (fss_opfd16) + amount;
                message = (fss_opfd16 *)xmem::safe_malloc(ltr);
                message->op = PIPE_WRITE;
                message->fd = fd;
                message->amount = amount;
                memcpy(&(message->data), data, amount);

                xmem::memory_send((uint8_t *)message, ltr, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                w_p = (uint16_t *) & reply->dat;
                count = *w_p;
                if(rc) {
                        if(!count) count = -1;
                }
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)
                fd--;

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_INVALID_PARAMETER;
                        if((fd < _VOLUMES) && (fhandles[fd]->fs != NULL)) {
                                UINT bc;
                                rc = f_write(fhandles[fd], data, amount, &bc);
                                count = bc;
                                if(rc) {
                                        if(!count) count = -1;
                                }
                        }
                        fs_err = rc;


                }
#endif
                return count;
        }

        /**
         * Remove (delete) file or directory. An absolute, fully resolved canonical path is required!
         * Relative paths and links are _not_ supported.
         *
         * @param path
         * @return FRESULT, 0 = success
         */
        uint8_t fs_unlink(const char *path) {

#ifdef __AVR__
                fss_r8 *reply;
                fss_opdrcol *message;
                uint16_t ltr;
#endif
                const char *pathtrunc;
                uint8_t vol = _fs_util_vol(path);
                uint8_t rc;

                if(vol == _VOLUMES) {
                        rc = FR_NO_PATH;
                } else {
                        pathtrunc = _fs_util_trunkpath(path, vol);
#ifdef __AVR__
                        {
                                ltr = strlen(pathtrunc) + sizeof (fss_opdrcol);

                                message = (fss_opdrcol *)xmem::safe_malloc(ltr);

                                message->op = PIPE_UNLINK;
                                message->drive = '0' + vol;
                                message->colon = ':';
                                strcpy((char *)&message->path, pathtrunc);
                                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                                free(message);
                                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                                rc = reply->res; // error code
                                free(reply);
                        }
                }
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
#elif defined(CORE_TEENSY) && defined(__arm__)

                        USB_ISR_PROTECTED_CALL() {
                                const char *name = _fs_util_FATpath(pathtrunc, vol);
                                rc = f_unlink(name);
                                free((void *)name);
                        }

                }
                fs_err = rc;
#endif

                return rc;
        }

        /**
         * Change attribute flags on a file.
         *
         * @param path file or directory
         * @param mode AM_RDO|AM_ARC|AM_SYS|AM_HID in any combination
         * @return FRESULT, 0 = success
         */
        uint8_t fs_chmod(const char *path, uint8_t mode) {
#ifdef __AVR__
                fss_r8 *reply;
#endif
                uint8_t rc;
                const char *pathtrunc;
                uint8_t vol = _fs_util_vol(path);

                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                }


                pathtrunc = _fs_util_trunkpath(path, vol);

#ifdef __AVR__
                uint16_t ltr = strlen(pathtrunc) + sizeof (fss_op8drcol);

                fss_op8drcol *message = (fss_op8drcol *)xmem::safe_malloc(ltr);
                message->op = PIPE_CHMOD;
                message->mode = mode;
                message->drive = '0' + vol;
                message->colon = ':';
                message->path = 0x00;
                strcat((char *)&message->path, (char *)pathtrunc);

                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);


                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        const char *name = _fs_util_FATpath(pathtrunc, vol);
                        rc = f_chmod((TCHAR *)name, mode, AM_RDO | AM_ARC | AM_SYS | AM_HID);
                        free((void *)name);
                }
                fs_err = rc;
#endif
                return rc;
        }

        /**
         * Create a new directory
         *
         * @param path new directory
         * @param mode AM_RDO|AM_ARC|AM_SYS|AM_HID in any combination
         * @return FRESULT, 0 = success
         */
        uint8_t fs_mkdir(const char *path, uint8_t mode) {
                uint8_t rc;
                const char *pathtrunc;

                uint8_t vol = _fs_util_vol(path);

                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                }
                pathtrunc = _fs_util_trunkpath(path, vol);
#ifdef __AVR__

                fss_r8 *reply;
                uint16_t ltr = strlen(pathtrunc) + sizeof (fss_op8drcol);
                fss_op8drcol *message = (fss_op8drcol *)xmem::safe_malloc(ltr);
                message->op = PIPE_MKDIR;
                message->mode = mode;
                message->drive = '0' + vol;
                message->colon = ':';
                message->path = 0x00;
                strcat((char *)&message->path, (char *)pathtrunc);

                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        const char *name = _fs_util_FATpath(pathtrunc, vol);
                        rc = f_mkdir((TCHAR *)name);
                        if((rc == FR_OK) && mode) {
                                rc = f_chmod((TCHAR *)name, mode, AM_RDO | AM_ARC | AM_SYS | AM_HID);
                        }
                        free((void *)name);
                }
#endif
                return rc;
        }

        /**
         * Modify file time stamp.
         *
         * @param path File to update time stamp
         * @param timesec time in seconds since 1/1/1900
         * @return FRESULT, 0 = success
         */
        uint8_t fs_utime(const char *path, time_t timesec) {
                uint8_t rc;
                const char *pathtrunc;
                uint8_t vol = _fs_util_vol(path);

                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                }

                pathtrunc = _fs_util_trunkpath(path, vol);
#ifdef __AVR__
                fss_r8 *reply;


                uint16_t ltr = strlen(pathtrunc) + sizeof (fss_op32drcol);

                fss_op32drcol *message = (fss_op32drcol *)xmem::safe_malloc(ltr);
                message->op = PIPE_UTIME;
                message->data = DateTime(timesec).FatPacked();
                message->drive = '0' + vol;
                message->colon = ':';
                message->path = 0x00;
                strcat((char *)&message->path, (char *)pathtrunc);

                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);

                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        const char *name = _fs_util_FATpath(pathtrunc, vol);
                        FILINFO finfo;
                        finfo.fdate = timesec;
                        rc = f_utime((TCHAR *)name, &finfo);
                        free((void *)name);
                }
                fs_err = rc;
#endif
                return rc;

        }

        /**
         * Return information about a file.
         *
         * @param path File to return information about
         * @param buf pointer to FILINFO structure, which is filled in upon success.
         * @return FRESULT, 0 = success
         */
        uint8_t fs_stat(const char *path, FILINFO *buf) {
                uint8_t rc;
                const char *pathtrunc;
                uint8_t vol = _fs_util_vol(path);
                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                }

                pathtrunc = _fs_util_trunkpath(path, vol);
#ifdef __AVR__
                fss_r8 *reply;

                uint16_t ltr = strlen(pathtrunc) + sizeof (fss_opdrcol);
                fss_opdrcol *message = (fss_opdrcol *)xmem::safe_malloc(ltr);

                message->op = PIPE_STAT;
                message->drive = '0' + vol;
                message->colon = ':';
                message->path = 0x00;
                strcat((char *)&message->path, pathtrunc);

                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                if(!rc) {
                        memcpy(buf, &reply->dat, sizeof (FILINFO));
                }
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        const char *name = _fs_util_FATpath(pathtrunc, vol);
                        rc = f_stat((TCHAR *)name, buf);
                        free((void *)name);
                }
                fs_err = rc;
#endif
                return rc;
        }

        /**
         * Rename a file, moving it between directories if required.
         * Does not move a file between mounted volumes.
         *
         * @param oldpath old file name
         * @param newpath new file name
         * @return FRESULT, 0 = success
         */
        uint8_t fs_rename(const char *oldpath, const char *newpath) {
                uint8_t rc;

                uint8_t vol = _fs_util_vol(oldpath);
                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                }
                if(vol != _fs_util_vol(newpath)) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                }

                const char *oldpathtrunc = _fs_util_trunkpath(oldpath, vol);
                const char *newpathtrunc = _fs_util_trunkpath(newpath, vol);

#ifdef __AVR__
                uint8_t *ptr;
                fss_r8 *reply;

                uint16_t ltr = strlen(oldpathtrunc) + strlen(newpathtrunc) + sizeof (fss_opdrcol) + 1;

                fss_opdrcol *message = (fss_opdrcol *)xmem::safe_malloc(ltr);
                message->op = PIPE_RENAME;
                message->drive = '0' + vol;
                message->colon = ':';
                message->path = 0x00;
                ptr = &(message->path);

                strcat((char *)ptr, (char *)oldpathtrunc);
                ptr += strlen(oldpathtrunc) + 1;
                *ptr = 0x00;
                strcat((char *)ptr, (char *)newpathtrunc);
                xmem::memory_send((uint8_t*)message, ltr, &to_usb_fs_task);
                free(message);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        const char *oldname = _fs_util_FATpath(oldpathtrunc, vol);
                        //const char *newname = _fs_util_FATpath(newpathtrunc, vol);

                        rc = f_rename(oldname, newpathtrunc);
                        //free((void *)newname);
                        free((void *)oldname);
                }
                fs_err = rc;

#endif
                return rc;
        }

        /**
         * Get free space available on mount point.
         *
         * @param path mount point
         * @return free space available in bytes. If there is no volume or write protected, returns zero (full).
         */
        uint64_t fs_getfree(const char *path) {
                uint8_t rc;
                uint8_t vol = _fs_util_vol(path);
                uint64_t rv = 0llu;
                if(vol == _VOLUMES) {
#ifdef __AVR__
                        fs_err[xmem::getcurrentBank()] = FR_NO_PATH;
#elif defined(CORE_TEENSY) && defined(__arm__)
                        fs_err = rc;
#endif
                        return FR_NO_PATH;
                } else {
#ifdef __AVR__
                        uint8_t *ptr;
                        fss_r8 *reply;
                        uint64_t *q;
                        fss_opdrcol message = {
                                PIPE_FREE,
                                '0' + vol,
                                ':',
                                0x00
                        };

                        xmem::memory_send((uint8_t*) & message, sizeof (fss_opdrcol), &to_usb_fs_task);

                        xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                        rc = reply->res; // error code
                        fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                        if(!rc) {
                                ptr++;
                                q = (uint64_t *) & reply->dat;
                                rv = *q;
                        }
                        free(reply);
                }
#elif defined(CORE_TEENSY) && defined(__arm__)

                        USB_ISR_PROTECTED_CALL() {
                                FATFS *fs;
                                uint32_t tv;
                                const char *name = _fs_util_FATpath("", vol);
                                rc = f_getfree((TCHAR *)name, &tv, &fs);
                                free((void *)name);
                                rv = tv; // blocks
                                rv *= fs->csize; // sectors in each block
                                rv *= fs->pfat->storage->SectorSize; // bytes in each sector
                                if(rc) rv = 0llu;
                        }
                }
                fs_err = rc;
#endif
                return (uint64_t)rv;
        }

        char *fs_mount_lbl(uint8_t vol) {
                uint8_t rc;
                char *lbl = NULL;
#ifdef __AVR__
                fss_r8 *reply;
                fss_opfd message = {
                        PIPE_MOUNT_LBL,
                        vol
                };
                xmem::memory_send((uint8_t *) & message, sizeof (fss_opfd), &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                if(!rc) {
                        char *rlbl = (char *)&reply->dat;
                        lbl = (char *)xmem::safe_malloc(1 + strlen(rlbl));
                        //*lbl = 0x00;
                        strcpy(lbl, rlbl);
                }
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)

                USB_ISR_PROTECTED_CALL() {
                        rc = FR_NO_PATH;
                        if(Fats[vol]) {
                                rc = FR_OK;
                                lbl = (char *)malloc(1 + strlen((char *)(Fats[vol]->label)));
                                strcpy(lbl, (char *)(Fats[vol]->label));
                        }
                }
                fs_err = rc;
#endif
                return lbl;
        }

        uint8_t fs_mountcount(void) {
#ifdef __AVR__
                uint8_t rc;
                uint8_t message = PIPE_MOUNT_CNT;
                fss_r8 *reply;
                xmem::memory_send(&message, 1U, &to_usb_fs_task);
                xmem::memory_recv((uint8_t**) & reply, &from_usb_fs_task);
                rc = reply->res; // error code
                fs_err[xmem::getcurrentBank()] = rc; // save it here in the event we want to use it.
                message = reply->dat;
                free(reply);
#elif defined(CORE_TEENSY) && defined(__arm__)
                uint8_t message = 0;

                USB_ISR_PROTECTED_CALL() {
                        fs_err = FR_OK;
                        for(uint8_t x = 0; x < _VOLUMES; x++) {
                                if(Fats[x]) {
                                        message++;
                                }
                        }
                }
#endif
                return message;
        }
}
