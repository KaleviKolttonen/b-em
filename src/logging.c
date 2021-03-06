/* Part of B-Em by Tom Walker */

#include "b-em.h"

#include <allegro.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#define LOG_DEST_FILE   0x01
#define LOG_DEST_STDERR 0x02
#define LOG_DEST_MSGBOX 0x04

typedef struct {
    uint32_t   mask;
    uint32_t   shift;
    const char *name;
} log_level_t;

static const log_level_t ll_fatal = { 0xf0000, 16, "FATAL"   };
static const log_level_t ll_error = { 0x0f000, 12, "ERROR"   };
static const log_level_t ll_warn  = { 0x00f00,  8, "WARNING" };
static const log_level_t ll_info  = { 0x000f0,  4, "INFO"    };
static const log_level_t ll_debug = { 0x0000f,  0, "DEBUG"   };

static const log_level_t *log_levels[] =
{
    &ll_fatal,
    &ll_error,
    &ll_warn,
    &ll_info,
    &ll_debug,
    NULL
};

static const char log_section[]    = "logging";
static const char log_default_fn[] = "b-emlog.txt";

static unsigned log_options = 0;

static FILE *log_fp;
static char   tmstr[20];
static time_t last = 0;

static void log_common(unsigned dest, const char *level, char *msg, size_t len)
{
    time_t now;

    while (msg[len-1] == '\n')
	len--;
    if ((dest & LOG_DEST_FILE) && log_fp) {
	time(&now);
	if (now != last)
	{
	    strftime(tmstr, sizeof(tmstr), "%d/%m/%Y %H:%M:%S", localtime(&now));
	    last = now;
	}
	fprintf(log_fp, "%s %s ", tmstr, level); 
	fwrite(msg, len, 1, log_fp);
	putc('\n', log_fp);
	fflush(log_fp);
    }
    if (dest & LOG_DEST_STDERR) {
	fwrite(msg, len, 1, stderr);
	putc('\n', stderr);
    }
    if (dest & LOG_DEST_MSGBOX)
	log_msgbox(level, msg);
}

static char msg_malloc[] = "log_format: out of space - following message truncated";

static void log_format(const log_level_t *ll, const char *fmt, va_list ap)
{
    unsigned opt, dest;
    va_list apc;
    char   abuf[200], *mbuf;
    size_t len;

    if ((opt = log_options & ll->mask)) {
        dest = opt >> ll->shift;
        va_copy(apc, ap);
        len = vsnprintf(abuf, sizeof abuf, fmt, ap);
        if (len < sizeof abuf)
            log_common(dest, ll->name, abuf, len);
        else if ((mbuf = malloc(len + 1))) {
            vsnprintf(mbuf, len+1, fmt, apc);
            log_common(dest, ll->name, mbuf, len);
            free(mbuf);
        } else {
            log_common(dest, ll->name, msg_malloc, sizeof msg_malloc);
            log_common(dest, ll->name, abuf, len);
        }
    }
}

#ifdef _DEBUG

void log_debug(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_format(&ll_debug, fmt, ap);
    va_end(ap);
}

#endif

void log_info(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_format(&ll_info, fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_format(&ll_warn, fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_format(&ll_error, fmt, ap);
    va_end(ap);
}

void log_fatal(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    log_format(&ll_fatal, fmt, ap);
    va_end(ap);
}

static int contains(const char *haystack, const char *needle)
{
    size_t needle_len;

    needle_len = strlen(needle);
    haystack--;
    do {
	while (*++haystack == ' ')
	    ;
	if (strncasecmp(haystack, needle, needle_len) == 0)
	    return 1;
	haystack = strchr(haystack, ',');
    } while (haystack);
    return 0;
}

void log_open(void)
{
    const char *log_fn, *to_file, *to_stderr, *to_msgbox;
    unsigned new_opt;
    const log_level_t **llp, *ll;
    int append;

    log_fn = get_config_string(log_section, "log_filename", log_default_fn);
    to_file = get_config_string(log_section, "to_file", "FATAL,ERROR,WARNING,INFO,DEBUG");
    to_stderr = get_config_string(log_section, "to_stderr", "FATAL,ERROR,WARNING");
    to_msgbox = get_config_string(log_section, "to_msgbox", "FATAL,ERROR");
    new_opt = 0;
    for (llp = log_levels; (ll = *llp++); )
    {
	if (contains(to_file, ll->name))
	    new_opt |= (LOG_DEST_FILE << ll->shift);
	if (contains(to_stderr, ll->name))
	    new_opt |= (LOG_DEST_STDERR << ll->shift);
	if (contains(to_msgbox, ll->name))
	    new_opt |= (LOG_DEST_MSGBOX << ll->shift);
    }
    log_options = new_opt;
    append = get_config_int(log_section, "append", 1);
    if ((log_fp = fopen(log_fn, append ? "at" : "wt")) == NULL)
	log_warn("log_open: unable to open log %s: %s", log_fn, strerror(errno));
    log_debug("log_open: log options=%x", log_options);
}

void log_close(void)
{
    if (log_fp)
	fclose(log_fp);
}
