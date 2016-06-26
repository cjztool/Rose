/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/widgets/scroll_text_box.hpp"

#include "gui/widgets/text_box.hpp"
#include "gui/auxiliary/widget_definition/scroll_text_box.hpp"
#include "gui/auxiliary/window_builder/scroll_text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scroll_text_box)

tscroll_text_box::tscroll_text_box()
	: tscroll_container(COUNT)
	, state_(ENABLED)
	, tb_(nullptr)
{
	require_capture_ = false;
}

void tscroll_text_box::finalize_subclass()
{
	VALIDATE(content_grid_ && !tb_, null_str);
	tb_ = dynamic_cast<ttext_box*>(content_grid()->find("_text_box", false));

	tb_->set_restrict_width();
	tb_->set_text_font_size(text_font_size_);
	tb_->set_text_color_tpl(text_color_tpl_);

	tb_->set_text_changed_callback(boost::bind(&tscroll_text_box::text_changed_callback, this, _1));
	tb_->set_mouse_moved_callback(boost::bind(&tscroll_text_box::mouse_moved_callback, this, _1));
}

bool tscroll_text_box::mini_mouse_motion(const tpoint& first, const tpoint& last)
{
	if (tb_->selectioning()) {
		return false;
	}
	if (!tb_->start_selectioning()) {
		return true;
	}

	const int abs_diff_x = abs(last.x - first.x);
	const int abs_diff_y = abs(last.y - first.y);
	if (abs_diff_x >= clear_click_threshold_ || abs_diff_x >= clear_click_threshold_) {
		tb_->cancel_start_selection();
		return true;
	}
	return false;
}

void tscroll_text_box::mini_place_content_grid(const tpoint& content_origin, const tpoint& content_size, const tpoint& desire_origin)
{
	tb_->set_text_maximum_width(content_size.x);

	const tpoint actual_size = content_grid_->get_best_size();
	bool changed = calculate_scrollbar(actual_size, content_size);
	if (changed) {
		tb_->clear_label_size_cache();
	}

	const tpoint size(std::max(actual_size.x, content_size.x), std::max(actual_size.y, content_size.y));
	tpoint origin2 = validate_content_grid_origin(content_origin, content_size, desire_origin, size);
	
	content_grid_->place(origin2, size);
}

bool tscroll_text_box::mini_content_empty() const
{
	const ttext_box* tb = dynamic_cast<const ttext_box*>(content_grid()->find("_text_box", false));
	return tb->label().empty();
}

void tscroll_text_box::set_text_editable(bool editable)
{
	if (content_grid()) {
		ttext_box* widget = find_widget<ttext_box>(content_grid(), "_text_box", false, true);
		widget->set_text_editable(editable);
	}
}

const std::string& tscroll_text_box::label() const
{
	return real_label_;
}

void tscroll_text_box::set_label(const std::string& text)
{
	if (content_grid()) {
		ttext_box* widget = find_widget<ttext_box>(content_grid(), "_text_box", false, true);
		if (text == widget->get_value()) {
			return;
		}
		widget->set_value(text);
	}
}

void tscroll_text_box::set_placeholder(const std::string& label)
{ 
	ttext_box* widget = find_widget<ttext_box>(content_grid(), "_text_box", false, true);
	widget->set_placeholder(label);
}

void tscroll_text_box::insert_img(const std::string& str)
{
	ttext_box* widget = find_widget<ttext_box>(content_grid(), "_text_box", false, true);
	widget->insert_img(str);
}

void tscroll_text_box::text_changed_callback(ttext_box* widget)
{
	real_label_ = widget->get_value();

	// scroll_label hasn't linked_group widget, to save time, don't calcuate linked_group.
	invalidate_layout(false);
}

void tscroll_text_box::mouse_moved_callback(ttext_box* widget)
{
	if (vertical_scrollbar_->get_visible() == twidget::INVISIBLE) {
		return;
	}
	unsigned cursor_at = widget->exist_selection()? widget->get_selection_end().y: widget->get_selection_start().y;
	unsigned cursor_height = dynamic_cast<ttext_box*>(widget)->cursor_height();
	unsigned y_offset = dynamic_cast<ttext_box*>(widget)->text_y_offset();
	unsigned item_position = vertical_scrollbar_->get_item_position();

	unsigned topest = cursor_at >= y_offset? cursor_at - y_offset: 0;
	if (item_position > topest) {
		item_position = topest;
	} else if (item_position + get_height() < y_offset + cursor_at + cursor_height) {
		item_position = y_offset + cursor_at  + cursor_height - get_height();
	} else {
		return;
	}

	tpoint origin = content_grid_->get_origin();
	origin.y -= item_position - vertical_scrollbar_->get_item_position();
	content_grid_->set_origin(origin);

	tpoint best_size = content_grid_->calculate_best_size();
	vertical_scrollbar_->set_item_count(best_size.y);
	vertical_scrollbar_->set_item_position(item_position);

	// inform place again!
	invalidate_layout(false);
}

const std::string& tscroll_text_box::get_control_type() const
{
	static const std::string type = "scroll_text_box";
	return type;
}

} // namespace gui2

