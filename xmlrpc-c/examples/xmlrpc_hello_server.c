/* A simple standalone XML-RPC server program written in C. */

/* This server knows one RPC class (besides the system classes):
   "sample.add".

   The program takes one argument: the HTTP port number on which the server
   is to accept connections, in decimal.

   You can use the example program 'xmlrpc_sample_add_client' to send an RPC
   to this server.

   Example:

   $ ./xmlrpc_sample_add_server 8080&
   $ ./xmlrpc_sample_add_client

   For more fun, run client and server in separate terminals and turn on
   tracing for each:

   $ export XMLRPC_TRACE_XML=1
*/

#define WIN32_LEAN_AND_MEAN  /* required by xmlrpc-c/server_abyss.h */

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#ifdef _WIN32
#  include <windows.h>
#  include <winsock2.h>
#else
#  include <unistd.h>
#endif

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_abyss.h>

#include "config.h"  /* information about this build environment */


#ifdef _WIN32
#define SLEEP(seconds) SleepEx(seconds * 1000, 1);
#else
#define SLEEP(seconds) sleep(seconds);
#endif

#define DEBUG

static xmlrpc_value *
hello_server(xmlrpc_env *   const envP,
	     xmlrpc_value * const paramArrayP,
	     void *         const serverInfo,
	     void *         const channelInfo) {

#ifdef DEBUG
  printf("start running server method.......");
#endif

  // xmlrpc_int32 x, y, z;
  char * x; // "Hello"
  xmlrpc_int32 y; // serverID

  /* Parse our argument array. */
  xmlrpc_decompose_value(envP, paramArrayP, "(si)", &x, &y);
  if (envP->fault_occurred) {

#ifdef DEBUG
    printf("fail in xmlrpc_decompose_value() in server method\n");
#endif
    return NULL;
  }


  // create server status, -1 means Idle, 1 means Busy
  xmlrpc_int32 server_status;
  // use random number to show status, if rand() = 0, then Idle
  // if rand() = 1, then Busy
  srand(y);
  //sleep(1);
  int random = rand() % 2;
  if (random == 0)
    server_status = (xmlrpc_int32)-1;
  else
    server_status = (xmlrpc_int32)1;


#ifdef DEBUG
  printf("server %d 's status is %d\n", y, server_status);
#endif



  /* Return our result. */
  // return xmlrpc_build_value(envP, "i", z);

  // "ACK" is a string, y is serverID, server_status is int32
  // this function builds an array which has three elements
  // the first is string "ACK", the second is serverID, 
  // the third is server_status ( 1 or -1)
  return xmlrpc_build_value(envP, "(sii)", "ACK", y, server_status);

}



int 
main(int           const argc, 
     const char ** const argv) {

  struct xmlrpc_method_info3 const methodInfo = {
    /* .methodName     = */ "hello.server",
    /* .methodFunction = */ &hello_server,
  };
  xmlrpc_server_abyss_parms serverparm;
  xmlrpc_registry * registryP;
  xmlrpc_env env;

  if (argc-1 != 1) {
    fprintf(stderr, "You must specify 1 argument:  The TCP port "
                "number on which the server will accept connections "
                "for RPCs (8080 is a common choice).  "
	    "You specified %d arguments.\n",  argc-1);
    exit(1);
  }
    
  xmlrpc_env_init(&env);

  registryP = xmlrpc_registry_new(&env);
  if (env.fault_occurred) {
    printf("xmlrpc_registry_new() failed.  %s\n", env.fault_string);
    exit(1);
  }

  xmlrpc_registry_add_method3(&env, registryP, &methodInfo);
  if (env.fault_occurred) {
    printf("xmlrpc_registry_add_method3() failed.  %s\n",
	   env.fault_string);
    exit(1);
  }

  serverparm.config_file_name = NULL;   /* Select the modern normal API */
  serverparm.registryP        = registryP;
  serverparm.port_number      = atoi(argv[1]);
  serverparm.log_file_name    = "/tmp/xmlrpc_log";

  printf("Running XML-RPC server...\n");

  xmlrpc_server_abyss(&env, &serverparm, XMLRPC_APSIZE(log_file_name));
  if (env.fault_occurred) {
    printf("xmlrpc_server_abyss() failed.  %s\n", env.fault_string);
    exit(1);
  }
  /* xmlrpc_server_abyss() never returns unless it fails */

  return 0;
}



