/**
 * Copyright (c) 2006-2026 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#ifndef LOVE_PHYSICS_BOX2D_LH_PHYSICS_H
#define LOVE_PHYSICS_BOX2D_LH_PHYSICS_H

// What the lh_*.cpp files of love.physics share. The binding is split by
// object type (World, Body, Shape, Joint, Contact) the way the wrap_*.cpp
// files were, but the L^ side sees five hostdata types: every shape kind is
// a love.physics.Shape and every joint kind a love.physics.Joint, with the
// kind-specific members raising when asked of the wrong kind (getType /
// getJointType tell them apart).

#include "lh/lh.h"
#include "common/Vector.h"

#include "Physics.h"
#include "World.h"
#include "Body.h"
#include "Shape.h"
#include "Joint.h"
#include "Contact.h"

#include <vector>

namespace love
{
namespace physics
{
namespace box2d
{

#define LH_PHYSICS "love.physics"

struct PhysicsBinding
{
	lh::Errors *errors = nullptr;
	lh::TypeRegistry *registry = nullptr;
};

extern PhysicsBinding physicsBinding;

// Argument readers. The checker admits only the declared types, so a miss
// here is the host's own mistake; it raises and answers nullptr.
World *checkWorld(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);
Body *checkBody(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);
Shape *checkShape(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);
Joint *checkJoint(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);
Contact *checkContact(LhatMachine *machine, const LhatValue *args, size_t count, size_t index);

float numberAt(const LhatValue *args, size_t count, size_t index, float fallback = 0.0f);
bool boolAt(const LhatValue *args, size_t count, size_t index, bool fallback = false);

// Reads (x, y) pairs from args[from..count). Raises and answers false when
// the count is odd or a value is not a number.
bool coordsOf(LhatMachine *machine, const LhatValue *args, size_t count, size_t from, std::vector<Vector2> &out);

// Fresh wrappers (each retains its object; see lh::pushObject).
LhatValue pushWorld(LhatMachine *machine, World *world);
LhatValue pushBody(LhatMachine *machine, Body *body);
LhatValue pushShape(LhatMachine *machine, Shape *shape);
LhatValue pushJoint(LhatMachine *machine, Joint *joint);
LhatValue pushContact(LhatMachine *machine, Contact *contact);

// A tuple of numbers; a list (t^{number^[]}) of numbers.
// 05 の 8.7: writes `count` answers into the machine's room.
void numbers(const float *values, size_t count, LhatValue *answers, int *answerCount);
LhatValue numberList(LhatMachine *machine, const std::vector<float> &values);

// A list of wrappers. `release` drops the reference the vector came with
// (World::getContacts and Body::getContacts hand out retained contacts).
template <typename T>
LhatValue objectList(LhatMachine *machine, const std::vector<T *> &objects, bool release = false)
{
	LhatValue table = lhat_nil();
	if (!lhat_machine_make_table(machine, &table))
		return lhat_nil();
	LhatTable *t = (LhatTable *) lhat_as_object(table);
	for (size_t i = 0; i < objects.size(); i++)
	{
		bool refused = false;
		lhat_table_set(t, lhat_integer((int64_t) i), lh::pushObject(machine, *physicsBinding.registry, objects[i]), &refused);
		if (release)
			objects[i]->release();
	}
	return table;
}

// Calls a parked L^ subroutine with `args`. Answers false when the call did
// not end normally; the fault (if any) ends the run the host function was
// called from, so the caller only has to stop what it was doing.
bool callParked(LhatMachine *machine, lh::Parked *callee, const LhatValue *args, size_t count, LhatValue *result);

// The per-type halves of lhopen_love_physics, each run in both phases.
bool lhPhysicsWorld(lh::Context &ctx);
bool lhPhysicsBody(lh::Context &ctx);
bool lhPhysicsShape(lh::Context &ctx);
bool lhPhysicsJoint(lh::Context &ctx);
bool lhPhysicsContact(lh::Context &ctx);

} // box2d
} // physics
} // love

#endif // LOVE_PHYSICS_BOX2D_LH_PHYSICS_H
