# hbus
process communication framework


TODO
<!-- Function -->
[o] sync -> async
[o] heart-beats
[o] lazy subscrib
[o] aync call handle in recv_cb
[o] don't recv own request

[o] static link to hcore
[o] c++ -> c
[x] auto test base
[x] interface export
[x] atexit
[x] log
[x] muti-thread
<!-- Performance -->
[x] request pool
[x] exec pool
[x] hide nng



TIPS:
每个msg_id的回复msg_id为 (UINT16_MAX-msg_id),所以msg_id 需要小于INT16_MAX









