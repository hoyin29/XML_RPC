#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/client_global.h>
#include <xmlrpc-c/config.h>
/* information about this build environment */

#define NAME "Multi Xmlrpc-c Asynchronous Test Client"
#define VERSION "1.0"

#define DEBUG

static void 
die_if_fault_occurred(xmlrpc_env * const envP) {
  if (envP->fault_occurred) {

#ifdef DEBUG
    printf("Something failed. %s (XML-RPC fault code %d)\n",
	   envP->fault_string, envP->fault_code);
#endif

    fprintf(stderr, "Something failed. %s (XML-RPC fault code %d)\n",
	    envP->fault_string, envP->fault_code);
    exit(1);
  }
}



static void 
handle_hello_server_response(const char *   const serverUrl,
                           const char *   const methodName,
                           xmlrpc_value * const paramArrayP,
                           void *         const user_data,
                           xmlrpc_env *   const faultP,
                           xmlrpc_value * const resultP) {

  xmlrpc_env env;
  // xmlrpc_int addend, adder;

  /* Initialize our error environment variable */
  xmlrpc_env_init(&env);

  /* Our first four arguments provide helpful context.  Let's grab the
       addends from our parameter array. 
  */
   
  if (faultP->fault_occurred)
    printf("The RPC failed.  %s\n", faultP->fault_string);
  else {
    //xmlrpc_int sum;
    char * serverACK;
    xmlrpc_int serverID;
    xmlrpc_int server_status;
    
    xmlrpc_decompose_value(&env, resultP, "(sii)", &serverACK, &serverID, &server_status);
    die_if_fault_occurred(&env);

    printf("Just got %s from server %d, its status is  %d\n", serverACK, 
	   serverID, server_status);
  }
}


int
main(int           const argc, 
     const char ** const argv)
{
  const char* const methodName = "hello.server";

  xmlrpc_env env;
  xmlrpc_env_init(&env);

  //create a global client
  xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);

  // create a multi_server_info struct
  multi_server_info_t multiServerInfo;

  multiServerInfo.numberOfServer = 3; // 3 servers, 8080, 8081, 8082
  multiServerInfo.serverUrlArray = malloc(sizeof(char *) *
					  multiServerInfo.numberOfServer);
  multiServerInfo.serverUrlArray[0] = "http://localhost:8080/RPC2";
  multiServerInfo.serverUrlArray[1] = "http://localhost:8081/RPC2";
  multiServerInfo.serverUrlArray[2] = "http://localhost:8082/RPC2";

  // how many times we need to run our multirpc_globalclient_asynch()
  //int * requestsForMultiRpc = malloc(sizeof(int));
  int requestsForMultiRpc = atoi(argv[1]);

  // in one single multi rpc call, how many requests we need to send
  // each server will get one request, so it is equal to number of server
  //int * requestsRequired = malloc(sizeof(int));
  int requestsRequired = multiServerInfo.numberOfServer;

  int counter;

  for (counter = 0; counter < (requestsForMultiRpc); ++counter)
    {
      // requestsRequired shows how many threads need to be created in 
      // the belowing function
      multirpc_globalclient_asynch(&multiServerInfo, methodName, 
				   handle_hello_server_response, 
				   requestsRequired);
      
      printf("Multi RPC has been done %d time. Waiting..\n", counter+1);
      
      // xmlrpc_client_event_loop_finish_asynch();
      
    }

  // sleep(10);

  xmlrpc_env_clean(&env);

  xmlrpc_client_cleanup();

  return 0;
}
