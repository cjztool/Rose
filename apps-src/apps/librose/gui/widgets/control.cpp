/* $Id: control.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "control.hpp"

#include "font.hpp"
#include "formula_string_utils.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "marked-up_text.hpp"
#include "display.hpp"
#include "hotkeys.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <iomanip>

#define LOG_SCOPE_HEADER "tcontrol(" + get_control_type() + ") [" \
		+ id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

void tfloat_widget::set_visible(bool visible)
{
	twindow::tinvalidate_layout_blocker blocker(window);
	widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
}

void tfloat_widget::set_ref_widget(tcontrol* widget)
{
	if (widget == ref_widget_) {
		return;
	}
	if (ref_widget_) {
		ref_widget_->associate_float_widget(*this, false);
	}
	ref_widget_ = widget;
	need_layout = true;
}

void tfloat_widget::set_ref(const std::string& ref)
{
	if (ref == ref_) {
		return;
	}
	if (!ref_.empty()) {
		tcontrol* ref_widget = dynamic_cast<tcontrol*>(window.find(ref, false));
		if (ref_widget) {
			ref_widget->associate_float_widget(*this, false);
		}
	}
	ref_ = ref;
	need_layout = true;
}

bool tcontrol::force_add_to_dirty_list = false;

tcontrol::tcontrol(const unsigned canvas_count)
	: definition_("default")
	, label_()
	, label_size_(std::make_pair(0, tpoint(0, 0)))
	, text_editable_(false)
	, post_anims_()
	, integrate_(NULL)
	, integrate_default_color_(font::BLACK_COLOR)
	, tooltip_()
	, canvas_(canvas_count)
	, config_(NULL)
	, text_maximum_width_(0)
	, restrict_width_(false)
	, size_is_max_(false)
	, text_font_size_(0)
	, text_color_tpl_(0)
	, text_alignment_(PANGO_ALIGN_LEFT)
	, enable_drag_draw_coordinate_(true)
	, draw_offset_(0, 0)
	, best_width_("")
	, best_height_("")
	, hdpi_off_width_(false)
	, hdpi_off_height_(false)
	, float_widget_(false)
{
	connect_signal<event::SHOW_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_show_tooltip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::NOTIFY_REMOVE_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_notify_remove_tooltip
			, this
			, _2
			, _3));
}

tcontrol::~tcontrol()
{
	while (!post_anims_.empty()) {
		erase_animation(post_anims_.front());
	}
	if (integrate_) {
		delete integrate_;
		integrate_ = NULL;
	}
}

void tcontrol::set_members(const string_map& data)
{
	/** @todo document this feature on the wiki. */
	/** @todo do we need to add the debug colors here as well? */
	string_map::const_iterator itor = data.find("id");
	if(itor != data.end()) {
		set_id(itor->second);
	}

	itor = data.find("linked_group");
	if(itor != data.end()) {
		set_linked_group(itor->second);
	}

	itor = data.find("label");
	if(itor != data.end()) {
		set_label(itor->second);
	}

	itor = data.find("tooltip");
	if(itor != data.end()) {
		set_tooltip(itor->second);
	}

	itor = data.find("editable");
	if (itor != data.end()) {
		set_text_editable(utils::string_bool(itor->second));
	}
}

bool tcontrol::disable_click_dismiss() const
{
	return get_visible() == twidget::VISIBLE && get_active();
}

tpoint tcontrol::get_config_min_size() const
{
	tpoint result(config_->min_width, config_->min_height);
	return result;
}

unsigned tcontrol::get_characters_per_line() const
{
	return 0;
}

void tcontrol::layout_init()
{
	// Inherited.
	twidget::layout_init();
	text_maximum_width_ = 0;
}

tpoint tcontrol::get_best_text_size(const tpoint& maximum_size) const
{
	VALIDATE(!label_.empty(), null_str);

	if (!label_size_.second.x || (maximum_size.x != label_size_.first)) {
		// Try with the minimum wanted size.
		label_size_.first = maximum_size.x;

		label_size_.second = font::get_rendered_text_size(label_, maximum_size.x, get_text_font_size(), font::NORMAL_COLOR, text_editable_);
	}

	const tpoint& size = label_size_.second;
	VALIDATE(size.x <= maximum_size.x, null_str);

	return size;
}

