/*
 * Copyright (C) 2016 Intel Corporation.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifdef HAVE_CONFIG_H
# include <darshan-runtime-config.h>
#endif

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <search.h>
#include <assert.h>
#include <libgen.h>
#include <pthread.h>
#include <regex.h>

#include "utlist.h"
#include "uthash.h"
#include "darshan.h"
#include "darshan-dynamic.h"
#include "darshan-dxt.h"

/* Check for LDMS libraries if Darshan is built --with-dxt-ldms */
#ifdef HAVE_LDMS
#include "darshan-core.h"
#include <ldms/ldms_sps.h>
#include <ldms/ldms.h>
#include <ldms/ldmsd_stream.h>
#include <ovis_util/util.h>
#include "ovis_json/ovis_json.h"
#endif

#ifndef HAVE_OFF64_T
typedef int64_t off64_t;
#endif

/* maximum amount of memory to use for storing DXT records */
#ifdef __DARSHAN_MOD_MEM_MAX
#define DXT_IO_TRACE_MEM_MAX (__DARSHAN_MOD_MEM_MAX * 1024L * 1024L)
#else
#define DXT_IO_TRACE_MEM_MAX (4 * 1024 * 1024) /* 4 MiB default */
#endif

/* initial size of read/write trace buffer (in number of segments) */
/* NOTE: when this size is exceeded, the buffer size is doubled */
#define IO_TRACE_BUF_SIZE       64

/* XXX: dirty hack -- If DXT runs out of memory to store trace data in,
 * we should set a flag so that log parsers know that the log has
 * incomplete data. This functionality is typically handled automatically
 * when registering records with Darshan, but DXT modules don't
 * register records and manage their own memory. Since DXT modules request
 * 0 memory when registering themselves, any attempt to register a record
 * will result in setting the partial flag for the module, which is
 * exactly what we do here.
 */
#define SET_DXT_MOD_PARTIAL_FLAG(mod_id) \
    darshan_core_register_record(0, NULL, mod_id, 1, NULL);

/* The dxt_file_record_ref structure maintains necessary runtime metadata
 * for the DXT file record (dxt_file_record structure, defined in
 * darshan-dxt-log-format.h) pointed to by 'file_rec'. This metadata
 * assists with the instrumenting of specific statistics in the file record.
 *
 * NOTE: we use the 'darshan_record_ref' interface (in darshan-common) to
 * associate different types of handles with this dxt_file_record_ref struct.
 * This allows us to index this struct (and the underlying file record) by using
 * either the corresponding Darshan record identifier (derived from the filename)
 * or by a generated file descriptor, for instance. Note that, while there should
 * only be a single Darshan record identifier that indexes a dxt_file_record_ref,
 * there could be multiple open file descriptors that index it.
 */
struct dxt_file_record_ref
{
    struct dxt_file_record *file_rec;

    int64_t write_available_buf;
    int64_t read_available_buf;

    segment_info *write_traces;
    segment_info *read_traces;

    char trace_enabled;
};

/* The dxt_runtime structure maintains necessary state for storing
 * DXT file records and for coordinating with darshan-core at
 * shutdown time.
 */
struct dxt_posix_runtime
{
    void *rec_id_hash;
    int file_rec_count;
    char *record_buf;
    int record_buf_size;
};

struct dxt_mpiio_runtime
{
    void *rec_id_hash;
    int file_rec_count;
    char *record_buf;
    int record_buf_size;
};

enum dxt_trigger_type
{
    DXT_FILE_TRIGGER,
    DXT_RANK_TRIGGER,
    DXT_SMALL_IO_TRIGGER,
    DXT_UNALIGNED_IO_TRIGGER
};

struct dxt_trigger_info
{
    int type;
    union {
        struct
        {
            regex_t regex;
        } file;
        struct
        {
            regex_t regex;
        } rank;
        struct
        {
            double thresh_pct;
        } small_io;
        struct
        {
            double thresh_pct;
        } unaligned_io;
    } u;
};

#ifdef HAVE_LDMS

/* Initialize darshanConnector struct and set extra metrics to add to json message if LDMS is enabled. */
struct darshanConnector dC;
static pthread_mutex_t ln_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ln_meta_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ln_extra_lock = PTHREAD_MUTEX_INITIALIZER;

void darshan_ldms_set_rank_info(int64_t fastest_rank, int64_t slowest_rank, double fastest_rank_time, double slowest_rank_time)
{
    dC.fastest_rank = fastest_rank;
    dC.slowest_rank = slowest_rank;
    dC.fastest_rank_time = fastest_rank_time;
    dC.slowest_rank_time = slowest_rank_time;
}
#endif

/* internal helper routines */
static int dxt_should_trace_rank(
    int rank);
static int dxt_should_trace_file(
    darshan_record_id rec_id);
static void check_wr_trace_buf(
    struct dxt_file_record_ref *rec_ref);
static void check_rd_trace_buf(
    struct dxt_file_record_ref *rec_ref);
static struct dxt_file_record_ref *dxt_posix_track_new_file_record(
    darshan_record_id rec_id);
static struct dxt_file_record_ref *dxt_mpiio_track_new_file_record(
    darshan_record_id rec_id);

/* DXT output/cleanup routines for darshan-core */
static void dxt_posix_output(
    void **dxt_buf, int *dxt_buf_sz);
static void dxt_mpiio_output(
    void **dxt_buf, int *dxt_buf_sz);
static void dxt_posix_cleanup(
    void);
static void dxt_mpiio_cleanup(
    void);

static struct dxt_posix_runtime *dxt_posix_runtime = NULL;
static struct dxt_mpiio_runtime *dxt_mpiio_runtime = NULL;
static pthread_mutex_t dxt_runtime_mutex =
            PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static int dxt_my_rank = -1;
static size_t dxt_total_mem = DXT_IO_TRACE_MEM_MAX;
static size_t dxt_mem_remaining = 0;

#define MAX_DXT_TRIGGERS 20
static int num_dxt_triggers = 0;
static struct dxt_trigger_info dxt_triggers[MAX_DXT_TRIGGERS];
static int dxt_use_file_triggers = 0;
static int dxt_use_rank_triggers = 0;
static int dxt_use_dynamic_triggers = 0;
static int dxt_trace_all = 0;

#define DXT_LOCK() pthread_mutex_lock(&dxt_runtime_mutex)
#define DXT_UNLOCK() pthread_mutex_unlock(&dxt_runtime_mutex)

/************************************************************
 *  DXT routines exposed to Darshan core and other modules  *
 ************************************************************/

