
	/* $Id$ */
	/* (c) 2009 Jerome Loyet */

#ifndef FPM_STATUS_H
#define FPM_STATUS_H 1
#include "fpm_worker_pool.h"
#include "fpm_shm.h"

#define FPM_STATUS_BUFFER_SIZE 512

struct fpm_status_s {
	int pm;
	int idle;
	int active;
	int total;
	unsigned long int accepted_conn;
	struct timeval last_update;
};

int fpm_status_init_child(struct fpm_worker_pool_s *wp);
void fpm_status_update_activity(struct fpm_shm_s *shm, int idle, int active, int total, int clear_last_update);
void fpm_status_update_accepted_conn(struct fpm_shm_s *shm, unsigned long int accepted_conn);
void fpm_status_increment_accepted_conn(struct fpm_shm_s *shm);
void fpm_status_set_pm(struct fpm_shm_s *shm, int pm);
int fpm_status_get(int *idle, int *active, int *total, int *pm);
int fpm_status_handle_status(char *uri, char **output);
char* fpm_status_handle_ping(char *uri);

extern struct fpm_shm_s *fpm_status_shm;

#endif
