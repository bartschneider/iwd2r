#include "CResStore.h"

// 0x402660
CResStore::CResStore()
{
    m_bParsed = FALSE;
}

// 0x4026A0
CResStore::~CResStore()
{
}

// 0x4026B0
void* CResStore::Demand()
{
    void* pData = CRes::Demand();
    if (!m_bParsed || GetDemands() <= 1) {
        // NOTE: Uninline.
        Parse(pData);

        if (!m_bParsed) {
            return NULL;
        }
    }

    return pData;
}

// NOTE: Inlined in `CResStore::Demand`.
BOOL CResStore::Parse(void* pData)
{
    if (pData == NULL) {
        return FALSE;
    }

    DWORD* header = reinterpret_cast<DWORD*>(pData);
    // NOTE: The signature as the file's bytes read little-endian: "STOR" or
    // "STOO", then "V9.0" (0x4026D4-0x4026E2).
    if ((header[0] != 0x524F5453 && header[0] != 0x4F4F5453) || header[1] != 0x302E3956) {
        return FALSE;
    }

    m_bParsed = TRUE;

    return m_bParsed;
}
