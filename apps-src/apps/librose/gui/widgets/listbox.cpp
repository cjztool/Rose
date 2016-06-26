/* $Id: listbox.cpp 54521 2012-06-30 17:46:54Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/listbox.hpp"

#include "gui/auxiliary/widget_definition/listbox.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/toggle_panel.hpp"

#include <boost/bind.hpp>

#include "posix2.h"

namespace gui2 {

REGISTER_WIDGET(listbox)

tlistbox::tlistbox(bool gc)
	: tscroll_container(2, true) // FIXME magic number
	, list_builder_(NULL)
	, did_row_changed_(NULL)
	, drag_started_(NULL)
	, list_grid_(NULL)
	, cursel_(NULL)
	, drag_at_(npos)
	, left_drag_grid_(NULL)
	, left_drag_grid_size_(0, 0)
	, row_align_(true)
	, gc_(gc)
	, gc_cursel_at_(twidget::npos)
	, gc_next_precise_at_(0)
{
	if (gc_) {
		row_align_ = false;
	}
}

tlistbox::~tlistbox()
{
	if (left_drag_grid_) {
		delete left_drag_grid_;
	}
}

ttoggle_panel& tlistbox::insert_row_internal(const std::map<std::string /* widget id */, string_map>& data, const int index)
{
	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(list_builder_->widgets[0]->build());
	widget->set_did_mouse_enter_leave(boost::bind(&tlistbox::did_focus_changed, this, _1, _2));
	widget->set_did_state_pre_change(boost::bind(&tlistbox::did_pre_change, this, _1));
	widget->set_did_state_changed(boost::bind(&tlistbox::did_changed, this, _1));
	widget->set_did_click(boost::bind(&tlistbox::did_click, this, _1, _2));
	widget->set_did_double_click(boost::bind(&tlistbox::did_double_click, this, _1));
	if (left_drag_grid_) {
		widget->set_callback_pre_impl_draw_children(boost::bind(&tlistbox::callback_pre_impl_draw_children, this, _1, _2, _3, _4));
		// widget->set_did_control_drag_detect(boost::bind(&tlistbox::callback_control_drag_detect, this, _1, _2, _3));
		// widget->set_did_drag_coordinate(boost::bind(&tlistbox::callback_set_drag_coordinate, this, _1, _2, _3));
	}
	widget->set_child_members(data);
	widget->at_ = list_grid_->listbox_insert_child(*widget, index);

	return *widget;

	// don't call invalidate_layout.
	// caller maybe call add_row continue, will result to large effect burden
}

ttoggle_panel& tlistbox::add_row(const std::map<std::string /* widget id */, string_map>& data, const int index)
{
	VALIDATE(!gc_, null_str);
	return insert_row_internal(data, index);
}

void tlistbox::insert_row_gc(const std::map<std::string /* widget id */, string_map>& data, const int index)
{
	VALIDATE(gc_, null_str);
	VALIDATE(index == twidget::npos, null_str);

	gc_rows_.push_back(std::unique_ptr<tgc_row>(new tgc_row(data)));
}

int tlistbox::calculate_total_height_gc() const
{
	VALIDATE(gc_, null_str);

	if (gc_next_precise_at_ == 0) {
		return 0;
	}
	const tgc_row* row = gc_rows_[gc_next_precise_at_ - 1].get();
	const int precise_height = row->distance + row->height;
	const int average_height = precise_height / gc_next_precise_at_;
	return precise_height + (gc_rows_.size() - gc_next_precise_at_) * average_height;
}

#define PRECISE_DISTANCE_THRESHOLD	500000000
#define join_estimated(value)		((value) + PRECISE_DISTANCE_THRESHOLD)
#define split_estimated(value)		((value) - PRECISE_DISTANCE_THRESHOLD)
#define is_estimated(value)			((value) != twidget::npos && (value) >= PRECISE_DISTANCE_THRESHOLD)
#define is_precise(value)			((value) != twidget::npos && (value) < PRECISE_DISTANCE_THRESHOLD)
#define distance_value(value)		(is_estimated(value)? split_estimated(value): value)

int tlistbox::which_precise_row_gc(const int distance) const
{
	VALIDATE(gc_ && gc_next_precise_at_ > 0, null_str);

	// 1. check in precise rows
	const tgc_row* current_gc_row = gc_rows_[gc_next_precise_at_ - 1].get();
	if (distance > current_gc_row->distance + current_gc_row->height) {
		return twidget::npos;
	} else if (distance == current_gc_row->distance + current_gc_row->height) {
		return gc_next_precise_at_;
	}
	const int max_one_by_one_range = 2;
	int start = 0;
	int end = gc_next_precise_at_ - 1;
	int mid = (end - start) / 2 + start;
	while (mid - start > 1) {
		current_gc_row = gc_rows_[mid].get();
		if (distance < current_gc_row->distance) {
			end = mid;

		} else if (distance > current_gc_row->distance) {
			start = mid;

		} else {
			return mid;
		}
		mid = (end - start) / 2 + start;
	}

	int row = start;
	for (; row <= end; row ++) {
		current_gc_row = gc_rows_[row].get();
		if (distance >= current_gc_row->distance && distance < current_gc_row->distance + current_gc_row->height) {
			break;
		}
	}
	VALIDATE(row <= end, null_str);

	return row;
}

int tlistbox::which_children_row_gc(const int distance) const
{
	tgc_row* current_gc_row = NULL;
	tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	VALIDATE(gc_ && childs > 0, null_str);

	const int first_at = dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_;
	const int last_at = dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_)->at_;

	const tgc_row* last_precise_row = gc_rows_[gc_next_precise_at_ - 1].get();
	const int precise_height = last_precise_row->distance + last_precise_row->height;
	const int average_height = precise_height / gc_next_precise_at_;

	current_gc_row = gc_rows_[first_at].get();
	if (distance >= distance_value(current_gc_row->distance) - average_height && distance < distance_value(current_gc_row->distance)) {
		return first_at - 1;
	}
	current_gc_row = gc_rows_[last_at].get();
	int children_end_distance = distance_value(current_gc_row->distance) + current_gc_row->height;
	if (distance >= children_end_distance && distance < children_end_distance + average_height) {
		return last_at + 1;
	}

	for (int n = 0; n < childs; n ++) {
		const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		current_gc_row = gc_rows_[widget->at_].get();
		const int distance2 = distance_value(current_gc_row->distance) + current_gc_row->height;
		if (distance >= distance_value(current_gc_row->distance) && distance < distance_value(current_gc_row->distance) + current_gc_row->height) {
			return widget->at_;
		}
	}
	return twidget::npos;
}

