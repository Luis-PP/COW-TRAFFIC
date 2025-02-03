#include "functional_area.h"

#include "sample.h"
#include "mapmaker.h"

#include "box2d/box2d.h"
#include "box2d/math_functions.h"

#include <assert.h>
#include <iostream>

MapMaker m;

FunctionalArea::FunctionalArea()
{
	bodyId = b2_nullBodyId;
	m_isSpawned = false;
}

void FunctionalArea::Spawn(b2WorldId worldId, float x, float y, int type, int orientation, float scale, float frictionTorque, float hertz,
						   int groupIndex, void *userData)
{
	assert(m_isSpawned == false);

	// from grid to world
	x *= 24.0f;
	y *= 24.0f;
	x += 12.0f;
	y += 12.0f;
	if (orientation == 1) // vertical
		y += 12.0f;
	if (orientation == 2) // horizontal
		x += 12.0f;

	aabb = m.ConvertToABB(x, y, orientation);

	b2BodyDef bodyDef = b2DefaultBodyDef();
	bodyDef.type = b2_staticBody;
	// bodyDef.sleepThreshold = 0.1f;
	bodyDef.userData = userData;
	bodyDef.position = {x, y};
	bodyId = b2CreateBody(worldId, &bodyDef);

	b2ShapeDef shapeDef = b2DefaultShapeDef();
	shapeDef.density = 10.0f;
	shapeDef.friction = 0.2f;
	shapeDef.customColor = color_arr[type];

	// shapeDef.filter.groupIndex = -groupIndex;
	// shapeDef.filter.maskBits = 1;

	b2Polygon box = b2MakeBox(orientation_arr[orientation].x, orientation_arr[orientation].y);
	b2CreatePolygonShape(bodyId, &shapeDef, &box);

	m_isSpawned = true;
}

void FunctionalArea::Despawn()
{
	assert(m_isSpawned == true);

	if (B2_IS_NON_NULL(bodyId))
	{
		b2DestroyBody(bodyId);
		bodyId = b2_nullBodyId;
	}

	m_isSpawned = false;
}
