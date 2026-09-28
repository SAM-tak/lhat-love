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

// love.graphics.Mesh and love.graphics.SpriteBatch for L^. The references
// are wrap_Mesh.cpp, wrap_SpriteBatch.cpp and wrap_Graphics.cpp beside this
// file. Meshes use the standard vertex format only (x, y, u, v, r, g, b, a
// -- wrap_Graphics's newStandardMesh); custom formats and buffers wait for
// a need.

#include "lh_Graphics.h"

#include "Mesh.h"
#include "SpriteBatch.h"

#include <cstring>

namespace love
{
namespace graphics
{

#define instance() graphicsInstance()
#define binding graphicsBinding

// ---------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------

static Mesh *checkMesh(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	Mesh *mesh = index < count ? lh::checkObject<Mesh>(args[index], *binding.registry) : nullptr;
	if (mesh == nullptr)
		lh::raise(machine, "Expected a Mesh");
	return mesh;
}

#define MESH_SELF() Mesh *mesh = checkMesh(machine, args, count, 0); if (mesh == nullptr) return

// A standard vertex from a table { x, y, u, v, r, g, b, a } (the last six
// optional) or from eight numbers.
static Vertex vertexOf(const float *v, size_t n)
{
	Vertex out;
	out.x = n > 0 ? v[0] : 0.0f;
	out.y = n > 1 ? v[1] : 0.0f;
	out.s = n > 2 ? v[2] : 0.0f;
	out.t = n > 3 ? v[3] : 0.0f;
	auto channel = [&](size_t i) { return (unsigned char) (std::min(std::max(n > i ? v[i] : 1.0f, 0.0f), 1.0f) * 255.0f); };
	out.color.r = channel(4);
	out.color.g = channel(5);
	out.color.b = channel(6);
	out.color.a = channel(7);
	return out;
}

static bool vertexTable(LhatMachine *machine, LhatValue value, Vertex &out)
{
	std::vector<float> parts;
	if (!numbersOf(value, parts) || parts.size() < 2)
	{
		lh::raise(machine, "A vertex is a table of at least x and y");
		return false;
	}
	out = vertexOf(parts.data(), parts.size());
	return true;
}

static bool drawModeAt(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, PrimitiveType &mode)
{
	std::string name = lh::optEnum(machine, args, count, index, "love.graphics", "MeshDrawMode", "fan");
	if (!getConstant(name.c_str(), mode))
	{
		lh::raise(machine, "Invalid mesh draw mode: " + name);
		return false;
	}
	return true;
}

static bool usageAt(LhatMachine *machine, const LhatValue *args, size_t count, size_t index, BufferDataUsage &usage)
{
	std::string name = lh::optEnum(machine, args, count, index, "love.graphics", "Usage", "dynamic");
	if (!getConstant(name.c_str(), usage))
	{
		lh::raise(machine, "Invalid usage hint: " + name);
		return false;
	}
	return true;
}

// newMesh(vertices[, mode[, usage]]) / newMesh(count[, mode[, usage]])
static void lh_newMesh(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
					   LhatValue *answers, int *answerCount)
{
	(void) context;
	PrimitiveType mode = PRIMITIVE_TRIANGLE_FAN;
	BufferDataUsage usage = BUFFERDATAUSAGE_DYNAMIC;
	if (!drawModeAt(machine, args, count, 1, mode) || !usageAt(machine, args, count, 2, usage))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	std::vector<Buffer::DataDeclaration> format = Mesh::getDefaultVertexFormat();

	if (count > 0 && lhat_is_object_kind(args[0], LHAT_OBJECT_TABLE))
	{
		const LhatTable *t = (const LhatTable *) lhat_as_object(args[0]);
		size_t n = lhat_table_length(t);
		std::vector<Vertex> vertices;
		vertices.reserve(n);
		for (size_t i = 0; i < n; i++)
		{
			Vertex v;
			if (!vertexTable(machine, lhat_table_get(t, lhat_integer((int64_t) i)), v))
			{
				answers[0] = lhat_nil();
				*answerCount = 1;
				return;
			}
			vertices.push_back(v);
		}
		lh::guard(machine, [&]() {
			StrongRef<Mesh> mesh(instance()->newMesh(format, vertices.data(), vertices.size() * sizeof(Vertex), mode, usage), Acquire::NORETAIN);
			answers[0] = lh::pushObject(machine, *binding.registry, mesh.get());
			*answerCount = 1;
		});
		return;
	}
	int n = (int) lh::optNumber(args, count, 0, 0);
	lh::guard(machine, [&]() {
		StrongRef<Mesh> mesh(instance()->newMesh(format, n, mode, usage), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, mesh.get());
		*answerCount = 1;
	});
}

// setVertex(index, x, y[, u, v[, r, g, b, a]]) / setVertex(index, table)
static void lh_Mesh_setVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	size_t index = (size_t) lh::optNumber(args, count, 1, 0);
	Vertex v;
	if (count >= 3 && lhat_is_object_kind(args[2], LHAT_OBJECT_TABLE))
	{
		if (!vertexTable(machine, args[2], v))
			return;
	}
	else
	{
		std::vector<float> parts;
		for (size_t i = 2; i < count; i++)
			parts.push_back((float) lh::optNumber(args, count, i, 0.0));
		v = vertexOf(parts.data(), parts.size());
	}
	lh::guard(machine, [&]() {
		size_t offset = 0;
		char *data = (char *) mesh->checkVertexDataOffset(index, &offset);
		memcpy(data, &v, sizeof(Vertex));
		mesh->setVertexDataModified(offset, mesh->getVertexStride());
	});
}

// getVertex(index) -> (x, y, u, v, r, g, b, a)
static void lh_Mesh_getVertex(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	size_t index = (size_t) lh::optNumber(args, count, 1, 0);
	lh::guard(machine, [&]() {
		const Vertex *v = (const Vertex *) mesh->checkVertexDataOffset(index, nullptr);
		float values[8] = {v->x, v->y, v->s, v->t, v->color.r / 255.0f, v->color.g / 255.0f, v->color.b / 255.0f, v->color.a / 255.0f};
		numberTuple(values, 8, answers, answerCount);
	});
}

// setVertices(vertices[, start])
static void lh_Mesh_setVertices(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	if (count < 2 || !lhat_is_object_kind(args[1], LHAT_OBJECT_TABLE))
	{
		lh::raise(machine, "setVertices needs a table of vertices");
		return;
	}
	size_t start = (size_t) lh::optNumber(args, count, 2, 0);
	const LhatTable *t = (const LhatTable *) lhat_as_object(args[1]);
	size_t n = lhat_table_length(t);
	std::vector<Vertex> vertices;
	vertices.reserve(n);
	for (size_t i = 0; i < n; i++)
	{
		Vertex v;
		if (!vertexTable(machine, lhat_table_get(t, lhat_integer((int64_t) i)), v))
			return;
		vertices.push_back(v);
	}
	lh::guard(machine, [&]() {
		if (start + vertices.size() > mesh->getVertexCount())
			throw love::Exception("Too many vertices for this Mesh.");
		size_t offset = 0;
		char *data = (char *) mesh->checkVertexDataOffset(start, &offset);
		memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
		mesh->setVertexDataModified(offset, vertices.size() * sizeof(Vertex));
	});
}

static void lh_Mesh_getVertexCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								   LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	answers[0] = lhat_integer((int64_t) mesh->getVertexCount());
	*answerCount = 1;
}

