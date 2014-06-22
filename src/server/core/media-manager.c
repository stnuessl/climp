#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <libvci/map.h>
#include <libvci/compare.h>
#include <libvci/hash.h>
#include <libvci/error.h>
#include <libvci/vector.h>

#include "util/climpd-log.h"

#include "core/media-manager.h"
#include "core/media.h"

static const char *tag = "media-manager";
static struct map *media_map;
static GstDiscoverer *async_disc;
static GstDiscoverer *sync_disc;

static void parse_tags(const GstTagList *list, const gchar *tag, void *data)
{
    struct media *m;
    const GValue *val;
    int i, num;
    const char *s;
    
    m  = data;
    
    num = gst_tag_list_get_tag_size(list, tag);
    
    for (i = 0; i < num; ++i) {
        val = gst_tag_list_get_value_index(list, tag, i);
        
        if(strcmp(GST_TAG_TITLE, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m->info.title, s, MEDIA_META_ELEMENT_SIZE);
            
        } else if(strcmp(GST_TAG_ALBUM, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m->info.album, s, MEDIA_META_ELEMENT_SIZE);
            
        } else if(strcmp(GST_TAG_ARTIST, tag) == 0) {
            s = g_value_get_string(val);
            strncpy(m->info.artist, s, MEDIA_META_ELEMENT_SIZE);
            
        } else if(strcmp(GST_TAG_TRACK_NUMBER, tag) == 0) {
            m->info.track = g_value_get_uint(val);
            
        }
    }
}

static void on_discovered(GstDiscoverer *disc, 
                          GstDiscovererInfo *info, 
                          GError *error, 
                          void *data)
{
    GstDiscovererResult result;
    struct media *m;
    const char *uri, *path;
    const GstTagList *tags;
    
    uri    = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    
    if(!data) {
        path = uri + sizeof("file://") - 1;
        m = map_retrieve(media_map, path);
        
        /* If we cannot find a media for this uri it already got deleted */
        if(!m)
            return;
    } else {
        /* called in synchronous context */
        m = data;
    }
    
    switch(result) {
        case GST_DISCOVERER_URI_INVALID:
            climpd_log_w(tag, "invalid uri '%s'\n", uri);
            break;
        case GST_DISCOVERER_ERROR:
            break;
        case GST_DISCOVERER_TIMEOUT:
            climpd_log_w(tag, "timeout for uri '%s'\n", uri);
            break;
        case GST_DISCOVERER_BUSY:
            climpd_log_w(tag, "busy\n");
            break;
        case GST_DISCOVERER_MISSING_PLUGINS:

            break;
        case GST_DISCOVERER_OK:
            m->info.seekable = gst_discoverer_info_get_seekable(info);
            m->info.duration = gst_discoverer_info_get_duration(info) / 1e9;
            
            tags = gst_discoverer_info_get_tags(info);
            if(!tags) {
                climpd_log_w(tag, "no tags for discovered uri '%s'\n", uri);
                break;
            }
            
            gst_tag_list_foreach(tags, &parse_tags, m);
            
            m->parsed = true;
            break;
        default:
            break;
    }
}

int media_manager_init(void)
{
    GError *error;
    int err;
    
    media_map = map_new(0, &compare_string, &hash_string);
    if(!media_map) {
        err = -errno;
        goto out;
    }
    
    map_set_data_delete(media_map, (void (*)(void *)) &media_delete);
    
    async_disc = gst_discoverer_new(10 * GST_SECOND, &error);
    if(!async_disc) {
        err = -error->code;
        g_error_free(error);
        goto cleanup1;
    }
    
    sync_disc = gst_discoverer_new(10 * GST_SECOND, &error);
    if(!sync_disc) {
        err = -error->code;
        g_error_free(error);
        goto cleanup2;
    }
    
    g_signal_connect(async_disc, "discovered", G_CALLBACK(on_discovered), NULL);
    
    gst_discoverer_start(async_disc);
    
    climpd_log_i(tag, "initialized\n");
    
    return 0;

cleanup2:
    g_object_unref(async_disc);
cleanup1:
    map_delete(media_map);
out:
    climpd_log_e(tag, "failed to initialize - %s\n", strerr(-err));
    
    return err;
}

void media_manager_destroy(void)
{
    g_object_unref(sync_disc);
    gst_discoverer_stop(async_disc);
    g_object_unref(async_disc);
    map_delete(media_map);
    
    climpd_log_i(tag, "destroyed\n");
}

struct media *media_manager_retrieve(const char *__restrict path)
{
    struct media *m;
    int err;
    gboolean ok;
    
    m = map_retrieve(media_map, path);
    if(!m) {
        m = media_new(path);
        if(!m) {
            climpd_log_e(tag, "media_new(%s) - %s\n", path, errstr);
            return NULL;
        }
        
        err = map_insert(media_map, media_path(m), m);
        if(err < 0) {
            climpd_log_e(tag, "map_insert(%s) - %s\n", path, strerr(-err));
            goto cleanup1;
        }
        
        ok = gst_discoverer_discover_uri_async(async_disc, media_uri(m));
        if(!ok) {
            climpd_log_e(tag, "failed to discover uri '%s'\n", media_uri(m));
            err = -ENOMEM;
            goto cleanup2;
        }
    }
    
    return m;

cleanup2:
    map_take(media_map, path);
cleanup1:
    media_delete(m);
    errno = -err;
    
    return NULL;
}

void media_manager_delete_media(const char *__restrict path)
{
    struct media *m;
    
    m = map_take(media_map, path);
    if(!m)
        return;
    
    media_delete(m);
}

