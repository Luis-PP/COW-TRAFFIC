// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "box2d/collision.h"
#include "box2d/id.h"
#include "box2d/types.h"

// todo this include is slow
#include "TaskScheduler.h"

#include <imgui.h>
#include <stdlib.h>

#define ARRAY_COUNT(A) (int)(sizeof(A) / sizeof(A[0]))

struct Settings;

#ifdef NDEBUG
constexpr bool g_sampleDebug = false;
#else
constexpr bool g_sampleDebug = true;
#endif

constexpr int32_t k_maxContactPoints = 12 * 2048;

struct ContactPoint
{
	b2ShapeId shapeIdA;
	b2ShapeId shapeIdB;
	b2Vec2 normal;
	b2Vec2 position;
	bool persisted;
	float normalImpulse;
	float tangentImpulse;
	float separation;
	int32_t constraintIndex;
	int32_t color;
};

class SampleTask : public enki::ITaskSet
{
public:
	SampleTask() = default;

	void ExecuteRange(enki::TaskSetPartition range, uint32_t threadIndex) override
	{
		m_task(range.start, range.end, threadIndex, m_taskContext);
	}

	b2TaskCallback *m_task = nullptr;
	void *m_taskContext = nullptr;
};

constexpr int32_t maxTasks = 64;
constexpr int32_t maxThreads = 64;

struct SampleFunctionalArea
{
	int type;		 // cubicle, feeder, drinker, etc
	int orientation; // horizontal or vertical
	float x;		 // center
	float y;		 // center
};

class Sample
{
public:
	explicit Sample(Settings &settings);
	virtual ~Sample();

	void DrawTitle(const char *string);
	virtual void Step(Settings &settings);
	virtual void UpdateUI()
	{
	}
	virtual void ShowTools()
	{
	}
	virtual void Keyboard(int)
	{
	}
	virtual void MouseDown(b2Vec2 p, int button, int mod);
	virtual void MouseUp(b2Vec2 p, int button);
	virtual void MouseMove(b2Vec2 p);

	void ResetProfile();
	void ReConstructLayout();
	void ShiftOrigin(b2Vec2 newOrigin);

	friend class DestructionListener;
	friend class BoundaryListener;
	friend class ContactListener;

	enki::TaskScheduler m_scheduler;
	SampleTask m_tasks[maxTasks];
	int32_t m_taskCount;
	int m_threadCount;

	b2BodyId m_groundBodyId;

	// DestructionListener m_destructionListener;
	int32_t m_textLine;
	b2WorldId m_worldId;
	b2JointId m_mouseJointId;
	int32_t m_stepCount;
	int32_t m_textIncrement;
	b2Profile m_maxProfile;
	b2Profile m_totalProfile;

	// --
	std::vector<SampleFunctionalArea> layout;
	std::pair<int, int> corner_layout;

	//

	int number_of_cows;
	int evade_probability;

	//

	std::vector<b2AABB> cow_map;
};

typedef Sample *
SampleCreateFcn(Settings &settings);

int RegisterSample(const char *category, const char *name, SampleCreateFcn *fcn);

struct SampleEntry
{
	const char *category;
	const char *name;
	SampleCreateFcn *createFcn;
};

#define MAX_SAMPLES 256
extern SampleEntry g_sampleEntries[MAX_SAMPLES];
extern int g_sampleCount;

#define RAND_LIMIT 32767

/// Random integer in range [lo, hi)
inline float RandomInt(int lo, int hi)
{
	return lo + rand() % (hi - lo);
}

/// Random number in range [-1,1]
inline float RandomFloat()
{
	float r = (float)(rand() & (RAND_LIMIT));
	r /= RAND_LIMIT;
	r = 2.0f * r - 1.0f;
	return r;
}

/// Random floating point number in range [lo, hi]
inline float RandomFloat(float lo, float hi)
{
	float r = (float)(rand() & (RAND_LIMIT));
	r /= RAND_LIMIT;
	r = (hi - lo) * r + lo;
	return r;
}

/// Random vector with coordinates in range [lo, hi]
inline b2Vec2 RandomVec2(float lo, float hi)
{
	b2Vec2 v;
	v.x = RandomFloat(lo, hi);
	v.y = RandomFloat(lo, hi);
	return v;
}

b2Polygon RandomPolygon(float extent);