// setVertexMap(indices...) / setVertexMap(table) / setVertexMap() to clear.
static void lh_Mesh_setVertexMap(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	std::vector<uint32> map;
	if (count >= 2 && lhat_is_object_kind(args[1], LHAT_OBJECT_TABLE))
	{
		std::vector<float> parts;
		numbersOf(args[1], parts);
		for (float f : parts)
			map.push_back((uint32) f);
	}
	else
	{
		for (size_t i = 1; i < count; i++)
			map.push_back((uint32) lh::optNumber(args, count, i, 0));
	}
	lh::guard(machine, [&]() {
		if (map.empty())
			mesh->setVertexMap();
		else
			mesh->setVertexMap(map);
	});
}

static void lh_Mesh_getVertexMap(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	std::vector<uint32> map;
	lh::guard(machine, [&]() {
		LhatValue table = lhat_nil();
		if (!lhat_machine_make_table(machine, &table))
		{
			answers[0] = lhat_nil();
			*answerCount = 1;
			return;
		}
		if (mesh->getVertexMap(map))
		{
			LhatTable *t = (LhatTable *) lhat_as_object(table);
			for (size_t i = 0; i < map.size(); i++)
			{
				bool refused = false;
				lhat_table_set(t, lhat_integer((int64_t) i), lhat_integer((int64_t) map[i]), &refused);
			}
		}
		answers[0] = table;
		*answerCount = 1;
	});
}

