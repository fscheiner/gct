/******************************************************************************
globus_i_gass_transfer.h
 
Description:
    This header defines the internal interface of the GASS transfer library
 
CVS Information:
 
    $Source$
    $Date$
    $Revision$
    $Author$
******************************************************************************/
#ifndef GLOBUS_GASS_INCLUDE_GLOBUS_I_GASS_TRANSFER_H
#define GLOBUS_GASS_INCLUDE_GLOBUS_I_GASS_TRANSFER_H

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

#include "globus_gass_transfer.h"
#include "globus_gass_transfer_proto.h"
#include "globus_i_gass_transfer_keyvalue.h"

EXTERN_C_BEGIN

extern globus_hashtable_t globus_i_gass_transfer_protocols;
extern globus_handle_table_t globus_i_gass_transfer_request_handles;
extern globus_handle_table_t globus_i_gass_transfer_listener_handles;
extern globus_list_t * globus_i_gass_transfer_requests;
extern globus_list_t * globus_i_gass_transfer_listeners;
extern globus_bool_t globus_i_gass_transfer_deactivating;

/* Default implemented protocols */
extern globus_module_descriptor_t globus_i_gass_transfer_http_module;
#define GLOBUS_I_GASS_TRANSFER_HTTP_MODULE (&globus_i_gass_transfer_http_module)

extern globus_gass_transfer_proto_descriptor_t
    globus_i_gass_transfer_http_descriptor;
extern globus_gass_transfer_proto_descriptor_t
    globus_i_gass_transfer_https_descriptor;
/*
 * The request status structure. This should only be accessed
 * through the functions globus_gass_transfer_request_get_*()
 */
typedef struct 
{
    char *					url;
    globus_gass_transfer_request_type_t		type;
    globus_gass_transfer_request_status_t	status;

    globus_bool_t				client_side;

    globus_size_t				length;
    globus_size_t				handled_length;
    globus_size_t				posted_length;

    char **					referral_url;
    globus_size_t				referral_count;

    globus_gass_transfer_callback_t		callback;
    void *					callback_arg;

    globus_gass_transfer_callback_t		fail_callback;
    void *					fail_callback_arg;

    /* subject of peer */
    char *					subject;

    /* queue of byte arrays to be sent or received */
    globus_fifo_t				pending_data;

    /* Denial reasons */
    int						denial_reason;
    char *					denial_message;

    globus_object_t *				attr;

    struct globus_gass_transfer_request_proto_s *
    						proto;
    void *					user_pointer;
} globus_gass_transfer_request_struct_t;

/*
 * The listener status structure. This should only be accessed
 * through the functions globus_gass_transfer_listener_get_*()
 */
typedef struct 
{
    char *					base_url;
    globus_gass_transfer_listener_status_t	status;
    struct globus_gass_transfer_listener_proto_s *
    						proto;

    globus_gass_transfer_listen_callback_t	listen_callback;
    void *					listen_callback_arg;

    globus_gass_transfer_close_callback_t	close_callback;
    void *					close_callback_arg;
    void *					user_pointer;
} globus_gass_transfer_listener_struct_t;

/* the pending_data fifo in the request structure is one of these */
typedef struct
{
    globus_bool_t				last_data;
    globus_size_t				length;
    globus_size_t				wait_for_length;
    globus_gass_transfer_request_t		request;

    /*
     * True when this pending block has been passed to the protocol
     * module
     */
    globus_bool_t				pending;

    globus_byte_t *				bytes;
    globus_gass_transfer_bytes_callback_t	callback;
    void *					callback_arg;
} globus_gass_transfer_pending_t;

/* implemented in globus_gass_transfer_request.c */
void
globus_i_gass_transfer_request_init(
    globus_gass_transfer_request_t *		request,
    globus_gass_transfer_requestattr_t *	attr,
    char *					url,
    globus_gass_transfer_request_type_t 	type,
    globus_gass_transfer_callback_t		callback,
    void *					user_arg);

int
globus_i_gass_transfer_request_destroy(
    globus_gass_transfer_request_t		request);

void
globus_gass_transfer_request_set_length(
    globus_gass_transfer_request_t		request,
    globus_size_t				length);

/* implemented in globus_gass_transfer_server.c */
int
globus_i_gass_transfer_listener_destroy(
    globus_gass_transfer_listener_t		listener);

/* implemented in globus_gass_transfer_client.c */
int
globus_i_gass_transfer_client_request(
    globus_gass_transfer_request_t *		request);

/* implemented in globus_gass_transfer_send_recv.c */
typedef void
(* globus_gass_transfer_dispatch_func_t) (
    globus_gass_transfer_request_t		request);

void
globus_i_gass_transfer_send_dispatcher(
    globus_gass_transfer_request_t		request);

void
globus_i_gass_transfer_recv_dispatcher(
    globus_gass_transfer_request_t		request);

extern globus_cond_t globus_i_gass_transfer_shutdown_cond;
extern globus_mutex_t globus_i_gass_transfer_mutex;

#define globus_i_gass_transfer_lock()   \
	globus_mutex_lock(&globus_i_gass_transfer_mutex)
#define globus_i_gass_transfer_unlock()	\
	globus_mutex_unlock(&globus_i_gass_transfer_mutex)

void
globus_i_gass_transfer_deactivate_callback(
    void *					user_arg,
    globus_gass_transfer_request_t		request);

EXTERN_C_END

#endif /* GLOBUS_GASS_INCLUDE_GLOBUS_I_GASS_TRANSFER_H */
