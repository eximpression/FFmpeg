/*
 * Copyright (c) 2014 Lukasz Marek <lukasz.m.luki@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libsmbclient.h>
#include "libavutil/avstring.h"
#include "libavutil/opt.h"
#include "url.h"
#if TARGET_OS_IPHONE
#else
pthread_mutex_t smb_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

typedef struct {
    const AVClass *class;
    SMBCCTX *ctx;
    int dh;
    int fd;
    int64_t filesize;
    int trunc;
    int timeout;
    char *workgroup;
    char *username;
    char *password;
} LIBSMBContext;

static void libsmbc_get_auth_data(SMBCCTX *c, const char *server, const char *share,
                                  char *workgroup, int workgroup_len,
                                  char *username, int username_len,
                                  char *password, int password_len)
{
    /* Do nothing yet. Credentials are passed via url.
     * Callback must exists, there might be a segmentation fault otherwise. */
    if (c == NULL) {
        return;
    }
    void *userdata = smbc_getOptionUserData(c);
    if (userdata == NULL) {
        return;
    }
    URLContext *h = (URLContext *)userdata;
    LIBSMBContext *smbContent = (LIBSMBContext *)h->priv_data;
    if (smbContent == NULL) {
        return;
    }
    if (username) {
        if (smbContent->username) {
            int len = FFMIN(strlen(smbContent->username), username_len-1);
            strncpy(username, smbContent->username, len);
        }
    }
    
    if (password) {
        if (smbContent->password) {
            int len = FFMIN(strlen(smbContent->password), password_len-1);
            strncpy(password, smbContent->password, len);
        }
    }
    
    if (workgroup) {
        if (smbContent->workgroup) {
            int len = FFMIN(strlen(smbContent->workgroup), workgroup_len-1);
            strncpy(workgroup, smbContent->workgroup, len);
        }
    }
}

static av_cold int libsmbc_connect(URLContext *h)
{
    LIBSMBContext *libsmbc = h->priv_data;
    SMBCCTX *old_context = NULL;
#if TARGET_OS_IPHONE
    requestExternalCtx(&old_context);
#endif
    if (old_context) {
        av_log(h, AV_LOG_WARNING, "requestExternalCtx(&old_context)\n");
        libsmbc->ctx = old_context;
    }else{
        pthread_mutex_lock(&smb_lock);
        old_context = smbc_set_context(NULL);
        pthread_mutex_unlock(&smb_lock);
        if (old_context) {
            av_log(h, AV_LOG_WARNING, "libsmbc->ctx = smbc_set_context(NULL)\n");
            libsmbc->ctx = old_context;
        }else{
            av_log(h, AV_LOG_WARNING, "libsmbc->ctx = smbc_new_context()\n");
            pthread_mutex_lock(&smb_lock);
            libsmbc->ctx = smbc_new_context();
            smbc_setOptionUserData(libsmbc->ctx, h);
            smbc_setFunctionAuthDataWithContext(libsmbc->ctx, libsmbc_get_auth_data);
            pthread_mutex_unlock(&smb_lock);
            if (!libsmbc->ctx) {
                int ret = AVERROR(errno);
                av_log(h, AV_LOG_ERROR, "Cannot create context: %s.\n", strerror(errno));
                return ret;
            }
            pthread_mutex_lock(&smb_lock);
            SMBCCTX *init_context = smbc_init_context(libsmbc->ctx);
            pthread_mutex_unlock(&smb_lock);
            if (!init_context) {
                int ret = AVERROR(errno);
                av_log(h, AV_LOG_ERROR, "Cannot initialize context: %s.\n", strerror(errno));
                return ret;
            }
            
            pthread_mutex_lock(&smb_lock);
            smbc_set_context(libsmbc->ctx);
            
            if (libsmbc->timeout != -1)
                smbc_setTimeout(libsmbc->ctx, libsmbc->timeout);
            if (libsmbc->workgroup)
                smbc_setWorkgroup(libsmbc->ctx, libsmbc->workgroup);
            
            int init_ret = smbc_init(NULL, 0);
            pthread_mutex_unlock(&smb_lock);
            
            if (init_ret < 0) {
                int ret = AVERROR(errno);
                av_log(h, AV_LOG_ERROR, "Initialization failed: %s\n", strerror(errno));
                return ret;
            }
        }
    }
    return 0;
}