static void lh_Mesh_setTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	if (count < 2)
	{
		mesh->setTexture();
		return;
	}
	Texture *texture = checkTexture(machine, args, count, 1);
	if (texture == nullptr)
		return;
	lh::guard(machine, [&]() {
		mesh->setTexture(texture);
	});
}

static void lh_Mesh_getTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	Texture *texture = mesh->getTexture();
	answers[0] = texture != nullptr ? lh::pushObject(machine, *binding.registry, texture) : lhat_nil();
	*answerCount = 1;
}

static void lh_Mesh_setDrawMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	PrimitiveType mode;
	if (!drawModeAt(machine, args, count, 1, mode))
		return;
	mesh->setDrawMode(mode);
}

static void lh_Mesh_getDrawMode(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	const char *name = "fan";
	getConstant(mesh->getDrawMode(), name);
	LhatValue out = lhat_nil();
	out = lh::pushEnum(machine, "love.graphics", "MeshDrawMode", name);
	answers[0] = out;
	*answerCount = 1;
}

// setDrawRange(start, count) / setDrawRange() for all.
static void lh_Mesh_setDrawRange(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	lh::guard(machine, [&]() {
		if (count >= 3)
			mesh->setDrawRange((int) lh::optNumber(args, count, 1, 0), (int) lh::optNumber(args, count, 2, 0));
		else
			mesh->setDrawRange();
	});
}

// getDrawRange() -> (start, count), (0, 0) when drawing everything.
static void lh_Mesh_getDrawRange(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	int start = 0, n = 0;
	if (!mesh->getDrawRange(start, n))
	{
		start = 0;
		n = 0;
	}
	answers[0] = lhat_integer(start);
	answers[1] = lhat_integer(n);
	*answerCount = 2;
}

static void lh_Mesh_setAttributeEnabled(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	std::string name = lh::optString(args, count, 1, "");
	bool enable = lh::optBool(args, count, 2, true);
	lh::guard(machine, [&]() {
		mesh->setAttributeEnabled(name, enable);
	});
}

static void lh_Mesh_isAttributeEnabled(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									   LhatValue *answers, int *answerCount)
{
	(void) context;
	MESH_SELF();
	std::string name = lh::optString(args, count, 1, "");
	lh::guard(machine, [&]() {
		answers[0] = lhat_bool(mesh->isAttributeEnabled(name));
		*answerCount = 1;
	});
}

