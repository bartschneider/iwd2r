#include "render/api.h"

#include "CVidCell.h"
#include "CVidPoly.h"

namespace render {

namespace detail {
    Backend* active = nullptr;
}

Backend* Set(Backend* backend)
{
    Backend* previous = detail::active;
    detail::active = backend;
    return previous;
}

BOOL CellHandled(CVidCell& cell)
{
    if (cell.m_pFrame != NULL) {
        cell.pRes->Release();
        cell.m_pFrame = NULL;
    }
    return TRUE;
}

BOOL CellHandled(CVidCell& cell, BOOLEAN bDemanded)
{
    if (cell.m_pFrame != NULL) {
        cell.m_pFrame = NULL;
        if (!bDemanded)
            cell.pRes->Release();
    }
    return TRUE;
}

std::vector<POINT> Vertices(const CVidPoly& poly)
{
    if (poly.m_nVertices <= 0)
        return {};
    if (poly.m_pVertices == nullptr)
        return std::vector<POINT>(poly.m_nVertices, POINT { 0, 0 });
    std::vector<POINT> points;
    points.reserve(poly.m_nVertices);
    for (int index = 0; index < poly.m_nVertices; index++)
        points.push_back(POINT { poly.m_pVertices[index].x, poly.m_pVertices[index].y });
    return points;
}

std::vector<POINT> Minus(const POINT* pts, int n, POINT offset)
{
    std::vector<POINT> points;
    if (pts == nullptr || n <= 0)
        return points;
    points.reserve(n);
    for (int index = 0; index < n; index++)
        points.push_back(POINT { pts[index].x - offset.x, pts[index].y - offset.y });
    return points;
}

}
