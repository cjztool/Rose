/* $Id: label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/auxiliary/widget_definition/label.hpp"
#include "wml_exception.hpp"

namespace gui2 {

tlabel_definition::tlabel_definition(const config& cfg)
	: tcontrol_definition(cfg)
{
	VALIDATE(text_font_size, null_str);
	load_resolutions<tresolution>(cfg);
	for (std::vector<tresolution_definition_ptr>::const_iterator it = resolutions.begin(); it != resolutions.end(); ++ it) {
		const tresolution_definition_ptr& res = *it;
		VALIDATE(res->label_is_text, null_str);
	}
}


tlabel_definition::tresolution::tresolution(const config& cfg)
	: tresolution_definition_(cfg)
{
/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = 1_label
 *
 * == Label ==
 *
 * @macro = label_description
 *
 * Although the label itself has no event interaction it still has two states.
 * The reason is that labels are often used as visual indication of the state
 * of the widget it labels.
 *
 * The following states exist:
 * * state_enabled, the label is enabled.
 * * state_disabled, the label is disabled.
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="label_definition"}{min=0}{max=-1}{super="generic/widget_definition"}
 * @begin{tag}{name="resolution"}{min=0}{max=-1}{super="generic/widget_definition/resolution"}
 * @begin{tag}{name="state_enabled"}{min=0}{max=1}{super="generic/state"}
 * @end{tag}{name="state_enabled"}
 * @begin{tag}{name="state_disabled"}{min=0}{max=1}{super="generic/state"}
 * @end{tag}{name="state_disabled"}
 * @end{tag}{name="resolution"}
 * @end{tag}{name="label_definition"}
 * @end{parent}{name="gui/"}
 */
	// Note the order should be the same as the enum tstate is label.hpp.
	state.push_back(tstate_definition(cfg.child("state_enabled")));
	state.push_back(tstate_definition(cfg.child("state_disabled")));
}

} // namespace gui2