tpoint tcontrol::best_size_from_builder() const
{
	tpoint result(npos, npos);
	const twindow* window = NULL;
	if (best_width_.has_formula2()) {
		window = get_window();
		result.x = best_width_(window->variables());
		if (!hdpi_off_width_) {
			result.x *= twidget::hdpi_scale;
		}
	}
	if (best_height_.has_formula2()) {
		if (!window) {
			window = get_window();
		}
		result.y = best_height_(window->variables());
		if (!hdpi_off_height_) {
			result.y *= twidget::hdpi_scale;
		}
	}
	return result;
}

tpoint tcontrol::calculate_best_size() const
{
	VALIDATE(config_, null_str);
	
	if (restrict_width_ && !text_maximum_width_) {
		return tpoint(0, 0);
	}

	// if has width/height field, use them. or calculate.
	const tpoint cfg_size = best_size_from_builder();
	if (cfg_size.x != npos && cfg_size.y != npos && !size_is_max_) {
		return cfg_size;
	}

	// calculate text size.
	tpoint text_size(0, 0);
	if (config_->label_is_text) {
		if (!label_.empty()) {
			tpoint maximum(settings::screen_width - config_->text_extra_width, settings::screen_height);
				
			if (text_maximum_width_ && maximum.x > text_maximum_width_) {
				maximum.x = text_maximum_width_;
			}
			text_size = get_best_text_size(maximum);
		}

	} else {
		text_size = mini_get_best_text_size();
	}

	// text size must >= minimum size. 
	const tpoint minimum = get_config_min_size();
	if (minimum.x > 0 && text_size.x < minimum.x) {
		text_size.x = minimum.x;
	}
	if (minimum.y > 0 && text_size.y < minimum.y) {
		text_size.y = minimum.y;
	}

	tpoint result(text_size.x + config_->text_extra_width, text_size.y + config_->text_extra_height);
	if (!size_is_max_) {
		if (cfg_size.x != npos) {
			result.x = cfg_size.x;
		}
		if (cfg_size.y != npos) {
			result.y = cfg_size.y;
		}
	} else {
		if (cfg_size.x != npos && result.x >= cfg_size.x) {
			result.x = cfg_size.x;
		}
		if (cfg_size.y != npos && result.y >= cfg_size.y) {
			result.y = cfg_size.y;
		}
	}

	return result;
}

void tcontrol::calculate_integrate()
{
	if (!text_editable_) {
		return;
	}
	if (integrate_) {
		delete integrate_;
	}
	int max = get_text_maximum_width();
	if (max > 0) {
		// before place, w_ = 0. it indicate not ready.
		integrate_ = new tintegrate(label_, get_text_maximum_width(), -1, get_text_font_size(), integrate_default_color_, text_editable_);
		if (!locator_.empty()) {
			integrate_->fill_locator_rect(locator_, true);
		}
	}
}

void tcontrol::set_integrate_default_color(const SDL_Color& color)
{
	integrate_default_color_ = color;
}

void tcontrol::refresh_locator_anim(std::vector<tintegrate::tlocator>& locator)
{
	if (!text_editable_) {
		return;
	}
	locator_.clear();
	if (integrate_) {
		integrate_->fill_locator_rect(locator, true);
	} else {
		locator_ = locator;
	}
}

void tcontrol::set_blits(const std::vector<image::tblit>& blits)
{ 
	blits_ = blits;
	set_dirty();
}

void tcontrol::set_blits(const image::tblit& blit)
{
	blits_.clear();
	blits_.push_back(blit);
	set_dirty();
}

void tcontrol::place(const tpoint& origin, const tpoint& size)
{
	if (restrict_width_) {
		VALIDATE(text_maximum_width_ >= size.x - (int)config_->text_extra_width, null_str);
	}

	SDL_Rect previous_rect = ::create_rect(x_, y_, w_, h_);

	// resize canvasses
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_width(size.x);
		canvas.set_height(size.y);
	}

	// inherited
	twidget::place(origin, size);

	calculate_integrate();

	// update the state of the canvas after the sizes have been set.
	update_canvas();

	if (callback_place_) {
		callback_place_(this, previous_rect);
	}

	if (::create_rect(x_, y_, w_, h_) != previous_rect) {
		for (std::set<tfloat_widget*>::const_iterator it = associate_float_widgets_.begin(); it != associate_float_widgets_.end(); ++ it) {
			tfloat_widget& item = **it;
			item.need_layout = true;
		}
	}
}