void dxt_load_trigger_conf(
    char *trigger_conf_path)
{
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    char *tok;
    struct dxt_trigger_info *next_trigger;
    int ret;

    fp = fopen(trigger_conf_path, "r");
    if(!fp)
    {
        darshan_core_fprintf(stderr, "darshan library warning: "\
            "unable to open DXT trigger config at path %s\n", trigger_conf_path);
        return;
    }

    while(getline(&line, &len, fp) != -1)
    {
        /* remove newline if present */
        if(line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';

        next_trigger = &dxt_triggers[num_dxt_triggers];

        /* extract trigger type and parameters */
        tok = strtok(line, " \t");
        if(strcmp(tok, "FILE") == 0)
        {
            next_trigger->type = DXT_FILE_TRIGGER;
            tok += strlen(tok) + 1;
            ret = regcomp(&next_trigger->u.file.regex, tok, REG_EXTENDED);
            if(ret)
            {
                darshan_core_fprintf(stderr, "darshan library warning: "\
                    "unable to compile DXT trigger regex from %s\n", line);
                continue;
            }
            dxt_use_file_triggers = 1;
        }
        else if(strcmp(tok, "RANK") == 0)
        {
            next_trigger->type = DXT_RANK_TRIGGER;
            tok += strlen(tok) + 1;
            ret = regcomp(&next_trigger->u.rank.regex, tok, REG_EXTENDED);
            if(ret)
            {
                darshan_core_fprintf(stderr, "darshan library warning: "\
                    "unable to compile DXT trigger regex from %s\n", line);
                continue;
            }
            dxt_use_rank_triggers= 1;
        }
        else if(strcmp(tok, "SMALL_IO") == 0)
        {
            next_trigger->type = DXT_SMALL_IO_TRIGGER;
            tok += strlen(tok) + 1;
            next_trigger->u.small_io.thresh_pct = atof(tok);
            dxt_use_dynamic_triggers= 1;
        }
        else if(strcmp(tok, "UNALIGNED_IO") == 0)
        {
            next_trigger->type = DXT_UNALIGNED_IO_TRIGGER;
            tok += strlen(tok) + 1;
            next_trigger->u.unaligned_io.thresh_pct = atof(tok);
            dxt_use_dynamic_triggers= 1;
        }
        else
        {
            darshan_core_fprintf(stderr, "darshan library warning: "\
                "unknown DXT trigger (%s) found in %s\n", tok, trigger_conf_path);
            continue;
        }
        ++num_dxt_triggers;
    }

    fclose(fp);
    free(line);
    return;
}

/* initialize internal DXT module data structures and register with darshan-core */
void dxt_posix_runtime_initialize()
{
    /* DXT modules request 0 memory -- buffers will be managed internally by DXT
     * and passed back to darshan-core at shutdown time to allow DXT more control
     * over realloc'ing module memory as needed.
     */
    size_t dxt_psx_buf_size = 0;
    darshan_module_funcs mod_funcs = {
#ifdef HAVE_MPI
    .mod_redux_func = NULL,
#endif
    .mod_output_func = &dxt_posix_output,
    .mod_cleanup_func = &dxt_posix_cleanup
    };

    /* determine whether tracing should be generally disabled/enabled */
    if(getenv("DXT_ENABLE_IO_TRACE"))
        dxt_trace_all = 1;
    else if(getenv("DXT_DISABLE_IO_TRACE"))
        return;

    /* register the DXT module with darshan core */
    darshan_core_register_module(
        DXT_POSIX_MOD,
        mod_funcs,
        &dxt_psx_buf_size,
        &dxt_my_rank,
        NULL);

    /* return if darshan-core allocates an unexpected amount of memory */
    if(dxt_psx_buf_size != 0)
    {
        darshan_core_unregister_module(DXT_POSIX_MOD);
        return;
    }

    /* determine whether we should avoid tracing on this rank */
    if(!dxt_should_trace_rank(dxt_my_rank))
    {
        if(!dxt_trace_all && !dxt_use_file_triggers && !dxt_use_dynamic_triggers)
        {
            /* nothing to trace, just back out */
            darshan_core_unregister_module(DXT_POSIX_MOD);
            return;
        }
    }
    else
    {
        dxt_trace_all = 1; /* trace everything */
    }

    DXT_LOCK();
    dxt_posix_runtime = malloc(sizeof(*dxt_posix_runtime));
    if(!dxt_posix_runtime)
    {
        darshan_core_unregister_module(DXT_POSIX_MOD);
        DXT_UNLOCK();
        return;
    }
    memset(dxt_posix_runtime, 0, sizeof(*dxt_posix_runtime));
    dxt_mem_remaining = dxt_total_mem;
    DXT_UNLOCK();

    return;
}

void dxt_mpiio_runtime_initialize()
{
    /* DXT modules request 0 memory -- buffers will be managed internally by DXT
     * and passed back to darshan-core at shutdown time to allow DXT more control
     * over realloc'ing module memory as needed.
     */
    size_t dxt_mpiio_buf_size = 0;
    darshan_module_funcs mod_funcs = {
#ifdef HAVE_MPI
    .mod_redux_func = NULL,
#endif
    .mod_output_func = &dxt_mpiio_output,
    .mod_cleanup_func = &dxt_mpiio_cleanup
    };

    /* determine whether tracing should be generally disabled/enabled */
    if(getenv("DXT_ENABLE_IO_TRACE"))
        dxt_trace_all = 1;
    else if(getenv("DXT_DISABLE_IO_TRACE"))
        return;

    /* register the DXT module with darshan core */
    darshan_core_register_module(
        DXT_MPIIO_MOD,
        mod_funcs,
        &dxt_mpiio_buf_size,
        &dxt_my_rank,
        NULL);

    /* return if darshan-core allocates an unexpected amount of memory */
    if(dxt_mpiio_buf_size != 0)
    {
        darshan_core_unregister_module(DXT_MPIIO_MOD);
        return;
    }

    /* determine whether we should avoid tracing on this rank */
    if(!dxt_should_trace_rank(dxt_my_rank))
    {
        if(!dxt_trace_all && !dxt_use_file_triggers && !dxt_use_dynamic_triggers)
        {
            /* nothing to trace, just back out */
            darshan_core_unregister_module(DXT_MPIIO_MOD);
            return;
        }
    }
    else
    {
        dxt_trace_all = 1; /* trace everything */
    }

    DXT_LOCK();
    dxt_mpiio_runtime = malloc(sizeof(*dxt_mpiio_runtime));
    if(!dxt_mpiio_runtime)
    {
        darshan_core_unregister_module(DXT_MPIIO_MOD);
        DXT_UNLOCK();
        return;
    }
    memset(dxt_mpiio_runtime, 0, sizeof(*dxt_mpiio_runtime));
    dxt_mem_remaining = dxt_total_mem; /* XXX is this right? better with memory */
    DXT_UNLOCK();

    return;
}

#ifdef HAVE_LDMS
ldms_t ldms_g;
static void event_cb(ldms_t x, ldms_xprt_event_t e, void *cb_arg)
{
	switch (e->type) {
	case LDMS_XPRT_EVENT_CONNECTED:
		sem_post(&dC.conn_sem);
		dC.conn_status = 0;
		break;
	case LDMS_XPRT_EVENT_REJECTED:
		ldms_xprt_put(x);
		dC.conn_status = ECONNREFUSED;
		break;
	case LDMS_XPRT_EVENT_DISCONNECTED:
		ldms_xprt_put(x);
		dC.conn_status = ENOTCONN;
		break;
	case LDMS_XPRT_EVENT_ERROR:
		dC.conn_status = ECONNREFUSED;
		break;
	case LDMS_XPRT_EVENT_RECV:
		sem_post(&dC.recv_sem);
		break;
	case LDMS_XPRT_EVENT_SEND_COMPLETE:
		break;
	default:
		printf("Received invalid event type %d\n", e->type);
	}
}

#define SLURM_NOTIFY_TIMEOUT 5

ldms_t setup_connection(const char *xprt, const char *host,
			const char *port, const char *auth)
{
	char hostname[PATH_MAX];
	const char *timeout = "5";
	int rc;
	struct timespec ts;

	if (!host) {
		if (0 == gethostname(hostname, sizeof(hostname)))
			host = hostname;
	}
	if (!timeout) {
		ts.tv_sec = time(NULL) + 5;
		ts.tv_nsec = 0;
	} else {
		int to = atoi(timeout);
		if (to <= 0)
			to = 5;
		ts.tv_sec = time(NULL) + to;
		ts.tv_nsec = 0;
	}
	ldms_g = ldms_xprt_new_with_auth(xprt, NULL, auth, NULL);
	if (!ldms_g) {
		printf("Error %d creating the '%s' transport\n",
		       errno, xprt);
		return NULL;
	}

	sem_init(&dC.recv_sem, 1, 0);
	sem_init(&dC.conn_sem, 1, 0);

	rc = ldms_xprt_connect_by_name(ldms_g, host, port, event_cb, NULL);
	if (rc) {
		printf("Error %d connecting to %s:%s\n",
		       rc, host, port);
		return NULL;
	}
	sem_timedwait(&dC.conn_sem, &ts);
	if (dC.conn_status)
		return NULL;
	return ldms_g;
}

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static int log_level = 0;
static void llog(int lvl, const char *fmt, ...) {
	if (lvl < log_level)
		return;
	pthread_mutex_lock(&log_lock);
	switch (lvl) {
	case 3:
		printf("ERR: ");
	default:
		printf("Level-%d: ", lvl);
	}
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
	pthread_mutex_unlock(&log_lock);
}

void dxt_darshan_ldms_connector_initialize()
{
    int i;
    int size;
    size = sizeof(dC.ldms_darsh)/sizeof(dC.ldms_darsh[0]);
    dC.env_ldms_stream  = getenv("DARSHAN_LDMS_STREAM");
    const int   env_ldms_port    = atoi(getenv("DARSHAN_LDMS_PORT"));
    const char* env_ldms_xprt    = getenv("DARSHAN_LDMS_XPRT");
    const char* env_ldms_host    = getenv("DARSHAN_LDMS_HOST");
    //const char* env_ldms_port    = getenv("DARSHAN_LDMS_PORT");
    const char* env_ldms_auth    = getenv("DARSHAN_LDMS_AUTH");
    dC.ln = ldms_sps_create_1(dC.env_ldms_stream, env_ldms_xprt, env_ldms_host, env_ldms_port, env_ldms_auth, 1, 5, llog, 0, "/projects/darshan/test/test_sps.send.log");

    //for(i = 0; i < size-1; i++){
    //dC.ldms_darsh[i] = setup_connection(env_ldms_xprt, env_ldms_host, env_ldms_port, env_ldms_auth);
    //if (dC.ldms_darsh[i]->disconnected){
    //            printf("Error setting up connection -- exiting\n");
    //            return;
    //    }
    
    //}
    
    if (!dC.ln)
    	printf("FAILED ldms_sps_create_1\n");
    
    return;
}

void darshan_ldms_connector_send_extra(char* rwo, char *mod_name, int64_t size_0_100, int64_t size_100_1K, int64_t size_1K_10K, int64_t size_10K_100K, int64_t size_100K_1M, int64_t size_1M_4M,int64_t size_4M_10M, int64_t size_10M_100M, int64_t size_100M_1G, int64_t size_1G_PLUS)
{
    int rc, ret;
    //struct ldms_sps_send_result r = LN_NULL_RESULT;

    dC.env_ldms_stream  = getenv("DARSHAN_LDMS_STREAM_EXTRA");

    // set hostname
    char hname[HOST_NAME_MAX];
    (void)gethostname(hname, sizeof(hname));

    pthread_mutex_lock(&ln_extra_lock);

    if (!dC.ldms_darsh[1]){
    //if (!ln){
        dxt_darshan_ldms_connector_initialize();
    }
    if (!dC.ldms_darsh[1]){
    //if (!ln){
        printf("ldms_extra does not exist \n");
        pthread_mutex_unlock(&ln_extra_lock);
        return;
    }

    pthread_mutex_unlock(&ln_extra_lock);
    
    if (strcmp(rwo, "write") == 0){
        dC.op_name = "write";
        dC.op_name2 = "writes_segment_";
    }
    if (strcmp(rwo, "read") == 0){
        dC.op_name = "read";
        dC.op_name2 = "reads_segment_";
    }

    jbuf_t jb, jbd;
    jbd = jb = jbuf_new(); if (!jb) goto out_1;
    jb = jbuf_append_str(jb, "{ "); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "job_id", "%d,", dC.jobid); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "rank", "%d,", dC.rank); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "ProducerName", "\"%s\",", hname); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "file", "\"%s\",", dC.filename); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "record_id","%"PRIu64",", dC.record_id); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "module", "\"%s\",", mod_name); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "type","\"%s\",", dC.op_name); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "fast_rank", "%d,", dC.fastest_rank); if (!jb) goto out_1;

    jb = jbuf_append_attr(jb, "fast_rank_tm","\"%f\",", dC.fastest_rank_time); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "slow_rank", "%d,", dC.slowest_rank); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "slow_rank_tm","\"%f\",", dC.slowest_rank_time); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "size", "[{"); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "0_100", "%d,", size_0_100); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "100_1K", "%d,", size_100_1K); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "1K_10K", "%d,", size_1K_10K); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "10K_100K", "%d,", size_10K_100K); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "100K_1M", "%d,", size_100K_1M); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "1M_4M", "%d,", size_1M_4M); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "4M_10M", "%d,", size_4M_10M); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "10M_100M", "%d,", size_10M_100M); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "100M_1G", "%d,", size_100M_1G); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "1G_PLUS", "%d", size_1G_PLUS); if (!jb) goto out_1;
    jb = jbuf_append_str(jb, "}]}");
    //printf("this is in jb for EXTRA: %s \n", jb->buf);

    rc = ldmsd_stream_publish(dC.ldms_darsh[1], dC.env_ldms_stream, LDMSD_STREAM_JSON, jb->buf, (jb->cursor) + 1);
    if (rc)
        printf("Error %d publishing data.\n", rc);

 out_1:
        if (!jb ){
            jbuf_free(jbd);
        }
    return;


}

