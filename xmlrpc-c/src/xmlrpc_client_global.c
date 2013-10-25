// xmlrpc_client_global.c modified

#include <stdarg.h>
#include <stdlib.h>
#include "xmlrpc_config.h"
#include <pthread.h>
#include "bool.h"
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/client_int.h>
#include <xmlrpc-c/client_global.h>
#include <stdio.h>
#include <unistd.h>


static struct xmlrpc_client * globalClientP;
static bool globalClientExists = false;

static int count = 0; 
// for synch multi rpc call, recording how many servers have been contacted

xmlrpc_value * globalresultP;

// call info struct for each thread using for asynch multi rpc call
typedef struct _asynch_request_thread_param
{
  xmlrpc_env * envP;
  xmlrpc_client * clientP;
  
  pthread_t tid;

  int requestID; // indicating this thread is for which server
  int * requestsRequired; // indicating how many servers to be contacted
  int * requestsCompleted; // indicating how many servers having been contacted
 
  pthread_mutex_t * requestMutex;
  // pointer to the multi-rpc server info, which contains the number of server 
  // in all and the detail Url for each server to be contacted
  multirpc_server_info * multiServerInfo;
  
  xmlrpc_response_handler responseHandler;
  const char * methodName;
  void * userData;
  xmlrpc_value * paramArrayP;

}asynch_request_thread_param_t;

/*
 * need to think about carefully
 */
// call info struct for each thread using for synch multi-rpc call
typedef struct _synch_request_thread_param
{
  xmlrpc_env * envP;
  xmlrpc_client * clientP;

  pthread_t tid;
  // pthread_attr_t attr;
  
  int semantic; // indicating how many server we need to contact in one multi rpc
  int requestID; // indicating this thread is for which server                 

  xmlrpc_server_info * serverInfoP;

  pthread_mutex_t * requestMutex;

  xmlrpc_response_handler responseHandler;
  const char * methodName;
  void * userData;
  xmlrpc_value * paramArrayP;
  xmlrpc_value ** resultPP;

  const char * format;
  va_list args;

}synch_request_thread_param_t;

