#include <stdarg.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include "xmlrpc_config.h"

#include "bool.h"

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/client_int.h>
#include <xmlrpc-c/client_global.h>

#define DEBUG

/*=========================================================================
   Global Client
   =========================================================================*/

static struct xmlrpc_client * globalClientP;
static bool globalClientExists = false;


void 
xmlrpc_client_event_loop_finish_asynch(void) {

  XMLRPC_ASSERT(globalClientExists);
  xmlrpc_client_event_loop_finish(globalClientP);
}



void 
xmlrpc_client_event_loop_finish_asynch_timeout(
					       unsigned long const milliseconds) {

  XMLRPC_ASSERT(globalClientExists);
  xmlrpc_client_event_loop_finish_timeout(globalClientP, milliseconds);
}


void 
xmlrpc_client_init2(xmlrpc_env *                      const envP,
                    int                               const flags,
                    const char *                      const appname,
                    const char *                      const appversion,
                    const struct xmlrpc_clientparms * const clientparmsP,
                    unsigned int                      const parmSize) {
  /*----------------------------------------------------------------------------
   This function is not thread-safe.
   -----------------------------------------------------------------------------*/
  if (globalClientExists)
    xmlrpc_faultf(
		  envP,
            "Xmlrpc-c global client instance has already been created "
            "(need to call xmlrpc_client_cleanup() before you can "
		  "reinitialize).");
  else {
    /* The following call is not thread-safe */
    xmlrpc_client_setup_global_const(envP);
    if (!envP->fault_occurred) {
      xmlrpc_client_create(envP, flags, appname, appversion,
			   clientparmsP, parmSize, &globalClientP);
      if (!envP->fault_occurred)
	globalClientExists = true;

      if (envP->fault_occurred)
	xmlrpc_client_teardown_global_const();
    }
  }
}