static av_cold int libsmbc_close(URLContext *h)
{
    pthread_mutex_lock(&smb_lock);
    LIBSMBContext *libsmbc = h->priv_data;
    if (libsmbc->fd >= 0) {
        smbc_close(libsmbc->fd);
        libsmbc->fd = -1;
    }
    if (libsmbc->password) {
        free(libsmbc->password);
        libsmbc->password = NULL;
    }
    if (libsmbc->username) {
        free(libsmbc->username);
        libsmbc->username = NULL;
    }
    if (libsmbc->workgroup) {
        free(libsmbc->workgroup);
        libsmbc->workgroup = NULL;
    }
    /*
    if (libsmbc->ctx) {
        smbc_setOptionUserData(libsmbc->ctx, NULL);
        smbc_setFunctionAuthDataWithContext(libsmbc->ctx, NULL);
        smbc_free_context(libsmbc->ctx, 0);
        libsmbc->ctx = NULL;
    }
     */
    pthread_mutex_unlock(&smb_lock);
    return 0;
}

static av_cold int libsmbc_open(URLContext *h, const char *url, int flags, AVDictionary **options)
{
    LIBSMBContext *libsmbc = h->priv_data;
    int access, ret;
    struct stat st;

    if(*options != NULL){
        AVDictionaryEntry *e = av_dict_get(*options, "everplay_user_name", NULL, 0);
        if (e != NULL && e->value != NULL){
            int leg = strlen(e->value);
            if (libsmbc->username) {
                free(libsmbc->username);
                libsmbc->username = NULL;
            }
            libsmbc->username = malloc(leg + 1);
            strcpy(libsmbc->username, e->value);
        }
        
        e = av_dict_get(*options, "everplay_password", NULL, 0);
        if (e != NULL && e->value != NULL){
            int len = strlen(e->value);
            if (libsmbc->password) {
                free(libsmbc->password);
                libsmbc->password = NULL;
            }
            libsmbc->password = malloc(len + 1);
            strcpy(libsmbc->password, e->value);
        }
        
        e = av_dict_get(*options, "everplay_workgroup", NULL, 0);
        if (e != NULL && e->value != NULL){
            int len = strlen(e->value);
            if (libsmbc->workgroup) {
                free(libsmbc->workgroup);
                libsmbc->workgroup = NULL;
            }
            libsmbc->workgroup = malloc(len + 1);
            strcpy(libsmbc->workgroup, e->value);
        }
    }
    
    libsmbc->fd = -1;
    libsmbc->filesize = -1;

    if ((ret = libsmbc_connect(h)) < 0)
        goto fail;

    if ((flags & AVIO_FLAG_WRITE) && (flags & AVIO_FLAG_READ)) {
        access = O_CREAT | O_RDWR;
        if (libsmbc->trunc)
            access |= O_TRUNC;
    } else if (flags & AVIO_FLAG_WRITE) {
        access = O_CREAT | O_WRONLY;
        if (libsmbc->trunc)
            access |= O_TRUNC;
    } else
        access = O_RDONLY;

    /* 0666 = -rw-rw-rw- = read+write for everyone, minus umask */
    pthread_mutex_lock(&smb_lock);
    libsmbc->fd = smbc_open(url, access, 0666);
    pthread_mutex_unlock(&smb_lock);
    if (libsmbc->fd < 0) {
        ret = AVERROR(errno);
        av_log(h, AV_LOG_ERROR, "File open failed: %s\n", strerror(errno));
        goto fail;
    }
    pthread_mutex_lock(&smb_lock);
    int fstat_ret = smbc_fstat(libsmbc->fd, &st);
    pthread_mutex_unlock(&smb_lock);
    if (fstat_ret < 0)
        av_log(h, AV_LOG_WARNING, "Cannot stat file: %s\n", strerror(errno));
    else
        libsmbc->filesize = st.st_size;

    return 0;
  fail:
    libsmbc_close(h);
    return ret;
}

static int64_t libsmbc_seek(URLContext *h, int64_t pos, int whence)
{
    LIBSMBContext *libsmbc = h->priv_data;
    int64_t newpos;

    if (whence == AVSEEK_SIZE) {
        if (libsmbc->filesize == -1) {
            av_log(h, AV_LOG_ERROR, "Error during seeking: filesize is unknown.\n");
            return AVERROR(EIO);
        } else
            return libsmbc->filesize;
    }
    pthread_mutex_lock(&smb_lock);
    newpos = smbc_lseek(libsmbc->fd, pos, whence);
    pthread_mutex_unlock(&smb_lock);
    if (newpos < 0) {
        int err = errno;
        av_log(h, AV_LOG_ERROR, "Error during seeking: %s\n", strerror(err));
        return AVERROR(err);
    }

    return newpos;
}

