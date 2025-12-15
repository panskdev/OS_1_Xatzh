#ifndef DEBUG_H
#define DEBUG_H

#include <errno.h>
#include <string.h>

#define errorMain 1
#define TEST printf("PASSED %d IN %s\n", __LINE__, __FILE__); if(getchar());

#define	CALL_VOID_SEM(call) {	\
	int code = call;	\
	if(code < 0) {	\
		fprintf(stderr, "Something went wrong at %d in %s (%s) : %s\n", __LINE__, __func__, __FILE__, strerror(errno));	\
		return;		\
	}	\
}

#define	CALL_SEM(call, error) {	\
	int code = call;	\
	if(code < 0) {	\
		fprintf(stderr, "Something went wrong at %d in %s (%s) : %s\n", __LINE__, __func__, __FILE__, strerror(errno));	\
		return error;		\
	}	\
}

#endif