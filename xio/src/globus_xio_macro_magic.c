#include "globus_i_xio.h"

/************************************************************************
 *                              open 
 *                              ----
 ***********************************************************************/


void
globus_xio_driver_pass_open_DEBUG(
    globus_result_t * _out_res,
    globus_xio_context_t * _out_context,
    globus_xio_operation_t  _in_op,
    globus_xio_driver_callback_t _in_cb,
    void * _in_user_arg) 
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_handle_t *                         _handle;                
    globus_i_xio_context_t *                        _context;               
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_op_entry_t *                       _my_op;                 
    int                                             _caller_ndx;            
    globus_result_t                                 _res;                   
    globus_xio_driver_t                             _driver;                
    GlobusXIOName(GlobusXIODriverPassOpen);                                 
                                                                            
    _op = (_in_op);                                                         
    globus_assert(_op->ndx < _op->stack_size);                              
    _handle = _op->_op_handle;                                              
    _context = _handle->context;                                            
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
                                                                            
    if(_op->canceled)                                                       
    {                                                                       
        _res = GlobusXIOErrorCanceled();                                    
    }                                                                       
    else                                                                    
    {                                                                       
        _my_context = &_context->entry[_op->ndx];                           
        _my_context->state = GLOBUS_XIO_HANDLE_STATE_OPENING;               
        _caller_ndx = _op->ndx;                                             
                                                                            
        do                                                                  
        {                                                                   
            _my_op = &_op->entry[_op->ndx];                                 
            _driver = _context->entry[_op->ndx].driver;                     
            _op->ndx++;                                                     
        }                                                                   
        while(_driver->transport_open_func == NULL &&                       
              _driver->transform_open_func == NULL);                        
                                                                            
                                                                            
        _my_op->cb = (_in_cb);                                              
        _my_op->user_arg = (_in_user_arg);                                  
        _my_op->in_register = GLOBUS_TRUE;                                  
        _my_op->caller_ndx = _caller_ndx;                                   
        /* at time that stack is built this will be varified */             
        globus_assert(_op->ndx <= _context->stack_size);                    
        if(_op->ndx == _op->stack_size)                                     
        {                                                                   
            _res = _driver->transport_open_func(                            
                        _my_op->target,                                     
                        _my_op->attr,                                       
                        _my_context,                                        
                        _op);                                               
        }                                                                   
        else                                                                
        {                                                                   
            _res = _driver->transform_open_func(                            
                        _my_op->target,                                     
                        _my_op->attr,                                       
                        _op);                                               
        }                                                                   
        _my_op->in_register = GLOBUS_FALSE;                                 
        GlobusXIODebugSetOut(_out_context, _my_context);                    
        GlobusXIODebugSetOut(_out_res, _res);                               
    }                                                                       
}                                                                           