int tlistbox::mini_handle_gc(const int x_offset, const int y_offset)
{
	if (!gc_) {
		return y_offset;
	}

	const SDL_Rect content_rect = content_->get_rect();
	if (content_rect.h <= 0) {
		return y_offset;
	}

	int least_height = content_rect.h * 2; // 2 * content_.height
	const int half_content_height = content_rect.h / 2;
	int rows = (int)gc_rows_.size();

	// original focus widget is toggle_panel, when gc, it maybe delete. it will result to focus is null.
	// then lose event.
	const twidget* mouse_focus = get_window()->mouse_click_widget();
	while (mouse_focus && mouse_focus->get_control_type() != "toggle_panel") {
		mouse_focus = mouse_focus->parent();
	}

	tgc_row* current_gc_row = NULL;
	tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	if (childs) {
		current_gc_row = gc_rows_[gc_next_precise_at_ - 1].get();
		const int precise_height = current_gc_row->distance + current_gc_row->height;
		const int average_height = precise_height / gc_next_precise_at_;

		const int first_at = dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_;
		const int last_at = dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_)->at_;
		const tgc_row* first_gc_row = gc_rows_[first_at].get();
		const tgc_row* last_gc_row = gc_rows_[last_at].get();
		const int start_distance = y_offset >= half_content_height? y_offset - half_content_height: 0;
		int estimated_start_row;
		bool children_changed = false;
		std::pair<int, int> smooth_scroll(twidget::npos, 0);

		if (y_offset < distance_value(first_gc_row->distance)) {
			if (first_at == 0 || last_at == 0) { // first row maybe enough height.
				return y_offset;
			}

			if (mouse_focus) {
				posix_print("will up, mouse_focus: %p, mouse_focus at_: %i, [%i, %i]\n", mouse_focus, dynamic_cast<const ttoggle_panel*>(mouse_focus)->at_, first_at, last_at);
			} else {
				posix_print("will up, mouse_focus: null\n");
			}

			// yoffset----
			// --first_gc_row---
			// 1) calculate start row.
			estimated_start_row = which_precise_row_gc(start_distance);
			if (estimated_start_row == twidget::npos) {
				estimated_start_row = which_children_row_gc(start_distance);
				if (estimated_start_row == twidget::npos) {
					estimated_start_row = gc_next_precise_at_ + (start_distance - precise_height) / average_height;
					VALIDATE(estimated_start_row < rows, null_str);
				}
			}
			if (estimated_start_row >= first_at) {
				estimated_start_row = first_at - 1;
			}
			// since drag up, up one at least.
			VALIDATE(estimated_start_row < first_at, null_str);

			// 2)at top, insert row if necessary.
			current_gc_row = gc_rows_[estimated_start_row].get();

			int valid_height = 0;
			int row = estimated_start_row;
			while (valid_height < least_height && row < (int)gc_rows_.size()) {
				current_gc_row = gc_rows_[row].get();
				if (row < first_at) {
					ttoggle_panel& panel = insert_row_internal(current_gc_row->data, row - estimated_start_row);
					panel.at_ = row;
					if (did_allocated_gc_) {
						did_allocated_gc_(*this, panel);
					}

					children = list_grid_->children(); // insert maybe modify children, reload it.
					panel.fill_placeable_width(content_->get_width());
					tpoint size = panel.get_best_size();
					panel.place(tpoint(0, 0), size);

					current_gc_row->height = size.y;

				}
				VALIDATE(current_gc_row->height != twidget::npos, null_str);
				valid_height += current_gc_row->height;

				row ++;
			}

			// 3) at bottom, erase children
			int require_erase_childs = childs; // in list_grid_->children_, new row stop at it.
			if (row > first_at) { // this allocate don't include row, so don't inlucde "==".
				require_erase_childs -= row - first_at;
				smooth_scroll = std::make_pair(first_at, distance_value(first_gc_row->distance));
			}

			for (int n = 0; n < require_erase_childs; n ++) {
				// in order to avoid memcpy, erase from back to front.
				if (mouse_focus && mouse_focus == list_grid_->children()[list_grid_->children_vsize() - 1].widget_) {
					get_window()->mouse_capture(true, list_grid_->children()[0].widget_);
					posix_print("find mouse_focus\n");
					mouse_focus = NULL;
				}
				list_grid_->listbox_erase_child(list_grid_->children_vsize() - 1);
			}
			VALIDATE(list_grid_->children_vsize() > 0, null_str);

			children_changed = true;

		} else if (y_offset + content_rect.h >= distance_value(last_gc_row->distance) + last_gc_row->height) {
			if (last_at == rows - 1) {
				return y_offset;
			}

			// --last_gc_row---
			// yoffset----
			// 1) calculate start row.
			estimated_start_row = which_precise_row_gc(start_distance);
			if (estimated_start_row == twidget::npos) {
				estimated_start_row = which_children_row_gc(start_distance);
				if (estimated_start_row == twidget::npos) {
					estimated_start_row = gc_next_precise_at_ + (start_distance - precise_height) / average_height;
					VALIDATE(estimated_start_row < rows, null_str);

				}
			}

			//
			// don't use below statment. if first_at height enogh, it will result to up-down jitter.
			// 
			// if (estimated_start_row == first_at) {
			//	estimated_start_row = first_at + 1;
			// }

			// 2)at bottom, insert row if necessary.
			int start_child_index = childs; // in list_grid_->children_, new row begin from it.
			const twidget* last_child_widget = children[childs - 1].widget_;
			if (estimated_start_row <= last_at) {
				start_child_index = estimated_start_row - first_at;
				VALIDATE(start_child_index >= 0, null_str);
				smooth_scroll = std::make_pair(estimated_start_row, distance_value(gc_rows_[estimated_start_row].get()->distance));
			}

			int valid_height = 0;
			int row = estimated_start_row;
			while (valid_height < least_height && row < rows) {
				current_gc_row = gc_rows_[row].get();
				if (row > last_at) {
					ttoggle_panel& panel = insert_row_internal(current_gc_row->data, twidget::npos);
					panel.at_ = row;
					if (did_allocated_gc_) {
						did_allocated_gc_(*this, panel);
					}
					
					children = list_grid_->children(); // insert maybe modify children, reload it.
					panel.fill_placeable_width(content_->get_width());
					tpoint size = panel.get_best_size();
					panel.place(tpoint(0, 0), size);

					current_gc_row->height = size.y;
				}

				VALIDATE(current_gc_row->height != twidget::npos, null_str);
				valid_height += current_gc_row->height;

				if (valid_height >= least_height && row <= last_at) {
					least_height += current_gc_row->height;
				}
								
				row ++;

				if (row == rows && did_more_rows_gc_) {
					did_more_rows_gc_(*this);
					VALIDATE((int)gc_rows_.size() >= rows, null_str);
					rows = (int)gc_rows_.size();
				}
			}

			if (row == rows) {
				VALIDATE(first_at > 0, null_str);

				// it is skip alloate and will to end almost.
				// avoid start large height and end samll height, 
				if (valid_height < content_rect.h) {
					row = estimated_start_row - 1;
					while (valid_height < content_rect.h) {
						current_gc_row = gc_rows_[row].get();
						if (row <= last_at && start_child_index) {
							start_child_index --;
							
						} else {
							ttoggle_panel& panel = insert_row_internal(current_gc_row->data, start_child_index);
							panel.at_ = row;
							if (did_allocated_gc_) {
								did_allocated_gc_(*this, panel);
							}
					
							children = list_grid_->children(); // insert maybe modify children, reload it.
							panel.fill_placeable_width(content_->get_width());
							tpoint size = panel.get_best_size();
							panel.place(tpoint(0, 0), size);

							current_gc_row->height = size.y;

						}

						VALIDATE(current_gc_row->height != twidget::npos, null_str);
						valid_height += current_gc_row->height;
						row --;
					}
					estimated_start_row = row + 1;
					VALIDATE(estimated_start_row > 0, null_str);
				}
			}

			// 3) at top, erase children
			VALIDATE(start_child_index < list_grid_->children_vsize(), null_str);
			for (int at = 0; at < start_child_index; at ++) {
				if (mouse_focus && mouse_focus == list_grid_->children()[0].widget_) {
					posix_print("find mouse_focus\n");
					get_window()->mouse_capture(true, list_grid_->children()[start_child_index - at].widget_);
					mouse_focus = NULL;
				}
				list_grid_->listbox_erase_child(0);
			}

			children_changed = true;
		}

		if (children_changed) {
			VALIDATE(estimated_start_row >= 0, null_str);

			// 4) from top to bottom fill distance
			bool is_precise = true;

			int next_distance = twidget::npos;
			children = list_grid_->children();
			childs = list_grid_->children_vsize();
			int row = dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_;
			for (int at = 0; at < childs; at ++, row ++) {
				current_gc_row = gc_rows_[row].get();
				if (at) {
					if (is_precise) {
						current_gc_row->distance = next_distance;
					} else {
						// if before is estimated, next must not be precise.
						VALIDATE(!is_precise(current_gc_row->distance), null_str);
						current_gc_row->distance = join_estimated(next_distance);
					}

				} else {
					if (!is_precise(current_gc_row->distance)) {
						// must be skip.
						VALIDATE(row == estimated_start_row || current_gc_row->distance != twidget::npos, null_str);
						current_gc_row->distance = join_estimated(start_distance);
					}
					is_precise = !is_estimated(current_gc_row->distance);
				}
				if (is_precise && row == gc_next_precise_at_) {
					gc_next_precise_at_ ++;
				}
				next_distance = distance_value(current_gc_row->distance) + current_gc_row->height;
			}

			bool extend_precise_distance = is_precise && gc_next_precise_at_ == row;
			if (gc_next_precise_at_ == dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_) {
				VALIDATE(!is_precise, null_str);
				current_gc_row = gc_rows_[gc_next_precise_at_ - 1].get();
				next_distance = current_gc_row->distance + current_gc_row->height;

				extend_precise_distance = true;
			}
			if (extend_precise_distance) {
				VALIDATE(gc_next_precise_at_ >= 1, null_str);
				// current_gc_row = gc_rows_[gc_next_precise_at_ - 1].get();
				// next_distance = current_gc_row->distance + current_gc_row->height;
				for (; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
					current_gc_row = gc_rows_[gc_next_precise_at_].get();
					if (current_gc_row->distance == twidget::npos) {
						break;
					} else {
						VALIDATE(is_estimated(current_gc_row->distance), null_str);
						VALIDATE(current_gc_row->height != twidget::npos, null_str);
						current_gc_row->distance = next_distance;
					}
					next_distance = current_gc_row->distance + current_gc_row->height;
				}
			}
			
			int new_item_position = vertical_scrollbar_->get_item_position();
			// emport align offset. in order to smoth scroll. 
			if (smooth_scroll.first != twidget::npos) {
				current_gc_row = gc_rows_[smooth_scroll.first].get();
				if (smooth_scroll.second != distance_value(current_gc_row->distance)) {
					int distance_diff = distance_value(current_gc_row->distance) - smooth_scroll.second;
					new_item_position = vertical_scrollbar_->get_item_position() + distance_diff;
				}
			}

			// if height called base this time distance > calculate_total_height_gc, substruct children's distance to move upward.
			const int height = calculate_total_height_gc();
			int height2 = height;
			ttoggle_panel* first_children_row = dynamic_cast<ttoggle_panel*>(children[0].widget_);
			ttoggle_panel* last_children_row = dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_);
			tgc_row* first_gc_row2 = gc_rows_[first_children_row->at_].get();
			tgc_row* last_gc_row2 = gc_rows_[last_children_row->at_].get();

			if (last_children_row->at_ > gc_next_precise_at_) {
				VALIDATE(first_children_row->at_ > gc_next_precise_at_, null_str);

				const int average_height2 = precise_height / gc_next_precise_at_;
				height2 = distance_value(last_gc_row2->distance) + last_gc_row2->height;
				// bottom out children.
				height2 += average_height2 * (rows - last_children_row->at_ - 1);
			}

			const int bonus_height = height2 - height;
			if (bonus_height) {
				for (int n = 0; n < childs; n ++) {
					const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[n].widget_);
					current_gc_row = gc_rows_[widget->at_].get();
					VALIDATE(is_estimated(current_gc_row->distance), null_str);
					current_gc_row->distance = join_estimated(distance_value(current_gc_row->distance)) - bonus_height;
				}
				new_item_position -= bonus_height;
			}

			// update height
			SDL_Rect rect = list_grid_->get_rect();
			SDL_Rect content_grid_rect = content_grid_->get_rect();
			const int diff = height - rect.h;
			list_grid_->twidget::place(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h + diff));
			content_grid_->twidget::place(tpoint(content_grid_rect.x, content_grid_rect.y), tpoint(content_grid_rect.w, content_grid_rect.h + diff));

			if (diff != 0) {
				set_scrollbar_mode(vertical_scrollbar_grid_, vertical_scrollbar_,
					vertical_scrollbar_mode_,
					content_grid_->get_height(),
					content_->get_height());
			}

			if (new_item_position != vertical_scrollbar_->get_item_position()) {
				vertical_scrollbar_->set_item_position(new_item_position);
			}
		}

	} else if (!gc_rows_.empty()) {
		// since no row, must first place.
		VALIDATE(!x_offset && !y_offset, null_str);
		VALIDATE(!gc_next_precise_at_, null_str);

		// 1. construct first row, and calculate row height.
		current_gc_row = gc_rows_[0].get();
		ttoggle_panel& panel = insert_row_internal(current_gc_row->data, twidget::npos);
		if (did_allocated_gc_) {
			did_allocated_gc_(*this, panel);
		}

		panel.fill_placeable_width(content_->get_width());
		current_gc_row->height = panel.get_best_size().y;
		current_gc_row->distance = 0;

		int last_distance = current_gc_row->height, row = 1;
		while (last_distance < least_height && row < (int)gc_rows_.size()) {
			current_gc_row = gc_rows_[row].get();
			ttoggle_panel& panel = insert_row_internal(current_gc_row->data, twidget::npos);
			if (did_allocated_gc_) {
				did_allocated_gc_(*this, panel);
			}
			panel.fill_placeable_width(content_->get_width());

			current_gc_row->height = panel.get_best_size().y;
			current_gc_row->distance = last_distance;

			last_distance = current_gc_row->distance + current_gc_row->height;
			row ++;
		}

		gc_next_precise_at_ = row;
		list_grid_->validate_children_continuous();

		// update height.
		int height = calculate_total_height_gc();
		SDL_Rect rect = list_grid_->get_rect();
		SDL_Rect content_grid_rect = content_grid_->get_rect();
		const int diff = height - rect.h;
		list_grid_->twidget::place(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h + diff));
		content_grid_->twidget::place(tpoint(content_grid_rect.x, content_grid_rect.y), tpoint(content_grid_rect.w, content_grid_rect.h + diff));
	}

	return vertical_scrollbar_->get_item_position();
}

