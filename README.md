# hbus
process communication framework


TODO
req-rep
[o]broker sync->async
hide nng

node
[o] aync
[o] lazy subscrib
[x] interface 
[x] atexit
[x] aync call handle in recv_cb
[x] log
hbusbroker
[o] aync
[o] heart-beats
[x] interface
[x] atexit
[x] log



tips:
每个msg_id的回复msg_id为 (UINT16_MAX-msg_id),所以msg_id 需要小于INT16_MAX