/* $Id: simple_item_selector.cpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "display.hpp"
#include "gui/dialogs/simple_item_selector.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_simple_item_selector
 *
 * == Simple item selector ==
 *
 * A simple one-column listbox with OK and Cancel buttons.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & control & m &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices. $
 *
 * -item & & control & m &
 *         Widget which shows a listbox item label. $
 *
 * ok & & button & m &
 *         OK button. $
 *
 * cancel & & button & m &
 *         Cancel button. $
 *
 * @end{table}
 */

REGISTER_DIALOG(rose, simple_item_selector)

tsimple_item_selector::tsimple_item_selector(display& disp, const std::string& title, const std::string& message, list_type const& items, bool title_uses_markup, bool message_uses_markup)
	: disp_(disp)
	, index_(-1)
	, title_(title)
	, msg_(message)
	, markup_title_(title_uses_markup)
	, markup_msg_(message_uses_markup)
	, single_button_(false)
	, items_(items)
	, ok_label_()
	, cancel_label_()
{
}

void tsimple_item_selector::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	tlabel& ltitle = find_widget<tlabel>(&window, "title", false);
	tcontrol& lmessage = find_widget<tcontrol>(&window, "message", false);
	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	window.keyboard_capture(&list);

	ltitle.set_label(title_);

	lmessage.set_label(msg_);

	int n = 0;
	for (std::vector<std::string>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		std::map<std::string, string_map> data;
		string_map column;

		column["label"] = str_cast(n);
		data.insert(std::make_pair("number", column));

		column["label"] = *it;
		data.insert(std::make_pair("item", column));

		list.insert_row_gc(data);

		n ++;
	}

	if (index_ != -1 && index_ < list.get_item_count()) {
		list.select_row(index_);
	}

	index_ = -1;

	list.set_did_allocated_gc(boost::bind(&tsimple_item_selector::did_allocated_gc, this, _1, _2));
	list.set_did_more_rows_gc(boost::bind(&tsimple_item_selector::did_more_rows_gc, this, _1));

	tbutton& button_ok = find_widget<tbutton>(&window, "ok", false);
	tbutton& button_cancel = find_widget<tbutton>(&window, "cancel", false);

	if(!ok_label_.empty()) {
		button_ok.set_label(ok_label_);
	}

	if(!cancel_label_.empty()) {
		button_cancel.set_label(cancel_label_);
	}

	if(single_button_) {
		button_cancel.set_visible(gui2::twidget::INVISIBLE);
	}
}

void tsimple_item_selector::did_allocated_gc(tlistbox& list, ttoggle_panel& widget)
{
	tbutton& like = find_widget<tbutton>(&widget, "like", false);
	connect_signal_mouse_left_click(
		like
		, boost::bind(
		&tsimple_item_selector::do_like
		, this
		, boost::ref(widget)));

	like.set_label(str_cast(widget.at()));
}

void tsimple_item_selector::did_more_rows_gc(tlistbox& list)
{
	const std::vector<std::unique_ptr<tlistbox::tgc_row> >& gc_rows = list.gc_rows();
	if (gc_rows.size() < 10010) {
		std::stringstream ss;
		for (int n = 0; n < 5; n ++) {
			std::map<std::string, string_map> data;
			string_map column;

			column["label"] = str_cast(n);
			data.insert(std::make_pair("number", column));

			ss.str("");
			ss << "add " << n;
			column["label"] = ss.str();
			data.insert(std::make_pair("item", column));

			list.insert_row_gc(data);
		}
	}
}

void tsimple_item_selector::do_like(const ttoggle_panel& widget) const
{
	std::stringstream ss;

	// ss << "Hello world. You press #" << widget.at() << " row!"; 
	ss << "Welcome to grid layout example of Rose!";
	gui2::show_message(disp_.video(), "Hello World", ss.str(), tmessage::ok_cancel_buttons);
}

void tsimple_item_selector::post_show(twindow& window)
{
	if(get_retval() != twindow::OK) {
		return;
	}

	tlistbox& list = find_widget<tlistbox>(&window, "listbox", false);
	ttoggle_panel* selected = list.cursel();
	index_ = selected? selected->at(): twidget::npos;
}

}
