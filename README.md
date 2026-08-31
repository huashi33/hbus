# hbus
process communication framework


TODO
node
[o] aync
[o] lazy subscrib
[o] aync call handle in recv_cb
[x] interface 
[x] atexit
[x] log
[x] muti-thread
[x] don't recv msg from self
<!-- Performance optimization -->
[x] request pool
[x] exec pool
[x] hide nng


hbusbroker
[o] sync->async
[o] heart-beats
[x] interface
[x] atexit
[x] log
[x] muti-thread



tips:
每个msg_id的回复msg_id为 (UINT16_MAX-msg_id),所以msg_id 需要小于INT16_MAX