bool tlistbox::callback_control_drag_detect(tcontrol* control, bool start, const tdrag_direction type)
{
	// set_visible spend a lot of time.
	twindow::tinvalidate_layout_blocker block(*this->get_window());

	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(control);
	if (start) {
		if (drag_started_) {
			drag_started_(*this, *widget, *left_drag_grid_, widget->at_);
		}
		// app's drag_started_ maybe modify grid's content, requrie calcualte size again.
		left_drag_grid_size_ = left_drag_grid_->get_best_size();

		if (cursel_ != NULL && widget->at_ != cursel_->at_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
			cursel_->set_dirty();
		}
		left_drag_grid_->place(tpoint(content_->get_x() + content_->get_width() - left_drag_grid_size_.x, widget->get_y()),
			tpoint(left_drag_grid_size_.x, widget->get_height()));
		left_drag_grid_->set_visible(twidget::VISIBLE);
		left_drag_grid_->set_visible_area(empty_rect);

		drag_at_ = widget->at_;

	} else if (type == drag_none) {
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		widget->set_draw_offset(0, 0);

		drag_at_ = npos;

	} else if (type == drag_left) {
		widget->set_draw_offset(-1 * (content_->get_x() + content_->get_width() - left_drag_grid_->get_x()), 0);
		left_drag_grid_->set_visible_area(left_drag_grid_->get_rect());
	}

	return true;
}