void darshan_ldms_set_meta(const char *filename, uint64_t record_id, int64_t rank)
{
    dC.rank = rank;
    dC.filename = filename;
    dC.record_id = record_id;
    return;

}
void dxt_darshan_ldms_connector_send(int64_t record_count, char *rwo, int64_t offset, int64_t length, int64_t max_byte, int64_t rw_switch, double start_time, double end_time, struct timeval tval_start, struct timeval tval_end, double total_time, char *mod_name, char *data_type)
{
    int rc, ret;
    struct ldms_sps_send_result r = LN_NULL_RESULT;
    dC.env_ldms_stream  = getenv("DARSHAN_LDMS_STREAM");
    
    // set hostname
    char hname[HOST_NAME_MAX];
    (void)gethostname(hname, sizeof(hname));

    pthread_mutex_lock(&ln_lock);
    //if (!dC.ldms_darsh[0])
    if (!dC.ln)
        dxt_darshan_ldms_connector_initialize();

    //if (!dC.ldms_darsh[0]){
    if (!dC.ln){
        printf("ldms_mod does not exist \n");
        pthread_mutex_unlock(&ln_lock);
        return;
    }
    
    pthread_mutex_unlock(&ln_lock);
    
    if (strcmp(rwo, "write") == 0){
        dC.op_name = "write";
        dC.op_name2 = "writes_segment_";
    }
    if (strcmp(rwo, "read") == 0){
        dC.op_name = "read";
        dC.op_name2 = "reads_segment_";
    }
    if (strcmp(rwo, "open") == 0){
        dC.op_name = "open";
        dC.op_name2 = "opens_segment_";
    }
    if (strcmp(data_type, "MOD") == 0)
        dC.filename = "N/A";

    jbuf_t jb, jbd;
    jbd = jb = jbuf_new(); if (!jb) goto out_1;
    
    jb = jbuf_append_str(jb, "{ "); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "job_id", "%d,", dC.jobid); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "rank", "%d,", dC.rank); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "ProducerName", "\"%s\",", hname); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "file", "\"%s\",", dC.filename); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "record_id","%"PRIu64",", dC.record_id); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "module", "\"%s\",", mod_name); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "type","\"%s\",", data_type); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "max_byte", "%lld,", max_byte);if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "switches", "%d,", rw_switch);if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "cnt", "%d,", record_count); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "op","\"%s%d\",", dC.op_name2,record_count-1); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "seg", "[{"); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "off", "%lld,", offset);if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "len", "%lld,", length);if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "dur", "%0.2f,", total_time); if (!jb) goto out_1;
    jb = jbuf_append_attr(jb, "timestamp", "%lu.%0.6lu", tval_end.tv_sec, tval_end.tv_usec); if (!jb) goto out_1;
    jb = jbuf_append_str(jb, "}]}");
    printf("this is in jb %s \n", jb->buf);