void tcontrol::set_definition(const std::string& definition)
{
	VALIDATE(!config(), null_str);
	definition_ = definition;

	set_config(get_control(get_control_type(), definition_));

	VALIDATE(canvas().size() == config()->state.size(), null_str);
	for (size_t i = 0; i < canvas().size(); ++i) {
		canvas(i) = config()->state[i].canvas;
		canvas(i).start_animation();
	}

	update_canvas();

	load_config_extra();

	VALIDATE(config(), null_str);
}

void tcontrol::clear_label_size_cache()
{
	label_size_.second.x = 0;
}

void tcontrol::set_best_size(const std::string& width, const std::string& height)
{
	best_width_ = tformula<unsigned>(width);
	best_height_ = tformula<unsigned>(height);
}

void tcontrol::set_label(const std::string& label)
{
	if (label == label_) {
		return;
	}

	label_ = label;
	label_size_.second.x = 0;
	update_canvas();
	set_dirty();

	calculate_integrate();
}

void tcontrol::set_text_editable(bool editable)
{
	if (editable == text_editable_) {
		return;
	}

	text_editable_ = editable;
	update_canvas();
	set_dirty();
}

void tcontrol::update_canvas()
{
	const int max_width = get_text_maximum_width();
	const int max_height = get_text_maximum_height();

	// set label in canvases
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable("text", variant(label_));
		canvas.set_variable("text_maximum_width", variant(max_width / twidget::hdpi_scale));
		canvas.set_variable("text_maximum_height", variant(max_height / twidget::hdpi_scale));
	}
}

int tcontrol::get_text_maximum_width() const
{
	return get_width() - config_->text_extra_width;
}

int tcontrol::get_text_maximum_height() const
{
	return get_height() - config_->text_extra_height;
}

void tcontrol::set_text_maximum_width(int maximum)
{
	if (restrict_width_) {
		text_maximum_width_ = maximum - config_->text_extra_width;
	}
}

void tcontrol::clear_texture()
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.clear_texture();
	}
}

bool tcontrol::exist_anim()
{
	if (!post_anims_.empty()) {
		return true;
	}
	if (integrate_ && integrate_->exist_anim()) {
		return true;
	}
	return !canvas_.empty() && canvas_[get_state()].exist_anim();
}

int tcontrol::insert_animation(const ::config& cfg)
{
	int id = start_cycle_float_anim(*display::get_singleton(), cfg);
	if (id != INVALID_ANIM_ID) {
		std::vector<int>& anims = post_anims_;
		anims.push_back(id);
	}
	return id;
}

void tcontrol::erase_animation(int id)
{
	bool found = false;
	std::vector<int>::iterator find = std::find(post_anims_.begin(), post_anims_.end(), id);
	if (find != post_anims_.end()) {
		post_anims_.erase(find);
		found = true;
	}
	if (found) {
		display& disp = *display::get_singleton();
		disp.erase_area_anim(id);
		set_dirty();
	}
}

void tcontrol::set_canvas_variable(const std::string& name, const variant& value)
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable(name, value);
	}
	set_dirty();
}

int tcontrol::get_text_font_size() const
{
	int ret = text_font_size_? text_font_size_: font::CFG_SIZE_DEFAULT;
	if (ret >= font_min_relative_size) {
		ret = font_size_from_relative(ret);
	}
	return ret * twidget::hdpi_scale;
}

class tshare_canvas_integrate_lock
{
public:
	tshare_canvas_integrate_lock(tintegrate* integrate)
		: integrate_(share_canvas_integrate)
	{
		share_canvas_integrate = integrate;
	}
	~tshare_canvas_integrate_lock()
	{
		share_canvas_integrate = integrate_;
	}

private:
	tintegrate* integrate_;
};

void tcontrol::impl_draw_background(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	tshare_canvas_integrate_lock lock2(integrate_);

	x_offset += draw_offset_.x;
	y_offset += draw_offset_.y;

	canvas(get_state()).blit(*this, frame_buffer, calculate_blitting_rectangle(x_offset, y_offset), get_dirty(), post_anims_);
}

texture tcontrol::get_canvas_tex()
{
	VALIDATE(get_dirty(), null_str);

	return canvas(get_state()).get_canvas_tex(*this, post_anims_);
}

void tcontrol::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (force_add_to_dirty_list && !canvas_.empty()) {
		caller.add_to_dirty_list(call_stack);
	}
}