void tlistbox::callback_pre_impl_draw_children(tcontrol* control, texture& frame_buffer, int x_offset, int y_offset)
{
	if (left_drag_grid_->get_visible() == twidget::VISIBLE) {
		left_drag_grid_->impl_draw_children(frame_buffer, 0, 0);
	}
}

bool tlistbox::callback_set_drag_coordinate(tcontrol* control, const tpoint& first, const tpoint& last)
{
	if (left_drag_grid_->get_visible() == twidget::VISIBLE) {
		int diff = first.x - last.x;
		if (diff > left_drag_grid_size_.x) {
			diff = left_drag_grid_size_.x;
		} else if (diff < 0) {
			diff = 0;
		}
		SDL_Rect area = ::create_rect(left_drag_grid_->get_x() + left_drag_grid_size_.x - diff, left_drag_grid_->get_y(), 
			diff, left_drag_grid_->get_height());
		left_drag_grid_->set_visible_area(area);
	}

	return true;
}

void tlistbox::cancel_drag()
{
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		// set_visible spend a lot of time.
		twindow::tinvalidate_layout_blocker block(*this->get_window());

		VALIDATE(drag_at_ >= 0 && drag_at_ < list_grid_->children_vsize(), null_str);
		dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_)->set_draw_offset(0, 0);
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		drag_at_ = npos;
	}
}

