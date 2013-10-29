#include <stdarg.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "xmlrpc_config.h"

#include "bool.h"

#include <xmlrpc-c/base_int.h>
#include <xmlrpc-c/string_int.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/client_int.h>
#include <xmlrpc-c/client_global.h>

/*=========================================================================
   Global Client
=========================================================================*/

static struct xmlrpc_client * globalClientP;
static bool globalClientExists = false;


/*=========================================================================                                                                    
   data structure for thread using for multi-rpc asynch call                                                                                                                               
   =========================================================================*/
typedef struct asynch_thread_param {

  xmlrpc_env * envP;
  xmlrpc_client * clientP;
  multi_server_info_t * multiServerInfo;

  pthread_t tid; // thread ID
  int requestID; // indicating this thread is for which server

  char * methodName;

  xmlrpc_value * paramArrayP;
  xmlrpc_response_handler * completionFn;

}asynch_thread_param_t;

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



static void
computeParamArray_globalclient(xmlrpc_env *    const envP,
			       const char *    const format,
			       va_list               args,
			       xmlrpc_value ** const paramArrayPP) {
  /*----------------------------------------------------------------------------
   'format' and 'args' specify the parameter list of an RPC, in the form
   of an XML-RPC array value, with one element per RPC parameter.

   'format' is an XML-RPC value format string, e.g. "(ii{s:i,s:i})".
   'args' is the list of substitution values for that string
   (6 values in this example, 4 integers and 2 strings).

   We return the XML-RPC value 'format' and 'args' represent, but throw an
   error if they don't validly specify a single array.

   Note that it is a common user error to specify the format string as a
   single or string of argument types, instead of as an array of argument
   types.  E.g. "i" or "ii" instead of "(i)" and "(ii)".  So we try
   especially hard to give an informative message for that case.
   -----------------------------------------------------------------------------*/
  xmlrpc_env env;
  xmlrpc_value * paramArrayP;
  const char * suffix;
  /* Stuff left over in format string after parameter array
           specification.
  */

  xmlrpc_env_init(&env);
  xmlrpc_build_value_va(&env, format, args, &paramArrayP, &suffix);
  if (env.fault_occurred)
    xmlrpc_env_set_fault_formatted(
				   envP, env.fault_code, "Invalid RPC arguments.  "
				   "The format argument must indicate a single array (each element "
				   "of which is one argument to the XML-RPC call), and the "
				   "following arguments must correspond to that format argument.  "
				   "The failure is: %s",
				   env.fault_string);
  else {
    XMLRPC_ASSERT_VALUE_OK(paramArrayP);
        
    if (*suffix != '\0')
      xmlrpc_faultf(envP,
		    "Junk after the parameter array specifier: '%s'.  "
		    "The format string must specify exactly one value: "
		    "an array of RPC parameters",
		    suffix);
    else {
      if (xmlrpc_value_type(paramArrayP) != XMLRPC_TYPE_ARRAY)
	xmlrpc_faultf(
		      envP,
		      "You must specify the parameter list as an "
		      "XML-RPC array value, "
		      "each element of which is a parameter of the RPC.  "
		      "But your format string specifies an XML-RPC %s, not "
		      "an array",
		      xmlrpc_type_name(xmlrpc_value_type(paramArrayP)));
    }
    if (env.fault_occurred)
      xmlrpc_DECREF(paramArrayP);
    else
      *paramArrayPP = paramArrayP;
  }
  xmlrpc_env_clean(&env);
}




/*=========================================================================                                                                   
  asynch_multiRPC_thread_routine()
  each thread created in multirpc_globalclient_asynch() will run this function
   =========================================================================*/
static void * 
asynch_multirpc_thread_routine(void * param){

  asynch_thread_param_t * paramPtr = (asynch_thread_param_t *) param;

  // create serverInfoP, we need to free(serverInfoP)
  xmlrpc_server_info * serverInfoP;
  serverInfoP = xmlrpc_server_info_new(paramPtr->envP, paramPtr->multiServerInfo
				       ->serverUrlArray[paramPtr->requestID]);
  if(!paramPtr->envP->fault_occurred){
    // nothing wrong, then we can call xmlrpc_client_start_rpc()
    xmlrpc_client_start_rpc(paramPtr->envP, 
			    paramPtr->clientP, 
			    serverInfoP, 
			    paramPtr->methodName, 
			    paramPtr->paramArrayP, 
			    paramPtr->completionFn, 
			    NULL);

    xmlrpc_server_info_free(serverInfoP);
  }
  else{
    printf("fail in create serverInfoP in thread %d\n", paramPtr->requestID);
  }

}


/*============================================================================
 * MULTIRPC_GLOBALCLIENT_ASYNCH()
 * userData is NULL, requestsRequired is the number of servers, that is also
 * the number of threads need to create
 * in order to let each thread call xmlrpc_client_start_rpc()
 * we need to set up the asynch_thread_param_t struct for each thread
============================================================================== */
void 
multirpc_globalclient_asynch(multi_server_info_t *        multiServerInfo, 
			     const char *           const methodName, 
			     xmlrpc_response_handler      responseHandler, 
			     void *                 const userData, 
			     int                          requestsRequired, 
			     const char *           const format, 
			     ...) {
  xmlrpc_env env;
  xmlrpc_env_init(&env);

  // first validate the global clientP
  validateGlobalClientExists(&env);
  if(!env.fault_occurred){
    //nothing wrong
    // then create paramArrayP from the format arguments
    xmlrpc_value * paramArrayP; // we need to free paramArrayP at the end
    
    va_list args;

    XMLRPC_ASSERT_PTR_OK(format);

    va_start(args, format);

    computeParamArray_globalclient(&env, format, args, &paramArrayP);
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
	// using it to get detail server url info from multiServerInfo array
	threadArray[counter].requestID = counter;
	threadArray[counter].completionFn = responseHandler;
	threadArray[counter].paramArrayP = paramArrayP;
	threadArray[counter].multiServerInfo = multiServerInfo;

	pthread_create(&threadArray[counter].tid, NULL, Asynch_multirpc_thread_routine, 
		       &threadArray[counter]);
	
      }
    
      //free paramArrayP
      xmlrpc_DECREF(paramArrayP);
    }
    else{
      printf("something wrong in computerParamArray_globalclient()\n"); 
    }
    va_end(args);
  }
  else{

    printf("something wrong in global client\n");
  }

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
