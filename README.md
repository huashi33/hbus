# hbus
process communication framework


TODO
node
[o] aync
[o] lazy subscrib
[o] aync call handle in recv_cb
[o] don't recv own request
[x] interface 
[x] atexit
[x] log
[x] muti-thread
<!-- Performance optimization -->
[x] request pool
[x] exec pool
[x] hide nng

typedef struct hnode_{
    uint16_t node_id;
    
}hnode_t;
typedef int (*subscrib_handler_t)(const hmsg_t* hmsg, void* p);
int hbus_publish(hnode_t* n,uint16_t msg_id,const void* d,uint32_t s);
int hbus_subsbrib(hnode_t* n,uint16_t msg_id,subscrib_handler_t h,void* p);

typedef struct hrequest_{
    uint16_t server_node_id;
    uint16_t align;
    uint32_t size;
    uint32_t timeout;
    void* req;
}hrequest_t;
typedef struct hresponse_{
    uint32_t size;
    void* rep;
}hresponse_t;
int hbus_request(hnode_t* n,hrequest_t* req,hresponse_t* res);


hbusbroker
[o] sync->async
[o] heart-beats
[x] interface
[x] atexit
[x] log
[x] muti-thread






tips:
每个msg_id的回复msg_id为 (UINT16_MAX-msg_id),所以msg_id 需要小于INT16_MAX