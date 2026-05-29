#include "Packet.h" 
#include "NavMeshManager.h"
#include <stdio.h>

struct TileCacheSetHeader 
{
    int magic;
    int version;
    int numTiles;
    dtNavMeshParams meshParams;
    dtTileCacheParams cacheParams;
};

struct TileCacheTileHeader 
{
    dtCompressedTileRef tileRef;
    int dataSize;
};

static const int TILECACHESET_MAGIC = 'T' << 24 | 'S' << 16 | 'E' << 8 | 'T';
static const int TILECACHESET_VERSION = 1;

static const int MAX_NODE = 2048;

static const int INCLUDE_FLAGS = 0xffff;
static const int EXCLUDE_FLAGS = 0;

static enum AreaID
{
    NONE = 0,
    Road = 1,
    Bush = 2,
};

NavMeshManager::~NavMeshManager()
{
    if (m_TileCache) dtFreeTileCache(m_TileCache);
    if (m_NavQuery) dtFreeNavMeshQuery(m_NavQuery);
    if (m_NavMesh) dtFreeNavMesh(m_NavMesh);
}

bool NavMeshManager::Init(const std::string& fileName)
{
    printf("[TileCache Init] 맵 파일 로드 (%s)\n", fileName.c_str());

    FILE* fp = nullptr;
    fopen_s(&fp, fileName.c_str(), "rb");
    if (!fp) return false;
    
    printf("[TileCache Init] 파일 열기 성공\n");

    // 헤더 파싱 (TSET)
    TileCacheSetHeader header;
    fread(&header, sizeof(TileCacheSetHeader), 1, fp);

    if (header.magic != TILECACHESET_MAGIC || header.version != TILECACHESET_VERSION)
    {
        printf("[TileCache Init ERROR] 맵 파일 버전 / 매직넘버 다름\n");
        fclose(fp);
        return false;
    }

    // NavMesh 할당
    m_NavMesh = dtAllocNavMesh();
    if (!m_NavMesh)
    {
        printf("[TileCache Init ERROR] dtAllocNavMesh 실패\n");
        fclose(fp);
        return false;
    }
    m_NavMesh->init(&header.meshParams);

    // TileCache 할당 및 초기화
    m_TileCache = dtAllocTileCache();
    m_TileCache->init(&header.cacheParams, &m_talloc, &m_tcomp, &m_tmproc);

    // 타일 데이터 읽어서 캐시에 넣기
    for (int i = 0; i < header.numTiles; ++i)
    {
        TileCacheTileHeader tileHeader;
        fread(&tileHeader, sizeof(TileCacheTileHeader), 1, fp);

        if (!tileHeader.tileRef || !tileHeader.dataSize) break;

        unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
        fread(data, tileHeader.dataSize, 1, fp);

        dtCompressedTileRef tile = 0;
        m_TileCache->addTile(data, tileHeader.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
    }
    fclose(fp);

    for (int i = 0; i < m_TileCache->getTileCount(); ++i)
    {
        const dtCompressedTile* tile = m_TileCache->getTile(i);
        if (tile && tile->header)
        {
            dtCompressedTileRef ref = m_TileCache->getTileRef(tile);
            m_TileCache->buildNavMeshTile(ref, m_NavMesh);
        }
    }
    printf("[TileCache Init] 타일 데이터 %d개 로드 \n", header.numTiles);

    // Query 할당
    m_NavQuery = dtAllocNavMeshQuery();
    if (!m_NavQuery)
    {
        printf("[TileCache Init ERROR] dtAllocNavMeshQuery 실패\n");
        return false;
    }

    dtStatus status = m_NavQuery->init(m_NavMesh, MAX_NODE);
    if (dtStatusFailed(status))
    {
        printf("[TileCache Init ERROR] m_NavQuery->init 실패 : %u\n", status);
        return false;
    }

    m_Filter.setIncludeFlags(INCLUDE_FLAGS);
    m_Filter.setExcludeFlags(EXCLUDE_FLAGS);

    printf("[TileCache Init SUCCESS] TileCache 초기화\n");
    return true;
}

bool NavMeshManager::GetValidMovePosition(const Vector3& startPos, const Vector3& targetPos, Vector3& realPos)
{
    if (!m_NavQuery) return false;

    float start[3] = { -startPos.x, startPos.y, startPos.z };
    float target[3] = { -targetPos.x, targetPos.y, targetPos.z };
    float result[3];

    dtPolyRef startRef;
    float nearStart[3];

    float searchExtents[3] = { 0.5f, 10.5f, 0.5f };

    m_NavQuery->findNearestPoly(start, searchExtents, &m_Filter, &startRef, nearStart);

    if (!startRef) return false;

    dtPolyRef visited[16];
    int visitedCount = 0;

    m_NavQuery->moveAlongSurface(startRef, nearStart, target, &m_Filter, result, visited, &visitedCount, 16);

    // 이동 위치에서 y값 다시 탐색
    dtPolyRef endRef;
    float nearEnd[3];
    m_NavQuery->findNearestPoly(result, searchExtents, &m_Filter, &endRef, nearEnd);

    if (endRef)
    {
        result[1] = nearEnd[1]; // 정확한 경사로 바닥 높이 적용
    }


    realPos = { -result[0], result[1], result[2] };

    return true;
}

//bool NavMeshManager::GetValidMovePosition(const Vector3& startPos, const Vector3& targetPos, Vector3& realPos)
//{
//    if (!m_NavQuery) return false;
//
//    // X는 그대로, Z축만 반전 (유니티 +Z <-> 서버 -Z)
//    float start[3] = { startPos.x, startPos.y, -startPos.z };
//    float target[3] = { targetPos.x, targetPos.y, -targetPos.z };
//    float result[3];
//
//    dtPolyRef startRef;
//    float nearStart[3];
//    float searchExtents[3] = { 1.0f, 2.0f, 1.0f }; // 탐색 범위 넉넉히
//
//    m_NavQuery->findNearestPoly(start, searchExtents, &m_Filter, &startRef, nearStart);
//    if (!startRef) return false;
//
//    dtPolyRef visited[16];
//    int visitedCount = 0;
//    m_NavQuery->moveAlongSurface(startRef, nearStart, target, &m_Filter, result, visited, &visitedCount, 16);
//
//    dtPolyRef endRef;
//    float nearEnd[3];
//    m_NavQuery->findNearestPoly(result, searchExtents, &m_Filter, &endRef, nearEnd);
//    if (endRef) result[1] = nearEnd[1];
//
//    // 결과값 복원 시에도 Z만 다시 반전
//    realPos = { result[0], result[1], -result[2] };
//
//    return true;
//}


bool NavMeshManager::IsInBush(const Vector3& pos)
{
    if (!m_NavQuery) return false;

    float p[3] = { pos.x, pos.y, pos.z };
    dtPolyRef ref;
    float nearest[3];

    m_NavQuery->findNearestPoly(p, m_Extents, &m_Filter, &ref, nearest);
    if (!ref) return false;

    unsigned char areaID;
    m_NavMesh->getPolyArea(ref, &areaID);

    return (areaID == AreaID::Bush);
}

dtObstacleRef NavMeshManager::AddObstacle(const Vector3& pos, const float radius, const float height)
{
    if (!m_TileCache) return 0;
    dtObstacleRef ref = 0;
    float p[3] = { pos.x, pos.y, pos.z };
    m_TileCache->addObstacle(p, radius, height, &ref);
    return ref;
}

void NavMeshManager::RemoveObstacle(dtObstacleRef ref)
{
    if (!m_TileCache || ref == 0) return;
    m_TileCache->removeObstacle(ref);
}

void NavMeshManager::UpdateTileCache(float dt)
{
    if (m_TileCache) m_TileCache->update(dt, m_NavMesh);
}