/* $Id: scrollbar_container.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/scroll_container.hpp"

#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/auxiliary/timer.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "posix2.h"

namespace gui2 {

tscroll_container::tscroll_container(const unsigned canvas_count, bool listbox)
	: tcontainer_(canvas_count)
	, listbox_(listbox)
	, state_(ENABLED)
	, vertical_scrollbar_mode_(auto_visible)
	, horizontal_scrollbar_mode_(auto_visible)
	, vertical_scrollbar_grid_(NULL)
	, horizontal_scrollbar_grid_(NULL)
	, vertical_scrollbar_(NULL)
	, horizontal_scrollbar_(NULL)
	, content_grid_(NULL)
	, content_(NULL)
	, content_visible_area_()
	, scroll_to_end_(false)
	, calculate_reduce_(false)
	, need_layout_(false)
	, find_content_grid_(false)
	, scroll_timer_(INVALID_TIMER_ID)
	, first_coordinate_(construct_null_coordinate())
	, require_capture_(true)
	, clear_click_threshold_(2 * twidget::hdpi_scale)
{
	set_container_grid(grid_);
	grid_.set_parent(this);

	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
			&tscroll_container::signal_handler_sdl_key_down
				, this, _2, _3, _5, _6));
}

tscroll_container::~tscroll_container() 
{ 
	if (scroll_timer_ != INVALID_TIMER_ID) {
		remove_timer(scroll_timer_);
	}
	delete content_grid_; 
}

void tscroll_container::layout_init()
{
	content_grid_->layout_init();
}

tpoint tscroll_container::scrollbar_size(const tgrid& scrollbar_grid, tscrollbar_mode scrollbar_mode) const
{
	if (scrollbar_mode == auto_visible) {
		return scrollbar_grid.get_best_size();
	}
	return tpoint(0, 0);
}

tpoint tscroll_container::mini_get_best_text_size() const
{
	const tpoint vertical_scrollbar_size = scrollbar_size(*vertical_scrollbar_grid_, vertical_scrollbar_mode_);
	const tpoint horizontal_scrollbar_size = scrollbar_size(*horizontal_scrollbar_grid_, horizontal_scrollbar_mode_);

	tpoint result = content_grid_->calculate_best_size();

	// make sure can place tow scrollbar.
	if (result.x < horizontal_scrollbar_size.x) {
		result.x = horizontal_scrollbar_size.x;
	}
	if (result.y < vertical_scrollbar_size.y) {
		result.y = vertical_scrollbar_size.y;
	}
	// if exist, add scrollbar's size.
	result.x += vertical_scrollbar_size.x;
	result.y += horizontal_scrollbar_size.y;

	return result;
}

tpoint tscroll_container::calculate_best_size() const
{
	// make grid of tcontainer_ have right col_width_'s count and row_height_'s count.
	tpoint container = tcontainer_::calculate_best_size();

	tpoint result = tcontrol::calculate_best_size();

	if (result.x < container.x) {
		result.x = container.x;
	}
	if (result.y < container.y) {
		result.y = container.y;
	}
	return result;
}

void tscroll_container::set_scrollbar_mode(tgrid* scrollbar_grid, tscrollbar_* scrollbar,
		tscroll_container::tscrollbar_mode& scrollbar_mode,
		const unsigned items, const unsigned visible_items)
{
/*
	if (scrollbar_mode == tscroll_container::always_invisible) {
		scrollbar_grid->set_visible(twidget::INVISIBLE);
		return;
	}
*/
	scrollbar->set_item_count(items);
	scrollbar->set_visible_items(visible_items);
	// old item_position maybe overflow the new boundary.
	if (scrollbar->get_item_position()) {
		scrollbar->set_item_position(scrollbar->get_item_position());
	}

	if (scrollbar_mode == tscroll_container::auto_visible) {
		const bool scrollbar_needed = items > visible_items;

		scrollbar_grid->set_visible(scrollbar_needed? twidget::VISIBLE: twidget::HIDDEN);
	}
}

