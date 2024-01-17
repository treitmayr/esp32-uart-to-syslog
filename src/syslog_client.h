#pragma once

/* severities */
#define SYSLOG_EMERG       0       /* system is unusable */
#define SYSLOG_ALERT       1       /* action must be taken immediately */
#define SYSLOG_CRIT        2       /* critical conditions */
#define SYSLOG_ERR         3       /* error conditions */
#define SYSLOG_WARNING     4       /* warning conditions */
#define SYSLOG_NOTICE      5       /* normal but significant condition */
#define SYSLOG_INFO        6       /* informational */
#define SYSLOG_DEBUG       7       /* debug-level messages */

/* facility codes */
#define SYSLOG_KERN        (0<<3)  /* kernel messages */
#define SYSLOG_USER        (1<<3)  /* random user-level messages */
#define SYSLOG_MAIL        (2<<3)  /* mail system */
#define SYSLOG_DAEMON      (3<<3)  /* system daemons */
#define SYSLOG_AUTH        (4<<3)  /* security/authorization messages */
#define SYSLOG_SYSLOG      (5<<3)  /* messages generated internally by syslogd */
#define SYSLOG_LPR         (6<<3)  /* line printer subsystem */
#define SYSLOG_NEWS        (7<<3)  /* network news subsystem */
#define SYSLOG_UUCP        (8<<3)  /* UUCP subsystem */
#define SYSLOG_CRON        (9<<3)  /* clock daemon */
#define SYSLOG_AUTHPRIV    (10<<3) /* security/authorization messages (private) */
#define SYSLOG_FTP         (11<<3) /* ftp daemon */
        /* other codes through 15 reserved for system use */
#define SYSLOG_LOCAL0      (16<<3) /* reserved for local use */
#define SYSLOG_LOCAL1      (17<<3) /* reserved for local use */
#define SYSLOG_LOCAL2      (18<<3) /* reserved for local use */
#define SYSLOG_LOCAL3      (19<<3) /* reserved for local use */
#define SYSLOG_LOCAL4      (20<<3) /* reserved for local use */
#define SYSLOG_LOCAL5      (21<<3) /* reserved for local use */
#define SYSLOG_LOCAL6      (22<<3) /* reserved for local use */
#define SYSLOG_LOCAL7      (23<<3) /* reserved for local use */

void syslog_client_start(const char *host, unsigned int port, int facility);

char *build_syslog_client_header(int severity, const char *app_name, const char *task_name);

void syslog_client_send_with_header(const char *str, int len);

void syslog_client_stop();
