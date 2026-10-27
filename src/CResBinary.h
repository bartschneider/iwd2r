#ifndef CRESBINARY_H_
#define CRESBINARY_H_

#include "CRes.h"

class CResBinary : public CRes {
public:
    CResBinary();
    ~CResBinary();

    void* Demand();
    int Release();

    /* 0050 */ char* m_resVersion;
    /* 0054 */ BOOL m_bParsed;
};

#endif /* CRESBINARY_H_ */