/*
void tcontrol::control_drag_detect(bool start, int x, int y, const tdrag_direction type)
{
	if (drag_ == drag_none || start == drag_detect_started_) {
		return;
	}

	if (start) {
		// ios sequence: SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN
		// it is called at SDL_MOUSEBUTTONDOWN! SDL_MOUSEMOTION before it is discard.
		first_drag_coordinate_.x = x;
		first_drag_coordinate_.y = y;

		last_drag_coordinate_.x = x;
		last_drag_coordinate_.y = y;
	} else if (enable_drag_draw_coordinate_) {
		set_draw_offset(0, 0);
	}
	drag_detect_started_ = start;

	bool require_dirty = false;
	// upcaller maybe update draw_offset.
	if (require_dirty) {
		set_dirty();
	}
}

void tcontrol::set_drag_coordinate(int x, int y)
{
	if (drag_ == drag_none || !drag_detect_started_) {
		return;
	}

	if (last_drag_coordinate_.x == x && last_drag_coordinate_.y == y) {
		return;
	}

	if (drag_ != drag_track) {
		// get rid of direct noise
		if (!(drag_ & (drag_left | drag_right))) {
			// concert only vertical
			if (abs(x - last_drag_coordinate_.x) > abs(y - last_drag_coordinate_.y)) {
				// horizontal variant is more than vertical, think no.
				return;
			}

		} else if (!(drag_ & (drag_up | drag_down))) {
			// concert only horizontal
			if (abs(x - last_drag_coordinate_.x) < abs(y - last_drag_coordinate_.y)) {
				// vertical variant is more than horizontal, think no.
				return;
			}
		}
	}

	last_drag_coordinate_.x = x;
	last_drag_coordinate_.y = y;

	if (enable_drag_draw_coordinate_) {
		set_draw_offset(last_drag_coordinate_.x - first_drag_coordinate_.x, 0);
	}

	bool require_dirty = false;

	if (require_dirty) {
		set_dirty();
	}
}

int tcontrol::drag_satisfied()
{
	if (drag_ == drag_none || !drag_detect_started_) {
		return drag_none;
	}
	tdrag_direction ret = drag_none;

	const int drag_threshold_mdpi = 40;
	const int drag_threshold = drag_threshold_mdpi * hdpi_scale;
	int w_threshold = w_ / 3;
	if (w_threshold > drag_threshold) {
		w_threshold = drag_threshold;
	}
	int h_threshold = h_ / 3;
	if (h_threshold > drag_threshold) {
		h_threshold = drag_threshold;
	}

	
	if (drag_ & drag_left) {
		if (last_drag_coordinate_.x < first_drag_coordinate_.x && first_drag_coordinate_.x - last_drag_coordinate_.x > w_threshold) {
			ret = drag_left;
		}
	} 
	if (drag_ & drag_right) {
		if (last_drag_coordinate_.x > first_drag_coordinate_.x && last_drag_coordinate_.x - first_drag_coordinate_.x > w_threshold) {
			ret = drag_right;
		}

	}
	if (drag_ & drag_up) {
		if (last_drag_coordinate_.y < first_drag_coordinate_.y && first_drag_coordinate_.y - last_drag_coordinate_.y > h_threshold) {
			ret = drag_up;
		}

	}
	if (drag_ & drag_down) {
		if (last_drag_coordinate_.y > first_drag_coordinate_.y && last_drag_coordinate_.y - first_drag_coordinate_.y > h_threshold) {
			ret = drag_down;
		}
	}

	// stop drag detect.
	control_drag_detect(false, npos, npos, ret);

	return ret;
}
*/
void tcontrol::set_draw_offset(int x, int y) 
{
	draw_offset_.x = x; 
	draw_offset_.y = y; 
}

void tcontrol::associate_float_widget(tfloat_widget& item, bool associate)
{
	if (associate) {
		associate_float_widgets_.insert(&item);
	} else {
		std::set<tfloat_widget*>::iterator it = associate_float_widgets_.find(&item);
		if (it != associate_float_widgets_.end()) {
			associate_float_widgets_.erase(it);
		}
	}
}

void tcontrol::signal_handler_show_tooltip(
		  const event::tevent event
		, bool& handled
		, const tpoint& location)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	return;
#endif

	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!tooltip_.empty()) {
		std::string tip = tooltip_;
		event::tmessage_show_tooltip message(tip, *this, location);
		handled = fire(event::MESSAGE_SHOW_TOOLTIP, *this, message);
	}
}

void tcontrol::signal_handler_notify_remove_tooltip(
		  const event::tevent event
		, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	/*
	 * This makes the class know the tip code rather intimately. An
	 * alternative is to add a message to the window to remove the tip.
	 * Might be done later.
	 */
	get_window()->remove_tooltip();
	// tip::remove();

	handled = true;
}

} // namespace gui2

