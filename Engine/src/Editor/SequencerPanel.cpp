#include "pch.h"
#include "SequencerPanel.h"
#include "UI.h"
#include "Application.h"

void SequencerPanel::update(float delta_time)
{
	Sequence &sequence = Scene::getCurrentScene()->sequence;
	SequencePlayer &player = Scene::getCurrentScene()->sequence_player;
	if (gInput.isKeyPressed(GLFW_KEY_SPACE) && !ImGui::GetIO().WantTextInput)
		player.togglePlay(sequence);
	player.update(sequence, delta_time);

	// Update value in current stopped frame to current field value. So if value is changed, key is also changed
	int frame = (int)player.frame;
	if (!player.is_playing && player.frame == frame)
	{
		for (SequenceBinding &binding : sequence.bindings)
		{
			for (PropertyTrack &track : binding.property_tracks)
			{
				if (!track.hasKey(frame))
					continue;

				ReflectedField field = SequencePlayer::findField(binding, track);
				if (field.info)
					track.setKey(*field.info, field.owner, frame);
			}
		}
	}
}

void SequencerPanel::renderImGui()
{
	if (!ImGui::Begin((eastl::string(ICON_FA_FILM) + " Sequencer###Sequencer").c_str()))
	{
		ImGui::End();
		return;
	}

	Sequence &sequence = Scene::getCurrentScene()->sequence;
	SequencePlayer &player = Scene::getCurrentScene()->sequence_player;
	draw_top_bar(sequence, player);

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));
	if (ImGui::BeginTable("##tracks", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed, 220.0f);
		ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupScrollFreeze(0, 1);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(1);
		draw_ruler(sequence, player);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		eastl::vector<Row> rows = draw_track_list(sequence);
		ImGui::TableSetColumnIndex(1);
		draw_keys(sequence, player, rows);

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();

	ImGui::End();
}

void SequencerPanel::draw_top_bar(Sequence &sequence, SequencePlayer &player)
{
	auto icon_button = [](const char *label, ImVec4 color, const char *tooltip)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		bool clicked = ImGui::Button(label, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
		ImGui::PopStyleColor(2);
		ImGui::SetItemTooltip("%s", tooltip);
		return clicked;
	};

	ImVec4 text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
	if (icon_button(player.is_playing ? ICON_FA_PAUSE "###play" : ICON_FA_PLAY "###play", player.is_playing ? text_color : UI::success_color, "Play (Space)"))
		player.togglePlay(sequence);

	ImGui::SameLine();
	int frame_number = (int)roundf(player.frame);
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::DragInt("##frame", &frame_number, 1.0f, 0, INT_MAX))
		player.frame = (float)frame_number;
	ImGui::SetItemTooltip("Frame");

	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragInt("##start_frame", &sequence.start_frame, 1.0f, 0, sequence.end_frame, "Start %d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	ImGui::DragInt("##end_frame", &sequence.end_frame, 1.0f, sequence.start_frame + 1, INT_MAX, "End %d", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::DragInt("##frame_rate", &sequence.frame_rate, 1.0f, 1, 240, "%d fps", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SameLine();
	if (icon_button(ICON_FA_EXPAND, text_color, "Fit"))
		fit_view(sequence);
}

