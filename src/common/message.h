/*
* Copyright (C) Xinjing Cho
*/

#ifndef MESSAGE_H_
#define MESSAGE_H_

/*
* Generic message sent over the network.
*/
typedef struct message {
	w_int_t len;
	void   *data;
} message_t;

#endif