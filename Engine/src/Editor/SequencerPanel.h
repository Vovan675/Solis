#pragma once
#include "Scene/Entity.h"
#include "imgui.h"

class SequencerPanel
{
public:
	void update(float delta_time);
	void renderImGui();

private:
	struct Row
	{
		int first_track = 0;
		int track_count = 0;
		float y = 0.0f;
	};

	struct KeyId
	{
		int track_index = 0;
		int frame = 0;

		bool operator==(const KeyId &other) const { return track_index == other.track_index && frame == other.frame; }
	};

	void draw_top_bar(Sequence &sequence, SequencePlayer &player);
	void draw_ruler(const Sequence &sequence, SequencePlayer &player);
	eastl::vector<Row> draw_track_list(const Sequence &sequence);
	void draw_keys(Sequence &sequence, SequencePlayer &player, const eastl::vector<Row> &rows);
	void draw_outside_and_playhead(const Sequence &sequence, const SequencePlayer &player, ImVec2 area_min, float height);

	void fit_view(const Sequence &sequence);
	float frame_to_x(float frame) const;
	float x_to_frame(float x) const;

	// View
	float view_start_frame = 0.0f;
	float view_end_frame = 0.0f;
	float timeline_x = 0.0f;
	float timeline_width = 0.0f;

	// Selection
	eastl::vector<KeyId> selected_keys;
	const Sequence *last_sequence = nullptr;
	int last_track_count = 0;

	// Key drag
	bool is_moving_keys = false;
	int drag_delta = 0;
};
