#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#include "util.h"
#include "log.h"

int util_base64_encode(const char *src, char **output, size_t *output_size)
{
	static char tbl[64] = {
		'A','B','C','D','E','F','G','H',
		'I','J','K','L','M','N','O','P',
		'Q','R','S','T','U','V','W','X',
		'Y','Z','a','b','c','d','e','f',
		'g','h','i','j','k','l','m','n',
		'o','p','q','r','s','t','u','v',
		'w','x','y','z','0','1','2','3',
		'4','5','6','7','8','9','+','/'
	};
        unsigned int i;
        size_t len;
	char *p = NULL;

	len = strlen(src);

	*output_size = 4 * ((len + 2) / 3) + 1;
	*output = malloc(*output_size);

	p = *output;
	/* Transform the 3x8 bits to 4x6 bits, as required by base64. */
	for (i = 0; i < len; i += 3)
	{
                /* square 1 */
		*p++ = tbl[src[0] >> 2];

                /* square 2 */
                if(i + 1 >= len)
                {
                        *p++ = tbl[((src[0] & 3) << 4)];
                        /* pad two equal char */
                        *p = *(p + 1) = '=';
                        p += 2;
                        break;
                }

                *p++ = tbl[((src[0] & 3) << 4) + (src[1] >> 4)];

                /* square 3 */
                if(i + 2 >= len)
                {
                        *p++ = tbl[((src[1] & 0xf) << 2)];
                        /* pad one equal char */
                        *p++ = '=';
                        break;
                }

		*p++ = tbl[((src[1] & 0xf) << 2) + (src[2] >> 6)];

                /* square 4 */
		*p++ = tbl[src[2] & 0x3f];

		src += 3;
	}

	/* ...and zero-teminate it.  */
	*p = '\0';

	return 0;
}

time_t util_getuptime(void)
{
	struct timespec tp;

        if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0)
	{
		log_error("Error getting clock %s !",
                          strerror(errno));
		return 0;
	}

	return tp.tv_sec;
}

int util_getifaddr(const char *ifname, struct in_addr *addr)
{
        /* SIOCGIFADDR struct ifreq *  */
	int s;
	struct ifreq ifr;
	int ifrlen;

	if(!ifname || ifname[0]=='\0')
        {
		return -1;
        }

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		log_error("socket(PF_INET, SOCK_DGRAM): %s",
                          strerror(errno));
		return -1;
	}

        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET; /* IPv4 IP address */
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
	ifrlen = sizeof(ifr);

	if(ioctl(s, SIOCGIFADDR, &ifr, &ifrlen) < 0)
	{
		log_error("ioctl(s, SIOCGIFADDR, ...): %s",
                          strerror(errno));
		close(s);
		return -1;
	}

	*addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;

	close(s);

	return 0;
}

char *strdup_trim(const char *s)
{
        size_t begin, end, len;
        char *r = NULL;

        if(!s)
        {
                return NULL;
        }

        end = strlen(s) - 1;

        for(begin = 0; begin < end; ++begin)
        {
                if(s[begin] != ' ' && s[begin] != '\t' && s[begin] != '"')
                {
                        break;
                }
        }

        for (; begin < end ; --end)
        {
                if(s[end] != ' ' && s[end] != '\t' && s[end] != '"'
                   && s[end] != '\n')
                {
                        break;
                }
        }

        len = end - begin + 1;

        r = malloc(len + 1);
        if(r != NULL)
        {
                strncpy(r, s + begin, len);
                r[len] = '\0';
        }

        return r;
}

long strtol_safe(char const *buf, long def)
{
        long ret;

        if(*buf == '\0')
        {
                return def;
        }

        errno = 0;
        ret = strtol(buf, NULL, 10);
        if(errno != 0)
        {
                log_error("%s - conversion failed, buffer was %s",
                          __func__, buf);
                return def;
        }

        return ret;
}