void
globus_xio_driver_finished_open_DEBUG(
    globus_xio_context_t                            _in_context,
    void *                                          _in_dh,
    globus_xio_operation_t                          _in_op,
    globus_result_t                                 _in_res)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_t *                        _context;               
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_result_t                                 _res;                   
    int                                             _ctr;                   
                                                                            
    _res = (_in_res);                                                       
    _op = (globus_i_xio_op_t *)(_in_op);                                    
    globus_assert(_op->ndx >= 0);                                           
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
                                                                            
    /*                                                                      
     * this means that we are finishing with a different context            
     * copy the finishing one into the operations;                          
     */                                                                     
    if(_op->_op_context != _in_context->whos_my_daddy &&                    
            _in_context != NULL)                                            
    {                                                                       
        globus_assert(0); /* for now we are dumping */                      
        /* iterate through them all and copy handles into new slot */       
        for(_ctr = _op->ndx + 1; _ctr < _op->stack_size; _ctr++)            
        {                                                                   
            _op->_op_context->entry[_ctr].driver_handle =                   
                _in_context->whos_my_daddy->entry[_ctr].driver_handle;      
        }                                                                   
    }                                                                       
                                                                            
    _context = _op->_op_context;                                            
    _my_op = &_op->entry[_op->ndx - 1];                                     
    _my_context = &_context->entry[_my_op->caller_ndx];                     
    _my_context->driver_handle = (_in_dh);                                  
    /* no operation can happen while in OPENING state so no need to lock */ 
    if(_res != GLOBUS_SUCCESS)                                              
    {                                                                       
        _my_context->state = GLOBUS_XIO_HANDLE_STATE_CLOSED;                
    }                                                                       
    else                                                                    
    {                                                                       
        _my_context->state = GLOBUS_XIO_HANDLE_STATE_OPEN;                  
        globus_mutex_lock(&_context->mutex);                                
        {                                                                   
            _context->ref++;                                                
        }                                                                   
        globus_mutex_unlock(&_context->mutex);                              
    }                                                                       
                                                                            
    /* if still in register call stack or at top level and a user           
       requested a callback space */                                        
    if(_my_op->in_register ||                                               
        _my_context->space != GLOBUS_CALLBACK_GLOBAL_SPACE)                 
    {                                                                       
        _op->cached_res = _res;                                             
        globus_callback_space_register_oneshot(                             
            NULL,                                                           
            NULL,                                                           
            globus_l_xio_driver_op_kickout,                                 
            (void *)_op,                                                    
            _my_context->space);                                            
    }                                                                       
    else                                                                    
    {                                                                       
        _op->ndx = _my_op->caller_ndx;                                      
        _my_op->cb(_op, _res,                                               
            _my_op->user_arg);                                              
    }                                                                       
}                                                                           

/************************************************************************
 *                          close
 *                          -----
 ***********************************************************************/

void
globus_xio_driver_pass_close_DEBUG(
    globus_result_t *                               _out_res,
    globus_xio_operation_t                          _in_op,
    globus_xio_driver_callback_t                    _in_cb,
    void *                                          _in_ua) 
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_handle_t *                         _handle;                
    globus_i_xio_context_t *                        _context;               
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_bool_t                                   _pass;                  
    globus_i_xio_op_entry_t *                       _my_op;                 
    int                                             _caller_ndx;            
    globus_result_t                                 _res = GLOBUS_SUCCESS;  
    globus_xio_driver_t                             _driver;                
    GlobusXIOName(GlobusXIODriverPassClose);                                
                                                                            
    _op = (_in_op);                                                         
    globus_assert(_op->ndx < _op->stack_size);                              
    _handle = _op->_op_handle;                                              
    _context = _handle->context;                                            
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
                                                                            
    if(_op->canceled)                                                       
    {                                                                       
        _res = GlobusXIOErrorCanceled();                                    
    }                                                                       
    else                                                                    
    {                                                                       
        _caller_ndx = _op->ndx;                                             
        _my_context = &_context->entry[_op->ndx];                           
                                                                            
        do                                                                  
        {                                                                   
            _my_op = &_op->entry[_op->ndx];                                 
            _driver = _context->entry[_op->ndx].driver;                     
            _op->ndx++;                                                     
        }                                                                   
        while(_driver->close_func == NULL);                                 
                                                                            
                                                                            
        /* deal with context state */                                       
        globus_mutex_lock(&_my_context->mutex);                             
        {                                                                   
            switch(_my_context->state)                                      
            {                                                               
                case GLOBUS_XIO_HANDLE_STATE_OPEN:                          
                    _my_context->state = GLOBUS_XIO_HANDLE_STATE_CLOSING;   
                    break;                                                  
                                                                            
                case GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED:                  
                    _my_context->state =                                    
                        GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED_AND_CLOSING;   
                    break;                                                  
                                                                            
                case GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED:                 
                    _my_context->state =                                    
                        GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING;  
                    break;                                                  
                                                                            
                default:                                                    
                    globus_assert(0);                                       
            }                                                               
            /* a barrier will never happen if the level above already did th
                close barrier and this level has not created any driver ops.
                in this case outstanding_operations is garentueed to be zero
            */                                                              
            if(_my_context->outstanding_operations == 0)                    
            {                                                               
                _pass = GLOBUS_TRUE;                                        
            }                                                               
            /* cache the op for close barrier */                            
            else                                                            
            {                                                               
                _pass = GLOBUS_FALSE;                                       
                _my_context->close_op = _op;                                
            }                                                               
        }                                                                   
        globus_mutex_unlock(&_my_context->mutex);                           
                                                                            
        _my_op->cb = (_in_cb);                                              
        _my_op->user_arg = (_in_ua);                                        
        _my_op->caller_ndx = _caller_ndx;                                   
        /* op can be checked outside of lock */                             
        if(_pass)                                                           
        {                                                                   
            _res = globus_i_xio_driver_start_close(_op, GLOBUS_TRUE);       
        }                                                                   
    }                                                                       
    if(_res != GLOBUS_SUCCESS)                                              
    {                                                                       
        _my_context->state = GLOBUS_XIO_HANDLE_STATE_CLOSED;                
    }                                                                       
    GlobusXIODebugSetOut(_out_res, _res);                                   
}                                                                           

