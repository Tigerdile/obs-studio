#include "lua.h"
#include "lstate.h"

void lua_lock_init_(lua_State *L)
{
	pthread_mutexattr_t attr;

	L->mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&L->mutex, &attr);
}

void lua_lock_destroy_(lua_State *L)
{
	pthread_mutex_destroy(&L->mutex);
}

void lua_lock_(lua_State *L)
{
	pthread_mutex_lock(&L->mutex);
}

void lua_unlock_(lua_State *L)
{
	pthread_mutex_unlock(&L->mutex);
}
