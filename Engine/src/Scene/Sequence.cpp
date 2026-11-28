#include "pch.h"
#include "Sequence.h"
#include "Entity.h"
#include "Core/ReflectionSerialization.h"

////////////
// SequenceChannel
////////////

// https://en.wikipedia.org/wiki/Monotone_cubic_interpolation
static float calc_auto_slope(const eastl::vector<SequenceKey> &keys, int index)
{
	// Ends
	if (index == 0 || index == keys.size() - 1)
		return 0.0f;

	const SequenceKey &prev = keys[index - 1];
	const SequenceKey &key = keys[index];
	const SequenceKey &next = keys[index + 1];
	float left_slope = (key.value - prev.value) / (key.frame - prev.frame);
	float right_slope = (next.value - key.value) / (next.frame - key.frame);

	// Extremum
	if (left_slope * right_slope <= 0.0f)
		return 0.0f;

	// No overshoot
	float limit = 3.0f * glm::min(abs(left_slope), abs(right_slope));
	return glm::clamp((next.value - prev.value) / (next.frame - prev.frame), -limit, limit);
}

// https://en.wikipedia.org/wiki/Cubic_Hermite_spline
static float hermite(float left_value, float left_tangent, float right_value, float right_tangent, float t)
{
	float t2 = t * t;
	float t3 = t2 * t;
	return (2.0f * t3 - 3.0f * t2 + 1.0f) * left_value + (t3 - 2.0f * t2 + t) * left_tangent + (3.0f * t2 - 2.0f * t3) * right_value + (t3 - t2) * right_tangent;
}

static int find_key_index_after(const eastl::vector<SequenceKey> &keys, float frame)
{
	return (int)(eastl::upper_bound(keys.begin(), keys.end(), frame, [](float search_frame, const SequenceKey &key) { return search_frame < key.frame; }) - keys.begin());
}

float SequenceChannel::evaluate(float frame) const
{
	int right_index = find_key_index_after(keys, frame);

	// Before first key
	if (right_index == 0)
		return keys.front().value;

	// After last key
	if (right_index == keys.size())
		return keys.back().value;

	const SequenceKey &left_key = keys[right_index - 1];
	const SequenceKey &right_key = keys[right_index];

	// Constant
	if (left_key.interpolation == SequenceInterpolation::CONSTANT)
		return left_key.value;

	float length = right_key.frame - left_key.frame;
	float t = (frame - left_key.frame) / length;

	// Linear
	if (left_key.interpolation == SequenceInterpolation::LINEAR)
		return glm::lerp(left_key.value, right_key.value, t);

	// Cubic
	float left_tangent = calc_auto_slope(keys, right_index - 1) * length;
	float right_tangent = calc_auto_slope(keys, right_index) * length;
	return hermite(left_key.value, left_tangent, right_key.value, right_tangent, t);
}

const eastl::string &SequenceChannel::evaluateSerialized(float frame) const
{
	int left_index = glm::max(find_key_index_after(keys, frame) - 1, 0);
	return keys[left_index].serialized_value;
}

static int find_key_index_at_or_after(const eastl::vector<SequenceKey> &keys, int frame)
{
	return (int)(eastl::lower_bound(keys.begin(), keys.end(), frame, [](const SequenceKey &key, int search_frame) { return key.frame < search_frame; }) - keys.begin());
}

bool SequenceChannel::hasKey(int frame) const
{
	int index = find_key_index_at_or_after(keys, frame);
	return index < keys.size() && keys[index].frame == frame;
}

SequenceKey *SequenceChannel::findKey(int frame)
{
	int index = find_key_index_at_or_after(keys, frame);
	return index < keys.size() && keys[index].frame == frame ? &keys[index] : nullptr;
}

SequenceKey &SequenceChannel::addKey(int frame)
{
	ENGINE_ASSERT(!hasKey(frame));

	SequenceKey key;
	key.frame = frame;
	return *keys.insert(keys.begin() + find_key_index_at_or_after(keys, frame), key);
}

bool SequenceChannel::canMoveKeys(const eastl::vector<int> &frames, int delta) const
{
	for (int frame : frames)
	{
		int dst_frame = frame + delta;
		bool is_dst_moving = eastl::find(frames.begin(), frames.end(), dst_frame) != frames.end();
		if (hasKey(frame) && hasKey(dst_frame) && !is_dst_moving)
			return false;
	}
	return true;
}

void SequenceChannel::moveKeys(const eastl::vector<int> &frames, int delta)
{
	ENGINE_ASSERT(canMoveKeys(frames, delta));

	for (SequenceKey &key : keys)
	{
		if (eastl::find(frames.begin(), frames.end(), key.frame) != frames.end())
			key.frame += delta;
	}

	// Re-sort
	eastl::sort(keys.begin(), keys.end(), [](const SequenceKey &left, const SequenceKey &right) { return left.frame < right.frame; });
}

void SequenceChannel::removeKey(int frame)
{
	if (SequenceKey *key = findKey(frame))
		keys.erase(key);
}