void
globus_xio_driver_finished_close_DEBUG(
    globus_xio_operation_t                          op,
    globus_result_t                              _in_res)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_t *                        _context;               
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_result_t                                 _res;                   
                                                                            
    _res = (_in_res);                                                       
    _op = (globus_i_xio_op_t *)(op);                                        
    globus_assert(_op->ndx > 0);                                            
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
                                                                            
    _context = _op->_op_context;                                            
    _my_op = &_op->entry[_op->ndx - 1];                                     
    _my_context = &_context->entry[_my_op->caller_ndx];                     
                                                                            
    /* don't need to lock because barrier makes contntion not possible */   
    _my_context->state = GLOBUS_XIO_HANDLE_STATE_CLOSED;                    
                                                                            
    globus_assert(_op->ndx >= 0); /* otherwise we are not in bad memory */  
    /* space is only not global by user request in the top level of the     
     * of operations */                                                     
    _op->cached_res = (_in_res);                                            
    if(_my_op->in_register ||                                               
            _my_context->space != GLOBUS_CALLBACK_GLOBAL_SPACE)             
    {                                                                       
        globus_callback_space_register_oneshot(                             
            NULL,                                                           
            NULL,                                                           
            globus_l_xio_driver_op_kickout,                                 
            (void *)_op,                                                    
            _my_context->space);                                            
    }                                                                       
    else                                                                    
    {                                                                       
        _op->ndx = _my_op->caller_ndx;                                      
        _my_op->cb(_op, _op->cached_res, _my_op->user_arg);                 
    }                                                                       
}                                                                           


/************************************************************************
 *                              write
 *                              -----
 ***********************************************************************/

