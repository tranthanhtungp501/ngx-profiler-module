/*
* Copyright (C) Duc Le Anh
*/

#include <ngx_config.h>
#include <ngx_core.h>

static char* ngx_http_profiler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void * ngx_http_profiler_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_profiler_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
void ngx_timer_fired(ngx_event_t *ev);

typedef struct {    
    ngx_uint_t      profiler;
    size_t          freq;
    ngx_str_t       path;//where to save data
} ngx_http_profiler_loc_conf_t;

static ngx_event_t      profiler_timer;
static ngx_msec_t       freq;

static ngx_command_t    ngx_http_profiler_commands[] = {
    {
        ngx_string("system_profiler"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_profiler,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_profiler_loc_conf_t, profiler),
        NULL
    },
    {
        ngx_string("system_profiler_freq"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_profiler_loc_conf_t, freq),
        NULL
    },
    {
        ngx_string("system_profiler_freq"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_profiler_loc_conf_t, freq),
        NULL
    },
    {
        ngx_string("system_profiler_path"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_profiler_loc_conf_t, path),
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_profiler_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    ngx_http_profiler_create_loc_conf,  /* create location configuration */
    ngx_http_profiler_merge_loc_conf /* merge location configuration */
};

ngx_module_t ngx_http_profiler_module = {
    NGX_MODULE_V1,
    &ngx_http_profiler_module_ctx,
    ngx_http_profiler_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static char* ngx_http_profiler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf){
    ngx_http_core_loc_conf_t    *clcf;
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_profiler_handler;
    return ngx_conf_set_flag_slot(cf, cmd, conf);
}

static ngx_int_t ngx_http_profiler_handler(ngx_http_request_t *r){
    ngx_http_profiler_loc_conf_t *plcf;
    
    plcf = ngx_http_get_module_loc_conf(r, ngx_http_profiler_module);
    if(plcf == NULL || plcf->profiler == 0){
        return NGX_DECLINED;
    }

}

static ngx_int_t ngx_http_profiler_ensure_directory(ngx_conf_t *cf, ngx_str_t *path){    
    ngx_file_info_t                 fi;
    static u_char                   zpath[NGX_MAX_PATH + 1];
    
    if(path->len + 1 > sizeof(zpath)){
        ngx_log_error(NGX_LOG_ERR, cf->log, 0, "profiler: too long path: %s", path->data);
        return NGX_ERROR;
    }
    ngx_snprintf(zpath, sizeof(zpath), "%V%Z", path);
    if(ngx_file_info(zpath, &fi) == NGX_FILE_ERROR){
        if(ngx_errno != NGX_ENOENT){
            ngx_log_error(NGX_LOG_ERR, cf->log, ngx_errono, "profiler: " ngx_file_info_n " failed on '%V'", path );
            return NGX_ERROR;
        }
        if(ngx_create_dir(zpath, NGX_HTTP_PROFILER_DIR_ACCESS) == NGX_FILE_ERROR){
            ngx_log_error(NGX_LOG_ERR, cf->log, ng_errno, "profiler: " ngx_create_dir_n " failed on '%V'", path);
            return NGX_ERROR;
        }        
    }else{
        if(!(ngx_is_dir(&fi))){
            ngx_log_error(NGX_LOG_ERR, cf->log, 0, "profiler: '%V' exists and is not a directory", path);
            return NGX_ERROR;
        }
    }
    return NGX_OK;
}

// https://www.nginx.com/resources/wiki/extending/api/event/
void ngx_timer_fired(ngx_event_t *ev){
    ngx_log_error(NGX_LOG_ERR, ev->log, 0, "Event fired!");
    if(ngx_exiting){
        return;
    }
    ngx_add_timer(ev, freq);
}

static void * ngx_http_profiler_create_loc_conf(ngx_conf_t *cf){
    ngx_http_profiler_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_profiler_loc_conf_t));
    if(conf == NULL){
        return NGX_CONF_ERROR;
    }
    conf->profiler = NGX_CONF_UNSET;
    conf->freq = NGX_CONF_UNSET_UINT;    
    return conf;
}

static char* ngx_http_profiler_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child){
    ngx_http_profiler_loc_conf_t *prev = parent;
    ngx_http_profiler_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->profiler, prev->profiler, 0);
    ngx_conf_merge_msec_value(conf->freq, prev->freq, 60000);//Default: 10m
    ngx_conf_merge_str_value(conf->path, prev->path, "");
    if(conf->profiler == 1 & conf->path.len == 0){        
        return NGX_CONF_ERROR;
    }
    //create dir
    if(ngx_http_profiler_ensure_directory(cf, conf->path) != NGX_OK){
        return NGX_CONF_ERROR;
    }
    //set timer to collect data
    profiler_timer.handler = ngx_timer_fired;
    profiler_timer.log = cf->log;
    profiler_timer.data = NULL;
    freq = conf->freq;
    ngx_add_timer(profiler_timer, conf->freq);
    return NGX_CONF_OK;
}