#include "cow.h"
#include "sample.h"
#include "rrt.h"

#include "box2d/box2d.h"
#include "box2d/math_functions.h"

#include <assert.h>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>
#include <vector>
#include <map>
#include <boost/algorithm/clamp.hpp>

std::vector<std::vector<float>> TRANSITION_MATRIX = {
    {0.10f, 0.30f, 0.19f, 0.05f, 0.36f}, // Cubicle
    {0.27f, 0.01f, 0.43f, 0.20f, 0.09f}, // Milker
    {0.75f, 0.01f, 0.03f, 0.05f, 0.16f}, // Feeder
    {0.50f, 0.01f, 0.35f, 0.03f, 0.11f}, // Concentrate
    {0.30f, 0.20f, 0.30f, 0.15f, 0.05f}, // Drinker
};

int ACTIVITY_FACTOR = 60;

std::vector<int> ACTIVITY_DURATION = {
    int(70 * ACTIVITY_FACTOR),
    int(8 * ACTIVITY_FACTOR),
    int(36.5 * ACTIVITY_FACTOR),
    int(9.74 * ACTIVITY_FACTOR),
    int(4.5 * ACTIVITY_FACTOR),
};

Cow::Cow()
{
    bodyId = b2_nullBodyId;
    m_isSpawned = false;
    cow_var.state = cow_starting;
    cow_var.waypoint_index = 0;
    cow_var.current_activity = rand() % 4;
    // cow_var.speed = 0.0f;
    // cow_var.steering_angle = 0.0f;
}

// std::vector<SampleFunctionalArea> Cow::cow_layout;
// std::vector<b2AABB> cow_map;

void Cow::Spawn(b2WorldId worldId, float x, float y, float orientation, float scale, float frictionTorque, float hertz,
                int groupIndex, void *userData)

{
    assert(m_isSpawned == false);

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    // bodyDef.sleepThreshold = 0.1f;
    bodyDef.userData = userData;
    bodyDef.position = {x, y};
    // bodyDef.angle = orientation;
    bodyId = b2CreateBody(worldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1 + rand() % 100; // 1.0f;
    shapeDef.friction = 0.1f;
    shapeDef.customColor = cow_color;

    // shapeDef.filter.groupIndex = -groupIndex;
    // shapeDef.filter.maskBits = 1;

    b2Polygon box = b2MakeRoundedBox(cow_height, cow_weight, 7.0f);
    b2CreatePolygonShape(bodyId, &shapeDef, &box);

    // b2Body_ApplyMassFromShapes(bodyId);
    // b2Body_ApplyForceToCenter(bodyId, {10000.0f, 0.0f}, true);

    m_isSpawned = true;
}

void Cow::Despawn()
{
    assert(m_isSpawned == true);

    if (B2_IS_NON_NULL(bodyId))
    {
        b2DestroyBody(bodyId);
        bodyId = b2_nullBodyId;

        cow_layout.clear();
        cow_map.clear();
        max_b_area = b2Vec2{0.0f, 0.0f};
        cow_path.clear();
        nodes.clear();
        cow_var = {};
    }

    m_isSpawned = false;
}

void Cow::Cow_move_model(cow_pose cow_pose)
{
    cow_var.speed = b2ClampFloat(cow_var.speed, 0.0f, float(cow_max_speed));
    cow_var.steering_angle = b2ClampFloat(cow_var.steering_angle, float(-cow_max_steering_angle), float(cow_max_steering_angle));
    float dx = cow_var.speed * cosf(cow_pose.angle);
    float dy = cow_var.speed * sinf(cow_pose.angle);
    float dt = (cow_var.speed / float(cow_leg_base)) * tanf(cow_var.steering_angle);
    b2Body_SetLinearVelocity(bodyId, {dx, dy});
    b2Body_SetAngularVelocity(bodyId, dt);
}

void Cow::Cow_control_to_point(cow_pose cow_pose)
{
    float target_distance = b2Distance(cow_var.waypoint, cow_pose.position);
    float target_heading = std::atan2(cow_var.waypoint.y - cow_pose.position.y, cow_var.waypoint.x - cow_pose.position.x);
    float heading_error = b2UnwindAngle(target_heading - cow_pose.angle);
    cow_var.speed = cow_params.k_v * target_distance;
    cow_var.steering_angle = b2ClampFloat(heading_error, -cow_max_steering_angle, cow_max_steering_angle);
}

int select_weighted_random(const std::vector<float> &weights)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    return dist(gen);
}

int next_manner_from_TM(const int &temp_manner, const std::vector<int> &available_manners)
{
    int index = select_weighted_random(TRANSITION_MATRIX[temp_manner]);
    // int next_manner = ACTIVITY_NAMES[index];
    // std::cout << "index: " << index << std::endl;

    // Check if the selected manner is in the available manners
    if (std::find(available_manners.begin(), available_manners.end(), index) == available_manners.end())
    {
        // If not found, recursively call the function
        return next_manner_from_TM(temp_manner, available_manners);
    }
    // std::cout << "index: " << index << std::endl;

    return index;
}

std::vector<int> Cow::get_availabe_activities()
{
    std::vector<int> availab_act;
    for (auto functional_area : cow_layout)
    {
        availab_act.push_back(functional_area.type);
    }

    if (availab_act.empty())
    {
        throw std::runtime_error("There is not available functional area.");
    }

    // for (int aa : availab_act)
    // {
    //     std::cout << "availab_act: " << aa << std::endl;
    // }

    return availab_act;
}

