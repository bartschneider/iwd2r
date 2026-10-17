#include "CGameObject84F14C.h"

#include "CBaldurChitin.h"
#include "CGameAnimationType84EEB8.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CPathSearch.h"
#include "CProjectileArea.h"
#include "CUtil.h"

// 0x84F260: the ticks between an incendiary cloud's new starts.
static const LONG INCENDIARY_INTERVAL = 100;

// 0x56DF00
CGameObject84F14C::CGameObject84F14C(const CProjectileAreaExplosion2& explosion, CGameArea* pArea, const CPoint& ptExact, LONG posZ, const CPoint& velocity, WORD nLife, BYTE nFade, BYTE nWallMode)
{
    m_pPoints = NULL;
    m_nPoint = 0;
    m_bWander = FALSE;
    m_bIncendiary = FALSE;
    m_nTicks = 0;

    // The animation faces the way the piece flies.
    m_animation.m_animation = new CGameAnimationType84EEB8(explosion,
        CGameSprite::GetDirection(CPoint(0, 0), velocity) % 16);

    m_sound.SetResRef(CResRef(CString(explosion.m_sound.c_str())), TRUE, TRUE);
    m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));

    m_bVisible = TRUE;
    m_ptExact.x = ptExact.x;
    m_ptExact.y = ptExact.y;
    m_velocity.x = velocity.x;
    m_velocity.y = velocity.y;
    m_nLife = nLife;
    m_nFade = nFade;
    m_nWallMode = nWallMode;

    // NOTE: The 16 bytes at 0x8A8154 are the visible terrain table's values.
    memcpy(m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(m_terrainTable));

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CGameObject::AddToArea(pArea,
            CPoint(ptExact.x >> CGameSprite::EXACT_SCALE, ptExact.y >> CGameSprite::EXACT_SCALE),
            posZ,
            LIST_FRONT);
    } else {
        // NOTE: The original deletes the piece in its own constructor; the
        // caller drops the pointer.
        delete this;
    }
}

// 0x56E280
CGameObject84F14C::CGameObject84F14C(const CProjectileAreaExplosion3& explosion, CGameArea* pArea, const CPoint& ptExact, LONG posZ, const CPoint& velocity, WORD nLife, BYTE nFade, BYTE nWallMode)
{
    // NOTE: The original sets the points, the point and the two flags only
    // once the piece is in the object array, so a piece it cannot add
    // deletes itself with them unset (its destructor reads the points).
    // Ours starts them empty (P4-10).
    m_pPoints = NULL;
    m_nPoint = 0;
    m_bWander = FALSE;
    m_bIncendiary = FALSE;
    m_nTicks = 0;

    m_animation.m_animation = new CGameAnimationType84EEB8(explosion,
        CGameSprite::GetDirection(CPoint(0, 0), velocity) % 16);

    m_sound.SetResRef(CResRef(CString(explosion.m_sound.c_str())), TRUE, TRUE);
    m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));

    m_bVisible = TRUE;
    m_ptExact.x = ptExact.x;
    m_ptExact.y = ptExact.y;
    m_velocity.x = velocity.x;
    m_velocity.y = velocity.y;
    m_nLife = nLife;
    m_nFade = nFade;
    m_nWallMode = nWallMode;

    // NOTE: The 16 bytes at 0x8A8154 are the visible terrain table's values.
    memcpy(m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(m_terrainTable));

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CGameObject::AddToArea(pArea,
            CPoint(ptExact.x >> CGameSprite::EXACT_SCALE, ptExact.y >> CGameSprite::EXACT_SCALE),
            posZ,
            LIST_FRONT);

        m_pPoints = explosion.m_pPoints;
        m_pPoints->m_nRefCount++;
        m_nPoint = explosion.field_30;
        m_bWander = explosion.field_34;
        m_bIncendiary = explosion.field_44;
    } else {
        delete this;
    }
}

// 0x56E580
CGameObject84F14C::~CGameObject84F14C()
{
    // The last piece deletes the explosion's points.
    if (m_pPoints != NULL) {
        m_pPoints->m_nRefCount--;
        if (m_pPoints->m_nRefCount >= 0 && m_pPoints->m_nRefCount == 0) {
            delete m_pPoints;
        }
    }
}

