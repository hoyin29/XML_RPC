#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/config.h>
/* information about this build environment */

#define NAME "Multi Xmlrpc-c Asynchronous Test Client"
#define VERSION "1.0"



static void 
handle_sample_add_response(const char *   const serverUrl,
                           const char *   const methodName,
                           xmlrpc_value * const paramArrayP,
                           void *         const user_data,
                           xmlrpc_env *   const faultP,
                           xmlrpc_value * const resultP) 
{
  xmlrpc_env env;
  xmlrpc_int addend, adder;
    
  /* Initialize our error environment variable */
  xmlrpc_env_init(&env);

  /* Our first four arguments provide helpful context.  Let's grab the
       addends from our parameter array. 
  */
  xmlrpc_decompose_value(&env, paramArrayP, "(ii)", &addend, &adder);
  die_if_fault_occurred(&env);

  printf("RPC with method '%s' at URL '%s' to add %d and %d "
	 "has completed\n", methodName, serverUrl, addend, adder);
    
  if (faultP->fault_occurred)
    printf("The RPC failed.  %s\n", faultP->fault_string);
  else {
    xmlrpc_int sum;

    xmlrpc_read_int(&env, resultP, &sum);
    die_if_fault_occurred(&env);

    printf("The sum is  %d\n", sum);
  }
}


// struct for holding the multi server Info
typedef struct multi_server_info
{
  int numberOfSerfer; // number of server the client need to send requests
  char ** serverUrlArray; // array of strings containing the detail URL info

}multi_server_info_t;



int
main(int           const argc, 
     const char ** const argv)
{
  const char* const methodName = "sample.add";

  xmlrpc_env env;
  xmlrpc_env_init(&env);

  //create a global client
  xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
  die_if_fault_occurred(&env);

  // create a multi_server_info struct
  multi_server_info_t multiServerInfo;

  multiServerInfo.numberOfServer = 3; // 3 servers, 8080, 8081, 8082
  multiServerInfo.serverUrlArray = malloc(sizeof(char *) *
					  multiServerInfo.numberOfServer);
  multiServerInfo.serverUrlArray[0] = "http://localhost:8080/RPC2";
  multiServerInfo.serverUrlArray[1] = "http://localhost:8081/RPC2";
  multiServerInfo.serverUrlArray[2] = "http://localhost:8082/RPC2";

  // how many times we need to run our multirpc_globalclient_asynch()
  int * requestsForMultiRpc = malloc(sizeof(int));
  *requestsForMultiRpc = atoi(argv[1]);

  // in one single multi rpc call, how many requests we need to send
  // each server will get one request, so it is equal to number of server
  int * requestsRequired = malloc(sizeof(int));
  *requestsRequired = multiServerInfo.numberOfServer;

  int counter;

  for (counter = 0; counter < (*requestsForMultiRpc); ++counter)
    {
      
      multirpc_globalclient_asynch(&multiServerInfo, methodName, 
				   handle_sample_add_response, NULL, 
				   requestsRequired, "(ii)", 
				   (xmlrpc_int32) 5, (xmlrpc_int32) 7);
      die_if_fault_occurred(&env);
      
      printf("RPCs all requested.  Waiting for & handling responses...\n");
      
      /* Wait for all RPCs to be done.  With some transports, this is also
	 what causes them to go.
      */

      // for asynch rpc calls, we need to wait for all of the responses
      xmlrpc_client_event_loop_finish_asynch();
      
      printf("All RPCs finished.\n");
      
      printf("Just finished %d th. Multi-RPC call to %d servers\n", 
	     (counter+1), (*requestsRequired));
    }

  xmlrpc_client_cleanup();

  return 0;
}