void
globus_xio_driver_pass_write_DEBUG(
    globus_result_t *                               _out_res,
    globus_xio_operation_t                          _in_op,
    globus_xio_iovec_t *                            _in_iovec,
    int                                             _in_iovec_count,
    globus_size_t                                   _in_wait_for,
    globus_xio_driver_data_callback_t               _in_cb,
    void *                                          _in_user_arg)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_entry_t *                  _next_context;          
    globus_i_xio_context_t *                        _context;               
    globus_bool_t                                   _close = GLOBUS_FALSE;  
    int                                             _caller_ndx;            
    globus_result_t                                 _res = GLOBUS_SUCCESS;  
    globus_xio_driver_t                             _driver;                
    GlobusXIOName(GlobusXIODriverPassWrite);                                
                                                                            
    _op = (_in_op);                                                         
    _context = _op->_op_context;                                            
    _my_context = &_context->entry[_op->ndx];                               
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
    _caller_ndx = _op->ndx;                                                 
                                                                            
    globus_assert(_op->ndx < _op->stack_size);                              
                                                                            
    globus_mutex_lock(&_my_context->mutex);                                 
                                                                            
    /* error checking */                                                    
    if(_my_context->state != GLOBUS_XIO_HANDLE_STATE_OPEN &&                
        _my_context->state != GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED &&       
        _my_context->state != GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED)        
    {                                                                       
        _res = GlobusXIOErrorInvalidState(_my_context->state);              
    }                                                                       
    else if(_op->canceled)                                                  
    {                                                                       
        _res = GlobusXIOErrorCanceled();                                    
    }                                                                       
    else                                                                    
    {                                                                       
        /* set up the entry */                                              
        _caller_ndx = _op->ndx;                                             
        do                                                                  
        {                                                                   
            _my_op = &_op->entry[_op->ndx];                                 
            _next_context = &_context->entry[_op->ndx];                     
            _driver = _next_context->driver;                                
            _op->ndx++;                                                     
        }                                                                   
        while(_driver->write_func == NULL);                                 
                                                                            
        _my_op->caller_ndx = _caller_ndx;                                   
        _my_op->_op_ent_data_cb = (_in_cb);                                 
        _my_op->user_arg = (_in_user_arg);                                  
        _my_op->_op_ent_iovec = (_in_iovec);                                
        _my_op->_op_ent_iovec_count = (_in_iovec_count);                    
        _my_op->_op_ent_nbytes = 0;                                         
        _my_op->_op_ent_wait_for = (_in_wait_for);                          
        /* set the callstack flag */                                        
        _my_op->in_register = GLOBUS_TRUE;                                  
                                                                            
        _my_context->outstanding_operations++;                              
                                                                            
        /* UNLOCK */                                                        
        globus_mutex_unlock(&_my_context->mutex);                           
        _res = _driver->write_func(                                         
                        _next_context->driver_handle,                       
                        _my_op->_op_ent_iovec,                              
                        _my_op->_op_ent_iovec_count,                        
                        _op);                                               
                                                                            
        /* LOCK */                                                          
        globus_mutex_lock(&_my_context->mutex);                             
        /* flip the callstack flag */                                       
        _my_op->in_register = GLOBUS_FALSE;                                 
        if(_res != GLOBUS_SUCCESS)                                          
        {                                                                   
            _my_context->outstanding_operations--;                          
            /* there is an off chance that we could need to close here */   
           if((_my_context->state == GLOBUS_XIO_HANDLE_STATE_CLOSING ||     
                _my_context->state ==                                       
                    GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING) &&   
                _my_context->outstanding_operations == 0)                   
            {                                                               
                globus_assert(_my_context->close_op != NULL);               
                _close = GLOBUS_TRUE;                                       
            }                                                               
        }                                                                   
    }                                                                       
    globus_mutex_unlock(&_my_context->mutex);                               
                                                                            
    if(_close)                                                              
    {                                                                       
        globus_i_xio_driver_start_close(_my_context->close_op,              
                GLOBUS_FALSE);                                              
    }                                                                       
    GlobusXIODebugSetOut(_out_res, _res);                                   
}                                                                           


