#pragma once
#include <vector>
#include <string>
#include <unordered_map>

#include "unity.h"

struct MapMeta
{
	int map_id;
	std::string map_name;
	std::string version;
};

struct NavMeshData
{
	std::string description;
	std::vector<Vector3> vertices;
	std::vector<int> indices;
};

struct GimmickData
{
	int gimmick_id;
	std::string type;
	Vector3 position;
	float rotation_y;
	std::unordered_map<std::string, float> properties;
};

struct StructureData
{
	std::string type;
	Vector3 position;
	Vector3 scale;
	float rotation_y;
};

struct MapExportData
{
	MapMeta meta;
	NavMeshData nav;
	std::vector<StructureData> structures;
	std::vector<GimmickData> gimmicks;
};