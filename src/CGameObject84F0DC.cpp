#include "CGameObject84F0DC.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObject84F14C.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CPathSearch.h"
#include "CUtil.h"
#include "CVidMode.h"

#include <math.h>

// 0x85BD6C
const BYTE CGameObject84F0DC::WALL_BOUNCE = 0;

// 0x85BD6D
const BYTE CGameObject84F0DC::WALL_STOP = 1;

const BYTE CGameObject84F0DC::STATE_MOVING = 0;
const BYTE CGameObject84F0DC::STATE_THROWN = 1;
const BYTE CGameObject84F0DC::STATE_STOPPED = 2;

// 0x56BF30
CGameObject84F0DC::CGameObject84F0DC(const CProjectileAreaExplosion& explosion1, const CProjectileAreaExplosion2& explosion2, const CProjectileAreaExplosion3& explosion3, CGameArea* pArea, const CPoint& pos, SHORT nRadius, BYTE nSpeed, BYTE nWallMode, WORD nDuration)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    m_nLingerCooldown = 0;
    m_bAnimation = FALSE;
    m_nTicks = 0;
    m_nDuration = nDuration;
    m_nPieces = 0;
    m_nExpandTicks = static_cast<BYTE>((nRadius - 1) / nSpeed + 1);
    m_nWallMode = nWallMode;

    if (!explosion1.m_animation.empty()) {
        m_animation.SetResRef(CResRef(CString(explosion1.m_animation.c_str())), FALSE, TRUE, TRUE);
        m_bAnimation = TRUE;
    }

    m_visualEffect = explosion1.m_visualEffect;
    m_explosion2 = explosion2;
    m_explosion3 = explosion3;

    m_animation.SequenceSet(0);
    m_animation.FrameSet(0);

    field_7E = 0;
    field_80 = 0;
    m_pParticles = NULL;
    m_pStates = NULL;

    // The grid of squares the particles have entered: 16 pixels a square
    // (on the scaled y), as many squares out from the centre as the radius
    // reaches. It holds the two arcs' steps first.
    LONG nRadiusSquares = (nRadius - 1) / CPathSearch::GRID_SQUARE_SIZEX;
    m_nGridX = nRadiusSquares + 1;
    m_nGridY = nRadiusSquares + 1;
    m_pGrid = new BYTE[(2 * m_nGridX + 1) * (2 * m_nGridY + 1)];
    if (m_pGrid != NULL) {
        // The two octants of a circle of one step per two squares: the x
        // steps (32 pixels) of the first going down by 0 or 24, the y steps
        // (24) of the second going left by 0 or 32, each ending on a step.
        LONG nAxis = (nRadius - 1) / (2 * CPathSearch::GRID_SQUARE_SIZEX) + 1;
        m_nGridX = nAxis;
        m_nGridY = nAxis;

        CVidMode* pVidMode = g_pBaldurChitin->GetCurrentVideoMode();
        m_nArc1 = pVidMode->GetEllipseArcPixelList(m_nGridX, nAxis, m_pGrid) + 1;
        m_pGrid[m_nArc1 - 1] = 1;

        pVidMode = g_pBaldurChitin->GetCurrentVideoMode();
        m_nArc2 = pVidMode->GetEllipseArcPixelList(m_nGridY, m_nGridX, m_pGrid + m_nArc1) + 1;
        m_pGrid[m_nArc1 + m_nArc2 - 1] = 1;

        m_nGridX = nRadiusSquares + 1;
        m_nGridY = nRadiusSquares + 1;

        LONG nParticles = m_nArc1 + m_nArc2;
        m_pParticles = new Particle[4 * nParticles];
        if (m_pParticles != NULL) {
            m_pStates = new BYTE[4 * nParticles];
            if (m_pStates == NULL) {
                delete this;
                return;
            }

            memset(m_pStates, 0, 4 * nParticles);

            // Each point of the two arcs gives a particle of each quarter,
            // flying from the centre through it at the speed; on an axis two
            // quarters would fly the same way and are stopped.
            LONG nCentreX = pos.x;
            LONG nCentreY = (pos.y << 2) / 3;
            LONG x = nCentreX;
            LONG y = nRadius + nCentreY;
            for (SHORT nPoint = 0; nPoint < m_nArc1; nPoint++) {
                LONG dy = y - nCentreY;
                LONG dx = x - nCentreX;
                LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

                Particle& particle = m_pParticles[nPoint];
                Particle& particle1 = m_pParticles[nParticles + nPoint];
                Particle& particle2 = m_pParticles[2 * nParticles + nPoint];
                Particle& particle3 = m_pParticles[3 * nParticles + nPoint];

                particle.x = 0;
                particle1.x = 0;
                particle2.x = 0;
                particle3.x = 0;
                particle.y = 0;
                particle1.y = 0;
                particle2.y = 0;
                particle3.y = 0;

                particle.vx = (dx << CGameSprite::EXACT_SCALE) * nSpeed / nDistance;
                particle1.vx = particle.vx;
                if (particle.vx == 0) {
                    m_pStates[2 * nParticles + nPoint] = STATE_STOPPED;
                    m_pStates[3 * nParticles + nPoint] = STATE_STOPPED;
                }
                particle2.vx = -particle.vx;
                particle3.vx = -particle.vx;

                particle.vy = (dy << CGameSprite::EXACT_SCALE) * nSpeed / nDistance;
                particle1.vy = -particle.vy;
                particle2.vy = particle.vy;
                particle3.vy = -particle.vy;

                x += 2 * CPathSearch::GRID_SQUARE_SIZEX;
                y -= 2 * CPathSearch::GRID_SQUARE_SIZEY * m_pGrid[nPoint];
            }

            x = nRadius + nCentreX;
            y = nCentreY;
            for (SHORT nPoint = 0; nPoint < m_nArc2; nPoint++) {
                LONG dx = x - nCentreX;
                LONG dy = y - nCentreY;
                LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

                LONG nIndex = m_nArc1 + nPoint;
                Particle& particle = m_pParticles[nIndex];
                Particle& particle1 = m_pParticles[nParticles + nIndex];
                Particle& particle2 = m_pParticles[2 * nParticles + nIndex];
                Particle& particle3 = m_pParticles[3 * nParticles + nIndex];

                particle.x = 0;
                particle1.x = 0;
                particle2.x = 0;
                particle3.x = 0;
                particle.y = 0;
                particle1.y = 0;
                particle2.y = 0;
                particle3.y = 0;

                particle.vx = (dx << CGameSprite::EXACT_SCALE) * nSpeed / nDistance;
                particle1.vx = particle.vx;
                particle2.vx = -particle.vx;
                particle3.vx = -particle.vx;

                particle.vy = (dy << CGameSprite::EXACT_SCALE) * nSpeed / nDistance;
                particle1.vy = -particle.vy;
                particle2.vy = particle.vy;
                particle3.vy = -particle.vy;

                // NOTE: The original tests the vy of particle nPoint (of the
                // first arc), not of this one (0x56CDAF), so the second
                // arc's two horizontal quarters both fly; ours too.
                if (m_pParticles[nPoint].vy == 0) {
                    m_pStates[nParticles + nIndex] = STATE_STOPPED;
                    m_pStates[3 * nParticles + nIndex] = STATE_STOPPED;
                }

                x -= 2 * CPathSearch::GRID_SQUARE_SIZEX * m_pGrid[nIndex];
                y += 2 * CPathSearch::GRID_SQUARE_SIZEY;
            }

            // NOTE: The 16 bytes at 0x8A8154 are the visible terrain table's
            // values.
            memcpy(m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(m_terrainTable));

            BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                CGameObject::AddToArea(pArea, pos, 0, LIST_FRONT);
                return;
            }
        }
    }

    // NOTE: The original deletes the explosion in its own constructor; the
    // caller drops the pointer.
    delete this;
}