// if count equal 0, remove all.
void tlistbox::remove_row(int row, int count)
{
	if (!list_grid_->children_vsize()) {
		return;
	}
	// cursel_ maybe NULL. when all item are invisible!
	const int first_row_at = dynamic_cast<ttoggle_panel*>(list_grid_->children()[0].widget_)->at_;
	int childs = list_grid_->children_vsize();
	if (!count) {
		row = first_row_at;
		count = childs;

	} else {
		VALIDATE(row >= first_row_at && row < first_row_at + childs, null_str);
		if (row + count > first_row_at + childs) {
			count = first_row_at + childs - row;
		}
	}
	
	const int original_cursel = cursel_? cursel_->at_: npos;
	bool remove_all = count == childs;
	bool cursel_is_remove = cursel_ != NULL && (cursel_->at_ >= row && cursel_->at_ < row + count);
	bool drag_is_remove = drag_at_ != npos && (drag_at_ >= row && drag_at_ < row + count);

	for (; count; -- count) {
		list_grid_->listbox_erase_child(row - first_row_at);
	}

	// update subsequent panel's at_.
	tgrid::tchild* children = list_grid_->children();
	childs = list_grid_->children_vsize();
	for (int at = row - first_row_at; at < childs; at ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
		widget->at_ = at + first_row_at;
	}

	if (!remove_all) {
		if (cursel_is_remove) {
			VALIDATE(original_cursel, null_str);
			ttoggle_panel* widget = get_row_panel(original_cursel);
			if (widget && !widget->can_selectable()) {
				widget = next_selectable_row(original_cursel, true);
			}
			if (widget) {
				widget->set_value(true);
			} else {
				select_row_internal(nullptr);
			}
		}
	} else {
		cursel_ = NULL;
	}

	if (drag_is_remove) {
		// set_visible spend a lot of time.
		twindow::tinvalidate_layout_blocker block(*this->get_window());

		VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		drag_at_ = npos;
	}

	// don't call invalidate_layout(true).
	// caller maybe call remove_row continue, will result to large effect burden
	invalidate_layout(false);
}

void tlistbox::clear()
{
	// Due to the removing from the linked group, don't use
	remove_row(0, 0);
}

class sort_func
{
public:
	sort_func(void* caller, bool (*callback)(void*, twidget&, twidget&)) : caller_(caller), callback_(callback)
	{}

	bool operator()(twidget* a, twidget* b) const
	{
		return callback_(caller_, *a, *b);
	}

private:
	void* caller_;
	bool (*callback_)(void*, twidget&, twidget&);
};

void tlistbox::sort(void* caller, bool (*callback)(void*, twidget&, twidget&))
{
	tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	if (!childs) {
		return;
	}

	ttoggle_panel* original_cursel = cursel_;
	bool check_cursel = cursel_ != NULL;

	std::vector<twidget*> tmp;
	tmp.resize(childs, NULL);
	for (int n = 0; n < childs; n ++) {
		tmp[n] = children[n].widget_;
	}
	std::stable_sort(tmp.begin(), tmp.end(), sort_func(caller, callback));
	for (int n = 0; n < childs; n ++) {
		children[n].widget_ = tmp[n];
		ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		if (check_cursel && panel->at_ == cursel_->at_) {
			cursel_ = panel;
			check_cursel = false;
		}
		panel->at_ = n;
	}
}

int tlistbox::get_item_count() const
{
	return list_grid_->children_vsize();
}

void tlistbox::set_row_active(const unsigned row, const bool active)
{
	tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(0, 0).widget_);
	widget->set_active(active);
}

void tlistbox::set_row_shown(const int row, const bool visible)
{
	if (row < 0 || row >= get_item_count()) {
		return;
	}
	twidget* widget = list_grid_->child(row, 0).widget_;
	if (visible) {
		if (widget->get_visible() == twidget::VISIBLE) {
			return;
		}
	} else if (widget->get_visible() == twidget::INVISIBLE) {
		return;
	}

	{
		twindow *window = get_window();
		twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*window);
		
		widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
		if (!visible) {
			ttoggle_panel* original_cursel = cursel_;
			cursel_ = NULL;
			ttoggle_panel* widget = next_selectable_row(original_cursel? original_cursel->at_: twidget::npos, true);
			if (widget) {
				widget->set_value(true);
			}
		}
	}

	invalidate_layout(false);
}

void tlistbox::set_row_shown(const std::vector<bool>& shown)
{
	if (!get_item_count()) {
		return;
	}

	VALIDATE(false, "Don't support!");
}

ttoggle_panel* tlistbox::get_row_panel(const unsigned row) const
{
	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	VALIDATE(row != npos, null_str);

	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(list_grid_->child(0, 0).widget_);
	if (widget->at_ == 0) {
		VALIDATE((int)row < childs, null_str);
		return dynamic_cast<ttoggle_panel*>(children[row].widget_);
	}

	VALIDATE(gc_, null_str);
	for (int at = 0; at < childs; at ++) {
		ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(list_grid_->child(at, 0).widget_);
		if (panel->at_ == row) {
			return panel;
		}
	}

	return nullptr;
}

bool tlistbox::select_row(const int row, const bool from_ui)
{
	ttoggle_panel* desire_widget = nullptr;
	if (row != twidget::npos) {
		const tgrid::tchild* children = list_grid_->children();
		int childs = list_grid_->children_vsize();
		if (row >= childs) {
			return false;
		} else if (children[row].widget_ == cursel_) {
			return true;
		}

		desire_widget = get_row_panel(row);
	}

	if (desire_widget) {
		// set_value-->did_changed-->select_row_internal
		const bool ret = desire_widget->set_value(true, from_ui);
		if (from_ui || !ret) {
			return ret;
		}
	}
	select_row_internal(desire_widget);
	return true;
}