void tscroll_container::place(const tpoint& origin, const tpoint& size)
{
	need_layout_ = false;

	// Inherited.
	tcontainer_::place(origin, size);

	// if ((!size.x || !size.y) && mini_content_empty()) {
	if (!size.x || !size.y) {
		return;
	}

	const tpoint content_origin = content_->get_origin();
	mini_handle_gc(0, 0);
	mini_place_content_grid(content_->get_origin(), content_->get_size(), content_origin);

	// Set vertical scrollbar
	set_scrollbar_mode(vertical_scrollbar_grid_, vertical_scrollbar_,
			vertical_scrollbar_mode_,
			content_grid_->get_height(),
			content_->get_height());

	// Set horizontal scrollbar
	set_scrollbar_mode(horizontal_scrollbar_grid_, horizontal_scrollbar_,
			horizontal_scrollbar_mode_,
			content_grid_->get_width(),
			content_->get_width());

	content_visible_area_ = content_->get_rect();
	if (!scroll_to_end_) {
		const unsigned x_offset = horizontal_scrollbar_->get_item_position();
		const unsigned y_offset = vertical_scrollbar_->get_item_position();
		mini_handle_gc(x_offset, y_offset);
		if (x_offset || y_offset) {
			// if previous exist item_postion, recover it.
			const tpoint content_grid_origin = tpoint(content_->get_x() - x_offset, content_->get_y() - y_offset);
			mini_set_content_grid_origin(content_->get_origin(), content_grid_origin);
		}

		// Now set the visible part of the content.
		mini_set_content_grid_visible_area(content_visible_area_);

	} else {
		scroll_vertical_scrollbar(tscrollbar_::END);
	}
}

tpoint tscroll_container::validate_content_grid_origin(const tpoint& content_origin, const tpoint& content_size, const tpoint& origin, const tpoint& size) const
{
	// verify desire_origin
	//  content_grid origin---> | <--- desire_origin
	//                          | <--- vertical scrollbar's item_position
	//  content origin -------> |
	//  content size            |
	tpoint origin2 = origin;
	VALIDATE(origin2.y <= content_origin.y, "y of content_grid must <= content.y!");
	VALIDATE(size.y >= content_size.y, "content_grid must >= content!");

	int item_position = (int)vertical_scrollbar_->get_item_position();
	if (origin2.y + item_position != content_origin.y) {
		origin2.y = content_origin.y - item_position;
	}
	if (item_position + content_size.y > size.y) {
		item_position = size.y - content_size.y;
		vertical_scrollbar_->set_item_position2(item_position);

		origin2.y = content_origin.y - item_position;
	}

	VALIDATE(origin2.y <= content_origin.y, "(2)y of content_grid must <= content.y!");
	return origin2;
}

void tscroll_container::set_origin(const tpoint& origin)
{
	// Inherited.
	tcontainer_::set_origin(origin);

	const tpoint content_origin = content_->get_origin();
	mini_set_content_grid_origin(origin, content_origin);

	// Changing the origin also invalidates the visible area.
	mini_set_content_grid_visible_area(content_visible_area_);
}

void tscroll_container::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_grid_origin)
{
	content_grid_->set_origin(content_grid_origin);
}

void tscroll_container::mini_set_content_grid_visible_area(const SDL_Rect& area)
{
	content_grid_->set_visible_area(area);
}

void tscroll_container::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	tcontainer_::set_visible_area(area);

	// Now get the visible part of the content.
	content_visible_area_ = intersect_rects(area, content_->get_rect());

	content_grid_->set_visible_area(content_visible_area_);
}

twidget* tscroll_container::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	VALIDATE(content_ && content_grid_, null_str);

	twidget* result = tcontainer_::find_at(coordinate, must_be_active);
	if (result == content_) {
		result = content_grid_->find_at(coordinate, must_be_active);
		if (!result) {
			// to support SDL_WHEEL_DOWN/SDL_WHEEL_UP, must can find at "empty" area.
			result = content_grid_;
		}
	}
	return result;
}

