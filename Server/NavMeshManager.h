#pragma once
#include <string>
#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourNavMeshBuilder.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "fastlz.h"

struct Vector3;

// TileCache 필수 인터페이스
struct LinearAllocator : public dtTileCacheAlloc 
{
    void* resetBase;
    LinearAllocator() { resetBase = dtAlloc(32000, DT_ALLOC_TEMP); }
    ~LinearAllocator() { dtFree(resetBase); }

    void* alloc(const size_t size) override { return dtAlloc(size, DT_ALLOC_TEMP); }
    void free(void* ptr) override { dtFree(ptr); }
    void reset() override {}
};

struct FastLZCompressor : public dtTileCacheCompressor 
{
    int maxCompressedSize(const int bufferSize) override { return (int)(bufferSize * 1.05f); }
    dtStatus compress(const unsigned char* buffer, const int bufferSize,
        unsigned char* compressed, const int maxCompressedSize, int* compressedSize) override 
    {
        *compressedSize = fastlz_compress((const void* const)buffer, bufferSize, compressed);
        return DT_SUCCESS;
    }

    dtStatus decompress(const unsigned char* compressed, const int compressedSize,
        unsigned char* buffer, const int maxBufferSize, int* bufferSize) override 
    {
        *bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
        return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
    }
};

struct MeshProcess : public dtTileCacheMeshProcess 
{
    void process(struct dtNavMeshCreateParams* params, unsigned char* polyAreas, unsigned short* polyFlags) override 
    {
        for (int i = 0; i < params->polyCount; ++i) 
        {
            polyAreas[i] = 0;
            polyFlags[i] = 1; // 무조건 걷기 가능하게 강제 플래그
        }
    }
};

class NavMeshManager
{
public:
    static NavMeshManager* GetInstance() 
    {
        static NavMeshManager instance;
        return &instance;
    }
    ~NavMeshManager();

    bool Init(const std::string& fileName);
    bool GetValidMovePosition(const Vector3& startPos, const Vector3& targetPos, Vector3& realPos);
    bool IsInBush(const Vector3& pos);

    // 동적 장애물 함수 추가
    dtObstacleRef AddObstacle(const Vector3& pos, const float radius, const float height);
    void RemoveObstacle(dtObstacleRef ref);
    void UpdateTileCache(float dt);

private:
    dtNavMesh* m_NavMesh = nullptr;
    dtNavMeshQuery* m_NavQuery = nullptr;
    dtTileCache* m_TileCache = nullptr; // 핵심!

    LinearAllocator m_talloc;
    FastLZCompressor m_tcomp;
    MeshProcess m_tmproc;

    dtQueryFilter m_Filter;
    float m_Extents[3] = { 2.0f, 4.0f, 2.0f };
};