// @widget: maybe null.
void tlistbox::select_row_internal(twidget* widget)
{
	ttoggle_panel* desire_panel = nullptr;
	if (widget) {
		desire_panel = dynamic_cast<ttoggle_panel*>(widget);
		VALIDATE(!cursel_ || desire_panel->at_ != cursel_->at_, null_str);
	}
	if (cursel_ != nullptr) {
		VALIDATE(cursel_->get_value(), "Previous toggle panel must be selected!");
		cursel_->set_value(false);
		cursel_->set_dirty();
	}

	if (desire_panel) {
		VALIDATE(desire_panel->get_value(), "Previous code must select desire panel!");
		// desire_panel->set_value(true);
		cursel_ = desire_panel;
		desire_panel->set_dirty();
	} else {
		cursel_ = nullptr;
	}
}

ttoggle_panel* tlistbox::cursel() const
{
	return cursel_;
}

void tlistbox::did_focus_changed(ttoggle_panel& widget, const bool enter)
{
	if (did_row_focus_changed_) {
		did_row_focus_changed_(*this, widget, enter);
	}
}

bool tlistbox::did_pre_change(ttoggle_panel& widget)
{
	if (did_row_pre_change_) {
		return did_row_pre_change_(*this, widget);
	}
	return true;
}

void tlistbox::did_changed(ttoggle_panel& widget)
{
	if (&widget == cursel_) {
		return;
	}

	if (cursel_ != NULL) {
		cursel_->set_draw_offset(0, 0);
	}

	select_row_internal(&widget);
	
	if (did_row_changed_) {
		did_row_changed_(*this, *cursel_);
	}
}

void tlistbox::did_click(ttoggle_panel& widget, const int type)
{
	/** @todo Hack to capture the keyboard focus. */
	get_window()->keyboard_capture(this);

	if (did_row_click_) {
		did_row_click_(*this, widget, type);
	}
}

void tlistbox::did_double_click(ttoggle_panel& widget)
{
	if (did_row_double_click_) {
		did_row_double_click_(*this, widget);
	}
}

void tlistbox::place(const tpoint& origin, const tpoint& size)
{
	// Inherited.
	tscroll_container::place(origin, size);

	/**
	 * @todo Work-around to set the selected item visible again.
	 *
	 * At the moment the listboxes and dialogs in general are resized a lot as
	 * work-around for sizing. So this function makes the selected item in view
	 * again. It doesn't work great in all cases but the proper fix is to avoid
	 * resizing dialogs a lot. Need more work later on.
	 */
	if (cursel_) {
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = list_grid_->child(cursel_->at_, 0).widget_->get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		// show_content_rect(rect);
		scrollbar_moved();
	}
}

void tlistbox::list_place(const tpoint& origin, const tpoint& size)
{
	list_grid_->tgrid::place(origin, size);
}

tpoint tlistbox::list_calculate_best_size()
{
	tpoint size = list_grid_->tgrid::calculate_best_size();
	if (gc_) {
		size.y = calculate_total_height_gc();
	}
	return size;
}

void tlistbox::adjust_offset(int& x_offset, int& y_offset)
{
	if (!row_align_) {
		return;
	}

	unsigned items = list_grid_->children_vsize();
	if (!items || !y_offset) {
		return;
	}
	int height = list_grid_->child(0, 0).widget_->get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
	}
}

void tlistbox::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	// Here need call twidget::set_visible_area in conent_grid_ only.
	content_grid()->twidget::set_origin(content_origin);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();

	header->set_origin(tpoint(content_origin.x, origin.y));
	list_grid_->set_origin(tpoint(content_origin.x, content_origin.y + size.y));

	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		const twidget* parent = list_grid_->child(drag_at_).widget_;
		left_drag_grid_->set_origin(tpoint(left_drag_grid_->get_x(), parent->get_y()));	
	}
}

void tlistbox::mini_set_content_grid_visible_area(const SDL_Rect& area)
{
	// Here need call twidget::set_visible_area in conent_gri_ only.
	content_grid()->twidget::set_visible_area(area);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();
	SDL_Rect header_area = intersect_rects(area, header->get_rect());
	header->set_visible_area(header_area);
	
	SDL_Rect list_area = area;
	list_area.y = area.y + size.y;
	list_area.h = area.h - size.y;
	list_grid_->set_visible_area(list_area);
}

twidget* tlistbox::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	twidget* result = NULL;
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_);
/*
		if (!widget->drag_detect_started()) { // remark because of drag
			result = left_drag_grid_->find_at(coordinate, must_be_active);
		}
*/
	}
	if (!result) {
		result = tscroll_container::find_at(coordinate, must_be_active);
	}
	return result;
}

void tlistbox::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		std::vector<twidget*> child_call_stack(call_stack);
		child_call_stack.push_back(list_grid_->child(drag_at_).widget_);
		left_drag_grid_->populate_dirty_list(caller, child_call_stack);
	}

	tscroll_container::child_populate_dirty_list(caller, call_stack);
}

void tlistbox::scroll_to_row(const unsigned row)
{
	//
	// This function only call list is complete! that mean no modify.
	//

	if ((int)row >= get_item_count()) {
		return;
	}

	const SDL_Rect& visible = content_visible_area();
	SDL_Rect rect = list_grid_->child(0, row).widget_->get_rect();

	rect.x = visible.x;
	if (rect.y < visible.y) {
		tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
		int header_height = header->get_best_size().y;
		rect.y -= header_height;
	}
	rect.w = visible.w;

	show_content_rect(rect);
}