twidget* tscroll_container::find(const std::string& id, const bool must_be_active)
{
	// Inherited.
	twidget* result = tcontainer_::find(id, must_be_active);

	// Can be called before finalize so test instead of assert for the grid.
	if (!result && find_content_grid_ && content_grid_) {
		result = content_grid_->find(id, must_be_active);
	}

	return result;
}

const twidget* tscroll_container::find(const std::string& id, const bool must_be_active) const
{
	// Inherited.
	const twidget* result = tcontainer_::find(id, must_be_active);

	// Can be called before finalize so test instead of assert for the grid.
	if (!result && find_content_grid_ && content_grid_) {
		result = content_grid_->find(id, must_be_active);
	}
	return result;
}

bool tscroll_container::disable_click_dismiss() const
{
	// return tcontainer_::disable_click_dismiss() || content_grid_->disable_click_dismiss();
	return content_grid_->disable_click_dismiss();
}

void tscroll_container::finalize_setup()
{
	/***** Setup vertical scrollbar *****/
	vertical_scrollbar_grid_ = find_widget<tgrid>(this, "_vertical_scrollbar_grid", false, true);
	vertical_scrollbar_grid_->set_visible(vertical_scrollbar_mode_ == auto_visible? twidget::HIDDEN: twidget::INVISIBLE);

	vertical_scrollbar_ = find_widget<tscrollbar_>(vertical_scrollbar_grid_, "_vertical_scrollbar", false, true);
	connect_signal_notify_modified(*vertical_scrollbar_
			, boost::bind(
				  &tscroll_container::vertical_scrollbar_moved
				, this));

	/***** Setup horizontal scrollbar *****/
	horizontal_scrollbar_grid_ = find_widget<tgrid>(this, "_horizontal_scrollbar_grid", false, true);
	horizontal_scrollbar_grid_->set_visible(horizontal_scrollbar_mode_ == auto_visible? twidget::HIDDEN: twidget::INVISIBLE);

	horizontal_scrollbar_ = find_widget<tscrollbar_>(horizontal_scrollbar_grid_, "_horizontal_scrollbar", false, true);
	connect_signal_notify_modified(*horizontal_scrollbar_
			, boost::bind(
				  &tscroll_container::horizontal_scrollbar_moved
				, this));

	/***** Setup the content *****/
	content_ = new tspacer();
	content_->set_definition("default");

	content_grid_ = dynamic_cast<tgrid*>(
			grid().swap_child("_content_grid", content_, true));
	assert(content_grid_);

	content_grid_->set_parent(this);
	/***** Let our subclasses initialize themselves. *****/
	finalize_subclass();

	{
		content_grid_->connect_signal<event::WHEEL_UP>(
			boost::bind(
				  &tscroll_container::signal_handler_sdl_wheel_up
				, this
				, _3
				, _6)
			, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_UP>(
			boost::bind(
				  &tscroll_container::signal_handler_sdl_wheel_up
				, this
				, _3
				, _6)
			, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_down
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_down
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_LEFT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_left
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_LEFT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_left
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::WHEEL_RIGHT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_right
					, this
					, _3
					, _6)
				, event::tdispatcher::back_post_child);

		content_grid_->connect_signal<event::WHEEL_RIGHT>(
				boost::bind(
					&tscroll_container::signal_handler_sdl_wheel_right
					, this
					, _3
					, _6)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_down
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_DOWN>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_down
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_UP>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_up
					, this
					, _5
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::LEFT_BUTTON_UP>(
				boost::bind(
					&tscroll_container::signal_handler_left_button_up
					, this
					, _5
					, false)
				, event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_LEAVE>(boost::bind(
					&tscroll_container::signal_handler_mouse_leave
					, this
					, _5
					, true)
				 , event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_LEAVE>(boost::bind(
					&tscroll_container::signal_handler_mouse_leave
					, this
					, _5
					, false)
				 , event::tdispatcher::back_child);

		content_grid_->connect_signal<event::MOUSE_MOTION>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_motion
					, this
					, _3
					, _5
					, _6
					, true)
				, event::tdispatcher::back_pre_child);

		content_grid_->connect_signal<event::MOUSE_MOTION>(
				boost::bind(
					&tscroll_container::signal_handler_mouse_motion
					, this
					, _3
					, _5
					, _6
					, false)
				, event::tdispatcher::back_child);
	}
}

void tscroll_container::
		set_vertical_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	if(vertical_scrollbar_mode_ != scrollbar_mode) {
		vertical_scrollbar_mode_ = scrollbar_mode;
	}
}

