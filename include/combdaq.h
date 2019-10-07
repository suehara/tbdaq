#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <linux/sockios.h> 
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <math.h>
#include <expat.h>
#include <errno.h>
#include <dlfcn.h>

#include "pyrame.h"

#include "stats.h"
#include "buffers.h"
#include "consumers.h"
#include "acquisition.h"
#include "commands.h"
#include "data_socket.h"
#include "shmem.h"
#include "cache.h"
#include "scheduler.h"
#include "uncap.h"

#define ETH_P_ALL	0x0003
#define BACKLOG 100
//#define NETMAXCLIENTS 100
#define MAXID 4

struct combdaq_workspace {
  int initialized; //0 before init_cmd, 1 after
  int nb_streams; // from init_cmd : param 0
  //acquisition state
  int raw_socket;
  char * interface; // from init_cmd : param 2
  unsigned char acq_active;
  pthread_mutex_t *acq_active_mutex;
  unsigned char packet_detector;
  unsigned char burst_state; 
  pthread_mutex_t *burst_state_mutex;
  //scheduler
  pthread_mutex_t **sched_clients;
  int sched_nb_clients;
  //data buffers
  struct data_descriptor *init_data;
  struct data_descriptor *init_ctrl;
  struct data_descriptor *begin_ctrl;
  struct data_descriptor *end_ctrl;
  struct buffer_pool *bpool;
  //netserv
  struct netserv_workspace *nw;
  //sockets
  struct netserv_client **sockets_nc;
  struct netserv_client **sockets_ss_nc;
  //files
  char * datadir; // from init_cmd : param 3
  char *stream_suffix; // from init_cmd : param 4
  FILE * transfer_file;
  FILE **bystream_files;
  int autoflush_active;
  int rawsize;
  pthread_mutex_t *file_mutex;
  //command module
  struct netserv_client *cmd_sockets;
  //shared memory
  int active_shmem;
  int *full_semid;
  int *empty_semid;
  int *mutex_semid;
  unsigned char **shdata;
  unsigned char **tmp;
  int *data_size;
  int *tmp_size;
  unsigned char *flushed;
  //uncap library
  char *uncap_lib_name; // from init_cmd : param 1
  void *lib_uncap; 
  void (*init_call)();
  void (*deinit_call)();
  int (*prefilter_call)();
  int (*uncap_call)();
  int (*select_packet_call)();
  struct ports_table *ports;
  int endinit;
  int deinit;
} combdaq_workspace;

//burst detector
#define OUTOFBURST 0
#define INBURST 1
#define BURST_TRESHOLD 2000 //microseconds


struct shared_cmds {
  unsigned char flush_requested;
  int flush_result;
  char *flush_prefix;
  pthread_mutex_t *flush_synchro;
} shared_cmds;

//shmem variables
#define SHARED_BUFFER_SIZE 8192


#define NB_DATA_DD 100000
#define NB_CTRL_DD 4096