// 0x56CF00
CGameObject84F0DC::~CGameObject84F0DC()
{
    if (m_pGrid != NULL) {
        delete[] m_pGrid;
    }

    if (m_pParticles != NULL) {
        delete[] m_pParticles;
    }

    if (m_pStates != NULL) {
        delete[] m_pStates;
    }
}

// 0x56D0A0
void CGameObject84F0DC::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    m_nTicks++;
    m_nExpandTicks--;
    BOOL bDone = TRUE;

    // Expanded, it waits for its animation's end.
    if (m_bAnimation == TRUE) {
        if (m_nExpandTicks == 0) {
            if (m_animation.IsEndOfSequence(FALSE)) {
                RemoveFromArea();
                return;
            }

            m_nExpandTicks++;
            m_animation.FrameAdvance();
            return;
        }
    } else if (m_nExpandTicks == 0) {
        RemoveFromArea();
        return;
    }

    LONG nGridSize = (2 * m_nGridY + 1) * (2 * m_nGridX + 1);
    memset(m_pGrid, 0, nGridSize);

    for (SHORT nParticle = 0; nParticle < 4 * (m_nArc1 + m_nArc2); nParticle++) {
        if (m_pStates[nParticle] == STATE_STOPPED) {
            continue;
        }

        Particle& particle = m_pParticles[nParticle];
        particle.x += particle.vx;
        particle.y += particle.vy;

        CPoint ptGrid(((particle.x >> CGameSprite::EXACT_SCALE) + m_pos.x) / CPathSearch::GRID_SQUARE_SIZEX,
            (((particle.y * 3) / 4 >> CGameSprite::EXACT_SCALE) + m_pos.y) / CPathSearch::GRID_SQUARE_SIZEY);

        // NOTE: The original checks no bounds, so a particle past the area's
        // left or top edge reads the search map before its start. Ours
        // takes such a square for a wall (P4-10).
        SHORT nTableIndex;
        BYTE nCost = CPathSearch::COST_IMPASSABLE;
        if (ptGrid.x >= 0 && ptGrid.y >= 0) {
            nCost = m_pArea->m_search.GetLOSCost(ptGrid, m_terrainTable, nTableIndex, FALSE);
        }

        if (nCost == CPathSearch::COST_IMPASSABLE) {
            if (m_nWallMode == WALL_STOP) {
                m_pStates[nParticle] = STATE_STOPPED;
            }

            if (m_nWallMode != WALL_BOUNCE) {
                if (m_pStates[nParticle] == STATE_MOVING) {
                    bDone = FALSE;
                }
                continue;
            }

            // Bounce off the wall.
            LONG nOldGridY = (((particle.y - particle.vy) * 3) / 4 >> CGameSprite::EXACT_SCALE) + m_pos.y;
            nOldGridY /= CPathSearch::GRID_SQUARE_SIZEY;
            LONG nOldGridX = ((particle.x - particle.vx) >> CGameSprite::EXACT_SCALE) + m_pos.x;
            nOldGridX /= CPathSearch::GRID_SQUARE_SIZEX;

            if (nOldGridX != ptGrid.x) {
                particle.x -= 2 * particle.vx;
                particle.vx = -particle.vx;
            }

            if (nOldGridY != ptGrid.y) {
                particle.y -= 2 * particle.vy;
                particle.vy = -particle.vy;
            }
        }

        LONG nCell = ((particle.y / CPathSearch::GRID_SQUARE_SIZEX >> CGameSprite::EXACT_SCALE) + m_nGridY) * (2 * m_nGridX + 1)
            + (particle.x / CPathSearch::GRID_SQUARE_SIZEX >> CGameSprite::EXACT_SCALE)
            + m_nGridX;

        // Now and then a lingering piece of the third description, the
        // likelier the longer the explosion lasts and in its last tick.
        if (!m_explosion3.m_animation.empty()) {
            if (m_nLingerCooldown < 1) {
                LONG nChance = m_nTicks * m_nTicks / m_explosion3.field_40 + m_explosion3.field_38;
                if (m_nExpandTicks < 2) {
                    nChance *= 2;
                }

                CPoint ptExact((m_pos.x << CGameSprite::EXACT_SCALE) + particle.x,
                    (m_pos.y << CGameSprite::EXACT_SCALE) + (particle.y * 3) / 4);

                LONG nRange = m_explosion3.GetChanceRange();
                LONG nRoll = nRange != 0 ? rand() % nRange : 0;

                CProjectileAreaPoints* pPoints = m_explosion3.m_pPoints;
                if (nRoll > nChance) {
                    pPoints->m_points.push_back(CProjectileAreaPoints::Point(ptExact.x, ptExact.y, FALSE));
                } else {
                    pPoints->m_points.push_back(CProjectileAreaPoints::Point(ptExact.x, ptExact.y, TRUE));
                    m_explosion3.field_30 = static_cast<LONG>(pPoints->m_points.size()) - 1;

                    new CGameObject84F14C(m_explosion3,
                        m_pArea,
                        ptExact,
                        0,
                        CPoint(0, 0),
                        static_cast<WORD>((m_nExpandTicks << 1) + m_nDuration),
                        0,
                        m_nWallMode);

                    m_nLingerCooldown = m_explosion3.field_3C;
                }
            } else {
                m_nLingerCooldown--;
            }
        }

        // NOTE: The original does not check the square against the grid; a
        // particle faster than 16 pixels a tick can pass the grid's edge and
        // write past it. Ours counts such a square as entered (P4-10).
        BYTE nEntered = 1;
        if (nCell >= 0 && nCell < nGridSize) {
            nEntered = m_pGrid[nCell]++;
        }

        if (nEntered == 0 && m_pStates[nParticle] == STATE_MOVING) {
            // The first particle in a square throws a flying piece of the
            // second description, as many as it allows.
            if (!m_explosion2.m_animation.empty() && ++m_nPieces <= m_explosion2.field_2E) {
                new CGameObject84F14C(m_explosion2,
                    m_pArea,
                    CPoint((m_pos.x << CGameSprite::EXACT_SCALE) + particle.x,
                        (m_pos.y << CGameSprite::EXACT_SCALE) + (particle.y * 3) / 4),
                    0,
                    CPoint(particle.vx, (particle.vy * 3) / 4),
                    m_nExpandTicks,
                    0,
                    m_nWallMode);
            }

            m_pStates[nParticle] = STATE_THROWN;
        } else if (m_pStates[nParticle] == STATE_MOVING) {
            bDone = FALSE;
        }
    }

    if (bDone) {
        if (m_bAnimation == TRUE) {
            if (m_animation.IsEndOfSequence(FALSE)) {
                // NOTE: The original reads +0x2A0 of the deleted explosion
                // next (0x56D6EF); ours returns (P4-10).
                RemoveFromArea();
                return;
            }
        } else {
            RemoveFromArea();
            return;
        }
    }

    if (m_bAnimation == TRUE && !m_animation.IsEndOfSequence(FALSE)) {
        m_animation.FrameAdvance();
    }
}