void tscroll_container::
		set_horizontal_scrollbar_mode(const tscrollbar_mode scrollbar_mode)
{
	if(horizontal_scrollbar_mode_ != scrollbar_mode) {
		horizontal_scrollbar_mode_ = scrollbar_mode;
	}
}

void tscroll_container::impl_draw_children(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	assert(get_visible() == twidget::VISIBLE
			&& content_grid_->get_visible() == twidget::VISIBLE);

	// Inherited.
	tcontainer_::impl_draw_children(frame_buffer, x_offset, y_offset);

	content_grid_->draw_children(frame_buffer, x_offset, y_offset);
}

void tscroll_container::broadcast_frame_buffer(texture& frame_buffer)
{
	tcontainer_::broadcast_frame_buffer(frame_buffer);

	content_grid_->broadcast_frame_buffer(frame_buffer);
}

void tscroll_container::clear_texture()
{
	tcontainer_::clear_texture();
	content_grid_->clear_texture();
}

void tscroll_container::layout_children()
{
	if (need_layout_) {
		place(get_origin(), get_size());

		// since place scroll_container again, set it dirty.
		set_dirty();

	} else {
		// Inherited.
		tcontainer_::layout_children();

		content_grid_->layout_children();
	}
}

void tscroll_container::invalidate_layout(bool calculate_linked_group)
{ 
	if (calculate_linked_group) {
		layout_init();
		get_window()->layout_linked_widgets(this);
	}

	need_layout_ = true;
}

void tscroll_container::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	// Inherited.
	tcontainer_::child_populate_dirty_list(caller, call_stack);

	std::vector<twidget*> child_call_stack(call_stack);
	content_grid_->populate_dirty_list(caller, child_call_stack);
}

bool tscroll_container::calculate_scrollbar(const tpoint& content_grid_size, const tpoint& content_size)
{
	// beause scrollbar always invisible or same space usage, change scrollbar visible/invislbe doesn't effect layout.
	// for example, change of vertical scrollbar doesn't effect content_grid's usable width. 

	// to prevent window from layouting, disable layout_window.
	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(*get_window());

	std::stringstream err;
	tvisible horizontal_visible = horizontal_scrollbar_grid_->get_visible();
	tvisible vertical_visible = vertical_scrollbar_grid_->get_visible();

	if (content_grid_size.x > content_size.x) {
		err << tintegrate::generate_format(id(), "yellow");
		err << " can not visible horizontal scrollbar because mode is always_invisible."
			<< " wanted size " << content_grid_size
			<< " available size " << content_size
			<< '.';
		VALIDATE(horizontal_scrollbar_mode_ != always_invisible, err.str());
		horizontal_scrollbar_grid_->set_visible(twidget::VISIBLE);

	} else if (horizontal_scrollbar_mode_ != always_invisible) {
		horizontal_scrollbar_grid_->set_visible(twidget::HIDDEN);
		horizontal_scrollbar_->set_item_position(0);
	}

	if (content_grid_size.y > content_size.y) {
		err.str("");
		err << tintegrate::generate_format(id(), "yellow");
		err << " can not visible vertical scrollbar because mode is always_invisible."
			<< " wanted size " << content_grid_size
			<< " available size " << content_size
			<< '.';
		VALIDATE(vertical_scrollbar_mode_ != always_invisible, err.str());
		vertical_scrollbar_grid_->set_visible(twidget::VISIBLE);

	} else if (vertical_scrollbar_mode_ != always_invisible) {
		vertical_scrollbar_grid_->set_visible(twidget::HIDDEN);
		vertical_scrollbar_->set_item_position(0);
	}

	return horizontal_visible != horizontal_scrollbar_grid_->get_visible() || vertical_visible != vertical_scrollbar_grid_->get_visible();
}