void
globus_xio_driver_finished_write_DEBUG(
    globus_xio_operation_t                          op,
    globus_result_t                                 result,
    globus_size_t                                   nbytes)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_result_t                                 _res;                   
    globus_bool_t                                   _fire_cb = GLOBUS_TRUE; 
    globus_xio_iovec_t *                            _tmp_iovec;             
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_entry_t *                  _next_context;          
    globus_i_xio_context_t *                        _context;               
    int                                             _iovec_count;           
                                                                            
    _op = (globus_i_xio_op_t *)(op);                                        
    _res = (result);                                                        
    _op->progress = GLOBUS_TRUE;                                            
     _op->block_timeout = GLOBUS_FALSE;                                     
                                                                            
    _context = _op->_op_context;                                            
    _my_op = &_op->entry[_op->ndx - 1];                                     
    _my_context = &_context->entry[_my_op->caller_ndx];                     
    _op->cached_res = _res;                                                 
                                                                            
    globus_assert(_my_context->state != GLOBUS_XIO_HANDLE_STATE_OPENING &&  
        _my_context->state != GLOBUS_XIO_HANDLE_STATE_CLOSED);              
                                                                            
    _my_op->_op_ent_nbytes += nbytes;                                       
    /* if not all bytes were written */                                     
    if(_my_op->_op_ent_nbytes < _my_op->_op_ent_wait_for &&                 
        _res == GLOBUS_SUCCESS)                                             
    {                                                                       
        /* if not enough bytes read set the fire_cb deafult to false */     
        _fire_cb = GLOBUS_FALSE;                                            
        /* allocate tmp iovec to the bigest it could ever be */             
        if(_my_op->_op_ent_fake_iovec == NULL)                              
        {                                                                   
            _my_op->_op_ent_fake_iovec = (globus_xio_iovec_t *)             
                globus_malloc(sizeof(globus_xio_iovec_t) *                  
                    _my_op->_op_ent_iovec_count);                           
        }                                                                   
        _tmp_iovec = _my_op->_op_ent_fake_iovec;                            
                                                                            
        GlobusIXIOUtilTransferAdjustedIovec(                                
            _tmp_iovec, _iovec_count,                                       
            _my_op->_op_ent_iovec, _my_op->_op_ent_iovec_count,             
            _my_op->_op_ent_nbytes);                                        
                                                                            
        _next_context = &_context->entry[_op->ndx - 1];                     
        /* repass the operation down */                                     
        _res = _next_context->driver->write_func(                           
                _next_context->driver_handle,                               
                _tmp_iovec,                                                 
                _iovec_count,                                               
                _op);                                                       
        if(_res != GLOBUS_SUCCESS)                                          
        {                                                                   
            _fire_cb = GLOBUS_TRUE;                                         
        }                                                                   
    }                                                                       
    if(_fire_cb)                                                            
    {                                                                       
        if(_my_op->_op_ent_fake_iovec != NULL)                              
        {                                                                   
            globus_free(_my_op->_op_ent_fake_iovec);                        
        }                                                                   
        if(_my_op->in_register)                                             
        {                                                                   
            globus_callback_space_register_oneshot(                         
                NULL,                                                       
                NULL,                                                       
                globus_l_xio_driver_op_write_kickout,                       
                (void *)_op,                                                
                GLOBUS_CALLBACK_GLOBAL_SPACE);                              
        }                                                                   
        else                                                                
        {                                                                   
            GlobusIXIODriverWriteDeliver(_op);                              
        }                                                                   
    }                                                                       
}                                                                           

void
globus_xio_driver_write_deliver_DEBUG(
    globus_xio_operation_t                          op)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_t *                        _context;               
    globus_bool_t                                   _close = GLOBUS_FALSE;  
                                                                            
    _op = (op);                                                             
    _context = _op->_op_context;                                            
    _my_op = &_op->entry[_op->ndx - 1];                                     
    _my_context = &_context->entry[_my_op->caller_ndx];                     
                                                                            
    _op->ndx = _my_op->caller_ndx;                                          
    _my_op->_op_ent_data_cb(_op, _op->cached_res, _my_op->_op_ent_nbytes,   
                _my_op->user_arg);                                          
                                                                            
    /* LOCK */                                                              
    globus_mutex_lock(&_my_context->mutex);                                 
    {                                                                       
        _my_context->outstanding_operations--;                              
                                                                            
        /* if we have a close delayed */                                    
        if((_my_context->state == GLOBUS_XIO_HANDLE_STATE_CLOSING ||        
            _my_context->state ==                                           
                GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING) &&       
            _my_context->outstanding_operations == 0)                       
        {                                                                   
            globus_assert(_my_context->close_op != NULL);                   
            _close = GLOBUS_TRUE;                                           
        }                                                                   
    }                                                                       
    globus_mutex_unlock(&_my_context->mutex);                               
                                                                            
    if(_close)                                                              
    {                                                                       
        globus_i_xio_driver_start_close(_my_context->close_op,              
            GLOBUS_FALSE);                                                  
    }                                                                       
}                                                                           

/************************************************************************
 *                           read
 *                           ----
 ***********************************************************************/