/*
    //save json to a file
    FILE *fp;
    fp = fopen("/projects/darshan/test/darshan_output.json", "a");
    fprintf(fp, "%s\n", jb->buf);
    fclose(fp);
  */  

    r = ldms_sps_send_event(dC.ln, jb);
    printf("this is the publish_count for %s: %i, rc: %i\n", mod_name, r.publish_count,r.rc);
    
    //rc = ldmsd_stream_publish(dC.ldms_darsh[0], dC.env_ldms_stream, LDMSD_STREAM_JSON, jb->buf, (jb->cursor) + 1);
    //if (rc)
    //    printf("Error %d publishing data.\n", rc);
    
 out_1:
        if (!jb ){
	    jbuf_free(jbd);
        }
    return;
}
#endif

void dxt_posix_write(darshan_record_id rec_id, int64_t offset,
        int64_t length, double start_time, double end_time, struct timeval tval_start, struct timeval tval_end)
{
    struct dxt_file_record_ref* rec_ref = NULL;
    struct dxt_file_record *file_rec;
    int should_trace_file;

    DXT_LOCK();

    if(!dxt_posix_runtime)
    {
        DXT_UNLOCK();
        return;
    }

    rec_ref = darshan_lookup_record_ref(dxt_posix_runtime->rec_id_hash,
        &rec_id, sizeof(darshan_record_id));
    if(!rec_ref)
    {
        /* check whether we should actually trace */
        should_trace_file = dxt_should_trace_file(rec_id);
        if(!should_trace_file && !dxt_trace_all && !dxt_use_dynamic_triggers)
        {
            DXT_UNLOCK();
            return;
        }

        /* track new dxt file record */
        rec_ref = dxt_posix_track_new_file_record(rec_id);
        if(!rec_ref)
        {
            DXT_UNLOCK();
            return;
        }
        if(should_trace_file)
            rec_ref->trace_enabled = 1;
    }

    file_rec = rec_ref->file_rec;
    check_wr_trace_buf(rec_ref);
    if(file_rec->write_count == rec_ref->write_available_buf)
    {
        /* no more memory for i/o segments ... back out */
        SET_DXT_MOD_PARTIAL_FLAG(DXT_POSIX_MOD);
        DXT_UNLOCK();
        return;
    }

    rec_ref->write_traces[file_rec->write_count].offset = offset;
    rec_ref->write_traces[file_rec->write_count].length = length;
    rec_ref->write_traces[file_rec->write_count].start_time = start_time;
    rec_ref->write_traces[file_rec->write_count].end_time = end_time;
    file_rec->write_count += 1;

    DXT_UNLOCK();
}