// 0x56D730
void CGameObject84F0DC::Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameFireball.cpp
    // __LINE__: 396
    UTIL_ASSERT(pVidMode != NULL);

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CPoint ptTile(min(max(m_pos.x, 0), pInfinity->nAreaX - 1),
        min(max(m_pos.y, 0), pInfinity->nAreaY - 1));
    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(ptTile))) {
        return;
    }

    if (!m_bAnimation) {
        return;
    }

    if (m_animation.IsEndOfSequence(FALSE)) {
        return;
    }

    CPoint ptReference;
    CSize frameSize;
    m_animation.GetCurrentCenterPoint(ptReference, FALSE);
    m_animation.GetCurrentFrameSize(frameSize, FALSE);

    CRect rFx(0, 0, frameSize.cx, frameSize.cy);

    CPoint ptPos(m_pos.x, m_pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y);

    LONG nLeft = ptPos.x - ptReference.x;
    LONG nTop = m_posZ - ptReference.y + ptPos.y;
    LONG nRight = rFx.right - rFx.left + nLeft;

    DWORD dwFlags = m_visualEffect.m_dwFlags | CInfinity::FXPREP_COPYFROMBACK;

    pInfinity->FXPrep(rFx, dwFlags, nSurface, ptPos, ptReference);
    if (pInfinity->FXLock(rFx, dwFlags)) {
        pInfinity->FXRender(&m_animation, ptReference.x, ptReference.y, m_visualEffect.m_dwFlags, 0);

        CRect rClip(nLeft,
            nTop - m_posZ,
            nRight,
            rFx.bottom - rFx.top + nTop - m_posZ);
        pInfinity->FXRenderClippingPolys(ptPos.x, ptPos.y - m_posZ, m_posZ, ptReference, rClip, FALSE, m_visualEffect.m_dwFlags);
        pInfinity->FXUnlock(dwFlags, NULL, CPoint(0, 0));
        pInfinity->FXBltFrom(nSurface, rFx, ptPos.x, ptPos.y, ptReference.x, ptReference.y, m_visualEffect.m_dwFlags | 0x1);
    }
}

// 0x56D9B0
void CGameObject84F0DC::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameFireball.cpp
        // __LINE__: 493
        UTIL_ASSERT(FALSE);
        return;
    }

    delete this;
}
