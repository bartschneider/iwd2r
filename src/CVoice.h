#ifndef CVOICE_H_
#define CVOICE_H_

#include "mfc.h"

// clang-format off
#include "platform/dsound_shim.h"
// clang-format on

class CSound;

// #memory-identical
class CVoice : public CObject {
public:
    CVoice();
    ~CVoice();

    void SetSound(CSound* pSound);
    BOOL GetLooping();
    BOOL IsSoundPlaying();
    int GetChannel();
    void ResetVolume();

    int GetPriority();

    /* 0004 */ CSound* m_pSound;
    /* 0008 */ IDirectSoundBuffer* m_pSoundBuffer;
    /* 000C */ int m_nChannel;
    /* 0010 */ int m_nPriority;
};

#endif /* CVOICE_H_ */