void dxt_posix_read(darshan_record_id rec_id, int64_t offset,
        int64_t length, double start_time, double end_time, struct timeval tval_start, struct timeval tval_end)
{
    struct dxt_file_record_ref* rec_ref = NULL;
    struct dxt_file_record *file_rec;
    int should_trace_file;

    DXT_LOCK();

    if(!dxt_posix_runtime)
    {
        DXT_UNLOCK();
        return;
    }

    rec_ref = darshan_lookup_record_ref(dxt_posix_runtime->rec_id_hash,
                &rec_id, sizeof(darshan_record_id));
    if (!rec_ref)
    {
        /* check whether we should actually trace */
        should_trace_file = dxt_should_trace_file(rec_id);
        if(!should_trace_file && !dxt_trace_all && !dxt_use_dynamic_triggers)
        {
            DXT_UNLOCK();
            return;
        }

        /* track new dxt file record */
        rec_ref = dxt_posix_track_new_file_record(rec_id);
        if(!rec_ref)
        {
            DXT_UNLOCK();
            return;
        }
        if(should_trace_file)
            rec_ref->trace_enabled = 1;
    }

    file_rec = rec_ref->file_rec;
    check_rd_trace_buf(rec_ref);
    if(file_rec->read_count == rec_ref->read_available_buf)
    {
        /* no more memory for i/o segments ... back out */
        SET_DXT_MOD_PARTIAL_FLAG(DXT_POSIX_MOD);
        DXT_UNLOCK();
        return;
    }

    rec_ref->read_traces[file_rec->read_count].offset = offset;
    rec_ref->read_traces[file_rec->read_count].length = length;
    rec_ref->read_traces[file_rec->read_count].start_time = start_time;
    rec_ref->read_traces[file_rec->read_count].end_time = end_time;
    file_rec->read_count += 1;

    DXT_UNLOCK();
}

void dxt_mpiio_write(darshan_record_id rec_id, int64_t offset,
        int64_t length, double start_time, double end_time, struct timeval tval_start, struct timeval tval_end)
{
    struct dxt_file_record_ref* rec_ref = NULL;
    struct dxt_file_record *file_rec;
    int should_trace_file;

    DXT_LOCK();

    if(!dxt_mpiio_runtime)
    {
        DXT_UNLOCK();
        return;
    }

    rec_ref = darshan_lookup_record_ref(dxt_mpiio_runtime->rec_id_hash,
                &rec_id, sizeof(darshan_record_id));
    if(!rec_ref)
    {
        /* check whether we should actually trace */
        should_trace_file = dxt_should_trace_file(rec_id);
        if(!should_trace_file && !dxt_trace_all && !dxt_use_dynamic_triggers)
        {
            DXT_UNLOCK();
            return;
        }

        /* track new dxt file record */
        rec_ref = dxt_mpiio_track_new_file_record(rec_id);
        if(!rec_ref)
        {
            DXT_UNLOCK();
            return;
        }
        if(should_trace_file)
            rec_ref->trace_enabled = 1;
    }

    file_rec = rec_ref->file_rec;
    check_wr_trace_buf(rec_ref);
    if(file_rec->write_count == rec_ref->write_available_buf)
    {
        /* no more memory for i/o segments ... back out */
        SET_DXT_MOD_PARTIAL_FLAG(DXT_MPIIO_MOD);
        DXT_UNLOCK();
        return;
    }

    rec_ref->write_traces[file_rec->write_count].length = length;
    rec_ref->write_traces[file_rec->write_count].offset = offset;
    rec_ref->write_traces[file_rec->write_count].start_time = start_time;
    rec_ref->write_traces[file_rec->write_count].end_time = end_time;
    file_rec->write_count += 1;

    DXT_UNLOCK();
}

void dxt_mpiio_read(darshan_record_id rec_id, int64_t offset,
        int64_t length, double start_time, double end_time, struct timeval tval_start, struct timeval tval_end)
{
    struct dxt_file_record_ref* rec_ref = NULL;
    struct dxt_file_record *file_rec;
    int should_trace_file;

    DXT_LOCK();

    if(!dxt_mpiio_runtime)
    {
        DXT_UNLOCK();
        return;
    }

    rec_ref = darshan_lookup_record_ref(dxt_mpiio_runtime->rec_id_hash,
                &rec_id, sizeof(darshan_record_id));
    if(!rec_ref)
    {
        /* check whether we should actually trace */
        should_trace_file = dxt_should_trace_file(rec_id);
        if(!should_trace_file && !dxt_trace_all && !dxt_use_dynamic_triggers)
        {
            DXT_UNLOCK();
            return;
        }

        /* track new dxt file record */
        rec_ref = dxt_mpiio_track_new_file_record(rec_id);
        if(!rec_ref)
        {
            DXT_UNLOCK();
            return;
        }
        if(should_trace_file)
            rec_ref->trace_enabled = 1;
    }

    file_rec = rec_ref->file_rec;
    check_rd_trace_buf(rec_ref);
    if(file_rec->read_count == rec_ref->read_available_buf)
    {
        /* no more memory for i/o segments ... back out */
        SET_DXT_MOD_PARTIAL_FLAG(DXT_MPIIO_MOD);
        DXT_UNLOCK();
        return;
    }

    rec_ref->read_traces[file_rec->read_count].length = length;
    rec_ref->read_traces[file_rec->read_count].offset = offset;
    rec_ref->read_traces[file_rec->read_count].start_time = start_time;
    rec_ref->read_traces[file_rec->read_count].end_time = end_time;
    file_rec->read_count += 1;

    DXT_UNLOCK();
}

