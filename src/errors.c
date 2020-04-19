#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#include "errors.h"

#define E(a,b) ((unsigned char)a),
static const unsigned char errid[] = {
#include "__errors.h"
};

#undef E
#define E(a,b) b "\0"
static const char errmsg[] =
#include "__errors.h"
;

static const char *
strerror_custom (int e)
{
	const char *s;
	int i;
	for (i = 0; errid[i] && errid[i] != e; i++);
	for (s = errmsg; i; s++, i--) for (; *s; s++);
	return s;
}

int
return_error (char *buf, size_t size, int e, const char *errmsg, ...)
{
	va_list args;
	int printed;

	if ((buf != NULL) && (size > 0)) {
		va_start(args, errmsg);
		printed = snprintf(buf, size, "%s", strerror_custom(e));
		vsnprintf(buf + printed, size - printed, errmsg, args);
		va_end(args);
	}
	return e;
}