void
globus_xio_driver_pass_read_DEBUG(
    globus_result_t *                               _out_res,
    globus_xio_operation_t                          _in_op,
    globus_xio_iovec_t *                            _in_iovec,
    int                                             _in_iovec_count,
    globus_size_t                                   _in_wait_for,
    globus_xio_driver_data_callback_t               _in_cb,
    void *                                          _in_user_arg)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_i_xio_context_entry_t *                  _next_context;          
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_t *                        _context;               
    int                                             _caller_ndx;            
    globus_result_t                                 _res;                   
    globus_bool_t                                   _close = GLOBUS_FALSE;  
    globus_xio_driver_t                             _driver;                
    GlobusXIOName(GlobusXIODriverPassRead);                                 
                                                                            
    _op = (_in_op);                                                         
    _context = _op->_op_context;                                            
    _my_context = &_context->entry[_op->ndx];                               
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
    _caller_ndx = _op->ndx;                                                 
                                                                            
    globus_assert(_op->ndx < _op->stack_size);                              
                                                                            
    /* LOCK */                                                              
    globus_mutex_lock(&_my_context->mutex);                                 
                                                                            
    /* error checking */                                                    
    if(_my_context->state != GLOBUS_XIO_HANDLE_STATE_OPEN &&                
        _my_context->state != GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED)         
    {                                                                       
        _res = GlobusXIOErrorInvalidState(_my_context->state);              
    }                                                                       
    else if(_op->canceled)                                                  
    {                                                                       
        _res = GlobusXIOErrorCanceled();                                    
    }                                                                       
    else if(_my_context->state == GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED)     
    {                                                                       
        _op->cached_res = GlobusXIOErrorEOF();                              
        globus_list_insert(&_my_context->eof_op_list, _op);                 
        _my_context->outstanding_operations++;                              
    }                                                                       
    else                                                                    
    {                                                                       
        /* find next slot. start on next and find first interseted */       
        do                                                                  
        {                                                                   
            _my_op = &_op->entry[_op->ndx];                                 
            _next_context = &_context->entry[_op->ndx];                     
            _driver = _next_context->driver;                                
            _op->ndx++;                                                     
        }                                                                   
        while(_driver->read_func == NULL);                                  
                                                                            
        _my_op->caller_ndx = _caller_ndx;                                   
        _my_op->_op_ent_data_cb = (_in_cb);                                 
        _my_op->user_arg = (_in_user_arg);                                  
        _my_op->_op_ent_iovec = (_in_iovec);                                
        _my_op->_op_ent_iovec_count = (_in_iovec_count);                    
        _my_op->_op_ent_nbytes = 0;                                         
        _my_op->_op_ent_wait_for = (_in_wait_for);                          
        /* set the callstack flag */                                        
                                                                            
        _my_context->outstanding_operations++;                              
        _my_context->read_operations++;                                     
        _my_op->in_register = GLOBUS_TRUE;                                  
                                                                            
        /* UNLOCK */                                                        
        globus_mutex_unlock(&_my_context->mutex);                           
                                                                            
        _res = _driver->read_func(                                          
                        _next_context->driver_handle,                       
                        _my_op->_op_ent_iovec,                              
                        _my_op->_op_ent_iovec_count,                        
                        _op);                                               
                                                                            
        /* LOCK */                                                          
        globus_mutex_lock(&_my_context->mutex);                             
                                                                            
        /* flip the callstack flag */                                       
        _my_op->in_register = GLOBUS_FALSE;                                 
        if(_res != GLOBUS_SUCCESS)                                          
        {                                                                   
            _my_context->outstanding_operations--;                          
            _my_context->read_operations--;                                 
            if((_my_context->state == GLOBUS_XIO_HANDLE_STATE_CLOSING ||    
                _my_context->state ==                                       
                    GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING) &&   
                _my_context->outstanding_operations == 0)                   
            {                                                               
                globus_assert(_my_context->close_op != NULL);               
                _close = GLOBUS_TRUE;                                       
            }                                                               
        }                                                                   
    }                                                                       
    globus_mutex_unlock(&_my_context->mutex);                               
                                                                            
    if(_close)                                                              
    {                                                                       
        globus_i_xio_driver_start_close(_my_context->close_op,              
                GLOBUS_FALSE);                                              
    }                                                                       
                                                                            
    GlobusXIODebugSetOut(_out_res, _res);                                   
}                                                                           


