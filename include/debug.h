#ifndef DEBUG_H
#define DEBUG_H

#define errorMain 1
#define TEST printf("PASSED %d IN %s\n", __LINE__, __FILE__); if(getchar());

#define	CALL_VOID_SEM(call) {	\
	int code = call;	\
	if(code < 0) {	\
		printf("Something went wrong at %d in %s (%s)\n", __LINE__, __func__, __FILE__);	\
		return;		\
	}	\
}

#define	CALL_SEM(call, error) {	\
	int code = call;	\
	if(code < 0) {	\
		printf("Something went wrong at %d in %s (%s)\n", __LINE__, __func__, __FILE__);	\
		return error;		\
	}	\
}

#endif