static int libsmbc_read(URLContext *h, unsigned char *buf, int size)
{
    LIBSMBContext *libsmbc = h->priv_data;
    pthread_mutex_lock(&smb_lock);
    int bytes_read = smbc_read(libsmbc->fd, buf, size);
    pthread_mutex_unlock(&smb_lock);

    if (bytes_read < 0) {
        int ret = AVERROR(errno);
        av_log(h, AV_LOG_ERROR, "Read error: %s\n", strerror(errno));
        return ret;
    }

    return bytes_read ? bytes_read : AVERROR_EOF;
}

static int libsmbc_write(URLContext *h, const unsigned char *buf, int size)
{
    LIBSMBContext *libsmbc = h->priv_data;
    pthread_mutex_lock(&smb_lock);
    int bytes_written = smbc_write(libsmbc->fd, buf, size);
    pthread_mutex_unlock(&smb_lock);
    
    if (bytes_written < 0) {
        int ret = AVERROR(errno);
        av_log(h, AV_LOG_ERROR, "Write error: %s\n", strerror(errno));
        return ret;
    }

    return bytes_written;
}

static int libsmbc_open_dir(URLContext *h)
{
    LIBSMBContext *libsmbc = h->priv_data;
    int ret;

    if ((ret = libsmbc_connect(h)) < 0)
        goto fail;
    pthread_mutex_lock(&smb_lock);
    libsmbc->dh = smbc_opendir(h->filename);
    pthread_mutex_unlock(&smb_lock);
    if (libsmbc->dh < 0) {
        ret = AVERROR(errno);
        av_log(h, AV_LOG_ERROR, "Error opening dir: %s\n", strerror(errno));
        goto fail;
    }

    return 0;

  fail:
    libsmbc_close(h);
    return ret;
}

static int libsmbc_read_dir(URLContext *h, AVIODirEntry **next)
{
    LIBSMBContext *libsmbc = h->priv_data;
    AVIODirEntry *entry;
    struct smbc_dirent *dirent = NULL;
    char *url = NULL;
    int skip_entry;

    *next = entry = ff_alloc_dir_entry();
    if (!entry)
        return AVERROR(ENOMEM);

    do {
        skip_entry = 0;
        pthread_mutex_lock(&smb_lock);
        dirent = smbc_readdir(libsmbc->dh);
        pthread_mutex_unlock(&smb_lock);
        if (!dirent) {
            av_freep(next);
            return 0;
        }
        switch (dirent->smbc_type) {
        case SMBC_DIR:
            entry->type = AVIO_ENTRY_DIRECTORY;
            break;
        case SMBC_FILE:
            entry->type = AVIO_ENTRY_FILE;
            break;
        case SMBC_FILE_SHARE:
            entry->type = AVIO_ENTRY_SHARE;
            break;
        case SMBC_SERVER:
            entry->type = AVIO_ENTRY_SERVER;
            break;
        case SMBC_WORKGROUP:
            entry->type = AVIO_ENTRY_WORKGROUP;
            break;
        case SMBC_COMMS_SHARE:
        case SMBC_IPC_SHARE:
        case SMBC_PRINTER_SHARE:
            skip_entry = 1;
            break;
        case SMBC_LINK:
        default:
            entry->type = AVIO_ENTRY_UNKNOWN;
            break;
        }
    } while (skip_entry || !strcmp(dirent->name, ".") ||
             !strcmp(dirent->name, ".."));

    entry->name = av_strdup(dirent->name);
    if (!entry->name) {
        av_freep(next);
        return AVERROR(ENOMEM);
    }

    url = av_append_path_component(h->filename, dirent->name);
    if (url) {
        struct stat st;
        pthread_mutex_lock(&smb_lock);
        int stat_ret = smbc_stat(url, &st);
        pthread_mutex_unlock(&smb_lock);
        
        if (!stat_ret) {
            entry->group_id = st.st_gid;
            entry->user_id = st.st_uid;
            entry->size = st.st_size;
            entry->filemode = st.st_mode & 0777;
            entry->modification_timestamp = INT64_C(1000000) * st.st_mtime;
            entry->access_timestamp =  INT64_C(1000000) * st.st_atime;
            entry->status_change_timestamp = INT64_C(1000000) * st.st_ctime;
        }
        av_free(url);
    }

    return 0;
}

