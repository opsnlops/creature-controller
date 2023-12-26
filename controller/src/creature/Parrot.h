
#pragma once

#include <memory>

#include "controller-config.h"

#include "Creature.h"

#include "device/Servo.h"
#include "logging/Logger.h"


// joint -> servo mappings
#define JOINT_NECK_LEFT         0
#define JOINT_NECK_RIGHT        1
#define JOINT_NECK_ROTATE       2
#define JOINT_BODY_LEAN         3

#define JOINT_BEAK              4
#define JOINT_CHEST             5
#define JOINT_STAND_ROTATE      6


// Servo mappings in the servo array
#define SERVO_NECK_LEFT         0
#define SERVO_NECK_RIGHT        1
#define SERVO_NECK_ROTATE       3
#define SERVO_BODY_LEAN         4
#define SERVO_CHEST             5
#define SERVO_BEAK              2

#define STEPPER_STAND_ROTATE    0



// Input mapping. Defines which axis is each
#define INPUT_HEAD_HEIGHT   1
#define INPUT_HEAD_TILT     0
#define INPUT_NECK_ROTATE   2
#define INPUT_BODY_LEAN     4

#define INPUT_BEAK          5
#define INPUT_CHEST         6
#define INPUT_STAND_ROTATE  3



#define HEAD_OFFSET_MAX  0.4       // The max percent of the total height that the head can be


typedef struct {
    u16 left;
    u16 right;
} head_position_t;

class Parrot : public creatures::creature::Creature {

public:

    explicit Parrot(const std::shared_ptr<creatures::Logger>& logger);

    /**
     * Convert a given y coordinate to where the head should be
     * @param y the y coordinate
     * @return head height
     */
    [[nodiscard]] u16 convertToHeadHeight(u16 y) const;


    /**
     * Convert the x axis into head tilt
     * @param x the x axis
     * @return head tilt
     */
    [[nodiscard]] int32_t configToHeadTilt(u16 x) const;

    head_position_t calculateHeadPosition(u16 height, int32_t offset);

private:

    u16 headOffsetMax;

    // Our worker thread
    [[noreturn]] void worker() override;

};