static void dxt_posix_filter_dynamic_traces_iterator(void *rec_ref_p, void *user_ptr)
{
    struct dxt_file_record_ref *psx_rec_ref, *mpiio_rec_ref;
    struct darshan_posix_file *(*rec_id_to_psx_file)(darshan_record_id);
    struct darshan_posix_file *psx_file;
    int i;
    int should_keep = 0;

    psx_rec_ref = (struct dxt_file_record_ref *)rec_ref_p;
    if(psx_rec_ref->trace_enabled)
        return; /* we're already tracing this file */

    rec_id_to_psx_file = (struct darshan_posix_file *(*)(darshan_record_id))user_ptr;
    psx_file = rec_id_to_psx_file(psx_rec_ref->file_rec->base_rec.id);

    /* analyze dynamic triggers to determine whether we should keep the record */
    for(i = 0; i < num_dxt_triggers; i++)
    {
        switch(dxt_triggers[i].type)
        {
            case DXT_SMALL_IO_TRIGGER:
            {
                int total_ops = psx_file->counters[POSIX_WRITES] +
                    psx_file->counters[POSIX_READS];
                int small_ops = psx_file->counters[POSIX_SIZE_WRITE_0_100] +
                    psx_file->counters[POSIX_SIZE_WRITE_100_1K] + 
                    psx_file->counters[POSIX_SIZE_WRITE_1K_10K] +
                    psx_file->counters[POSIX_SIZE_READ_0_100] +
                    psx_file->counters[POSIX_SIZE_READ_100_1K] + 
                    psx_file->counters[POSIX_SIZE_READ_1K_10K];
                double small_pct = (small_ops / (double)(total_ops));
                if(small_pct > dxt_triggers[i].u.small_io.thresh_pct)
                    should_keep = 1;
                break;
            }
            case DXT_UNALIGNED_IO_TRIGGER:
            {
                int total_ops = psx_file->counters[POSIX_WRITES] +
                    psx_file->counters[POSIX_READS];
                int unaligned_ops = psx_file->counters[POSIX_FILE_NOT_ALIGNED];
                double unaligned_pct = (unaligned_ops / (double)(total_ops));
                if(unaligned_pct > dxt_triggers[i].u.unaligned_io.thresh_pct)
                    should_keep = 1;
                break;
            }
            default:
                continue;
        }
        if(should_keep)
            break;
    }

    /* drop the record if no dynamic trace triggers occurred */
    if(!should_keep)
    {
        if(dxt_mpiio_runtime && dxt_mpiio_runtime->rec_id_hash)
        {
            /* first check the MPI-IO traces to see if we should drop there */
            mpiio_rec_ref = darshan_delete_record_ref(&dxt_mpiio_runtime->rec_id_hash,
                &psx_file->base_rec.id, sizeof(darshan_record_id));
            if(mpiio_rec_ref)
            {
                free(mpiio_rec_ref->write_traces);
                free(mpiio_rec_ref->read_traces);
                free(mpiio_rec_ref->file_rec);
                free(mpiio_rec_ref);
            }
        }

        if(dxt_posix_runtime && dxt_posix_runtime->rec_id_hash)
        {
            /* then delete the POSIX trace records */
            psx_rec_ref = darshan_delete_record_ref(&dxt_posix_runtime->rec_id_hash,
                &psx_file->base_rec.id, sizeof(darshan_record_id));
            if(psx_rec_ref)
            {
                free(psx_rec_ref->write_traces);
                free(psx_rec_ref->read_traces);
                free(psx_rec_ref->file_rec);
                free(psx_rec_ref);
            }
        }
    }

    return;
}

void dxt_posix_filter_dynamic_traces(
    struct darshan_posix_file *(*rec_id_to_psx_file)(darshan_record_id))
{
    DXT_LOCK();

    if(!dxt_posix_runtime || !dxt_use_dynamic_triggers || dxt_trace_all)
    {
        DXT_UNLOCK();
        return;
    }

    darshan_iter_record_refs(dxt_posix_runtime->rec_id_hash,
        dxt_posix_filter_dynamic_traces_iterator, rec_id_to_psx_file);

    DXT_UNLOCK();

    return;
}

/***********************************
 *  internal DXT helper routines   *
 ***********************************/

static int dxt_should_trace_rank(int my_rank)
{
    int i;
    char rank_str[16] = {0};

    sprintf(rank_str, "%d", my_rank);

    if(!dxt_use_rank_triggers)
        return(0);

    for(i = 0; i < num_dxt_triggers; i++)
    {
        if((dxt_triggers[i].type == DXT_RANK_TRIGGER) &&
           (regexec(&dxt_triggers[i].u.rank.regex, rank_str, 0, NULL, 0) == 0))
            return(1);
    }

    return(0);
}

static int dxt_should_trace_file(darshan_record_id rec_id)
{
    char *rec_name;
    int i;

    if(!dxt_use_file_triggers)
        return(0);

    rec_name  = darshan_core_lookup_record_name(rec_id);
    if(rec_name)
    {
        /* compare file name against cached triggers to see if we should trace */
        for(i = 0; i < num_dxt_triggers; i++)
        {
            if((dxt_triggers[i].type == DXT_FILE_TRIGGER) &&
               (regexec(&dxt_triggers[i].u.file.regex, rec_name, 0, NULL, 0) == 0))
                return(1);
        }
    }

    return(0);
}

static void check_wr_trace_buf(struct dxt_file_record_ref *rec_ref)
{
    struct dxt_file_record *file_rec = rec_ref->file_rec;

    int write_count = file_rec->write_count;
    int write_available_buf = rec_ref->write_available_buf;

    if (write_count >= write_available_buf) {
        int write_count_inc;
        if(write_available_buf == 0)
            write_count_inc = IO_TRACE_BUF_SIZE;
        else
            write_count_inc = write_available_buf;

        DXT_LOCK();
        if((write_count_inc * sizeof(segment_info)) > dxt_mem_remaining)
            write_count_inc = dxt_mem_remaining / sizeof(segment_info);

        dxt_mem_remaining -= (write_count_inc * sizeof(segment_info));
        DXT_UNLOCK();

        if(write_count_inc > 0)
        {
            write_available_buf += write_count_inc;
            rec_ref->write_traces =
                (segment_info *)realloc(rec_ref->write_traces,
                        write_available_buf * sizeof(segment_info));

            rec_ref->write_available_buf = write_available_buf;
        }
    }
}

static void check_rd_trace_buf(struct dxt_file_record_ref *rec_ref)
{
    struct dxt_file_record *file_rec = rec_ref->file_rec;

    int read_count = file_rec->read_count;
    int read_available_buf = rec_ref->read_available_buf;

    if (read_count >= read_available_buf) {
        int read_count_inc;
        if(read_available_buf == 0)
            read_count_inc = IO_TRACE_BUF_SIZE;
        else
            read_count_inc = read_available_buf;

        DXT_LOCK();
        if((read_count_inc * sizeof(segment_info)) > dxt_mem_remaining)
            read_count_inc = dxt_mem_remaining / sizeof(segment_info);

        dxt_mem_remaining -= (read_count_inc * sizeof(segment_info));
        DXT_UNLOCK();

        if(read_count_inc > 0)
        {
            read_available_buf += read_count_inc;
            rec_ref->read_traces =
                (segment_info *)realloc(rec_ref->read_traces,
                        read_available_buf * sizeof(segment_info));
            
            rec_ref->read_available_buf = read_available_buf;
        }
    }
}