void tscroll_container::show_content_rect(const SDL_Rect& rect)
{
	VALIDATE(content_, null_str);
	VALIDATE(horizontal_scrollbar_ && vertical_scrollbar_, null_str);

	// Set the bottom right location first if it doesn't fit the top left
	// will look good. First calculate the left and top position depending on
	// the current position.
	const int left_position = horizontal_scrollbar_->get_item_position()
			+ (rect.x - content_->get_x());
	const int top_position = vertical_scrollbar_->get_item_position()
			+ (rect.y - content_->get_y());

	// bottom.
	const int wanted_bottom = rect.y + rect.h;
	const int current_bottom = content_->get_y() + content_->get_height();
	int distance = wanted_bottom - current_bottom;
	if (distance > 0) {
		vertical_scrollbar_->set_item_position(
				vertical_scrollbar_->get_item_position() + distance);
	}

	// right.
	const int wanted_right = rect.x + rect.w;
	const int current_right = content_->get_x() + content_->get_width();
	distance = wanted_right - current_right;
	if (distance > 0) {
		horizontal_scrollbar_->set_item_position(
				horizontal_scrollbar_->get_item_position() + distance);
	}

	// top.
	if (top_position < static_cast<int>(
				vertical_scrollbar_->get_item_position())) {

		vertical_scrollbar_->set_item_position(top_position);
	}

	// left.
	if (left_position < static_cast<int>(
				horizontal_scrollbar_->get_item_position())) {

		horizontal_scrollbar_->set_item_position(left_position);
	}

	// Update.
	scrollbar_moved(true);
}

void tscroll_container::scroll_vertical_scrollbar(
		const tscrollbar_::tscroll scroll)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(scroll);
	scrollbar_moved();
}

void tscroll_container::scroll_horizontal_scrollbar(
		const tscrollbar_::tscroll scroll)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(scroll);
	scrollbar_moved();
}