bool lhGraphicsMesh(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "Mesh", Mesh::type, m, "Drawable"))
		return false;
	if (ctx.types())
		return true;
	const char *M = "Mesh";
	return ctx.func(m, "newMesh", "p^t^{t^{number^[]}[]} -> love.graphics.Mesh;", lh_newMesh, nullptr)
		&& ctx.func(m, "newMesh", "p^t^{t^{number^[]}[]}, love.graphics.MeshDrawMode -> love.graphics.Mesh;", lh_newMesh, nullptr)
		&& ctx.func(m, "newMesh", "p^t^{t^{number^[]}[]}, love.graphics.MeshDrawMode, love.graphics.Usage -> love.graphics.Mesh;", lh_newMesh, nullptr)
		&& ctx.func(m, "newMesh", "p^number^ -> love.graphics.Mesh;", lh_newMesh, nullptr)
		&& ctx.func(m, "newMesh", "p^number^, love.graphics.MeshDrawMode -> love.graphics.Mesh;", lh_newMesh, nullptr)
		&& ctx.func(m, "newMesh", "p^number^, love.graphics.MeshDrawMode, love.graphics.Usage -> love.graphics.Mesh;", lh_newMesh, nullptr)
		&& ctx.member(m, M, "setVertex", "p^self^, number^, number^, number^, ...;", lh_Mesh_setVertex, nullptr)
		&& ctx.member(m, M, "setVertex", "p^self^, number^, t^{number^[]};", lh_Mesh_setVertex, nullptr)
		&& ctx.member(m, M, "getVertex", "f^self^, number^ -> (number^, number^, number^, number^, number^, number^, number^, number^);", lh_Mesh_getVertex, nullptr)
		&& ctx.member(m, M, "setVertices", "p^self^, t^{t^{number^[]}[]};", lh_Mesh_setVertices, nullptr)
		&& ctx.member(m, M, "setVertices", "p^self^, t^{t^{number^[]}[]}, number^;", lh_Mesh_setVertices, nullptr)
		&& ctx.member(m, M, "getVertexCount", "f^self^ -> number^;", lh_Mesh_getVertexCount, nullptr)
		&& ctx.member(m, M, "setVertexMap", "p^self^;", lh_Mesh_setVertexMap, nullptr)
		&& ctx.member(m, M, "setVertexMap", "p^self^, t^{number^[]};", lh_Mesh_setVertexMap, nullptr)
		&& ctx.member(m, M, "setVertexMap", "p^self^, number^, ...;", lh_Mesh_setVertexMap, nullptr)
		&& ctx.member(m, M, "getVertexMap", "p^self^ -> t^{number^[]};", lh_Mesh_getVertexMap, nullptr)
		&& ctx.member(m, M, "setTexture", "p^self^;", lh_Mesh_setTexture, nullptr)
		&& ctx.member(m, M, "setTexture", "p^self^, love.graphics.Texture;", lh_Mesh_setTexture, nullptr)
		&& ctx.member(m, M, "getTexture", "p^self^ -> love.graphics.Texture|nil^;", lh_Mesh_getTexture, nullptr)
		&& ctx.member(m, M, "setDrawMode", "p^self^, love.graphics.MeshDrawMode;", lh_Mesh_setDrawMode, nullptr)
		&& ctx.member(m, M, "getDrawMode", "f^self^ -> love.graphics.MeshDrawMode;", lh_Mesh_getDrawMode, nullptr)
		&& ctx.member(m, M, "setDrawRange", "p^self^;", lh_Mesh_setDrawRange, nullptr)
		&& ctx.member(m, M, "setDrawRange", "p^self^, number^, number^;", lh_Mesh_setDrawRange, nullptr)
		&& ctx.member(m, M, "getDrawRange", "f^self^ -> (number^, number^);", lh_Mesh_getDrawRange, nullptr)
		&& ctx.member(m, M, "setAttributeEnabled", "p^self^, string^, bool^;", lh_Mesh_setAttributeEnabled, nullptr)
		&& ctx.member(m, M, "isAttributeEnabled", "f^self^, string^ -> bool^;", lh_Mesh_isAttributeEnabled, nullptr);
}

// ---------------------------------------------------------------------------
// SpriteBatch
// ---------------------------------------------------------------------------