b2Vec2 Cow::Get_target()
{
    std::vector<int> available_activities;
    available_activities = get_availabe_activities();

    // std::cout << "cow_var.current_activity: " << cow_var.current_activity << std::endl;

    int next_activity = next_manner_from_TM(cow_var.current_activity, available_activities);
    std::vector<SampleFunctionalArea> matchingAreas;

    for (auto func_ara : cow_layout)
    {
        if (func_ara.type == next_activity)
            matchingAreas.push_back(func_ara);
    }

    // std::cout << "matchingAreas: " << matchingAreas.size() << std::endl;

    // Randomly select one from the matching areas
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, matchingAreas.size() - 1);

    cow_var.current_functional_area = matchingAreas[dis(gen)];

    // std::cout << "cow_var.current_functional_area: " << cow_var.current_functional_area.type << std::endl;

    // times 24 + 12 to match world
    b2Vec2 goal;
    goal = b2Vec2{cow_var.current_functional_area.x, cow_var.current_functional_area.y} * 24;
    goal.x += 12;
    goal.y += 12;

    return goal;
}

void Cow::Routine()
{
    assert(m_isSpawned == true);

    if (cow_var.state == cow_starting)
    {

        cow_var.start = b2Body_GetPosition(bodyId);
        cow_var.end = Get_target();
        // std::cout << "end: " << cow_var.end.x << "|" << cow_var.end.y << std::endl;
        cow_path.clear();
        nodes.clear();
        cow_path = FindPath(cow_var.start, cow_var.end, cow_map, max_b_area, 24.0f, 56.0f);
        cow_var.state = cow_traslating;
    }
    else if (cow_var.state == cow_traslating)
    {

        // std::cout << cow_var.waypoint_index << "/" << cow_path.size() << std::endl;
        // std::cout << cow_path.back().x << "|" << cow_path.back().y << std::endl;

        b2Vec2 position = b2Body_GetPosition(bodyId);
        b2Rot rotation = b2Body_GetRotation(bodyId);
        float angle = b2Rot_GetAngle(rotation);
        cow_pose cow_pose;
        cow_pose.position = position;
        cow_pose.angle = angle;

        // cow_var.end = path[cow_var.waypoint_index];
        cow_var.waypoint = cow_path[cow_var.waypoint_index];
        Cow_control_to_point(cow_pose);
        Cow_move_model(cow_pose);

        // Check if the robot is close enough to the target waypoint
        float distance_to_target = b2Distance(cow_pose.position, cow_var.waypoint);
        // std::cout << "distance_to_target: " << distance_to_target << std::endl;

        if (distance_to_target < cow_waypoint_threshold)
        {
            // Move to the next waypoint if available
            if (cow_var.waypoint_index < cow_path.size() - 1)
            {
                cow_var.waypoint_index++;
            }
            else
            {
                cow_var.speed = 0;
                cow_var.waypoint_index = 0;

                cow_var.state = cow_in_activity;
            }
        }
    }

    else if (cow_var.state == cow_idling)
    {
    }
    else if (cow_var.state == cow_in_activity)
    {
        int time_doing = ACTIVITY_DURATION[cow_var.current_activity];

        for (int i = 0; i < time_doing; i++)
        {
        }
        cow_var.start = b2Body_GetPosition(bodyId);
        cow_var.end = Get_target();
        cow_path.clear();
        nodes.clear();
        cow_path = FindPath(cow_var.start, cow_var.end, cow_map, max_b_area, 24.0f, 56.0f);
  
        cow_var.state = cow_traslating;
    }
    // b2Vec2 currentTarget = path[cow_var.waypoint_index];
    // b2Vec2 position = b2Body_GetPosition(bodyId);
    // b2Vec2 direction = currentTarget - position;
    // // direction.Normalize(); // Get the direction as a unit vector
    // // std::cout << direction.x << "|" << direction.y << std::endl;
    // float length = b2Length(direction); // Get the length of the vector
    // if (length > 0.0f)
    // {
    //     direction *= 1.0f / length; // Normalize the vector
    // }

    // // std::cout << direction.x << "|" << direction.y << std::endl;
    // float speed = 1000.0f; // Speed at which the body should move

    // // Option 1: Apply a force towards the target
    // b2Vec2 force = speed * direction;
    // // b2Body_ApplyForceToCenter(bodyId, force, true);
    // b2Body_ApplyForce(bodyId, force, cow_head, true);

    // // Option 2: Set the body's velocity towards the target
    // // body->SetLinearVelocity(speed * direction);
    // // b2Body_SetLinearVelocity(bodyId, speed * direction);

    // float tolerance = 10.0f; // How close to get to the target before switching
    // std::cout << "Pos: " << position.x << "|" << position.y << std::endl;
    // std::cout << "Tar: " << currentTarget.x << "|" << currentTarget.y << std::endl;

    // if (b2DistanceSquared(position, currentTarget) < tolerance * tolerance)
    // {
    //     std::cout << "yes! point reached" << std::endl;

    //     cow_var.waypoint_index = cow_var.waypoint_index + 1;
    //     if (cow_var.waypoint_index < path.size())
    //     {
    //         currentTarget = path[cow_var.waypoint_index];
    //     }
    //     // else
    //     // {
    //     //     // Optionally stop the body or loop the path
    //     //     body->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
    //     // }
    // }
    // }
}