void tscroll_container::handle_key_home(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_ && horizontal_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::BEGIN);
	horizontal_scrollbar_->scroll(tscrollbar_::BEGIN);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::handle_key_end(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::END);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_page_up(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_page_down(SDL_Keymod /*modifier*/, bool& handled)

{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::JUMP_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_up_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::
		handle_key_down_arrow( SDL_Keymod /*modifier*/, bool& handled)
{
	assert(vertical_scrollbar_);

	vertical_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container
		::handle_key_left_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_BACKWARDS);
	scrollbar_moved();

	handled = true;
}

void tscroll_container
		::handle_key_right_arrow(SDL_Keymod /*modifier*/, bool& handled)
{
	assert(horizontal_scrollbar_);

	horizontal_scrollbar_->scroll(tscrollbar_::ITEM_FORWARD);
	scrollbar_moved();

	handled = true;
}

void tscroll_container::scrollbar_moved(bool gc_handled)
{
	/*** Update the content location. ***/
	int x_offset = horizontal_scrollbar_->get_item_position() * horizontal_scrollbar_->get_step_size();
	int y_offset = vertical_scrollbar_->get_item_position() * vertical_scrollbar_->get_step_size();

	if (!gc_handled) {
		// if previous handled gc, do not, until mini_set_content_grid_origin, miin_set_content_grid_visible_area.
		y_offset = mini_handle_gc(x_offset, y_offset);
	}

	adjust_offset(x_offset, y_offset);
	const tpoint content_grid_origin = tpoint(
			content_->get_x() - x_offset, content_->get_y() - y_offset);

	mini_set_content_grid_origin(content_->get_origin(), content_grid_origin);
	mini_set_content_grid_visible_area(content_visible_area_);
	content_grid_->set_dirty();
}

const std::string& tscroll_container::get_control_type() const
{
	static const std::string type = "scrollbar_container";
	return type;
}

void tscroll_container::signal_handler_sdl_key_down(
		const event::tevent event
		, bool& handled
		, const SDL_Keycode key
		, SDL_Keymod modifier)
{
	switch(key) {
		case SDLK_HOME :
			handle_key_home(modifier, handled);
			break;

		case SDLK_END :
			handle_key_end(modifier, handled);
			break;


		case SDLK_PAGEUP :
			handle_key_page_up(modifier, handled);
			break;

		case SDLK_PAGEDOWN :
			handle_key_page_down(modifier, handled);
			break;


		case SDLK_UP :
			handle_key_up_arrow(modifier, handled);
			break;

		case SDLK_DOWN :
			handle_key_down_arrow(modifier, handled);
			break;

		case SDLK_LEFT :
			handle_key_left_arrow(modifier, handled);
			break;

		case SDLK_RIGHT :
			handle_key_right_arrow(modifier, handled);
			break;
		default:
			/* ignore */
			break;
		}
}

void tscroll_container::scroll_timer_handler(const bool vertical, const bool up, const int level)
{
	bool scrolled = scroll(vertical, up, level, false);
	if (scrolled) {
		scrollbar_moved();
	}
	scroll_elapse_.second ++;
	if ((!scrolled || scroll_elapse_.first == scroll_elapse_.second) && scroll_timer_ != INVALID_TIMER_ID) {
		remove_timer(scroll_timer_);
		scroll_timer_ = INVALID_TIMER_ID;
	}
}

bool tscroll_container::scroll(const bool vertical, const bool up, const int level, const bool first)
{
	VALIDATE(level > 0, null_str);

	tscrollbar_& scrollbar = vertical? *vertical_scrollbar_: *horizontal_scrollbar_;

	const bool wheel = level > tevent_handler::swipe_wheel_level_gap;
	int level2 = wheel? level - tevent_handler::swipe_wheel_level_gap: level;
	const int offset = level2 * scrollbar.get_visible_items() / tevent_handler::swipe_max_normal_level;
	const unsigned int item_position = scrollbar.get_item_position();
	const unsigned int item_count = scrollbar.get_item_count();
	const unsigned int visible_items = scrollbar.get_visible_items();
	unsigned int item_position2;
	if (up) {
		item_position2 = (int)item_position >= offset? item_position - offset : 0;
	} else {
		item_position2 = item_position + offset;
		item_position2 = item_position2 > item_count - visible_items? item_count - visible_items: item_position2;
	}
	if (item_position2 == item_position) {
		return false;
	}
	scrollbar.set_item_position(item_position2);
	if (!wheel && first) {
		if (scroll_timer_ != INVALID_TIMER_ID) {
			remove_timer(scroll_timer_);
		}
		// [3, 10]
		const int min_times = 3;
		const int max_times = 10;
		int times = min_times + (max_times - min_times) * level2 / tevent_handler::swipe_max_normal_level;
		scroll_elapse_ = std::make_pair(times, 0);
		scroll_timer_ = add_timer(200, boost::bind(&tscroll_container::scroll_timer_handler, this, vertical, up, level), true);
	}
	return true;
}

void tscroll_container::signal_handler_sdl_wheel_up(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(vertical_scrollbar_grid_ && vertical_scrollbar_, null_str);

	if (scroll(true, true, coordinate2.y, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_down(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(vertical_scrollbar_grid_ && vertical_scrollbar_, null_str);

	if (scroll(true, false, coordinate2.y, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_left(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(horizontal_scrollbar_grid_ && horizontal_scrollbar_, null_str);

	if (scroll(false, true, coordinate2.x, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_sdl_wheel_right(bool& handled, const tpoint& coordinate2)
{
	VALIDATE(horizontal_scrollbar_grid_ && horizontal_scrollbar_, null_str);

	if (scroll(false, false, coordinate2.x, true)) {
		scrollbar_moved();
	}
	handled = true;
}

void tscroll_container::signal_handler_left_button_down(const tpoint& coordinate, bool pre_child)
{
	VALIDATE(point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);

	if (scroll_timer_ != INVALID_TIMER_ID) {
		remove_timer(scroll_timer_);
		scroll_timer_ = INVALID_TIMER_ID;
	}

	VALIDATE(is_null_coordinate(first_coordinate_), null_str);
	first_coordinate_ = coordinate;
	if (!pre_child && require_capture_) {
		get_window()->mouse_capture(true);
	}
}

void tscroll_container::signal_handler_left_button_up(const tpoint& coordinate, bool pre_child)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	VALIDATE(window->mouse_focus_widget() || point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);
	set_null_coordinate(first_coordinate_);
}

void tscroll_container::signal_handler_mouse_leave(const tpoint& coordinate, bool pre_child)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	if (is_magic_coordinate(coordinate) || (window->mouse_focus_widget() != content_grid_ && !point_in_rect(coordinate.x, coordinate.y, content_->get_rect()))) {
		set_null_coordinate(first_coordinate_);
	}
}

void tscroll_container::signal_handler_mouse_motion(bool& handled, const tpoint& coordinate, const tpoint& coordinate2, bool pre_child)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	twindow* window = get_window();
	VALIDATE(window->mouse_focus_widget() || point_in_rect(coordinate.x, coordinate.y, content_->get_rect()), null_str);

	const int abs_diff_x = abs(coordinate.x - first_coordinate_.x);
	const int abs_diff_y = abs(coordinate.y - first_coordinate_.y);
	if (require_capture_ && window->mouse_focus_widget() != content_grid_) {
		if (pre_child) {
			if (abs_diff_x >= clear_click_threshold_ || abs_diff_x >= clear_click_threshold_) {
				window->mouse_capture(true, content_grid_);
			}
		} else {
			window->mouse_capture(true);
		}
	}

	if (window->mouse_click_widget()) {
		if (abs_diff_x >= clear_click_threshold_ || abs_diff_x >= clear_click_threshold_) {
			window->clear_mouse_click();
		}
	}

	if (!mini_mouse_motion(first_coordinate_, coordinate)) {
		return;
	}

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
#else
	// return;
#endif

	VALIDATE(vertical_scrollbar_ && horizontal_scrollbar_, null_str);
	int abs_x_offset = abs(coordinate2.x);
	int abs_y_offset = abs(coordinate2.y);

	if (abs_y_offset >= abs_x_offset) {
		abs_x_offset = 0;
	} else {
		abs_y_offset = 0;
	}

	if (abs_y_offset) {
		unsigned int item_position = vertical_scrollbar_->get_item_position();
		if (coordinate2.y < 0) {
			item_position = item_position + abs_y_offset;
		} else {
			item_position = (int)item_position >= abs_y_offset? item_position - abs_y_offset: 0;
		}
		vertical_scrollbar_->set_item_position(item_position);
	}

	if (abs_x_offset) {
		unsigned int item_position = horizontal_scrollbar_->get_item_position();
		if (coordinate2.x < 0) {
			item_position = item_position + abs_x_offset;
		} else {
			item_position = (int)item_position >= abs_x_offset? item_position - abs_x_offset: 0;
		}
		horizontal_scrollbar_->set_item_position(item_position);
	}

	if (coordinate2.x || coordinate2.y) {
		scrollbar_moved();
		handled = true;
	}
}

} // namespace gui2