static SpriteBatch *checkBatch(LhatMachine *machine, const LhatValue *args, size_t count, size_t index)
{
	SpriteBatch *batch = index < count ? lh::checkObject<SpriteBatch>(args[index], *binding.registry) : nullptr;
	if (batch == nullptr)
		lh::raise(machine, "Expected a SpriteBatch");
	return batch;
}

#define BATCH_SELF() SpriteBatch *batch = checkBatch(machine, args, count, 0); if (batch == nullptr) return

// newSpriteBatch(texture[, size[, usage]])
static void lh_newSpriteBatch(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							  LhatValue *answers, int *answerCount)
{
	(void) context;
	Texture *texture = checkTexture(machine, args, count, 0);
	if (texture == nullptr)
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	int size = (int) lh::optNumber(args, count, 1, 1000);
	BufferDataUsage usage = BUFFERDATAUSAGE_DYNAMIC;
	if (count >= 3 && !usageAt(machine, args, count, 2, usage))
	{
		answers[0] = lhat_nil();
		*answerCount = 1;
		return;
	}
	lh::guard(machine, [&]() {
		StrongRef<SpriteBatch> batch(instance()->newSpriteBatch(texture, size, usage), Acquire::NORETAIN);
		answers[0] = lh::pushObject(machine, *binding.registry, batch.get());
		*answerCount = 1;
	});
}

// add(x, y, ...) / add(quad, x, y, ...) -> the sprite's id
static void lh_SpriteBatch_add(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	lh::guard(machine, [&]() {
		if (count >= 2 && lhat_is_object_kind(args[1], LHAT_OBJECT_HOSTDATA))
		{
			Quad *quad = checkQuad(machine, args, count, 1);
			if (quad == nullptr)
			{
				answers[0] = lhat_nil();
				*answerCount = 1;
				return;
			}
			answers[0] = lhat_integer(batch->add(quad, transformOf(args, count, 2)));
			*answerCount = 1;
			return;
		}
		answers[0] = lhat_integer(batch->add(transformOf(args, count, 1)));
		*answerCount = 1;
	});
}

// set(id, x, y, ...) / set(id, quad, x, y, ...)
static void lh_SpriteBatch_set(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
							   LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	int id = (int) lh::optNumber(args, count, 1, 0);
	lh::guard(machine, [&]() {
		if (count >= 3 && lhat_is_object_kind(args[2], LHAT_OBJECT_HOSTDATA))
		{
			Quad *quad = checkQuad(machine, args, count, 2);
			if (quad == nullptr)
				return;
			batch->add(quad, transformOf(args, count, 3), id);
		}
		else
			batch->add(transformOf(args, count, 2), id);
	});
}

static void lh_SpriteBatch_clear(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	batch->clear();
}

static void lh_SpriteBatch_flush(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
								 LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	lh::guard(machine, [&]() {
		batch->flush();
	});
}

static void lh_SpriteBatch_setTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	Texture *texture = checkTexture(machine, args, count, 1);
	if (texture == nullptr)
		return;
	lh::guard(machine, [&]() {
		batch->setTexture(texture);
	});
}

static void lh_SpriteBatch_getTexture(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									  LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	answers[0] = lh::pushObject(machine, *binding.registry, batch->getTexture());
	*answerCount = 1;
}

// setColor(r, g, b[, a]) / setColor() to reset to white.
static void lh_SpriteBatch_setColor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	if (count >= 4)
		batch->setColor(colorOf(args, count, 1));
	else
		batch->setColor(Colorf(1, 1, 1, 1));
}

static void lh_SpriteBatch_getColor(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	colorTuple(batch->getColor(), answers, answerCount);
}

static void lh_SpriteBatch_getCount(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
									LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	answers[0] = lhat_integer(batch->getCount());
	*answerCount = 1;
}

static void lh_SpriteBatch_getBufferSize(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										 LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	answers[0] = lhat_integer(batch->getBufferSize());
	*answerCount = 1;
}