static bool is_playable(const char *__restrict uri)
{
    GstDiscovererResult result;
    GstDiscovererInfo *info;
    GstDiscovererStreamInfo *s_info, *s_info_next;
    GError *err;
    
    info = gst_discoverer_discover_uri(sync_disc, uri, &err);
    
    if(err)
        g_error_free(err);
    
    if(!info)
        return false;
    
    result = gst_discoverer_info_get_result(info);
    
    if(result != GST_DISCOVERER_OK) {
        gst_discoverer_info_unref(info);
        return false;
    } 
    
    s_info = gst_discoverer_info_get_stream_info(info);
    
    while(s_info) {
        if(GST_IS_DISCOVERER_VIDEO_INFO(s_info)) {
            gst_discoverer_stream_info_unref(s_info);
            gst_discoverer_info_unref(info);
            return false;
        }
        
        s_info_next = gst_discoverer_stream_info_get_next(s_info);
        
        gst_discoverer_stream_info_unref(s_info);
        
        s_info = s_info_next;
    }
    
    gst_discoverer_stream_info_unref(s_info);
    gst_discoverer_info_unref(info);
    
    return true;
}

int media_manager_parse_media(struct media *__restrict media)
{
    GstDiscovererInfo *info;
    GError *err;
    const char *uri;
    
    uri = media_uri(media);
    
    info = gst_discoverer_discover_uri(sync_disc, uri, &err);
    if(!info) {
        climpd_log_w(tag, "sync parsing '%s' failed - %s\n", uri, err->message);
        return -EINVAL;
    }
    
    on_discovered(sync_disc, info, err, media);
    
    gst_discoverer_info_unref(info);
    
    return 0;
}

struct discoverer {
    struct vector *dir_vec;
    int fd;
};

static int discoverer_init(struct discoverer *__restrict d,
                           const char *path,
                           int fd)
{
    char *path_dup;
    size_t len;
    int err;
    
    d->dir_vec = vector_new(64);
    if(!d->dir_vec)
        return -errno;
    
    vector_set_data_delete(d->dir_vec, &free);
    
    len = strlen(path);
    
    path_dup = malloc(len + 1 + 1);
    if(!path_dup)
        goto cleanup1;
    
    path_dup = strcpy(path_dup, path);
    
    if(path[len] != '/')
        path_dup = strcat(path_dup, "/");
    
    err = vector_insert_back(d->dir_vec, path_dup);
    if(err < 0)
        goto cleanup2;
    
    d->fd = fd;
    
    return 0;

cleanup2:
    free(path_dup);
cleanup1:
    vector_delete(d->dir_vec);
    
    return err;
}

static void discoverer_destroy(struct discoverer *__restrict d)
{
    vector_delete(d->dir_vec);
}

static void discoverer_run(struct discoverer *__restrict d)
{
    DIR *dir;
    struct dirent buf, *ent;
    char *current_dir, *path, *uri;
    size_t len, len_path;
    unsigned int i;
    int err;
    
    /* 
     * Don't use 'vector_for_each()', the iterator pointer may become invalid
     * if the vector gets resized during a insert-operation.
     */
    for(i = 0; i < vector_size(d->dir_vec); ++i) {
        current_dir = *vector_at(d->dir_vec, i);
        
        len_path = strlen(current_dir);
        
        dir = opendir(current_dir);
        if(!dir) {
            climpd_log_w(tag, "failed to open directory");
            climpd_log_append(" %s - %s\n", current_dir, errstr);
            continue;
        }
        
        while(1) {
            err = readdir_r(dir, &buf, &ent);
            if(err || !ent)
                break;
            
            if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;
            
            if(ent->d_type == DT_DIR) {
                path = malloc(len_path + strlen(ent->d_name) + 2);
                if(!path) {
                    err = -errno;
                    climpd_log_w(tag, "failed to open directory ");
                    climpd_log_append("%s - %s\n", ent->d_name, strerr(-err));
                    continue;
                }
                
                path = strcpy(path, current_dir);
                path = strcat(path, ent->d_name);
                path = strcat(path, "/");
                                
                err = vector_insert_back(d->dir_vec, path);
                if(err < 0) {
                   climpd_log_w(tag, "skipping directory ");
                   climpd_log_append("%s - %s\n", path, strerr(-err));
                   continue;
                }
                
            } else if(ent->d_type == DT_REG) {
                /* 
                 * Add an additional byte if the trailing '/' 
                 * is missing in 'd->path'
                 */
                len = sizeof("file://") + len_path + strlen(ent->d_name);
                
                uri = malloc(len);
                if(!uri) {
                    climpd_log_w(tag, "ignoring file");
                    climpd_log_append("%s - %s\n", ent->d_name, strerr(ENOMEM));
                    continue;
                }
                
                uri = strcpy(uri, "file://");
                uri = strcat(uri, current_dir);
                uri = strcat(uri, ent->d_name);
                
                if(is_playable(uri))
                    dprintf(d->fd, "%s\n", uri + sizeof("file://") - 1);
                
                free(uri);
            }
        }
        
        closedir(dir);
        free(current_dir);
        
        *vector_at(d->dir_vec, i) = NULL;
    }
}

void media_manager_discover_directory(const char *__restrict path, int fd)
{
    struct discoverer disc;
    int err;
    
    err = discoverer_init(&disc, path, fd);
    if(err < 0) {
        dprintf(fd, "unable to discover %s - %s\n", path, strerr(-err));
        return;
    }
    
    discoverer_run(&disc);
    discoverer_destroy(&disc);
}