static struct dxt_file_record_ref *dxt_posix_track_new_file_record(
    darshan_record_id rec_id)
{
    struct dxt_file_record_ref *rec_ref = NULL;
    struct dxt_file_record *file_rec = NULL;
    int ret;

    /* check if we have enough room for a new DXT record */
    DXT_LOCK();
    if(dxt_mem_remaining < sizeof(struct dxt_file_record))
    {
        SET_DXT_MOD_PARTIAL_FLAG(DXT_POSIX_MOD);
        DXT_UNLOCK();
        return(NULL);
    }

    rec_ref = malloc(sizeof(*rec_ref));
    if(!rec_ref)
    {
        DXT_UNLOCK();
        return(NULL);
    }
    memset(rec_ref, 0, sizeof(*rec_ref));

    file_rec = malloc(sizeof(*file_rec));
    if(!file_rec)
    {
        free(rec_ref);
        DXT_UNLOCK();
        return(NULL);
    }
    memset(file_rec, 0, sizeof(*file_rec));

    /* add a reference to this file record based on record id */
    ret = darshan_add_record_ref(&(dxt_posix_runtime->rec_id_hash), &rec_id,
            sizeof(darshan_record_id), rec_ref);
    if(ret == 0)
    {
        free(file_rec);
        free(rec_ref);
        DXT_UNLOCK();
        return(NULL);
    }

    dxt_mem_remaining -= sizeof(struct dxt_file_record);
    DXT_UNLOCK();

    /* initialize record and record reference fields */
    file_rec->base_rec.id = rec_id;
    file_rec->base_rec.rank = dxt_my_rank;
    gethostname(file_rec->hostname, HOSTNAME_SIZE);

    rec_ref->file_rec = file_rec;
    dxt_posix_runtime->file_rec_count++;

    return(rec_ref);
}

static struct dxt_file_record_ref *dxt_mpiio_track_new_file_record(
    darshan_record_id rec_id)
{
    struct dxt_file_record *file_rec = NULL;
    struct dxt_file_record_ref *rec_ref = NULL;
    int ret;

    /* check if we have enough room for a new DXT record */
    DXT_LOCK();
    if(dxt_mem_remaining < sizeof(struct dxt_file_record))
    {
        SET_DXT_MOD_PARTIAL_FLAG(DXT_MPIIO_MOD);
        DXT_UNLOCK();
        return(NULL);
    }

    rec_ref = malloc(sizeof(*rec_ref));
    if(!rec_ref)

    {
        DXT_UNLOCK();
        return(NULL);
    }
    memset(rec_ref, 0, sizeof(*rec_ref));

    file_rec = malloc(sizeof(*file_rec));
    if(!file_rec)
    {
        free(rec_ref);
        DXT_UNLOCK();
        return(NULL);
    }
    memset(file_rec, 0, sizeof(*file_rec));

    /* add a reference to this file record based on record id */
    ret = darshan_add_record_ref(&(dxt_mpiio_runtime->rec_id_hash), &rec_id,
            sizeof(darshan_record_id), rec_ref);
    if(ret == 0)
    {
        free(file_rec);
        free(rec_ref);
        DXT_UNLOCK();
        return(NULL);
    }

    dxt_mem_remaining -= sizeof(struct dxt_file_record);
    DXT_UNLOCK();

    /* initialize record and record reference fields */
    file_rec->base_rec.id = rec_id;
    file_rec->base_rec.rank = dxt_my_rank;
    gethostname(file_rec->hostname, HOSTNAME_SIZE);

    rec_ref->file_rec = file_rec;
    dxt_mpiio_runtime->file_rec_count++;

    return(rec_ref);
}

static void dxt_free_record_data(void *rec_ref_p, void *user_ptr)
{
    struct dxt_file_record_ref *dxt_rec_ref = (struct dxt_file_record_ref *)rec_ref_p;

    free(dxt_rec_ref->write_traces);
    free(dxt_rec_ref->read_traces);
    free(dxt_rec_ref->file_rec);
}

/********************************************************************************
 *     functions exported by this module for coordinating with darshan-core     *
 ********************************************************************************/

static void dxt_serialize_posix_records(void *rec_ref_p, void *user_ptr)
{
    struct dxt_file_record_ref *rec_ref = (struct dxt_file_record_ref *)rec_ref_p;
    struct dxt_file_record *file_rec;
    int64_t record_size = 0;
    int64_t record_write_count = 0;
    int64_t record_read_count = 0;
    void *tmp_buf_ptr;

    assert(rec_ref);
    file_rec = rec_ref->file_rec;
    assert(file_rec);

    record_write_count = file_rec->write_count;
    record_read_count = file_rec->read_count;
    if (record_write_count == 0 && record_read_count == 0)
        return;

    /*
     * Buffer format:
     * dxt_file_record + write_traces + read_traces
     */
    record_size = sizeof(struct dxt_file_record) +
            (record_write_count + record_read_count) * sizeof(segment_info);

    tmp_buf_ptr = (void *)(dxt_posix_runtime->record_buf +
        dxt_posix_runtime->record_buf_size);

    /*Copy struct dxt_file_record */
    memcpy(tmp_buf_ptr, (void *)file_rec, sizeof(struct dxt_file_record));
    tmp_buf_ptr = (void *)(tmp_buf_ptr + sizeof(struct dxt_file_record));

    /*Copy write record */
    memcpy(tmp_buf_ptr, (void *)(rec_ref->write_traces),
            record_write_count * sizeof(segment_info));
    tmp_buf_ptr = (void *)(tmp_buf_ptr +
                record_write_count * sizeof(segment_info));

    /*Copy read record */
    memcpy(tmp_buf_ptr, (void *)(rec_ref->read_traces),
            record_read_count * sizeof(segment_info));
    tmp_buf_ptr = (void *)(tmp_buf_ptr +
                record_read_count * sizeof(segment_info));

    dxt_posix_runtime->record_buf_size += record_size;

#if 0
    int i;
    int64_t rank;
    char *hostname;
    int64_t offset;
    int64_t length;
    double start_time;
    double end_time;

    rank = file_rec->base_rec.rank;
    hostname = file_rec->hostname;

    printf("X_POSIX, record_id: %" PRIu64 "\n", rec_ref->file_rec->base_rec.id);
    printf("X_POSIX, write_count is: %d read_count is: %d\n",
                file_rec->write_count, file_rec->read_count);
    printf("X_POSIX, rank: %d hostname: %s\n", rank, hostname);

    for (i = 0; i < file_rec->write_count; i++) {
        offset = rec_ref->write_traces[i].offset;
        length = rec_ref->write_traces[i].length;
        start_time = rec_ref->write_traces[i].start_time;
        end_time = rec_ref->write_traces[i].end_time;

        printf("X_POSIX, rank %d writes segment %lld [offset: %lld length: %lld start_time: %fs end_time: %fs]\n", rank, i, offset, length, start_time, end_time);
    }

    for (i = 0; i < file_rec->read_count; i++) {
        offset = rec_ref->read_traces[i].offset;
        length = rec_ref->read_traces[i].length;
        start_time = rec_ref->read_traces[i].start_time;
        end_time = rec_ref->read_traces[i].end_time;

        printf("X_POSIX, rank %d reads segment %lld [offset: %lld length: %lld start_time: %fs end_time: %fs]\n", rank, i, offset, length, start_time, end_time);
    }
#endif
}