bool tlistbox::list_grid_handle_key_up_arrow()
{
	if (cursel_ == NULL) {
		return false;
	}

	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();

	if (!childs) {
		return false;
	}

	const int cursel_at = cursel_->at_; // in gc, mini_handle_gc maybe delete cursel_.
	if (gc_ && cursel_at == dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_) {
		if (dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_ > 0) {
			const twidget* widget = children[0].widget_;
			const int orginal_x = widget->get_x();
			// use last row distance in children_.
			const int content_grid_y = distance_value(gc_rows_[cursel_at]->distance);
			mini_handle_gc(content_->get_x() - content_grid_->get_x(), content_grid_y - 1);

			children = list_grid_->children();
			childs = list_grid_->children_vsize();

			twidget* new_widget = get_row_panel(cursel_at - 1); // show_content_rect require right origin.
			VALIDATE(new_widget, null_str);
			// new_widget->set_origin(tpoint(widget->get_x(), widget->get_y() - new_widget->get_height()));
			const tgc_row* current_gc_row = gc_rows_[cursel_at - 1].get();
			int origin_y = distance_value(current_gc_row->distance) + (content_->get_y() - vertical_scrollbar_->get_item_position());
			{
				tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
				int header_height = header->get_best_size().y;
				origin_y += header_height;
			}

			new_widget->set_origin(tpoint(orginal_x, origin_y));
		}
	}

	// after mini_handle_gc, cursel_ may be deleted. if selecting is true, deleted.
	bool selecting = dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_)->at_ < cursel_at;
	for (int n = childs - 1; n >= 0; -- n) {
		// NOTE we check the first widget to be active since grids have no
		// active flag. This method might not be entirely reliable.
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		if (!selecting) {
			if (widget->at_ == cursel_at) {
				selecting = true;
			}
			continue;
		}
		if (widget->can_selectable()) {
			return widget->set_value(true, true);
		}
	}
	return false;
}

// invert: if can not find next, invert=false think find fail immeditaly. invert=true, will find before continue.
ttoggle_panel* tlistbox::next_selectable_row(int start_at, bool invert)
{
	if (start_at == npos) {
		return NULL;
	}

	const tgrid::tchild* children = list_grid_->children();
	int childs = list_grid_->children_vsize();
	if (!childs) {
		return NULL;
	}

	if (gc_ && start_at == dynamic_cast<ttoggle_panel*>(children[childs - 1].widget_)->at_) {
		if (start_at < (int)gc_rows_.size() - 1) {
			const twidget* widget = children[childs - 1].widget_;
			const int orginal_x = widget->get_x();
			// use last row distance in children_.
			const int distance = distance_value(gc_rows_[start_at]->distance) + gc_rows_[start_at]->height;
			mini_handle_gc(content_->get_x() - widget->get_x(), distance - content_->get_height());

			children = list_grid_->children();
			childs = list_grid_->children_vsize();
			twidget* new_widget = get_row_panel(start_at + 1); // show_content_rect require right origin.
			VALIDATE(new_widget, null_str);
			// new_widget->set_origin(tpoint(widget->get_x(), widget->get_y() + widget->get_height()));
			const tgc_row* current_gc_row = gc_rows_[start_at + 1].get();
			new_widget->set_origin(tpoint(orginal_x, 
				distance_value(current_gc_row->distance) + (content_->get_y() - vertical_scrollbar_->get_item_position())));
		}
	}

	// after mini_handle_gc, start_at may be deleted. if selecting is true, deleted.
	int start_index = dynamic_cast<ttoggle_panel*>(children[0].widget_)->at_ <= start_at? npos: childs;
	for (int i = 0; i < childs; i ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[i].widget_);
		if (start_index == npos) {
			if (widget->at_ == start_at) {
				start_index = i;
			}
			continue;
		}
		if (widget->can_selectable()) {
			return widget;
		}
	}
	if (!invert || start_index == npos) {
		return NULL;
	}
	for (int i = start_index - 1; i >= 0; i --) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[i].widget_);
		if (widget->can_selectable()) {
			return widget;
		}
	}
	return NULL;
}

bool tlistbox::list_grid_handle_key_down_arrow()
{
	if (cursel_ == NULL) {
		return false;
	}

	ttoggle_panel* valid = next_selectable_row(cursel_->at_, false);
	if (valid) {
		return valid->set_value(true, true);
	}
	return false;
}

void tlistbox::handle_key_up_arrow(SDL_Keymod modifier, bool& handled)
{
	bool changed = list_grid_handle_key_up_arrow();

	if (changed) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = cursel_->get_rect();

		rect.x = visible.x;
		// if (rect.y < visible.y) {
			// make rows overlapped by header move downward.
			tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
			int header_height = header->get_best_size().y;
			rect.y -= header_height;
		// }
		rect.w = visible.w;

		show_content_rect(rect);
	}

	handled = true;
}

void tlistbox::handle_key_down_arrow(SDL_Keymod modifier, bool& handled)
{
	bool changed = list_grid_handle_key_down_arrow();

	if (changed) {
		// When scrolling make sure the new items is visible but leave the
		// horizontal scrollbar position.
		const SDL_Rect& visible = content_visible_area();
		SDL_Rect rect = cursel_->get_rect();

		rect.x = visible.x;
		rect.w = visible.w;

		show_content_rect(rect);
	}

	handled = true;
}

//
// listbox
//
void tlistbox::tgrid3::listbox_init()
{
	VALIDATE(!children_vsize_, "Must no child in report!");
	VALIDATE(!rows_ && !cols_, "Must no row and col in report!");

	cols_ = 1; // It is toggle_panel widget.
	col_grow_factor_.push_back(0);
}

int tlistbox::tgrid3::listbox_insert_child(twidget& widget, int at)
{
	// make sure the new child is valid before deferring
	widget.set_parent(this);
	if (at == npos || at > children_vsize_) {
		at = children_vsize_;
	}

	// below memmove require children_size_ are large than children_vsize_!
	// resize_children require large than exactly rows_*cols_. because children_vsize_ maybe equal it.

	// i think, all grow factor is 0. except last stuff.
	row_grow_factor_.push_back(0);
	rows_ ++;
	row_height_.push_back(0);

	resize_children((rows_ * cols_) + 1);
	if (children_vsize_ - at) {
		memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
	}

	children_[at].widget_ = &widget;
	children_vsize_ ++;

	// replacement
	children_[at].flags_ = VERTICAL_ALIGN_TOP | HORIZONTAL_GROW_SEND_TO_CLIENT;

	return at;
}