void SequencerPanel::draw_ruler(const Sequence &sequence, SequencePlayer &player)
{
	const float MIN_TIMELINE_WIDTH = 50.0f;
	const float MIN_VISIBLE_FRAMES = 10.0f;
	const float MAX_VISIBLE_FRAMES = 100000.0f;
	const float ZOOM_STEP = 1.2f;
	const float PAN_STEP = 60.0f;
	const float MIN_TICK_SPACING = 70.0f;
	const int TICK_STEPS[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};

	ImGuiIO &io = ImGui::GetIO();
	ImVec2 ruler_min = ImGui::GetCursorScreenPos();
	float row_height = ImGui::GetFrameHeight();

	// View
	timeline_x = ruler_min.x;
	timeline_width = glm::max(ImGui::GetContentRegionAvail().x, MIN_TIMELINE_WIDTH);
	if (view_end_frame <= view_start_frame)
		fit_view(sequence);

	// Zoom
	bool is_timeline_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && io.MousePos.x >= timeline_x;
	if (is_timeline_hovered && io.KeyCtrl && io.MouseWheel != 0.0f)
	{
		float visible_frames = view_end_frame - view_start_frame;
		float zoomed_frames = glm::clamp(visible_frames * powf(ZOOM_STEP, -io.MouseWheel), MIN_VISIBLE_FRAMES, MAX_VISIBLE_FRAMES);
		float mouse_frame = x_to_frame(io.MousePos.x);
		view_start_frame = mouse_frame - (mouse_frame - view_start_frame) * zoomed_frames / visible_frames;
		view_end_frame = view_start_frame + zoomed_frames;
	}

	// Pan
	float pan_pixels = 0.0f;
	if (is_timeline_hovered && io.KeyShift)
		pan_pixels = -io.MouseWheel * PAN_STEP;
	if (is_timeline_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		pan_pixels = -io.MouseDelta.x;
	float pan_frames = pan_pixels / timeline_width * (view_end_frame - view_start_frame);
	view_start_frame += pan_frames;
	view_end_frame += pan_frames;

	ImGui::BeginChild("##ruler", ImVec2(timeline_width, row_height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();

	// Play head move
	ImGui::InvisibleButton("##ruler", ImVec2(timeline_width, row_height));
	if (ImGui::IsItemActive())
	{
		player.is_playing = false;
		player.frame = glm::max(roundf(x_to_frame(io.MousePos.x)), 0.0f);
	}

	// Current frame label
	eastl::string frame_text = eastl::to_string((int)roundf(player.frame));
	ImVec2 frame_text_size = ImGui::CalcTextSize(frame_text.c_str());
	float frame_text_x = floorf(frame_to_x(player.frame) - frame_text_size.x * 0.5f);
	ImVec2 label_min(frame_text_x - ImGui::GetStyle().FramePadding.x, ruler_min.y);
	ImVec2 label_max(frame_text_x + frame_text_size.x + ImGui::GetStyle().FramePadding.x, ruler_min.y + row_height);

	// Ticks
	float frame_width = timeline_width / (view_end_frame - view_start_frame);
	int tick_step = 1;
	for (int step : TICK_STEPS)
	{
		tick_step = step;
		if (step * frame_width >= MIN_TICK_SPACING)
			break;
	}

	int first_tick = (int)ceilf(view_start_frame / tick_step) * tick_step;
	for (int tick = first_tick; tick <= view_end_frame; tick += tick_step)
	{
		float x = frame_to_x((float)tick);
		draw_list->AddLine(ImVec2(x, ruler_min.y + row_height * 0.5f), ImVec2(x, ruler_min.y + row_height), ImGui::GetColorU32(ImGuiCol_TextDisabled));

		eastl::string tick_text = eastl::to_string(tick);
		float tick_text_x = x + 3.0f;
		bool is_under_label = tick_text_x < label_max.x && tick_text_x + ImGui::CalcTextSize(tick_text.c_str()).x > label_min.x;
		if (!is_under_label)
			draw_list->AddText(ImVec2(tick_text_x, ruler_min.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), tick_text.c_str());
	}

	draw_outside_and_playhead(sequence, player, ruler_min, row_height);
	draw_list->AddRectFilled(label_min, label_max, ImGui::GetColorU32(UI::accent_color), ImGui::GetStyle().FrameRounding);
	draw_list->AddText(ImVec2(frame_text_x, ruler_min.y + (row_height - frame_text_size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), frame_text.c_str());

	ImGui::EndChild();
}

eastl::vector<SequencerPanel::Row> SequencerPanel::draw_track_list(const Sequence &sequence)
{
	eastl::vector<Row> rows;
	auto add_row = [&](int first_track, int track_count)
	{
		Row row;
		row.first_track = first_track;
		row.track_count = track_count;
		row.y = ImGui::GetCursorScreenPos().y;
		rows.push_back(row);
	};

	int first_track = 0;
	ImGuiTreeNodeFlags row_flags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
	for (int binding_index = 0; binding_index < sequence.bindings.size(); binding_index++)
	{
		const SequenceBinding &binding = sequence.bindings[binding_index];
		int track_count = (int)binding.property_tracks.size();
		Entity entity(binding.entity);

		ImGui::PushID(binding_index);
		add_row(first_track, track_count);
		if (ImGui::TreeNodeEx("##binding", row_flags | ImGuiTreeNodeFlags_DefaultOpen, "%s", entity ? entity.getTransform().name.c_str() : "(missing entity)"))
		{
			for (int i = 0; i < track_count; i++)
			{
				const PropertyTrack &track = binding.property_tracks[i];
				ReflectedField field = SequencePlayer::findField(binding, track);
				eastl::string label;
				if (field.info && field.info->displayLabel && track.field_name == field.info->name)
					label = eastl::string(field.info->displayLabel);
				else
					label = prettifyName(track.field_name.c_str());

				ImGui::PushID(i);
				add_row(first_track + i, 1);
				ImGui::TreeNodeEx("##track", row_flags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", label.c_str());
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
		first_track += track_count;
	}
	ImGui::PopStyleVar();
	return rows;
}

void SequencerPanel::draw_keys(Sequence &sequence, SequencePlayer &player, const eastl::vector<Row> &rows)
{
	const float KEY_RADIUS = 5.0f;

	ImGuiIO &io = ImGui::GetIO();
	float row_height = ImGui::GetFrameHeight();
	ImVec2 area_min = ImGui::GetCursorScreenPos();
	float area_height = glm::max(glm::max(ImGui::GetContentRegionAvail().y, rows.size() * row_height), row_height);

	struct TrackRef
	{
		SequenceBinding *binding = nullptr;
		PropertyTrack *track = nullptr;
	};
	eastl::vector<TrackRef> tracks;
	for (SequenceBinding &binding : sequence.bindings)
	{
		for (PropertyTrack &track : binding.property_tracks)
		{
			TrackRef track_ref;
			track_ref.binding = &binding;
			track_ref.track = &track;
			tracks.push_back(track_ref);
		}
	}

	// Remove selection when changed
	int track_count = (int)tracks.size();
	if (&sequence != last_sequence || track_count != last_track_count)
		selected_keys.clear();
	last_sequence = &sequence;
	last_track_count = track_count;

	auto get_row_frames = [&](const Row &row)
	{
		eastl::vector<int> frames;
		for (int i = row.first_track; i < row.first_track + row.track_count; i++)
		{
			eastl::vector<int> track_frames = tracks[i].track->getKeyFrames();
			frames.insert(frames.end(), track_frames.begin(), track_frames.end());
		}
		eastl::sort(frames.begin(), frames.end());
		frames.erase(eastl::unique(frames.begin(), frames.end()), frames.end());
		return frames;
	};
	auto get_row_keys = [&](const Row &row, int frame)
	{
		eastl::vector<KeyId> keys;
		for (int i = row.first_track; i < row.first_track + row.track_count; i++)
		{
			if (!tracks[i].track->hasKey(frame))
				continue;

			KeyId key;
			key.track_index = i;
			key.frame = frame;
			keys.push_back(key);
		}
		return keys;
	};
	auto is_selected = [&](const KeyId &key) { return eastl::find(selected_keys.begin(), selected_keys.end(), key) != selected_keys.end(); };
	auto are_selected = [&](const eastl::vector<KeyId> &keys) { return !keys.empty() && eastl::all_of(keys.begin(), keys.end(), is_selected); };
	auto set_selected = [&](const eastl::vector<KeyId> &keys, bool is_key_selected)
	{
		for (const KeyId &key : keys)
		{
			eastl::erase(selected_keys, key);
			if (is_key_selected)
				selected_keys.push_back(key);
		}
	};

	ImGui::BeginChild("##keys", ImVec2(timeline_width, area_height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();

	// Mouse
	ImGui::InvisibleButton("##keys", ImVec2(timeline_width, area_height));
	bool is_hovered = ImGui::IsItemHovered();
	bool is_active = ImGui::IsItemActive();
	bool is_additive = io.KeyCtrl || io.KeyShift;
	bool is_dragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= io.MouseDragThreshold * io.MouseDragThreshold;

	eastl::vector<KeyId> hovered_keys;
	for (const Row &row : rows)
	{
		if (!is_hovered || io.MousePos.y < row.y || io.MousePos.y >= row.y + row_height)
			continue;

		for (int frame : get_row_frames(row))
		{
			if (fabsf(frame_to_x((float)frame) - io.MousePos.x) > KEY_RADIUS + 1.0f)
				continue;

			hovered_keys = get_row_keys(row, frame);
		}
	}

	// Press
	if (ImGui::IsItemActivated())
	{
		is_moving_keys = !hovered_keys.empty() && !is_additive;
		drag_delta = 0;

		if (is_additive)
			set_selected(hovered_keys, !are_selected(hovered_keys));
		else if (!are_selected(hovered_keys))
			selected_keys = hovered_keys;
	}

	// Release without a drag
	if (ImGui::IsItemDeactivated() && is_moving_keys && !is_dragged)
		selected_keys = hovered_keys;

	// Menu
	if (is_hovered && !hovered_keys.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		if (!are_selected(hovered_keys))
			selected_keys = hovered_keys;
		ImGui::OpenPopup("##key_menu");
	}

	bool keys_changed = false;
	bool is_removing = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete);
	if (ImGui::BeginPopup("##key_menu"))
	{
		is_removing |= ImGui::MenuItem(ICON_FA_TRASH " Delete", "Del");

		eastl::vector<SequenceKey *> interpolated_keys;
		for (const KeyId &key : selected_keys)
		{
			const TrackRef &track_ref = tracks[key.track_index];
			ReflectedField field = SequencePlayer::findField(*track_ref.binding, *track_ref.track);
			if (!field.info || !PropertyTrack::canInterpolate(*field.info))
				continue;

			for (SequenceKey *channel_key : track_ref.track->findKeys(key.frame))
				interpolated_keys.push_back(channel_key);
		}

		if (!interpolated_keys.empty())
		{
			ImGui::SeparatorText("Interpolation");
			for (int i = 0; i < IM_ARRAYSIZE(sequence_interpolation_items); i++)
			{
				SequenceInterpolation interpolation = (SequenceInterpolation)i;
				bool is_current = eastl::all_of(interpolated_keys.begin(), interpolated_keys.end(), [=](const SequenceKey *key) { return key->interpolation == interpolation; });
				if (ImGui::MenuItem(sequence_interpolation_items[i], nullptr, is_current))
				{
					for (SequenceKey *key : interpolated_keys)
						key->interpolation = interpolation;
					keys_changed = true;
				}
			}
		}
		ImGui::EndPopup();
	}

	// Delete
	if (is_removing && !selected_keys.empty())
	{
		for (const KeyId &key : selected_keys)
			tracks[key.track_index].track->removeKeys(key.frame);
		selected_keys.clear();
		keys_changed = true;
	}

	// Move
	if (is_active && is_moving_keys)
	{
		int first_frame = INT_MAX;
		eastl::vector<eastl::vector<int>> selected_frames(tracks.size());
		for (const KeyId &key : selected_keys)
		{
			selected_frames[key.track_index].push_back(key.frame);
			first_frame = glm::min(first_frame, key.frame);
		}

		float dragged_frames = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x / timeline_width * (view_end_frame - view_start_frame);
		int delta = glm::max((int)roundf(dragged_frames) - drag_delta, -first_frame);

		bool can_move = delta != 0;
		for (int i = 0; i < track_count; i++)
			can_move &= tracks[i].track->canMoveKeys(selected_frames[i], delta);

		if (can_move)
		{
			for (int i = 0; i < track_count; i++)
				tracks[i].track->moveKeys(selected_frames[i], delta);
			for (KeyId &key : selected_keys)
				key.frame += delta;

			drag_delta += delta;
			keys_changed = true;
		}
	}

	// Box select
	ImVec2 box_start = io.MouseClickedPos[ImGuiMouseButton_Left];
	ImVec2 box_min(glm::min(box_start.x, io.MousePos.x), glm::min(box_start.y, io.MousePos.y));
	ImVec2 box_max(glm::max(box_start.x, io.MousePos.x), glm::max(box_start.y, io.MousePos.y));
	bool is_box_selecting = is_active && !is_moving_keys && is_dragged;
	if (is_box_selecting)
	{
		if (!is_additive)
			selected_keys.clear();

		for (const Row &row : rows)
		{
			float key_y = row.y + row_height * 0.5f;
			if (key_y < box_min.y || key_y > box_max.y)
				continue;

			for (int frame : get_row_frames(row))
			{
				float key_x = frame_to_x((float)frame);
				if (key_x >= box_min.x && key_x <= box_max.x)
					set_selected(get_row_keys(row, frame), true);
			}
		}
	}

	// Rows
	for (const Row &row : rows)
	{
		draw_list->AddRectFilled(ImVec2(area_min.x, row.y), ImVec2(area_min.x + timeline_width, row.y + row_height - 1.0f), ImGui::GetColorU32(ImGuiCol_FrameBg));
		for (int frame : get_row_frames(row))
		{
			ImVec2 center(frame_to_x((float)frame), row.y + row_height * 0.5f);
			ImU32 color = ImGui::GetColorU32(are_selected(get_row_keys(row, frame)) ? UI::accent_color : ImGui::GetStyleColorVec4(ImGuiCol_Text));
			draw_list->AddNgonFilled(center, KEY_RADIUS, color, 4);
		}
	}
	if (rows.empty())
		draw_list->AddText(ImVec2(area_min.x + ImGui::GetStyle().FramePadding.x, area_min.y + ImGui::GetStyle().FramePadding.y), ImGui::GetColorU32(ImGuiCol_TextDisabled), "Key a component field in Parameters to animate it");

	draw_outside_and_playhead(sequence, player, area_min, area_height);

	if (is_box_selecting)
	{
		draw_list->AddRectFilled(box_min, box_max, ImGui::GetColorU32(ImVec4(UI::selection_color.x, UI::selection_color.y, UI::selection_color.z, 0.4f)));
		draw_list->AddRect(box_min, box_max, ImGui::GetColorU32(UI::active_color));
	}

	if ((is_active && is_moving_keys))
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	ImGui::EndChild();

	if (keys_changed)
	{
		sequence.removeEmptyTracks();
		player.apply(sequence);
	}
}

void SequencerPanel::draw_outside_and_playhead(const Sequence &sequence, const SequencePlayer &player, ImVec2 area_min, float height)
{
	ImDrawList *draw_list = ImGui::GetWindowDrawList();
	ImVec2 area_max(area_min.x + timeline_width, area_min.y + height);

	ImU32 outside_range_color = IM_COL32(0, 0, 0, 110);
	float start_x = glm::max(frame_to_x((float)sequence.start_frame), area_min.x);
	float end_x = glm::min(frame_to_x((float)sequence.end_frame), area_max.x);
	draw_list->AddRectFilled(area_min, ImVec2(start_x, area_max.y), outside_range_color);
	draw_list->AddRectFilled(ImVec2(end_x, area_min.y), area_max, outside_range_color);

	float playhead_x = frame_to_x(player.frame);
	draw_list->AddLine(ImVec2(playhead_x, area_min.y), ImVec2(playhead_x, area_max.y), ImGui::GetColorU32(UI::accent_color), 2.0f);
}

void SequencerPanel::fit_view(const Sequence &sequence)
{
	float pad = glm::max((sequence.end_frame - sequence.start_frame) * 0.05f, 1.0f);
	view_start_frame = sequence.start_frame - pad;
	view_end_frame = sequence.end_frame + pad;
}

float SequencerPanel::frame_to_x(float frame) const
{
	return timeline_x + (frame - view_start_frame) / (view_end_frame - view_start_frame) * timeline_width;
}

float SequencerPanel::x_to_frame(float x) const
{
	return view_start_frame + (x - timeline_x) / timeline_width * (view_end_frame - view_start_frame);
}