void
globus_xio_driver_finished_read_DEBUG(
    globus_xio_operation_t                          op,
    globus_result_t                                 result,
    globus_size_t                                   nbytes)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_result_t                                 _res;                   
    globus_bool_t                                   _fire_cb = GLOBUS_TRUE; 
    globus_xio_iovec_t *                            _tmp_iovec;             
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_i_xio_context_entry_t *                  _next_context;          
    globus_i_xio_context_t *                        _context;               
    int                                             _iovec_count;           
                                                                            
    _op = (globus_i_xio_op_t *)(op);                                        
    _res = (result);                                                        
    _op->progress = GLOBUS_TRUE;                                            
    _op->block_timeout = GLOBUS_FALSE;                                      
                                                                            
    _context = _op->_op_context;                                            
    _my_op = &_op->entry[_op->ndx - 1];                                     
    _my_context = &_context->entry[_my_op->caller_ndx];                     
    _op->cached_res = _res;                                                 
                                                                            
    globus_assert(_op->ndx > 0);                                            
    globus_assert(_my_context->state != GLOBUS_XIO_HANDLE_STATE_OPENING &&  
        _my_context->state != GLOBUS_XIO_HANDLE_STATE_CLOSED &&             
        _my_context->state != GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED &&      
        _my_context->state !=                                               
            GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING);             
                                                                            
    _my_op->_op_ent_nbytes += nbytes;                                       
                                                                            
    if(_res != GLOBUS_SUCCESS && globus_xio_error_is_eof(_res))             
    {                                                                       
        globus_mutex_lock(&_my_context->mutex);                             
        {                                                                   
            switch(_my_context->state)                                      
            {                                                               
                case GLOBUS_XIO_HANDLE_STATE_OPEN:                          
                    _my_context->state =                                    
                        GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED;               
                    break;                                                  
                                                                            
                case GLOBUS_XIO_HANDLE_STATE_CLOSING:                       
                    _my_context->state =                                    
                        GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED_AND_CLOSING;   
                    break;                                                  
                                                                            
                case GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED_AND_CLOSING:      
                case GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED:                  
                    break;                                                  
                                                                            
                default:                                                    
                    globus_assert(0);                                       
                    break;                                                  
            }                                                               
            _my_context->read_eof = GLOBUS_TRUE;                            
            _my_context->read_operations--;                                 
            if(_my_context->read_operations > 0)                            
            {                                                               
                globus_list_insert(&_my_context->eof_op_list, _op);         
                _fire_cb = GLOBUS_FALSE;                                    
            }                                                               
        }                                                                   
        globus_mutex_unlock(&_my_context->mutex);                           
    }                                                                       
    /* if not all bytes were read */                                        
    else if(_my_op->_op_ent_nbytes < _my_op->_op_ent_wait_for &&            
        _res == GLOBUS_SUCCESS)                                             
    {                                                                       
        /* if not enough bytes read set the fire_cb deafult to false */     
        _fire_cb = GLOBUS_FALSE;                                            
        /* allocate tmp iovec to the bigest it could ever be */             
        if(_my_op->_op_ent_fake_iovec == NULL)                              
        {                                                                   
            _my_op->_op_ent_fake_iovec = (globus_xio_iovec_t *)             
                globus_malloc(sizeof(globus_xio_iovec_t) *                  
                    _my_op->_op_ent_iovec_count);                           
        }                                                                   
        _tmp_iovec = _my_op->_op_ent_fake_iovec;                            
                                                                            
        GlobusIXIOUtilTransferAdjustedIovec(                                
            _tmp_iovec, _iovec_count,                                       
            _my_op->_op_ent_iovec, _my_op->_op_ent_iovec_count,             
            _my_op->_op_ent_nbytes);                                        
                                                                            
        _next_context = &_context->entry[_op->ndx - 1];                     
        /* repass the operation down */                                     
        _res = _next_context->driver->read_func(                            
                _next_context->driver_handle,                               
                _tmp_iovec,                                                 
                _iovec_count,                                               
                _op);                                                       
        if(_res != GLOBUS_SUCCESS)                                          
        {                                                                   
            _fire_cb = GLOBUS_TRUE;                                         
        }                                                                   
    }                                                                       
                                                                            
    if(_fire_cb)                                                            
    {                                                                       
        if(_my_op->in_register)                                             
        {                                                                   
            _op->cached_res = (_res);                                       
            globus_callback_space_register_oneshot(                         
                NULL,                                                       
                NULL,                                                       
                globus_l_xio_driver_op_read_kickout,                        
                (void *)_op,                                                
                GLOBUS_CALLBACK_GLOBAL_SPACE);                              
        }                                                                   
        else                                                                
        {                                                                   
            GlobusIXIODriverReadDeliver(_op);                               
        }                                                                   
    }                                                                       
}                                                                           