void tlistbox::tgrid3::listbox_erase_child(int at)
{
	if (at == npos || at >= children_vsize_) {
		return;
	}
	if (children_[at].widget_ == listbox_.cursel_) {
		listbox_.gc_cursel_at_ = listbox_.cursel_->at_;
		listbox_.select_row_internal(nullptr);
	}
	delete children_[at].widget_;
	children_[at].widget_ = NULL;
	if (at < children_vsize_ - 1) {
		memcpy(&(children_[at]), &(children_[at + 1]), (children_vsize_ - at - 1) * sizeof(tchild));
	}
	children_[children_vsize_ - 1].widget_ = NULL;

	children_vsize_ --;
	rows_ --;
	row_grow_factor_.pop_back();
	row_height_.pop_back();
}

void tlistbox::tgrid3::validate_children_continuous() const
{
	int next_row_at = twidget::npos;
	for (int at = 0; at < children_vsize_; at ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at].widget_);
		if (next_row_at == twidget::npos) {
			next_row_at = widget->at_ + 1;
			continue;
		}
		VALIDATE(next_row_at == widget->at_, null_str);

		next_row_at ++;
	}
}

void tlistbox::tgrid3::set_origin(const tpoint& origin)
{
	if (!listbox_.gc_) {
		tgrid::set_origin(origin);
		return;
	}

	// Inherited.
	twidget::set_origin(origin);

	if (!children_vsize_) {
		return;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[n].widget_);
		VALIDATE(widget, null_str);

		const tgc_row* current_gc_row = listbox_.gc_rows_[widget->at_].get();
		// distance  
		//    0    ---> origin.y
		//    1    ---> origin.y + 1
		widget->set_origin(tpoint(origin.x, origin.y + distance_value(current_gc_row->distance)));
	}

}

namespace {

/**
 * Swaps an item in a grid for another one.*/
void swap_grid(tgrid* grid,
		tgrid* content_grid, twidget* widget, const std::string& id)
{
	assert(content_grid);
	assert(widget);

	// Make sure the new child has same id.
	widget->set_id(id);

	// Get the container containing the wanted widget.
	tgrid* parent_grid = NULL;
	if(grid) {
		parent_grid = find_widget<tgrid>(grid, id, false, false);
	}
	if(!parent_grid) {
		parent_grid = find_widget<tgrid>(content_grid, id, true, false);
	}
	parent_grid = dynamic_cast<tgrid*>(parent_grid->parent());
	assert(parent_grid);

	// Replace the child.
	widget = parent_grid->swap_child(id, widget, false);
	assert(widget);

	delete widget;
}

} // namespace

void tlistbox::finalize(
		tbuilder_grid_const_ptr header,
		tbuilder_grid_const_ptr footer,
		const std::vector<string_map>& list_data)
{
	// "Inherited."
	tscroll_container::finalize_setup();

	if (header) {
		swap_grid(&grid(), content_grid(), header->build(), "_header_grid");
	}

	if (footer) {
		swap_grid(&grid(), content_grid(), footer->build(), "_footer_grid");
	}

	list_grid_ = new tgrid3(*this);
	list_grid_->listbox_init();
	swap_grid(NULL, content_grid(), list_grid_, "_list_grid");
}

void tlistbox::mini_place_content_grid(const tpoint& content_origin, const tpoint& desire_size, const tpoint& origin)
{
	tpoint size = desire_size;
	unsigned items = list_grid_->children_vsize();
	if (row_align_ && items) {
		tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
		// by this time, hasn't called place(), cannot use get_size().
		int header_height = header->get_best_size().y;
		int height = list_grid_->child(0, 0).widget_->get_best_size().y;
		if (header_height + height <= size.y) {
			int list_height = size.y - header_height;
			list_height = list_height / height * height;

			// reduce hight if allow height > header_height + get_best_size().y
			height = list_grid_->get_best_size().y;
			if (list_height > height) {
				list_height = height;
			}

			size.y = header_height + list_height;

			if (size.y != desire_size.y) {
				content_->set_size(size);
			}
		}
	}

	content_grid_->fill_placeable_width(size.x);

	const tpoint actual_size = content_grid_->get_best_size();
	calculate_scrollbar(actual_size, size);

	if (size.y != desire_size.y) {
		int diff_y = desire_size.y - size.y;
		if (vertical_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
			tpoint vertical_size = vertical_scrollbar_grid_->get_size();
			vertical_size.y -= diff_y;
			vertical_scrollbar_grid_->place(vertical_scrollbar_grid_->get_origin(), vertical_size);
		}
		if (horizontal_scrollbar_grid_->get_visible() == twidget::VISIBLE) {
			tpoint horizontal_origin = horizontal_scrollbar_grid_->get_origin();
			horizontal_origin.y -= diff_y;
			horizontal_scrollbar_grid_->set_origin(horizontal_origin);
		}
	}

	size.x = std::max(actual_size.x, size.x);
	size.y = std::max(actual_size.y, size.y);

	if (!simple_place) {
/*
		if (id() == "default") {
			posix_print("1, listbox, origin: (%i, %i), size: (%i, %i), actual_size: (%i, %i)\n", origin.x, origin.y, size.x, size.y, actual_size.x, actual_size.y);
		}
*/
		content_grid_->place(origin, size);
	} else {
/*
		if (id() == "default") {
			posix_print("2, listbox, origin: (%i, %i), size: (%i, %i), actual_size: (%i, %i)\n", origin.x, origin.y, size.x, size.y, actual_size.x, actual_size.y);
		}
*/
		content_grid_->twidget::place(origin, size);
	}
}

void tlistbox::set_list_builder(tbuilder_grid_ptr list_builder)
{ 
	VALIDATE(!list_builder_.get(), null_str);

	list_builder_ = list_builder;
	if (list_builder_->widgets.size() >= 2) {
		left_drag_grid_ = dynamic_cast<tgrid*>(list_builder_->widgets[1]->build());
		left_drag_grid_->set_visible(twidget::INVISIBLE);

		// don't set row(toggle_panel) to it's parent.
		// grid mayb containt "erase" button, it will erase row(toggle_panel). if it is parent, thine became complex.
		left_drag_grid_->set_parent(this);
	}
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}

} // namespace gui2