#ifndef CGAMETIMER_H_
#define CGAMETIMER_H_

#include "mfc.h"

class CGameTimer {
public:
    LONG GetTime();
    BYTE GetId();
    void SetTime(LONG time);
    void SetId(BYTE id);

    /* 0000 */ LONG m_time;
    /* 0004 */ BYTE m_id;
};

#endif /* CGAMETIMER_H_ */