static void lh_SpriteBatch_setDrawRange(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	lh::guard(machine, [&]() {
		if (count >= 3)
			batch->setDrawRange((int) lh::optNumber(args, count, 1, 0), (int) lh::optNumber(args, count, 2, 0));
		else
			batch->setDrawRange();
	});
}

static void lh_SpriteBatch_getDrawRange(LhatMachine *machine, void *context, const LhatValue *args, size_t count,
										LhatValue *answers, int *answerCount)
{
	(void) context;
	BATCH_SELF();
	int start = 0, n = 0;
	if (!batch->getDrawRange(start, n))
	{
		start = 0;
		n = 0;
	}
	answers[0] = lhat_integer(start);
	answers[1] = lhat_integer(n);
	*answerCount = 2;
}

bool lhGraphicsSpriteBatch(lh::Context &ctx)
{
	const char *m = LH_GRAPHICS;
	if (!ctx.objectType(m, "SpriteBatch", SpriteBatch::type, m, "Drawable"))
		return false;
	if (ctx.types())
		return true;
	const char *B = "SpriteBatch";
	return ctx.func(m, "newSpriteBatch", "p^love.graphics.Texture -> love.graphics.SpriteBatch;", lh_newSpriteBatch, nullptr)
		&& ctx.func(m, "newSpriteBatch", "p^love.graphics.Texture, number^ -> love.graphics.SpriteBatch;", lh_newSpriteBatch, nullptr)
		&& ctx.func(m, "newSpriteBatch", "p^love.graphics.Texture, number^, love.graphics.Usage -> love.graphics.SpriteBatch;", lh_newSpriteBatch, nullptr)
		&& ctx.member(m, B, "add", "p^self^, number^, number^, ... -> number^;", lh_SpriteBatch_add, nullptr)
		&& ctx.member(m, B, "add", "p^self^, love.graphics.Quad, number^, number^, ... -> number^;", lh_SpriteBatch_add, nullptr)
		&& ctx.member(m, B, "set", "p^self^, number^, number^, number^, ...;", lh_SpriteBatch_set, nullptr)
		&& ctx.member(m, B, "set", "p^self^, number^, love.graphics.Quad, number^, number^, ...;", lh_SpriteBatch_set, nullptr)
		&& ctx.member(m, B, "clear", "p^self^;", lh_SpriteBatch_clear, nullptr)
		&& ctx.member(m, B, "flush", "p^self^;", lh_SpriteBatch_flush, nullptr)
		&& ctx.member(m, B, "setTexture", "p^self^, love.graphics.Texture;", lh_SpriteBatch_setTexture, nullptr)
		&& ctx.member(m, B, "getTexture", "p^self^ -> love.graphics.Texture;", lh_SpriteBatch_getTexture, nullptr)
		&& ctx.member(m, B, "setColor", "p^self^;", lh_SpriteBatch_setColor, nullptr)
		&& ctx.member(m, B, "setColor", "p^self^, number^, number^, number^;", lh_SpriteBatch_setColor, nullptr)
		&& ctx.member(m, B, "setColor", "p^self^, number^, number^, number^, number^;", lh_SpriteBatch_setColor, nullptr)
		&& ctx.member(m, B, "getColor", "f^self^ -> (number^, number^, number^, number^);", lh_SpriteBatch_getColor, nullptr)
		&& ctx.member(m, B, "getCount", "f^self^ -> number^;", lh_SpriteBatch_getCount, nullptr)
		&& ctx.member(m, B, "getBufferSize", "f^self^ -> number^;", lh_SpriteBatch_getBufferSize, nullptr)
		&& ctx.member(m, B, "setDrawRange", "p^self^;", lh_SpriteBatch_setDrawRange, nullptr)
		&& ctx.member(m, B, "setDrawRange", "p^self^, number^, number^;", lh_SpriteBatch_setDrawRange, nullptr)
		&& ctx.member(m, B, "getDrawRange", "f^self^ -> (number^, number^);", lh_SpriteBatch_getDrawRange, nullptr);
}

} // graphics
} // love
