#pragma once

#include "box2d/types.h"

#include <vector>

const std::vector<b2Vec2> orientation_arr = {{12, 12}, {12, 24}, {24, 12}}; //
// const char *type_arr_[] = {"Cubicle", "Milking Robot", "Feeder", "Concentrate", "Drinker", "Docking station", "Obstacle", "Empty"};
const b2HexColor color_arr[] = {b2_colorSaddleBrown, b2_colorRed, b2_colorPaleGreen, b2_colorDarkOliveGreen, b2_colorDodgerBlue, b2_colorOrange, b2_colorSlateGray};

class FunctionalArea
{
public:
	FunctionalArea();
	void Spawn(b2WorldId worldId, float x, float y, int type, int orientation, float scale, float frictionTorque, float hertz,
			   int groupIndex, void *userData);
	void Despawn();
	b2BodyId bodyId;
	bool m_isSpawned;

	b2AABB aabb;
};