////////////
// PropertyTrack
////////////

static int get_scalar_count(const FieldInfo &field)
{
	return field.structInfo ? field.structInfo->fieldsCount : 1;
}

static size_t get_scalar_offset(const FieldInfo &field, int index)
{
	return field.offset + (field.structInfo ? field.structInfo->fields[index].offset : 0);
}

static ValueType get_scalar_type(const FieldInfo &field, int index)
{
	return (field.structInfo ? field.structInfo->fields[index] : field).valueInfo->type;
}

static float read_scalar(const FieldInfo &field, const void *owner, int index)
{
	const void *value = (const uint8_t *)owner + get_scalar_offset(field, index);
	switch (get_scalar_type(field, index))
	{
		case VALUE_TYPE_BOOL: return *(const bool *)value;
		case VALUE_TYPE_INT32: return (float)*(const int32_t *)value;
		case VALUE_TYPE_UINT32: return (float)*(const uint32_t *)value;
		case VALUE_TYPE_FLOAT: return *(const float *)value;
	}
	ENGINE_ASSERT(false);
	return 0.0f;
}

static void write_scalar(const FieldInfo &field, void *owner, int index, float scalar)
{
	void *value = (uint8_t *)owner + get_scalar_offset(field, index);
	switch (get_scalar_type(field, index))
	{
		case VALUE_TYPE_BOOL: *(bool *)value = scalar != 0.0f; return;
		case VALUE_TYPE_INT32: *(int32_t *)value = (int32_t)scalar; return;
		case VALUE_TYPE_UINT32: *(uint32_t *)value = (uint32_t)scalar; return;
		case VALUE_TYPE_FLOAT: *(float *)value = scalar; return;
	}
	ENGINE_ASSERT(false);
}

static bool is_numeric(const FieldInfo &field)
{
	if (field.valueInfo)
	{
		ValueType type = field.valueInfo->type;
		bool is_number = type == VALUE_TYPE_BOOL || type == VALUE_TYPE_INT32 || type == VALUE_TYPE_UINT32 || type == VALUE_TYPE_FLOAT;
		return is_number && !field.isType<entt::entity>();
	} else if (field.structInfo)
	{
		for (int i = 0; i < field.structInfo->fieldsCount; i++)
		{
			const FieldInfo &member = field.structInfo->fields[i];
			if (!member.valueInfo || !is_numeric(member))
				return false;
		}
		return true;
	}
	return false;
}

bool PropertyTrack::canInterpolate(const FieldInfo &field)
{
	if (!is_numeric(field))
		return false;

	for (int i = 0; i < get_scalar_count(field); i++)
	{
		if (get_scalar_type(field, i) != VALUE_TYPE_FLOAT)
			return false;
	}
	return true;
}

bool PropertyTrack::apply(const FieldInfo &field, void *owner, float frame) const
{
	if (!is_numeric(field))
	{
		if (channels.empty() || channels[0].keys.empty())
			return false;

		void *value = field.getAddress(owner);
		const eastl::string &serialized_value = channels[0].evaluateSerialized(frame);
		if (ReflectionYaml::saveToString(field, value) == serialized_value)
			return false;

		ReflectionYaml::loadFromString(field, value, serialized_value);
		return true;
	}

	bool is_changed = false;
	int scalar_count = glm::min((int)channels.size(), get_scalar_count(field));
	for (int i = 0; i < scalar_count; i++)
	{
		if (channels[i].keys.empty())
			continue;

		float evaluated_value = channels[i].evaluate(frame);
		if (read_scalar(field, owner, i) == evaluated_value)
			continue;

		write_scalar(field, owner, i, evaluated_value);
		is_changed = true;
	}
	return is_changed;
}

void PropertyTrack::setKey(const FieldInfo &field, const void *owner, int frame)
{
	bool is_field_numeric = is_numeric(field);
	channels.resize(is_field_numeric ? get_scalar_count(field) : 1);
	for (int i = 0; i < channels.size(); i++)
	{
		SequenceKey *key = channels[i].findKey(frame);
		if (!key)
		{
			key = &channels[i].addKey(frame);
			key->interpolation = canInterpolate(field) ? SequenceInterpolation::CUBIC : SequenceInterpolation::CONSTANT;
		}

		if (is_field_numeric)
			key->value = read_scalar(field, owner, i);
		else
			key->serialized_value = ReflectionYaml::saveToString(field, field.getAddress(owner));
	}
}

bool PropertyTrack::hasKey(int frame) const
{
	for (const SequenceChannel &channel : channels)
	{
		if (channel.hasKey(frame))
			return true;
	}
	return false;
}

eastl::vector<SequenceKey *> PropertyTrack::findKeys(int frame)
{
	eastl::vector<SequenceKey *> keys;
	for (SequenceChannel &channel : channels)
	{
		if (SequenceKey *key = channel.findKey(frame))
			keys.push_back(key);
	}
	return keys;
}

