#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

//#define DO_MDNS_QUERY
#ifdef DO_MDNS_QUERY
#include "mdns.h"
#endif

#include "syslog_client.h"

/* #define SYSLOG_UTF8 */

/* from https://datatracker.ietf.org/doc/html/rfc5424#section-6 */
#define SYSLOG_NILVALUE "-"
#define SYSLOG_VERSION "1"
#define SYSLOG_SP " "
#define SYSLOG_TIMESTAMP SYSLOG_NILVALUE
#define SYSLOG_MSGID SYSLOG_NILVALUE
#define SYSLOG_STRUCTURED_DATA SYSLOG_NILVALUE
#ifdef SYSLOG_UTF8
#define SYSLOG_BOM "\xEF\xBB\xBF"         /* UTF-8 byte order mask */
#else
#define SYSLOG_BOM ""
#endif

#define SYSLOG_TEMPLATE "<%d>" SYSLOG_VERSION SYSLOG_SP \
                        SYSLOG_TIMESTAMP SYSLOG_SP \
                        "%s" SYSLOG_SP \
                        "%s" SYSLOG_SP \
                        "%s" SYSLOG_SP \
                        SYSLOG_MSGID SYSLOG_SP \
                        SYSLOG_STRUCTURED_DATA SYSLOG_SP \
                        SYSLOG_BOM

static const char TAG[] = "SYSLOG";

static const char wifi_sta_if_key[] = "WIFI_STA_DEF";

static int syslog_fd;
static struct sockaddr_in dest_addr;
static int syslog_facility;
const char *syslog_own_hostname;

static char *syslog_header = NULL;
static size_t syslog_header_len = 0;


static int get_socket_error_code(int socket)
{
	int result;
	u32_t optlen = sizeof(result);
	if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen) == -1)
    {
    	printf("getsockopt failed");
    	result = -1;
	}
	return result;
}


static void show_socket_error_reason(int socket)
{
	int err = get_socket_error_code(socket);
    if (err >= 0)
    {
    	ESP_LOGE(TAG, "UDP socket error %d %s", err, strerror(err));
    }
}

#ifdef DO_MDNS_QUERY
static uint32_t resolve_mdns_host(const char *host_name)
{
    ESP_LOGD(TAG, "Query A: %s.local", host_name);

    esp_ip4_addr_t addr;
    addr.addr = 0;
    uint32_t result = 0;

    esp_err_t err = mdns_query_a(host_name, 2000, &addr);
    if (err)
    {
        if (err == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Host name '%s' was not found by mDNS query", host_name);
        }
        else
        {
            ESP_LOGE(TAG, "mDNS query failed for host name '%s'", host_name);
        }
    }
    else
    {
        ESP_LOGD(TAG, "Host '%s' has IP address " IPSTR, host_name, IP2STR(&addr));
        result = addr.addr;
    }

    return result;
}
#endif

static uint32_t resolve_host(const char *host)
{
    uint32_t result = 0;

    /*
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey(wifi_sta_if_key);
    esp_netif_dns_info_t gdns1, gdns2, gdns3;
    ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &gdns1));
    ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &gdns2));
    ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_FALLBACK, &gdns3));

    ESP_LOGI(TAG, "DNS servers : " IPSTR " , " IPSTR " , " IPSTR,
      IP2STR(&gdns1.ip.u_addr.ip4),
      IP2STR(&gdns2.ip.u_addr.ip4),
      IP2STR(&gdns3.ip.u_addr.ip4));
    */

    struct hostent *he = gethostbyname(host);
    if (he && (he->h_length >= sizeof(result)) && he->h_addr_list && he->h_addr_list[0])
    {
        result = *((uint32_t *)(he->h_addr_list[0]));
        ESP_LOGD(TAG, "DNS query for host name '%s' returned %s", host, inet_ntoa(result));
    }
#ifdef DO_MDNS_QUERY
    else
    {
        result = resolve_mdns_host(host);
    }
#endif
    return result;
}


static void syslog_socket_close()
{
    if (syslog_fd > 0)
    {
        shutdown(syslog_fd, 2);
        close(syslog_fd);
    }
    syslog_fd = 0;
}


void syslog_client_start(const char *host, unsigned int port, int facility)
{
	syslog_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (syslog_fd > 0)
    {
        const uint32_t dest_addr_bytes = resolve_host(host);
        if (dest_addr_bytes)
        {
        	ESP_LOGI(TAG, "Logging to %s:%d", inet_ntoa(dest_addr_bytes), port);
            bzero(&dest_addr, sizeof(dest_addr));
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);
            dest_addr.sin_addr.s_addr = dest_addr_bytes;
        	struct timeval send_to = {100,0};

            int err = setsockopt(syslog_fd, SOL_SOCKET, SO_SNDTIMEO, &send_to, sizeof(send_to));
            if (err >= 0)
            {
                esp_netif_t* netif = esp_netif_get_handle_from_ifkey(wifi_sta_if_key);
                if (esp_netif_get_hostname(netif, &syslog_own_hostname) != ESP_OK)
                {
                    syslog_own_hostname = SYSLOG_NILVALUE;
                }

                syslog_facility = facility;

                ESP_LOGI(TAG, "Remote logging to %s:%d set up successfully", host, port);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to set SO_SNDTIMEO. Error %d", err);
                syslog_socket_close();
            }
        }
        else
        {
            ESP_LOGE(TAG, "Cannot resolve syslog host name '%s'", host);
            syslog_socket_close();
        }
    }
    else
    {
       ESP_LOGE(TAG, "Cannot open socket!");
    }
}


char *build_syslog_client_header(int severity, const char *app_name, const char *task_name)
{
    /* check validity of app_name and task_name */
    const char *app_name_use = (app_name && *app_name) ? app_name : SYSLOG_NILVALUE;
    const char *task_name_use = (task_name && *task_name) ? task_name : SYSLOG_NILVALUE;

    size_t max_header_size = strlen(SYSLOG_TEMPLATE) +
                             strlen(syslog_own_hostname) +
                             strlen(app_name_use) +
                             strlen(task_name_use) + 1;
    char *syslog_header = (char *)malloc(max_header_size);
    assert(syslog_header);
    (void) snprintf(syslog_header, max_header_size, SYSLOG_TEMPLATE,
                    syslog_facility | severity, syslog_own_hostname,
                    app_name_use, task_name_use);

    ESP_LOGD(TAG, "Intermediate template '%s'", syslog_header);

    return syslog_header;
}


void syslog_client_send_with_header(const char *str, int len)
{
    bool retry = true;
    int err;
    if (len < 0)
    {
        len = strlen(str);
    }
    while (retry)
    {
        err = sendto(syslog_fd, str, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if ((err < 0) && (errno == ENOMEM))
        {
            /* let network stack empty out its send buffers,
               see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/lwip.html#limitations */
            vTaskDelay(1);
        }
        else
        {
            retry = false;
        }
    }
    if (err < 0)
    {
        show_socket_error_reason(syslog_fd);
        ESP_LOGE(TAG, "sendto failed with %d", err);
        syslog_client_stop();   /* FIXME: */
    }
}


void syslog_client_stop()
{
    syslog_socket_close();

    if (syslog_header && false)    /* FIXME: */
    {
        char *temp = syslog_header;
        syslog_header = NULL;
        syslog_header_len = 0;
        free(temp);
    }
}
