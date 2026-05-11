#pragma once

#include "Mapdata.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <rapidjson\document.h>
#include <rapidjson\istreamwrapper.h>

class MapDataLoader
{
private:

public:
	MapDataLoader() = default;
	~MapDataLoader();

	static bool LoadMapFromJson(const std::string& path, MapExportData& outData)
	{
		std::ifstream ifs(path, std::ios::ate | std::ios::binary);
		if (!ifs.is_open()) return false;

		size_t fileSize = (size_t)ifs.tellg();
		ifs.seekg(0, std::ios::beg);

		std::vector<char> buffer(fileSize + 1);
		ifs.read(buffer.data(), fileSize);
		buffer[fileSize] = '\0';

		// In-situ ÆÄ½Ì
		rapidjson::Document doc;
		doc.ParseInsitu(buffer.data());

		if (doc.HasParseError()) return false;

		//meta
		if (doc.HasMember("meta") && doc["meta"].IsObject())
		{
			const auto& meta = doc["meta"];
			outData.meta.map_id = meta["map_id"].GetInt();
			outData.meta.map_name = meta["map_name"].GetString();
			outData.meta.version = meta["version"].GetString();
		}

		//nav
		if (doc.HasMember("nav") && doc["nav"].IsObject())
		{
			const auto& nav = doc["nav"];

			if (nav.HasMember("vertices") && nav["vertices"].IsArray())
			{
				for (const auto& v : nav["vertices"].GetArray())
				{
					outData.nav.vertices.push_back({ v["x"].GetFloat(),
														v["y"].GetFloat(),
														v["z"].GetFloat()});
				}
			}

			if (nav.HasMember("indices") && nav["indices"].IsArray())
			{
				for (const auto& i : nav["indices"].GetArray())
				{
					outData.nav.indices.push_back(i.GetInt());
				}
			}
		}

		//StructureData
		if (doc.HasMember("structures") && doc["structures"].IsArray())
		{
			for (const auto& s : doc["structures"].GetArray())
			{
				StructureData sd;
				sd.position = { s["position"]["x"].GetFloat(),
								s["position"]["y"].GetFloat(),
								s["position"]["z"].GetFloat() };
				
				sd.scale = { s["scale"]["x"].GetFloat(),
								s["scale"]["y"].GetFloat(),
								s["scale"]["z"].GetFloat() };

				sd.rotation_y = s["rotation_y"].GetFloat();
				sd.type = s["type"].GetString();
				outData.structures.push_back(sd);
			}
		}

		//gimmick
		if (doc.HasMember("gimmicks") && doc["gimmicks"].IsArray())
		{
			for (const auto& g : doc["gimmicks"].GetArray())
			{
				GimmickData gd;
				gd.gimmick_id = g["gimmick_id"].GetInt();
				gd.type = g["type"].GetString();
				gd.position = { g["position"]["x"].GetFloat(),
								g["position"]["y"].GetFloat(),
								g["position"]["z"].GetFloat() };

				gd.rotation_y = g["rotation_y"].GetFloat();
				
				if (g.HasMember("properties") && g["properties"].IsObject())
				{
					const auto& p = g["properties"].GetObject();
					for (auto itr = p.begin(); itr != p.end(); ++itr)
					{
						gd.properties[itr->name.GetString()] = itr->value.GetFloat();
					}
				}

				outData.gimmicks.push_back(gd);
			}
		}
		return true;
	}
};