#ifndef _YADDNS_CFGSTR_H_
#define _YADDNS_CFGSTR_H_

#include <stdlib.h>
#include <string.h>

struct cfgstr {
        enum {
                cfgstr_status_notset = 0,
                cfgstr_status_set,
                cfgstr_status_dupped,
        } s;
        union {
                char *chs;
                const char *chs_const;
        } v;
};

/* memset(cfgstr, 0, sizeof(*cfgstr)) do the same thing */
static inline void cfgstr_init(struct cfgstr *cfgstr)
{
        cfgstr->s = cfgstr_status_notset;
}

static inline void cfgstr_unset(struct cfgstr *cfgstr) 
{
        if(cfgstr->s == cfgstr_status_dupped)
        {
                free(cfgstr->v.chs);
        }

        cfgstr->s = cfgstr_status_notset;
}

static inline void cfgstr_set(struct cfgstr *cfgstr, const char *value)
{
        cfgstr_unset(cfgstr);

        cfgstr->s = cfgstr_status_set;
        cfgstr->v.chs_const = value;
}

static inline void cfgstr_dup(struct cfgstr *cfgstr, const char *value)
{
        cfgstr_unset(cfgstr);

        cfgstr->s = cfgstr_status_dupped;
        cfgstr->v.chs = strdup(value);
}

static inline const char *cfgstr_get(const struct cfgstr *cfgstr)
{
        return (cfgstr->s == cfgstr_status_notset ? "" : cfgstr->v.chs_const);
}

static inline int cfgstr_is_set(const struct cfgstr *cfgstr)
{
        return (cfgstr->s != cfgstr_status_notset);
}

static inline void cfgstr_copy(const struct cfgstr *cfgstrsrc,
                               struct cfgstr *cfgstrdst)
{
        cfgstr_dup(cfgstrdst, cfgstrsrc->v.chs_const);
}

static inline void cfgstr_move(struct cfgstr *cfgstrsrc,
                               struct cfgstr *cfgstrdst)
{
        cfgstr_unset(cfgstrdst);

        cfgstrdst->s = cfgstrsrc->s;
        cfgstrdst->v.chs = cfgstrsrc->v.chs;

        cfgstr_init(cfgstrsrc);
}

#endif