void 
globus_xio_driver_read_deliver_DEBUG(
    globus_xio_operation_t                          op)
{                                                                           
    globus_i_xio_op_t *                             _op;                    
    globus_i_xio_op_entry_t *                       _my_op;                 
    globus_i_xio_context_entry_t *                  _my_context;            
    globus_bool_t                                   _purge;                 
    globus_bool_t                                   _close = GLOBUS_FALSE;  
    globus_i_xio_context_t *                        _context;               
                                                                            
    _op = (op);                                                             
    _context = _op->_op_context;                                            
    _my_op = &_op->entry[_op->ndx - 1];                                     
    _my_context = &_context->entry[_my_op->caller_ndx];                     
                                                                            
    /* call the callback */                                                 
    _op->ndx = _my_op->caller_ndx;                                          
    _my_op->_op_ent_data_cb(_op, _op->cached_res, _my_op->_op_ent_nbytes,   
                _my_op->user_arg);                                          
                                                                            
                                                                            
    /* if a temp iovec struct was used for fullfulling waitfor,             
      we can free it now */                                                 
    if(_my_op->_op_ent_fake_iovec != NULL)                                  
    {                                                                       
        globus_free(_my_op->_op_ent_fake_iovec);                            
    }                                                                       
                                                                            
    globus_mutex_lock(&_my_context->mutex);                                 
    {                                                                       
        _purge = GLOBUS_FALSE;                                              
        if(_my_context->read_eof)                                           
        {                                                                   
            switch(_my_context->state)                                      
            {                                                               
                case GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED:                  
                    _purge = GLOBUS_TRUE;                                   
                    _my_context->state =                                    
                        GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED;              
                    break;                                                  
                                                                            
                case GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED_AND_CLOSING:      
                    _purge = GLOBUS_TRUE;                                   
                    _my_context->state =                                    
                        GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING;  
                    break;                                                  
                                                                            
                case GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING:     
                case GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED:                 
                    break;                                                  
                                                                            
                default:                                                    
                    globus_assert(0);                                       
            }                                                               
                                                                            
            /* if we get an operation with EOF type we definitly must       
               have no outstanding reads */                                 
            globus_assert(_my_context->read_operations == 0);               
        }                                                                   
        else                                                                
        {                                                                   
            _my_context->read_operations--;                                 
            /* if no more read operations are outstanding and we are waiting
             * on EOF, purge eof list */                                    
            if(_my_context->read_operations == 0 &&                         
                (_my_context->state ==                                      
                    GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED ||                 
                 _my_context->state ==                                      
                    GLOBUS_XIO_HANDLE_STATE_EOF_RECEIVED_AND_CLOSING))      
            {                                                               
                _purge = GLOBUS_TRUE;                                       
            }                                                               
        }                                                                   
                                                                            
        _my_context->outstanding_operations--;                              
        if(_purge)                                                          
        {                                                                   
             globus_l_xio_driver_purge_read_eof(_my_context);               
        }                                                                   
                                                                            
        if((_my_context->state == GLOBUS_XIO_HANDLE_STATE_CLOSING ||        
            _my_context->state ==                                           
                GLOBUS_XIO_HANDLE_STATE_EOF_DELIVERED_AND_CLOSING) &&       
           _my_context->outstanding_operations == 0)                        
        {                                                                   
            _close = GLOBUS_TRUE;                                           
        }                                                                   
    }                                                                       
    globus_mutex_unlock(&_my_context->mutex);                               
                                                                            
    if(_close)                                                              
    {                                                                       
        globus_i_xio_driver_start_close(_my_context->close_op,              
                GLOBUS_FALSE);                                              
    }                                                                       
}                                                                           