static int libsmbc_close_dir(URLContext *h)
{
    pthread_mutex_unlock(&smb_lock);
    LIBSMBContext *libsmbc = h->priv_data;
    if (libsmbc->dh >= 0) {
        pthread_mutex_lock(&smb_lock);
        smbc_closedir(libsmbc->dh);
        libsmbc->dh = -1;
    }
    pthread_mutex_unlock(&smb_lock);
    libsmbc_close(h);
    return 0;
}

static int libsmbc_delete(URLContext *h)
{
    LIBSMBContext *libsmbc = h->priv_data;
    int ret;
    struct stat st;

    if ((ret = libsmbc_connect(h)) < 0)
        goto cleanup;
    
    pthread_mutex_lock(&smb_lock);
    libsmbc->fd = smbc_open(h->filename, O_WRONLY, 0666);
    pthread_mutex_unlock(&smb_lock);
    if (libsmbc->fd < 0) {
        ret = AVERROR(errno);
        goto cleanup;
    }
    
    pthread_mutex_lock(&smb_lock);
    int fstat_ret = smbc_fstat(libsmbc->fd, &st);
    pthread_mutex_unlock(&smb_lock);
    if (fstat_ret < 0) {
        ret = AVERROR(errno);
        goto cleanup;
    }
    pthread_mutex_lock(&smb_lock);
    smbc_close(libsmbc->fd);
    pthread_mutex_unlock(&smb_lock);
    libsmbc->fd = -1;

    if (S_ISDIR(st.st_mode)) {
        pthread_mutex_lock(&smb_lock);
        int rmdir_ret = smbc_rmdir(h->filename);
        pthread_mutex_unlock(&smb_lock);
        if (rmdir_ret < 0) {
            ret = AVERROR(errno);
            goto cleanup;
        }
    } else {
        pthread_mutex_lock(&smb_lock);
        int unlink_ret = smbc_unlink(h->filename);
        pthread_mutex_unlock(&smb_lock);
        if (unlink_ret < 0) {
            ret = AVERROR(errno);
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    libsmbc_close(h);
    return ret;
}

static int libsmbc_move(URLContext *h_src, URLContext *h_dst)
{
    LIBSMBContext *libsmbc = h_src->priv_data;
    int ret;

    if ((ret = libsmbc_connect(h_src)) < 0)
        goto cleanup;

    pthread_mutex_lock(&smb_lock);
    libsmbc->dh = smbc_rename(h_src->filename, h_dst->filename);
    pthread_mutex_unlock(&smb_lock);
    if (libsmbc->dh < 0) {
        ret = AVERROR(errno);
        goto cleanup;
    }

    ret = 0;

cleanup:
    libsmbc_close(h_src);
    return ret;
}

#define OFFSET(x) offsetof(LIBSMBContext, x)
#define D AV_OPT_FLAG_DECODING_PARAM
#define E AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    {"timeout",   "set timeout in ms of socket I/O operations",    OFFSET(timeout), AV_OPT_TYPE_INT, {.i64 = -1}, -1, INT_MAX, D|E },
    {"truncate",  "truncate existing files on write",              OFFSET(trunc),   AV_OPT_TYPE_INT, { .i64 = 1 }, 0, 1, E },
    {"workgroup", "set the workgroup used for making connections", OFFSET(workgroup), AV_OPT_TYPE_STRING, { 0 }, 0, 0, D|E },
    {"username", "set the username used for making connections", OFFSET(username), AV_OPT_TYPE_STRING, { 0 }, 0, 0, D|E },
    {"password", "set the password used for making connections", OFFSET(password), AV_OPT_TYPE_STRING, { 0 }, 0, 0, D|E },
    {NULL}
};

static const AVClass libsmbclient_context_class = {
    .class_name     = "libsmbc",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};

const URLProtocol ff_libsmbclient_protocol = {
    .name                = "smb",
    .url_open2           = libsmbc_open,
    .url_read            = libsmbc_read,
    .url_write           = libsmbc_write,
    .url_seek            = libsmbc_seek,
    .url_close           = libsmbc_close,
    .url_delete          = libsmbc_delete,
    .url_move            = libsmbc_move,
    .url_open_dir        = libsmbc_open_dir,
    .url_read_dir        = libsmbc_read_dir,
    .url_close_dir       = libsmbc_close_dir,
    .priv_data_size      = sizeof(LIBSMBContext),
    .priv_data_class     = &libsmbclient_context_class,
    .flags               = URL_PROTOCOL_FLAG_NETWORK,
};