// 0x56E650
void CGameObject84F14C::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    m_nTicks++;

    // A piece without a life lasts to the end of its sequence.
    if (m_nLife == 0) {
        // NOTE: Uninline.
        if (m_animation.IsEndOfSequence()) {
            RemoveFromArea();
            return;
        }
    } else {
        if (--m_nLife == 0) {
            RemoveFromArea();
            return;
        }
    }

    CPoint ptOldGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);

    m_ptExact.x += m_velocity.x;
    m_ptExact.y += m_velocity.y;
    m_pos.x = m_ptExact.x >> CGameSprite::EXACT_SCALE;
    m_pos.y = m_ptExact.y >> CGameSprite::EXACT_SCALE;

    CPoint ptNewGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
    if (ptOldGrid.x != ptNewGrid.x || ptOldGrid.y != ptNewGrid.y) {
        // NOTE: The original checks no bounds, so a piece past the area's
        // left or top edge reads the search map before its start. Ours
        // takes such a square for a wall (P4-10).
        SHORT nTableIndex;
        BYTE nCost = CPathSearch::COST_IMPASSABLE;
        if (ptNewGrid.x >= 0 && ptNewGrid.y >= 0) {
            nCost = m_pArea->m_search.GetLOSCost(ptNewGrid, m_terrainTable, nTableIndex, FALSE);
        }

        if (nCost == CPathSearch::COST_IMPASSABLE) {
            if (m_nWallMode == 1) {
                RemoveFromArea();
                return;
            }

            if (m_nWallMode == 0) {
                // Bounce off the wall and face the new way.
                CPoint ptOld = m_ptExact;

                if (ptOldGrid.x != ptNewGrid.x) {
                    m_ptExact.x -= 2 * m_velocity.x;
                    m_velocity.x = -m_velocity.x;
                    m_pos.x = m_ptExact.x >> CGameSprite::EXACT_SCALE;
                }

                if (ptOldGrid.y != ptNewGrid.y) {
                    m_ptExact.y -= 2 * m_velocity.y;
                    m_velocity.y = -m_velocity.y;
                    m_pos.y = m_ptExact.y >> CGameSprite::EXACT_SCALE;
                }

                // NOTE: Uninline.
                m_animation.ChangeDirection(CGameSprite::GetDirection(ptOld, m_ptExact));
            } else {
                m_bVisible = FALSE;
            }
        } else {
            m_bVisible = TRUE;
        }
    }

    CGameAnimationType84EEB8* pAnimation = static_cast<CGameAnimationType84EEB8*>(m_animation.m_animation);

    // An incendiary cloud starts again every 100 ticks (ICloudB) and goes
    // on burning (ICloudA) when that ends.
    if (m_bIncendiary == TRUE) {
        // NOTE: Uninline.
        if (m_animation.IsEndOfSequence()) {
            if (pAnimation->m_sAnimation.compare(0, pAnimation->m_sAnimation.size(), "ICloudB") == 0) {
                pAnimation->SetAnimation("ICloudA");
            }
        }

        if (m_nTicks != 0 && m_nTicks % INCENDIARY_INTERVAL == 0) {
            pAnimation->SetAnimation("ICloudB");
        }
    }

    // A wandering piece moves to another free point at the end of its
    // sequence, one of the later ones more likely.
    if (m_bWander == TRUE) {
        // NOTE: Uninline.
        if (m_animation.IsEndOfSequence()) {
            std::vector<CProjectileAreaPoints::Point>& points = m_pPoints->m_points;

            // NOTE: The original draws until it finds a point not taken,
            // forever when every point is (its own is). Ours stays where it
            // is then; it draws the same numbers otherwise (P4-10).
            BOOL bFree = FALSE;
            for (size_t nFree = 0; nFree < points.size(); nFree++) {
                if (points[nFree].m_bTaken != 1) {
                    bFree = TRUE;
                    break;
                }
            }

            if (bFree) {
                LONG nPoint;
                do {
                    LONG nPoints = static_cast<LONG>(points.size());
                    LONG nBack = 0;
                    if (nPoints != 0) {
                        LONG nRange = rand() % nPoints;
                        if (nRange != 0) {
                            nBack = rand() % nRange;
                        }
                    }
                    nPoint = nPoints - nBack - 1;
                } while (points[nPoint].m_bTaken == 1);

                points[nPoint].m_bTaken = 1;
                points[m_nPoint].m_bTaken = 0;
                m_nPoint = nPoint;

                m_ptExact.x = points[nPoint].x;
                m_ptExact.y = points[nPoint].y;
                m_pos.x = m_ptExact.x >> CGameSprite::EXACT_SCALE;
                m_pos.y = m_ptExact.y >> CGameSprite::EXACT_SCALE;
            }
        }
    }

    // NOTE: Uninline.
    m_animation.IncrementFrame();
}

// 0x56EA90
void CGameObject84F14C::Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameTemporal.cpp
    // __LINE__: 354
    UTIL_ASSERT(pVidMode != NULL);

    BOOL bFadeOut = FALSE;
    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    if (!m_bVisible) {
        return;
    }

    if (m_pos.x < 0 || m_pos.y < 0 || m_pos.x > 0x1400 || m_pos.y > 0xF00) {
        return;
    }

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    if (m_nLife < static_cast<SHORT>(m_nFade)) {
        bFadeOut = TRUE;
    }

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    CRect rFx;
    CPoint ptReference;

    // NOTE: Uninline.
    m_animation.CalculateFxRect(rFx, ptReference, m_posZ);

    CPoint ptPos(m_pos.x, m_pos.y);
    ptPos.y += m_pArea->GetHeightOffset(m_pos, LIST_BACK);

    CRect rGCBounds;
    m_animation.CalculateGCBoundsRect(rGCBounds,
        ptPos,
        ptReference,
        m_posZ,
        rFx.right - rFx.left,
        rFx.bottom - rFx.top);

    if (!rView.IntersectRect(rGCBounds, rView)) {
        return;
    }

    COLORREF rgbTint = m_pArea->GetTintColor(CPoint(ptPos.x, ptPos.y + m_posZ), m_listType);

    // NOTE: Uninline.
    m_animation.Render(pInfinity,
        pVidMode,
        nSurface,
        rFx,
        ptPos,
        ptReference,
        0x20000,
        rgbTint,
        rGCBounds,
        FALSE,
        bFadeOut,
        m_posZ,
        0);
}

// 0x56ECF0
void CGameObject84F14C::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameTemporal.cpp
        // __LINE__: 435
        UTIL_ASSERT(FALSE);
        return;
    }

    delete this;
}