void
xmlrpc_client_init(int          const flags,
                   const char * const appname,
                   const char * const appversion) {
  /*----------------------------------------------------------------------------
   This function is not thread-safe.
   -----------------------------------------------------------------------------*/
  struct xmlrpc_clientparms clientparms;

  /* As our interface does not allow for failure, we just fail silently ! */
    
  xmlrpc_env env;
  xmlrpc_env_init(&env);

  clientparms.transport = NULL;

  /* The following call is not thread-safe */
  xmlrpc_client_init2(&env, flags,
		      appname, appversion,
		      &clientparms, XMLRPC_CPSIZE(transport));

  xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_cleanup() {
  /*----------------------------------------------------------------------------
   This function is not thread-safe
   -----------------------------------------------------------------------------*/
  XMLRPC_ASSERT(globalClientExists);

  xmlrpc_client_destroy(globalClientP);

  globalClientExists = false;

  /* The following call is not thread-safe */
  xmlrpc_client_teardown_global_const();
}



static void
validateGlobalClientExists(xmlrpc_env * const envP) {

  if (!globalClientExists)
    xmlrpc_faultf(envP,
                      "Xmlrpc-c global client instance "
                      "has not been created "
		  "(need to call xmlrpc_client_init2()).");
}



void
xmlrpc_client_transport_call(
			     xmlrpc_env *               const envP,
			     void *                     const reserved ATTR_UNUSED, 
			     /* for client handle */
			     const xmlrpc_server_info * const serverP,
			     xmlrpc_mem_block *         const callXmlP,
			     xmlrpc_mem_block **        const respXmlPP) {

  validateGlobalClientExists(envP);
  if (!envP->fault_occurred)
    xmlrpc_client_transport_call2(envP, globalClientP, serverP,
				  callXmlP, respXmlPP);
}



xmlrpc_value * 
xmlrpc_client_call(xmlrpc_env * const envP,
                   const char * const serverUrl,
                   const char * const methodName,
                   const char * const format,
                   ...) {

  xmlrpc_value * resultP;

  validateGlobalClientExists(envP);

  if (!envP->fault_occurred) {
    va_list args;

    va_start(args, format);
    
    xmlrpc_client_call2f_va(envP, globalClientP, serverUrl,
			    methodName, format, &resultP, args);

    va_end(args);
  }
  return resultP;
}



xmlrpc_value * 
xmlrpc_client_call_server(xmlrpc_env *               const envP,
                          const xmlrpc_server_info * const serverInfoP,
                          const char *               const methodName,
                          const char *               const format, 
                          ...) {

  xmlrpc_value * resultP;

  validateGlobalClientExists(envP);

  if (!envP->fault_occurred) {
    va_list args;

    va_start(args, format);

    xmlrpc_client_call_server2_va(envP, globalClientP, serverInfoP,
				  methodName, format, args, &resultP);
    va_end(args);
  }
  return resultP;
}



xmlrpc_value *
xmlrpc_client_call_server_params(
				 xmlrpc_env *               const envP,
				 const xmlrpc_server_info * const serverInfoP,
				 const char *               const methodName,
				 xmlrpc_value *             const paramArrayP) {

  xmlrpc_value * resultP;

  validateGlobalClientExists(envP);

  if (!envP->fault_occurred)
    xmlrpc_client_call2(envP, globalClientP,
			serverInfoP, methodName, paramArrayP,
			&resultP);

  return resultP;
}



xmlrpc_value * 
xmlrpc_client_call_params(xmlrpc_env *   const envP,
                          const char *   const serverUrl,
                          const char *   const methodName,
                          xmlrpc_value * const paramArrayP) {

  xmlrpc_value * resultP;

  validateGlobalClientExists(envP);

  if (!envP->fault_occurred) {
    xmlrpc_server_info * serverInfoP;

    serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
        
    if (!envP->fault_occurred) {
      xmlrpc_client_call2(envP, globalClientP,
			  serverInfoP, methodName, paramArrayP,
			  &resultP);
            
      xmlrpc_server_info_free(serverInfoP);
    }
  }
  return resultP;
}                            



void 
xmlrpc_client_call_server_asynch_params(
					xmlrpc_server_info * const serverInfoP,
					const char *         const methodName,
					xmlrpc_response_handler    responseHandler,
					void *               const userData,
					xmlrpc_value *       const paramArrayP) {

  xmlrpc_env env;

  xmlrpc_env_init(&env);

  validateGlobalClientExists(&env);

  if (!env.fault_occurred)
    xmlrpc_client_start_rpc(&env, globalClientP,
			    serverInfoP, methodName, paramArrayP,
			    responseHandler, userData);

  if (env.fault_occurred) {
    /* Unfortunately, we have no way to return an error and the
           regular callback for a failed RPC is designed to have the
           parameter array passed to it.  This was probably an oversight
           of the original asynch design, but now we have to be as
           backward compatible as possible, so we do this:
    */
    (*responseHandler)(serverInfoP->serverUrl,
		       methodName, paramArrayP, userData,
		       &env, NULL);
  }
  xmlrpc_env_clean(&env);
}



/*===================================================================================================

Starting from here, below is project 3

=================================================================================================== */

/*=========================================================================                                                                   
  asynch_multiRPC_thread_routine()
  each thread created in multirpc_globalclient_asynch() will run this function
  and in this routine, each thread will call xmlrpc_client_start_rpcf()
  =========================================================================*/
static void * 
asynch_multirpc_thread_routine(void * param){

  asynch_thread_param_t * paramPtr = (asynch_thread_param_t *) param;

  //getting the detail URL
  char * serverUrl = paramPtr->multiServerInfo->serverUrlArray[paramPtr->requestID];

  srand(time(NULL)+(paramPtr->requestID));

  xmlrpc_int32 val1 = (xmlrpc_int32)rand() % 10;
  xmlrpc_int32 val2 = (xmlrpc_int32)paramPtr->requestID;

#ifdef DEBUG
  printf("running thread %d now......\n", paramPtr->requestID+1);
  printf("try to do the sum of %d and %d \n", val1, val2);
#endif

  xmlrpc_client_start_rpcf(paramPtr->envP, 
			   paramPtr->clientP, 
			   serverUrl, 
			   paramPtr->methodName, 
			   *(paramPtr->responseHandler), 
			   NULL, 
			   "(ii)", val1, val2);
  //  die_if_fault_occurred(paramPtr->envP);

  //pthread_exit(NULL);

  return NULL;
}


/*============================================================================
 * MULTIRPC_GLOBALCLIENT_ASYNCH()
 * userData is NULL, requestsRequired is the number of servers, that is also
 * the number of threads need to create
 * in order to let each thread call xmlrpc_client_start_rpcf()
 * we need to set up the asynch_thread_param_t struct for each thread
 * all threads share the same client, which is the globalClientP
 ============================================================================== */

void 
  multirpc_globalclient_asynch(multi_server_info_t *        multiServerInfo, 
    const char *           const methodName, 
    xmlrpc_response_handler responseHandler,
    int                          requestsRequired) {
 
  xmlrpc_env env;
  xmlrpc_env_init(&env);

  // first validate the global clientP
  validateGlobalClientExists(&env);
  if(!env.fault_occurred){
    // everything is all right, then next step
    // then create thread_param_array, which stores the thread param info
    asynch_thread_param_t * threadArray = malloc(sizeof(asynch_thread_param_t) * 
						 requestsRequired);
      
    int counter;

    for (counter = 0; counter < requestsRequired; ++counter){
      // to create multiple threads
      threadArray[counter].envP = &env;
      threadArray[counter].clientP = globalClientP;
      threadArray[counter].methodName = methodName;
      threadArray[counter].responseHandler = responseHandler;
      // using it to get detail server url info from multiServerInfo array
      threadArray[counter].requestID = counter; // it is also the server ID
      threadArray[counter].multiServerInfo = multiServerInfo;

#ifdef DEBUG
      printf("creating thread %d now.....\n", counter+1);
#endif

      pthread_create(&threadArray[counter].tid, NULL, asynch_multirpc_thread_routine, 
		     &threadArray[counter]);
      
    }

#ifdef DEBUG
    printf("finishing creating %d threads, now waiting.......\n", requestsRequired);
#endif

    sleep(5);
       
    int err;
    // wait for threads over, join, without this, error shows up
    for (counter = 0; counter < requestsRequired; ++counter){

#ifdef DEBUG
      printf("thread %ld is running pthread_join()\n", threadArray[counter].tid);
#endif

      err = pthread_join(threadArray[counter].tid, NULL);
      if (!err){
	printf("thread %ld terminates successfully\n", threadArray[counter].tid);
      }
    }

    // make sure all the asynch rpc calls finish, which means all asynch rpc calls 
    // should call response_handler
    xmlrpc_client_event_loop_finish_asynch();
  }
  else{

    printf("Something Wrong In Global Client\n");
  }

  xmlrpc_env_clean(&env);
}




void 
xmlrpc_client_call_asynch(const char * const serverUrl,
                          const char * const methodName,
                          xmlrpc_response_handler responseHandler,
                          void *       const userData,
                          const char * const format,
                          ...) {

  xmlrpc_env env;

  xmlrpc_env_init(&env);

  validateGlobalClientExists(&env);

  if (!env.fault_occurred) {
    va_list args;

    va_start(args, format);
    
    xmlrpc_client_start_rpcf_va(&env, globalClientP,
				serverUrl, methodName,
				responseHandler, userData,
				format, args);

    va_end(args);
  }
  if (env.fault_occurred)
    (*responseHandler)(serverUrl, methodName, NULL, userData, &env, NULL);

  xmlrpc_env_clean(&env);
}



void
xmlrpc_client_call_asynch_params(const char *   const serverUrl,
                                 const char *   const methodName,
                                 xmlrpc_response_handler responseHandler,
                                 void *         const userData,
                                 xmlrpc_value * const paramArrayP) {
  xmlrpc_env env;
  xmlrpc_server_info * serverInfoP;

  xmlrpc_env_init(&env);

  serverInfoP = xmlrpc_server_info_new(&env, serverUrl);

  if (!env.fault_occurred) {
    xmlrpc_client_call_server_asynch_params(
					    serverInfoP, methodName, responseHandler, userData, paramArrayP);
        
    xmlrpc_server_info_free(serverInfoP);
  }
  if (env.fault_occurred)
    (*responseHandler)(serverUrl, methodName, paramArrayP, userData,
		       &env, NULL);
  xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_call_server_asynch(xmlrpc_server_info * const serverInfoP,
                                 const char *         const methodName,
                                 xmlrpc_response_handler    responseHandler,
                                 void *               const userData,
                                 const char *         const format,
                                 ...) {

  xmlrpc_env env;

  validateGlobalClientExists(&env);

  if (!env.fault_occurred) {
    va_list args;
    
    xmlrpc_env_init(&env);

    va_start(args, format);

    xmlrpc_client_start_rpcf_server_va(
				       &env, globalClientP, serverInfoP, methodName,
				       responseHandler, userData, format, args);

    va_end(args);
  }
  if (env.fault_occurred)
    (*responseHandler)(serverInfoP->serverUrl, methodName, NULL,
		       userData, &env, NULL);

  xmlrpc_env_clean(&env);
}