static void dxt_posix_output(
    void **dxt_posix_buf,
    int *dxt_posix_buf_sz)
{
    assert(dxt_posix_runtime);

    *dxt_posix_buf_sz = 0;

    dxt_posix_runtime->record_buf = malloc(dxt_total_mem);
    if(!(dxt_posix_runtime->record_buf))
        return;
    memset(dxt_posix_runtime->record_buf, 0, dxt_total_mem);
    dxt_posix_runtime->record_buf_size = 0;

    /* iterate all dxt posix records and serialize them to the output buffer */
    darshan_iter_record_refs(dxt_posix_runtime->rec_id_hash,
        dxt_serialize_posix_records, NULL);

    /* set output */
    *dxt_posix_buf = dxt_posix_runtime->record_buf;
    *dxt_posix_buf_sz = dxt_posix_runtime->record_buf_size;

    return;
}

static void dxt_posix_cleanup()
{
    assert(dxt_posix_runtime);

    free(dxt_posix_runtime->record_buf);

    /* cleanup internal structures used for instrumenting */
    darshan_iter_record_refs(dxt_posix_runtime->rec_id_hash,
        dxt_free_record_data, NULL);
    darshan_clear_record_refs(&(dxt_posix_runtime->rec_id_hash), 1);

    free(dxt_posix_runtime);
    dxt_posix_runtime = NULL;

    return;
}

static void dxt_serialize_mpiio_records(void *rec_ref_p, void *user_ptr)
{
    struct dxt_file_record_ref *rec_ref = (struct dxt_file_record_ref *)rec_ref_p;
    struct dxt_file_record *file_rec;
    int64_t record_size = 0;
    int64_t record_write_count = 0;
    int64_t record_read_count = 0;
    void *tmp_buf_ptr;

    assert(rec_ref);
    file_rec = rec_ref->file_rec;
    assert(file_rec);

    record_write_count = file_rec->write_count;
    record_read_count = file_rec->read_count;
    if (record_write_count == 0 && record_read_count == 0)
        return;

    /*
     * Buffer format:
     * dxt_file_record + write_traces + read_traces
     */
    record_size = sizeof(struct dxt_file_record) +
            (record_write_count + record_read_count) * sizeof(segment_info);

    tmp_buf_ptr = (void *)(dxt_mpiio_runtime->record_buf +
        dxt_mpiio_runtime->record_buf_size);

    /*Copy struct dxt_file_record */
    memcpy(tmp_buf_ptr, (void *)file_rec, sizeof(struct dxt_file_record));
    tmp_buf_ptr = (void *)(tmp_buf_ptr + sizeof(struct dxt_file_record));

    /*Copy write record */
    memcpy(tmp_buf_ptr, (void *)(rec_ref->write_traces),
            record_write_count * sizeof(segment_info));
    tmp_buf_ptr = (void *)(tmp_buf_ptr +
                record_write_count * sizeof(segment_info));

    /*Copy read record */
    memcpy(tmp_buf_ptr, (void *)(rec_ref->read_traces),
            record_read_count * sizeof(segment_info));
    tmp_buf_ptr = (void *)(tmp_buf_ptr +
                record_read_count * sizeof(segment_info));

    dxt_mpiio_runtime->record_buf_size += record_size;

#if 0
    int i;
    int64_t rank;
    char *hostname;
    int64_t length;
    double start_time;
    double end_time;

    rank = file_rec->base_rec.rank;
    hostname = file_rec->hostname;

    printf("X_MPIIO, record_id: %" PRIu64 "\n", rec_ref->file_rec->base_rec.id);
    printf("X_MPIIO, write_count is: %d read_count is: %d\n",
                file_rec->write_count, file_rec->read_count);
    printf("X_MPIIO, rank: %d hostname: %s\n", rank, hostname);

    for (i = 0; i < file_rec->write_count; i++) {
        length = rec_ref->write_traces[i].length;
        start_time = rec_ref->write_traces[i].start_time;
        end_time = rec_ref->write_traces[i].end_time;

        printf("X_MPIIO, rank %d writes segment %lld [length: %lld start_time: %fs end_time: %fs]\n", rank, i, length, start_time, end_time);
    }

    for (i = 0; i < file_rec->read_count; i++) {
        length = rec_ref->read_traces[i].length;
        start_time = rec_ref->read_traces[i].start_time;
        end_time = rec_ref->read_traces[i].end_time;

        printf("X_MPIIO, rank %d reads segment %lld [length: %lld start_time: %fs end_time: %fs]\n", rank, i, length, start_time, end_time);
    }
#endif
}

static void dxt_mpiio_output(
    void **dxt_mpiio_buf,
    int *dxt_mpiio_buf_sz)
{
    assert(dxt_mpiio_runtime);

    *dxt_mpiio_buf_sz = 0;

    dxt_mpiio_runtime->record_buf = malloc(dxt_total_mem);
    if(!(dxt_mpiio_runtime->record_buf))
        return;
    memset(dxt_mpiio_runtime->record_buf, 0, dxt_total_mem);
    dxt_mpiio_runtime->record_buf_size = 0;

    /* iterate all dxt posix records and serialize them to the output buffer */
    darshan_iter_record_refs(dxt_mpiio_runtime->rec_id_hash,
        dxt_serialize_mpiio_records, NULL);

    /* set output */ 
    *dxt_mpiio_buf = dxt_mpiio_runtime->record_buf;
    *dxt_mpiio_buf_sz = dxt_mpiio_runtime->record_buf_size;

    return;
}

static void dxt_mpiio_cleanup()
{
    assert(dxt_mpiio_runtime);

    free(dxt_mpiio_runtime->record_buf);

    /* cleanup internal structures used for instrumenting */
    darshan_iter_record_refs(dxt_mpiio_runtime->rec_id_hash,
        dxt_free_record_data, NULL);
    darshan_clear_record_refs(&(dxt_mpiio_runtime->rec_id_hash), 1);

    free(dxt_mpiio_runtime);
    dxt_mpiio_runtime = NULL;

    return;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