eastl::vector<int> PropertyTrack::getKeyFrames() const
{
	eastl::vector<int> frames;
	for (const SequenceChannel &channel : channels)
	{
		for (const SequenceKey &key : channel.keys)
			frames.push_back(key.frame);
	}
	eastl::sort(frames.begin(), frames.end());
	frames.erase(eastl::unique(frames.begin(), frames.end()), frames.end());
	return frames;
}

bool PropertyTrack::canMoveKeys(const eastl::vector<int> &frames, int delta) const
{
	for (const SequenceChannel &channel : channels)
	{
		if (!channel.canMoveKeys(frames, delta))
			return false;
	}
	return true;
}

void PropertyTrack::moveKeys(const eastl::vector<int> &frames, int delta)
{
	for (SequenceChannel &channel : channels)
		channel.moveKeys(frames, delta);
}

void PropertyTrack::removeKeys(int frame)
{
	for (SequenceChannel &channel : channels)
		channel.removeKey(frame);
}

////////////
// SequenceBinding
////////////

PropertyTrack *SequenceBinding::findTrack(const char *target, const char *field_name)
{
	for (PropertyTrack &track : property_tracks)
	{
		if (track.target == target && track.field_name == field_name)
			return &track;
	}
	return nullptr;
}

PropertyTrack &SequenceBinding::findOrAddTrack(const char *target, const char *field_name)
{
	if (PropertyTrack *track = findTrack(target, field_name))
		return *track;

	PropertyTrack &track = property_tracks.push_back();
	track.target = target;
	track.field_name = field_name;
	return track;
}

////////////
// Sequence
////////////

SequenceBinding *Sequence::findBinding(entt::entity entity)
{
	for (SequenceBinding &binding : bindings)
	{
		if (binding.entity == entity)
			return &binding;
	}
	return nullptr;
}

SequenceBinding &Sequence::findOrAddBinding(entt::entity entity)
{
	if (SequenceBinding *binding = findBinding(entity))
		return *binding;

	SequenceBinding &binding = bindings.push_back();
	binding.entity = entity;
	return binding;
}

void Sequence::removeEmptyTracks()
{
	auto has_no_keys = [](const PropertyTrack &track)
	{
		return eastl::all_of(track.channels.begin(), track.channels.end(), [](const SequenceChannel &channel) { return channel.keys.empty(); });
	};

	for (SequenceBinding &binding : bindings)
		eastl::erase_if(binding.property_tracks, has_no_keys);
	eastl::erase_if(bindings, [](const SequenceBinding &binding) { return binding.property_tracks.empty(); });
}

////////////
// SequencePlayer
////////////

void SequencePlayer::togglePlay(const Sequence &sequence)
{
	is_playing = !is_playing;
	// Wrap to start
	if (is_playing && frame >= sequence.end_frame)
		frame = sequence.start_frame;
}

void SequencePlayer::update(const Sequence &sequence, float delta_time)
{
	if (is_playing)
	{
		frame = glm::min(frame + delta_time * sequence.frame_rate, (float)sequence.end_frame);
		// Stop at end frame
		is_playing = frame < sequence.end_frame;
	}

	if (frame != applied_frame)
		apply(sequence);
}

void SequencePlayer::apply(const Sequence &sequence)
{
	for (const SequenceBinding &binding : sequence.bindings)
	{
		for (const PropertyTrack &track : binding.property_tracks)
		{
			// Find field from track field name, and apply track frames to it
			eastl::vector<ReflectedField> steps = findFieldSteps(binding, track);
			if (steps.empty())
				continue;

			if (!track.apply(*steps.back().info, steps.back().owner, frame))
				continue;

			// Apply notifyChanged from leaf to root. Root may have onChange event on its struct for example
			for (int i = steps.size() - 1; i >= 0; i--)
				steps[i].info->notifyChanged(steps[i].owner);
		}
	}
	applied_frame = frame;
}

static ReflectedObject find_component(const SequenceBinding &binding, const PropertyTrack &track)
{
	Entity entity(binding.entity);
	return entity ? entity.findComponent(track.target.c_str()) : ReflectedObject{};
}

eastl::vector<ReflectedField> SequencePlayer::findFieldSteps(const SequenceBinding &binding, const PropertyTrack &track)
{
	return find_component(binding, track).findFieldSteps(track.field_name.c_str());
}

ReflectedField SequencePlayer::findField(const SequenceBinding &binding, const PropertyTrack &track)
{
	return find_component(binding, track).findField(track.field_name.c_str());
}

void SequencePlayer::sortTracks(SequenceBinding &binding)
{
	auto get_order = [&](const PropertyTrack &track)
	{
		return eastl::make_pair(Entity::getComponentIndex(track.target.c_str()), findFieldSteps(binding, track));
	};
	eastl::sort(binding.property_tracks.begin(), binding.property_tracks.end(), [&](const PropertyTrack &left, const PropertyTrack &right) { return get_order(left) < get_order(right); });
}
