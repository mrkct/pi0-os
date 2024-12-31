#include <kernel/lib/more_time.h>

#include "pl031.h"


PL031::PL031(Config const *config)
    : m_config(*config)
{
}

int32_t PL031::init()
{
    if (r != nullptr)
        return 0;
    
    r = reinterpret_cast<RegisterMap volatile*>(ioremap(m_config.physaddr, sizeof(RegisterMap)));
    iowrite32(&r->cr, 1);

    return 0;
}

int32_t PL031::shutdown()
{
    if (r == nullptr)
        return 0;
    
    iowrite32(&r->cr, 0);

    return 0;
}

int32_t PL031::get_time(api::DateTime &dt)
{
    if (r == nullptr)
        return -ERR_NODEV;

    uint32_t data = ioread32(&r->dr);

    struct tm time;
    secs_to_tm(data, &time);
    dt = datetime_from_tm(time);

    return 0;
}

int32_t PL031::set_time(const api::DateTime dt)
{
    if (r == nullptr)
        return -ERR_NODEV;
    
    struct tm time;
    datetime_to_tm(dt, &time);
    uint32_t data = tm_to_secs(&time);
    iowrite32(&r->lr, data);

    return 0;
}
