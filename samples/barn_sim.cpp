// SPDX-FileCopyrightText: 2022 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw.h"
#include "functional_area.h"
#include "cow.h"
#include "sample.h"
#include "settings.h"
#include "mapmaker.h"
#include "rrt.h"

#include "box2d/box2d.h"
#include "box2d/math_functions.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>

// generate random integer
int randomInt() { return rand(); }

// generate random integer in a range [Min , Max)
int randomInt(int a, int b)
{
	if (a > b)
		return randomInt(b, a);
	if (a == b)
		return a;
	return a + (rand() % (b - a));
}

// generate random fraction

float randomFloat()
{
	return (float)(rand()) / (float)(RAND_MAX);
}
// generate random float in a range
float randomFloat(int a, int b)
{
	if (a > b)
		return randomFloat(b, a);
	if (a == b)
		return a;

	return (float)randomInt(a, b) + randomFloat();
}
// std::vector<SampleFunctionalArea> Cow::cow_layout;
RRT rrt;
// Note: resetting the scene is non-deterministic because the world uses freelists
class Barn : public Sample
{
public:
	enum
	{
		e_maxColumns = 100,
		e_maxRows = 100,
	};

	explicit Barn(Settings &settings)
		: Sample(settings)
	{
		if (settings.restart == false)
		{
			g_camera.m_center = {30.0f, 275.0f};
			g_camera.m_zoom = 630.0f;
		}

		settings.drawJoints = false;
		CreateWorld();
	}

	void CreateWorld()
	{
		b2World_SetGravity(m_worldId, b2Vec2_zero);
		CreateLayout();
	}

	void CreateLayout()
	{
		// Destoy barns before create
		for (int i = 0; i < e_maxRows * e_maxColumns; ++i)
		{
			if (B2_IS_NULL(m_functinoal_areas[i].bodyId))
			{
				continue;
			}

			if (m_functinoal_areas[i].m_isSpawned)
			{
				m_functinoal_areas[i].Despawn();
			}
		}

		// Destroy map before create
		map.DestroyMaps();

		// Destroy cows before create
		for (int i = 0; i < e_maxRows * e_maxColumns; ++i)
		{
			if (B2_IS_NULL(m_cows[i].bodyId))
			{
				continue;
			}

			if (m_cows[i].m_isSpawned)
			{
				m_cows[i].Despawn();
			}
		}

		// Cow::cow_layout = layout;

		// // Create Chain
		b2BodyDef bodyDef = b2DefaultBodyDef();
		b2BodyId groundId = b2CreateBody(m_worldId, &bodyDef);
		b2ShapeDef shapeDef = b2DefaultShapeDef();

		int max_x = corner_layout.first * 24; // times 24 to fit the world
		int max_y = corner_layout.second * 24;

		b2Vec2 points[4] = {{0.0f, float(max_y)}, {float(max_x), float(max_y)}, {float(max_x), 0.0f}, {0.0f, 0.0}};
		b2ChainDef chainDef = b2DefaultChainDef();
		chainDef.points = points;
		chainDef.count = 4;
		chainDef.isLoop = true;
		b2CreateChain(groundId, &chainDef);

		// Create Funactional areas
		int index = 0;
		for (SampleFunctionalArea func_are : layout)
		{
			m_functinoal_areas[index].Spawn(m_worldId, func_are.x, func_are.y, func_are.type, func_are.orientation, 0.05f, 0.0f, 0.0f, index + 1, nullptr);
			map.cow_aabbs.push_back(m_functinoal_areas[index].aabb);
			index += 1;
		}

		// Create cow map
		map.CreateCowMap();
		cow_map = map.cow_map;
		map.layout = layout;
		map.corner_layout = corner_layout;
		map.LayoutToGrid();


		CreateCows();
	}

	void CreateCows()
	{
		srand(36);							  // random seed
		int max_x = corner_layout.first * 24; // times 24 to fit the world
		int max_y = corner_layout.second * 24;

		// Destoy cows before create
		for (int i = 0; i < e_maxRows * e_maxColumns; ++i)
		{
			if (B2_IS_NULL(m_cows[i].bodyId))
			{
				continue;
			}

			if (m_cows[i].m_isSpawned)
			{
				m_cows[i].Despawn();
			}
		}

		// Create cows
		int index = 0;
		for (int i = 0; i < number_of_cows; i++)
		{
			b2Vec2 point;
			int inflate = 42;
			bool traped = true;
			while (traped) // check cows start free
			{
				point = b2Vec2{rand() % max_x, rand() % max_y};
				traped = false;
				for (const auto &obstacle : map.cow_map)
				{
					if ((((obstacle.lowerBound.x - inflate) <= point.x) && (point.x <= (obstacle.upperBound.x + inflate))) &&
						(((obstacle.lowerBound.y - inflate) <= point.y) && (point.y <= (obstacle.upperBound.y + inflate))))
					{
						traped = true;
						break;
					}
				}
			}
			float cow_orientation = randomFloat(0, 360);
			m_cows[index].Spawn(m_worldId, point.x, point.y, cow_orientation, 0.05f, 0.0f, 0.0f, index + 1, nullptr);
			m_cows[index].cow_layout = layout;
			m_cows[index].cow_map = map.cow_map;

			m_cows[index].max_b_area = b2Vec2{max_x, max_x};
			
			index += 1;
		}
	}

	void ShowTools() override
	{
		float height = 80.0f;
		ImGui::SetNextWindowPos(ImVec2(10.0f, g_camera.m_height - height - 50.0f), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(220.0f, height));
		ImGui::Begin("Barn", nullptr, ImGuiWindowFlags_NoResize);
		bool changed_scene = false;
		bool changed_herd = false;
		changed_scene = changed_scene || ImGui::Button("Reset Scene");
		changed_herd = changed_herd || ImGui::Button("Reset Cows");
		if (changed_scene)
		{
			CreateLayout();
		}
		if (changed_herd)
		{
			CreateLayout();

			// CreateCows();
		}
		ImGui::End();
	}

	void Step(Settings &settings) override
	{
		for (int i = 0; i < e_maxRows * e_maxColumns; ++i)
		{
			if (B2_IS_NULL(m_cows[i].bodyId))
			{
				continue;
			}

			if (m_cows[i].m_isSpawned)
			{
				m_cows[i].Routine();
			}
		}
		Sample::Step(settings);
	}

	static Sample *Create(Settings &settings)
	{
		return new Barn(settings);
	}

	FunctionalArea m_functinoal_areas[e_maxRows * e_maxColumns];
	Cow m_cows[e_maxRows * e_maxColumns];
	MapMaker map;
	// MapMaker map(layout);
	// int m_columnCount;
	// int m_rowCount;
};

static int barn = RegisterSample("Barn", "Barn", Barn::Create);
