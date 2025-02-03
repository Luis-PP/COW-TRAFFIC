#pragma once

#include "rrt.h"
#include "sample.h"

#include "box2d/types.h"
// Include the string library
#include <math.h>
#include <iostream>
#include <map>
#include <vector>
#include <string>


enum cow_states {
    cow_starting,
    cow_idling,
    cow_traslating,
    cow_in_activity,
};

enum cow_definition
{
    cow_height = 14,
    cow_weight = 2,
    cow_color = b2_colorFloralWhite,
    cow_leg_base = 10,
    cow_desired_speed = 50,
    cow_max_speed = 30,
    cow_max_steering_angle = 1,
    cow_waypoint_threshold = 48,
};

struct
{
    float k_p = -10.0;
    float k_v = 0.5;
    float k_h = 4.0;
    float zeta = 1.0;
} cow_params;

struct cow_pose
{
    b2Vec2 position;
    float angle;
};

class Cow : public RRT
{
public:
    Cow();
    void Spawn(b2WorldId worldId, float x, float y, float orientation, float scale, float frictionTorque, float hertz,
               int groupIndex, void *userData);
    void Despawn();
    b2BodyId bodyId;
    bool m_isSpawned;
    // std::vector<float, float> start;
    // std::vector<float, float> end;
    void Routine();
    b2Vec2 cow_head = b2Vec2({10.5f, 1.0f});
    std::vector<b2Vec2> cow_path;
    b2Vec2 max_b_area;

    struct
    {
        cow_states state;
        b2Vec2 start;
        b2Vec2 end;
        b2Vec2 waypoint;
        float confidence;
        int waypoint_index;
        float speed;
        float steering_angle;
        int current_activity;
        SampleFunctionalArea current_functional_area;
    } cow_var;

    void Cow_move_model(cow_pose);
    void Cow_control_to_point(cow_pose);
    // void Find_path(RRT rrt);
    std::vector<SampleFunctionalArea> cow_layout;
    std::vector<b2AABB> cow_map;

    b2Vec2 Get_target();
    std::vector<int> get_availabe_activities();
};