void 
xmlrpc_client_init2(xmlrpc_env *                      const envP,
                    int                               const flags,
                    const char *                      const appname,
                    const char *                      const appversion,
                    const struct xmlrpc_clientparms * const clientparmsP,
                    unsigned int                      const parmSize) 
{
  /*----------------------------------------------------------------------------
   This function is not thread-safe.
   -----------------------------------------------------------------------------*/
  if (globalClientExists)
    {
      xmlrpc_faultf(
		    envP,
		    "Xmlrpc-c global client instance has already been created "
		    "(need to call xmlrpc_client_cleanup() before you can "
		    "reinitialize).");
    }
  else 
    {
      /* The following call is not thread-safe */
      xmlrpc_client_setup_global_const(envP);
      
      if (!envP->fault_occurred) 
	{
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
                   const char * const appversion) 
{
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
xmlrpc_client_cleanup() 
{
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
validateGlobalClientExists(xmlrpc_env * const envP) 
{
  if (!globalClientExists)
    {
      xmlrpc_faultf(envP,
		    "Xmlrpc-c global client instance "
		    "has not been created "
		    "(need to call xmlrpc_client_init2()).");
    }
}



void
xmlrpc_client_transport_call(xmlrpc_env *               const envP,
			     void *            const reserved ATTR_UNUSED, 
			     const xmlrpc_server_info * const serverP,
			     xmlrpc_mem_block *         const callXmlP,
			     xmlrpc_mem_block **        const respXmlPP) 
{  
  validateGlobalClientExists(envP);
  
  if (!envP->fault_occurred)
    {
      xmlrpc_client_transport_call2(envP, globalClientP, serverP,
				    callXmlP, respXmlPP);
    }
}



static void
clientCall_va(xmlrpc_env *                  const envP,
	      const xmlrpc_server_info *    const serverInfoP,
	      const char *                  const methodName,
	      const char *                  const format,
	      va_list                             args,
	      xmlrpc_value **               const resultPP)
{
  validateGlobalClientExists(envP);
  
  if (!envP->fault_occurred)
    {
      xmlrpc_value * paramArrayP;
      const char * suffix;

      // building paramArrayP from the format arguments
      xmlrpc_build_value_va(envP, format, args, &paramArrayP, &suffix);

      if (!envP->fault_occurred)
	{
	  if (*suffix != '\0')
	    {
	      xmlrpc_faultf(envP, "Junk after the argument "
			    "specifier: '%s'.  "
			    "There must be exactly one arument.",
			    suffix);
	    }
	  else
	    {
	      xmlrpc_client_call2(envP, globalClientP, serverInfoP,
				  methodName, paramArrayP, resultPP);
	    }

	  xmlrpc_DECREF(paramArrayP);
	}
    }
}



////need to think about //////////
// the thread_routine for each thread using for synch multi rpc call
static void *
performSynch(void * param)
{
  synch_request_thread_param * paramPtr = (synch_request_thread_param *) param;

  validateGlobalClientExists(paramPtr->envP);

  xmlrpc_int32 sum;

  xmlrpc_value * resultP;

  if (!paramPtr->envP->fault_occurred)
    {
      paramPtr->resultPP = &resultP;
      
      xmlrpc_value * paramArrayP;
      const char * suffix;
      va_list args;

      va_start(args, format);
      xmlrpc_build_value_va(paramPtr->envP, paramPtr->format, paramPtr->args, 
			    &paramArrayP, &suffix);
      va_end(args);

      if (!paramPtr->envP->fault_occurred)
	{
	  if (*suffix != '\0')
	    {
	      xmlrpc_faultf(paramPtr->envP, "Junk after the argument "
			    "specifier: '%s'.  "
			    "There must be exactly one arument.",
			    suffix);
	    }
	  else
	    {
	      xmlrpc_client_call2(paramPtr->envP, globalClientP, 
				  paramPtr->serverInfoP, paramPtr->methodName, 
				  paramArrayP, paramPtr->resultPP);
	    }
	  xmlrpc_DECREF(paramArrayP);
	}
    }

  // successfully send request and receive response from server
  xmlrpc_read_int(paramPtr->envP, resultP, &sum);
  if (paramPtr->envP->fault_occurred) 
    {
      fprintf(stderr, "XML-RPC Fault: %s (%d)\n",
	      paramPtr->envP->fault_string, paramPtr->envP->fault_code);
      exit(1);
    }

  pthread_mutex_lock(paramPtr->requestMutex);

  if(count >= paramPtr->semantic)
    {
      pthread_mutex_unlock(paramPtr->requestMutex);
      pthread_exit(NULL);
    }
  else
    {
      count++;
    }

  pthread_mutex_unlock(paramPtr->requestMutex);

}



xmlrpc_value * 
xmlrpc_client_call(xmlrpc_env * const envP,
                   const char * const serverUrl,
                   const char * const methodName,
                   const char * const format,
                   ...) 
{
  xmlrpc_value * resultP;   
  xmlrpc_server_info * serverInfoP;

  serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
        
  if (!envP->fault_occurred) 
    {
      va_list args;
      
      va_start(args, format);
      clientCall_va(envP, serverInfoP, methodName, format, args, &resultP);
      va_end(args);
     
      xmlrpc_server_info_free(serverInfoP);
    }
  
  return resultP;
}



// my implementation for synch multi-rpc 
void 
xmlrpc_globalclient_multi_synch(int                        semantic, 
				xmlrpc_env *         const envP, 
				char **                    serverUrls, 
				const char *         const methodName, 
				const char *         const format, 
				...)
{
  xmlrpc_value * resultP;
  int counter;
  int i;
  int numberOfServer = semantic;

  xmlrpc_server_info * serverInfoP;

  pthread_mutex_t * requestMutex = malloc(sizeof(pthread_mutex_t));
  if (requestMutex == NULL){
    fprintf(stderr, "could not allocate request mutex");
    exit(-1);
  }
  pthread_mutex_init(requestMutex, NULL);

  synch_request_thread_param_t * threadParamArray = malloc(numberOfServer * 
							 sizeof(synch_request_thread_param));

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  for (i = 0; i < numberOfServer; ++i)
    printf("sending synch request to server %s\n", serverUrls[i]);

  for (counter = 0; counter < numberOfServer; ++counter)
    {
      serverInfoP = xmlrpc_server_info_new(envP, serverUrls[counter]);
      if(!envP->fault_occurred)
	{
	  va_list args;
	  va_start(args, format);
	  
	  // initializing the call info struct for each thread
	  threadParamArray[counter].envP = envP;
	  threadParamArray[counter].requestID = counter;
	  threadParamArray[counter].semantic = semantic;
	  threadParamArray[counter].serverInfoP=serverInfoP;
	  threadParamArray[counter].format=format;
	  threadParamArray[counter].args=args;
	  threadParamArray[counter].requestMutex = requestMutex;
	  threadParamArray[counter].resultPP = &resultP;
	  threadParamArray[counter].methodName = methodName;
	  threadParamArray[counter].attr = attr;
	  
	  // creating thread which will do synch rpc call
	  pthread_create(&(threadParamArray[counter].tid), &attr, performSynch, 
			 (void *) &(threadParamArray[counter]));

	  // since this is synch call, so actually we can't create next thread
	  // until current one finish it synch call, which means receive 
	  // response from corresponding server
	  pthread_join(threadParamArray[counter].tid, NULL);

	  va_end(args);

	  xmlrpc_server_info_free(serverInfoP);

	}    
    }
  
}



xmlrpc_value * 
xmlrpc_client_call_server(xmlrpc_env *               const envP,
                          const xmlrpc_server_info * const serverP,
                          const char *               const methodName,
                          const char *               const format, 
                          ...) 
{
  va_list args;
  xmlrpc_value * resultP;

  va_start(args, format);
  clientCall_va(envP, serverP, methodName, format, args, &resultP);
  va_end(args);

  return resultP;
}



xmlrpc_value *
xmlrpc_client_call_server_params(xmlrpc_env *               const envP,
				 const xmlrpc_server_info * const serverInfoP,
				 const char *               const methodName,
				 xmlrpc_value *             const paramArrayP) 
{
  xmlrpc_value * resultP;

  validateGlobalClientExists(envP);

  if (!envP->fault_occurred)
    {
      // invoking xmlrpc_client_call2()
      xmlrpc_client_call2(envP, globalClientP,
			  serverInfoP, methodName, paramArrayP,
			  &resultP);
    }

  return resultP;
}


xmlrpc_value * 
xmlrpc_client_call_params(xmlrpc_env *   const envP,
                          const char *   const serverUrl,
                          const char *   const methodName,
                          xmlrpc_value * const paramArrayP) 
{
  xmlrpc_value * resultP;

  validateGlobalClientExists(envP);

  if (!envP->fault_occurred) 
    {
      xmlrpc_server_info * serverInfoP;

      serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
        
      if (!envP->fault_occurred) 
	{
	  // invoking xmlrpc_client_call2(), this is synch
	  xmlrpc_client_call2(envP, globalClientP,
			      serverInfoP, methodName, paramArrayP,
			      &resultP);
	}
      
      xmlrpc_server_info_free(serverInfoP);
    }

  return resultP;
}

                            
//above is synchronous call
/////////////////////////////////////////////////////////////////////////////////////////////////
//below is asynchronous call


void
xmlrpc_client_call_server_asynch_params(xmlrpc_server_info *    const serverInfoP,
					const char *            const methodName,
					xmlrpc_response_handler       responseHandler,
					void *                  const userData,
					xmlrpc_value *          const paramArrayP)
{
  xmlrpc_env env;
  xmlrpc_env_init(&env);

  validateGlobalClientExists(&env);

  // for regular asynch rpc call, which means sends request to 1 server for one time
  // requestInfos corresponding to requestsRequired
  // requestInfos+1 corresponding to requestsCompleted
  int * requestInfos = malloc(sizeof(int) * 2);
  
  *requestInfos = 1;
  *(requestInfos + 1) = 0;

  pthread_mutex_t * requestMutex = malloc(sizeof(pthread_mutex_t));
  if (requestMutex == NULL){
    fprintf(stderr, "could not allocate request mutex");
    exit(-1);
  }

  pthread_mutex_init(requestMutex, NULL);

  if (!env.fault_occurred)
    {
      // invoking the modified xmlrpc_client_start_rpc(), the real asynch rpc call
      xmlrpc_client_start_rpc(&env, globalClientP, serverInfoP, methodName, 
			      paramArrayP, responseHandler, requestInfos, requestInfos+1, 
			      requestMutex, userData);
    }
  else
    {
      // call responseHandler to handle the error
      (*responseHandler)(serverInfoP->serverUrl, methodName, paramArrayP, 
			 userData, &env, NULL);
    }

  xmlrpc_env_clean(&env);
}




// thread_routine for threads which do asynch call
// each thread will invoke the real asynch rpc call: xmlrpc_client_start_rpc()
static void *
performAsynch(void * param)
{
  asynch_request_thread_param_t * paramPtr = (asynch_request_thread_param_t *)param;
  
  // creating a single serverInfo for xmlrpc_client_start_rpc()
  xmlrpc_server_info * serverInfoP = xmlrpc_server_info_new(
				     paramPtr->envP, 
		        	     paramPtr->multiServerInfo->serverUrlArray[paramPtr->requestID]);

  if (!(paramPtr->envP->fault_occurred))
    {
      // invoking modified xmlrpc_client_start_rpc(), this is asynch call
      xmlrpc_client_start_rpc(paramPtr->envP, 
			      paramPtr->clientP, 
			      serverInfoP, 
			      paramPtr->methodName, 
			      paramPtr->paramArrayP, 
			      paramPtr->responseHandler, 
			      paramPtr->requestsRequired, 
			      paramPtr->requestsCompleted, 
			      paramPtr->requestMutex, 
			      paramPtr->userData);
    }
  else
    {
      // something wrong happened, call responseHandler
      paramPtr->responseHandler(paramPtr->multiServerInfo->serverUrlArray[paramPtr->requestID], 
				paramPtr->methodName, 
				paramPtr->paramArrayP, 
				paramPtr->userData, 
				paramPtr->envP, 
				NULL);
    }
  
  xmlrpc_server_info_free(serverInfoP);

  // end this thread
  pthread_exit(NULL);
				    
}



// asynch multi-rpc implementation
void
multirpc_globalclient_asynch(multirpc_server_info *    const multiServerInfo,
			     const char *              const methodName,
			     xmlrpc_response_handler         responseHandler,
			     void *                    const userData,
			     int *                           requestsRequired,
			     const char *              const format,
			     ...)
{
  xmlrpc_env env;
  xmlrpc_env_init(&env);

  validateGlobalClientExists(&env);
  
  if (!env.fault_occurred)
    {
      xmlrpc_client * clientP = globalClientP;
      
      xmlrpc_env * envP = &env;
      
      int counter;
      // recording how many servers we need to send request
      int * newRequestsRequired = malloc(sizeof(int));
      // recording how many servers we have sent requests
      int * newRequestsCompleted = malloc(sizeof(int));
      if (newRequestsRequired== NULL || newRequestsCompleted == NULL){
	fprintf(stderr, "could not allocate request Data\n");
	exit(-1);
      }
      
      *newRequestsRequired = requestsRequired;
      *newRequestsCompleted = 0;
      
      xmlrpc_value * paramArrayP;

      const char * suffix;
      
      va_list args;
      // creating paramArray from format arguments
      va_start(args, format);
      xmlrpc_build_value_va(envP, format, args, &paramArrayP, &suffix);
      va_end(args);
      
      // this array stores the calling param for each thread
      asynch_request_thread_param_t * threadParamArray = malloc((*requestsRequired) * 
								sizeof(asynch_request_thread_param_t));
      
      pthread_mutex_t * requestMutex = malloc(sizeof(pthread_mutex_t));
      if (requestMutex == NULL){
	fprintf(stderr, "could not allocate request mutex");
	exit(-1);
      }
      pthread_mutex_init(requestMutex, NULL);
  
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      
      // When a thread is created detached (PTHREAD_CREATE_DETACHED), its thread ID 
      //and other resources can be reused as soon as the thread terminates. 
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      
      // to create (*requestsRequired) threads
      for (counter = 0; counter < (*requestsRequired); ++counter)
	{
	  // initializing the calling param for each thread
	  threadParamArray[counter].envP = envP;
	  threadParamArray[counter].clientP = clientP;
	  // indicating this thread is for which server
	  threadParamArray[counter].requestID = counter; 
	  threadParamArray[counter].requestsRequired = newRequestsRequired;
	  threadParamArray[counter].requestsCompleted = newRequestsCompleted;
	  threadParamArray[counter].requestMutex = requestMutex;
	  threadParamArray[counter].multiServerInfo = multiServerInfo;
	  threadParamArray[counter].responseHandler = responseHandler;
	  threadParamArray[counter].methodName = methodName;
	  threadParamArray[counter].userData = userData; // This is NULL
	  threadParamArray[counter].paramArrayP = paramArrayP;
	  
	  // creating thread which will do asynch xmlrpc call
	  pthread_create(&(threadParamArray[counter].tid), &attr, performAsynch, 
			 (void *) &(threadParamArray[counter]));
	}
    }
  else
    {
      printf("fail in multirpc_globalclient_asynch()\n");
    }
  return;
}




void 
xmlrpc_client_call_asynch(const char * const serverUrl,
                          const char * const methodName,
                          xmlrpc_response_handler responseHandler,
                          void *       const userData,
                          const char * const format,
                          ...) 
{
  xmlrpc_env env;

  xmlrpc_env_init(&env);

  validateGlobalClientExists(&env);

  if (!env.fault_occurred) 
    {
      xmlrpc_value * paramArrayP;
      const char * suffix;
      va_list args;
    
      // creating paramArrayP from the format arguments
      va_start(args, format);
      xmlrpc_build_value_va(&env, format, args, &paramArrayP, &suffix);
      va_end(args);
    
      if (!env.fault_occurred) 
	{
	  if (*suffix != '\0')
	    {
	      xmlrpc_faultf(&env, "Junk after the argument "
			    "specifier: '%s'.  "
			    "There must be exactly one arument.",
			    suffix);
	    }
	  else
	    {
	      xmlrpc_client_call_asynch_params(serverUrl, methodName, 
					       responseHandler, userData, paramArrayP);
	    }
	}
    }
  else
    {
      // call responseHandler to handle the error
      (*responseHandler)(serverUrl, methodName, NULL, userData, &env, NULL);
    }

  xmlrpc_env_clean(&env);
}



void
xmlrpc_client_call_asynch_params(const char *   const serverUrl,
                                 const char *   const methodName,
                                 xmlrpc_response_handler responseHandler,
                                 void *         const userData,
                                 xmlrpc_value * const paramArrayP) 
{
  xmlrpc_env env;
  xmlrpc_server_info * serverInfoP;

  xmlrpc_env_init(&env);

  serverInfoP = xmlrpc_server_info_new(&env, serverUrl);

  if (!env.fault_occurred) 
    {
      // invoking xmlrpc_client_call_server_asynch_params()
      xmlrpc_client_call_server_asynch_params(serverInfoP, methodName, 
					      responseHandler, userData, paramArrayP);
        
      xmlrpc_server_info_free(serverInfoP);
    }
  else
    {
      // call responseHandler to handle the error
      (*responseHandler)(serverUrl, methodName, paramArrayP, userData,
		       &env, NULL);
    }

  xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_call_server_asynch(xmlrpc_server_info * const serverInfoP,
                                 const char *         const methodName,
                                 xmlrpc_response_handler    responseHandler,
                                 void *               const userData,
                                 const char *         const format,
                                 ...) 
{
  xmlrpc_env env;
  xmlrpc_value * paramArrayP;
  const char * suffix;

  va_list args;
    
  xmlrpc_env_init(&env);

  // creating paramArrayP from format arguments
  va_start(args, format);
  xmlrpc_build_value_va(&env, format, args, &paramArrayP, &suffix);
  va_end(args);

  if (!env.fault_occurred) 
    {
    if (*suffix != '\0')
      {
	xmlrpc_faultf(&env, "Junk after the argument "
		      "specifier: '%s'.  "
		      "There must be exactly one arument.",
		      suffix);
      }
    else
      {
	// invoking xmlrpc_client_call_server_asynch_params()
	xmlrpc_client_call_server_asynch_params(serverInfoP, methodName, 
						responseHandler, userData,
						paramArrayP);
      }
    // free the paramArrayP
    xmlrpc_DECREF(paramArrayP);
    }
  else
    {
      // something wrong happend, call responseHandler
      (*responseHandler)(serverInfoP->serverUrl, methodName, NULL,
		       userData, &env, NULL);
    }

  xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_event_loop_finish_asynch(void) 
{
  XMLRPC_ASSERT(globalClientExists);
  xmlrpc_client_event_loop_finish(globalClientP);
}



void 
xmlrpc_client_event_loop_finish_asynch_timeout(unsigned long const milliseconds)
{
  XMLRPC_ASSERT(globalClientExists);
  xmlrpc_client_event_loop_finish_timeout(globalClientP, milliseconds);
}

