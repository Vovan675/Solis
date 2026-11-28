#pragma once
#include <entt/entt.hpp>
#include "Core/Reflection.h"

//////
// Sequence (holds lots of bindings):
//													FRAME_1				FRAME_2					FRAME_3
//		Entity1 (SequenceBinding) -> tracks:
//			Track[0] (LightComponent, Intensity):
//				SeqChannel[0]						SeqKey({0, 0})		SeqKey({1, 100})		SeqKey({2, 50})
//			Track[1] (LightComponent, Color):
//				SeqChannel[0] (red)					SeqKey({0, r=0}
//				SeqChannel[1] (green)				SeqKey({0, r=255}
//				SeqChannel[2] (blue)				SeqKey({0, r=128}
//		Entity2 (SequenceBinding) -> tracks:
//				...
/////

enum class SequenceInterpolation
{
	CONSTANT,
	LINEAR,
	CUBIC,
};
inline const char *const sequence_interpolation_items[] = {"Constant", "Linear", "Cubic"};

// Frame, Value pair
struct SequenceKey
{
	int frame = 0;
	float value = 0.0f;
	SequenceInterpolation interpolation = SequenceInterpolation::CUBIC; // from this to next key
};

// All keys for one property scalar (r, g, b scalars for example can have different keys separately)
struct SequenceChannel
{
	eastl::vector<SequenceKey> keys;

	float evaluate(float frame) const;

	bool hasKey(int frame) const;
	SequenceKey *findKey(int frame);
	SequenceKey &addKey(int frame);
	bool canMoveKeys(const eastl::vector<int> &frames, int delta) const;
	void moveKeys(const eastl::vector<int> &frames, int delta);
	void removeKey(int frame);
};

// Track of one property
struct PropertyTrack
{
	eastl::string target; // example: "LightComponent"
	eastl::string field_name; // example: "intensity"
	eastl::vector<SequenceChannel> channels; // example: one scalar element (because its just float), or three for vec3

	static bool isAnimatable(const FieldInfo &field);
	static bool canInterpolate(const FieldInfo &field);
	void apply(const FieldInfo &field, void *owner, float frame) const;

	void setKey(const FieldInfo &field, const void *owner, int frame);
	bool hasKey(int frame) const;
	eastl::vector<SequenceKey *> findKeys(int frame);
	eastl::vector<int> getKeyFrames() const;
	bool canMoveKeys(const eastl::vector<int> &frames, int delta) const;
	void moveKeys(const eastl::vector<int> &frames, int delta);
	void removeKeys(int frame);
};

// (Entity -> Properties tracks) binding, one per entity
struct SequenceBinding
{
	entt::entity entity = entt::null;
	eastl::vector<PropertyTrack> property_tracks;

	PropertyTrack *findTrack(const char *target, const char *field_name);
	PropertyTrack &findOrAddTrack(const char *target, const char *field_name);
};

// One big sequence with several different bindings
struct Sequence
{
	int frame_rate = 60;
	int start_frame = 0;
	int end_frame = 300;
	eastl::vector<SequenceBinding> bindings;

	SequenceBinding *findBinding(entt::entity entity);
	SequenceBinding &findOrAddBinding(entt::entity entity);
	void removeEmptyTracks();
};

// Player is responsible to apply sequence properties
struct SequencePlayer
{
	float frame = 0.0f;
	bool is_playing = false;

	void togglePlay(const Sequence &sequence);
	void update(const Sequence &sequence, float delta_time);
	void apply(const Sequence &sequence);

	static ReflectedField findField(const SequenceBinding &binding, const PropertyTrack &track);
	static void sortTracks(SequenceBinding &binding);

private:
	float applied_frame = -1.0f;
};

REFLECT_BEGIN(SequenceKey)
	REFLECT_FIELD(frame),
	REFLECT_FIELD(value),
	REFLECT_FIELD(interpolation),
REFLECT_END()

REFLECT_BEGIN(SequenceChannel)
	REFLECT_FIELD(keys),
REFLECT_END()

REFLECT_BEGIN(PropertyTrack)
	REFLECT_FIELD(target),
	REFLECT_FIELD(field_name),
	REFLECT_FIELD(channels),
REFLECT_END()

REFLECT_BEGIN(SequenceBinding)
	REFLECT_FIELD(entity),
	REFLECT_FIELD(property_tracks),
REFLECT_END()

REFLECT_BEGIN(Sequence)
	REFLECT_FIELD(frame_rate),
	REFLECT_FIELD(start_frame),
	REFLECT_FIELD(end_frame),
	REFLECT_FIELD(bindings),
REFLECT_END()
