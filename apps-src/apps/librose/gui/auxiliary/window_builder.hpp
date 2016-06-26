/* $Id: window_builder.hpp 54322 2012-05-28 08:21:28Z mordante $ */
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

#ifndef GUI_AUXILIARY_WINDOW_BUILDER_HPP_INCLUDED
#define GUI_AUXILIARY_WINDOW_BUILDER_HPP_INCLUDED

#include "gui/auxiliary/formula.hpp"
#include "gui/widgets/grid.hpp"
#include "reference_counted_object.hpp"

#include <boost/function.hpp>

#include <map>
#include <string>
#include <vector>

class config;
class CVideo;

namespace gui2 {

class twindow;

/**
 * Builds a window.
 *
 * @param video                   The frame buffer to draw upon.
 * @param type                    The type id string of the window, this window
 *                                must be registered at startup.
 */
twindow* build(CVideo& video, const std::string& type, const unsigned explicit_x, const unsigned explicit_y);

/** Contains the info needed to instantiate a widget. */
struct tbuilder_widget
	: public reference_counted_object
{
public:

	explicit tbuilder_widget(const config& cfg);

	virtual ~tbuilder_widget() {}

	virtual twidget* build() const = 0;

	/** Parameters for the widget. */
	std::string id;
	std::string linked_group;
};

typedef boost::intrusive_ptr<tbuilder_widget> tbuilder_widget_ptr;
typedef boost::intrusive_ptr<const tbuilder_widget> const_tbuilder_widget_ptr;

/**
 * Registers a widget to be build.
 *
 * @warning This function runs before @ref main() so needs to be careful
 * regarding the static initialization problem.
 *
 * @param id                      The id of the widget as used in WML.
 * @param functor                 The functor to create the widget.
 */
void register_builder_widget(const std::string& id
		, boost::function<tbuilder_widget_ptr(config)> functor);


/**
 * Create a widget builder.
 *
 * This object holds the instance builder for a single widget.
 *
 * @param cfg                     The config object holding the information
 *                                regarding the widget instance.
 *
 * @returns                       The builder for the widget instance.
 */
tbuilder_widget_ptr create_builder_widget(const config& cfg);

tbuilder_widget_ptr create_builder_widget2(const std::string& type, const config& cfg);

/**
 * Helper to generate a widget from a WML widget instance.
 *
 * Mainly used as functor for @ref register_builder_widget.
 *
 * @param cfg                     The config with the information to
 *                                instanciate the widget.
 *
 * @returns                       A generic widget builder pointer.
 */
template<class T>
tbuilder_widget_ptr build_widget(const config& cfg)
{
	return new T(cfg);
}

struct tbuilder_grid
	: public tbuilder_widget
{
public:
	explicit tbuilder_grid(const config& cfg);

	unsigned rows;
	unsigned cols;

	/** The grow factor for the rows / columns. */
	std::vector<unsigned> row_grow_factor;
	std::vector<unsigned> col_grow_factor;
	std::vector<unsigned> row_shrink_factor;
	std::vector<unsigned> col_shrink_factor;

	/** The flags per grid cell. */
	std::vector<unsigned> flags;

	/** The border size per grid cell. */
	std::vector<unsigned> border_size;

	/** The widgets per grid cell. */
	std::vector<tbuilder_widget_ptr> widgets;

	tgrid* build() const;


	tgrid* build(tgrid* grid) const;
};

typedef boost::intrusive_ptr<tbuilder_grid> tbuilder_grid_ptr;
typedef boost::intrusive_ptr<const tbuilder_grid> tbuilder_grid_const_ptr;

struct tlinked_group
{
	tlinked_group()
		: id()
		, fixed_width(false)
		, fixed_height(false)
	{}
	tlinked_group(const std::string& id, bool width, bool height)
		: id(id)
		, fixed_width(width)
		, fixed_height(height)
	{}
	tlinked_group(const config& cfg)
		: id(cfg["id"])
		, fixed_width(cfg["fixed_width"].to_bool())
		, fixed_height(cfg["fixed_height"].to_bool())
	{}

	std::string id;
	bool fixed_width;
	bool fixed_height;
};

class twindow_builder
{
public:
	struct tfloat_widget 
	{
		tfloat_widget(const config& cfg);

		std::string ref;
		std::vector<std::pair<std::string, int> > x;
		std::vector<std::pair<std::string, int> > y;
		tbuilder_widget_ptr widget;
	};

	twindow_builder()
		: resolutions()
		, id_()
		, app_()
	{
	}

	std::string read(const config& cfg);

	struct tresolution
	{
	public:
		explicit tresolution(const config& cfg);

		bool automatic_placement;

		tformula<unsigned> x;
		tformula<unsigned> y;
		tformula<unsigned> width;
		tformula<unsigned> height;

		unsigned vertical_placement;
		unsigned horizontal_placement;

		bool click_dismiss;
		bool leave_dismiss;

		std::string definition;
		bool scene;
		twidget::torientation orientation;

		std::vector<tlinked_group> linked_groups;

		config context_menus;
		config theme_cfg;

		std::vector<tfloat_widget> float_widgets;

		tbuilder_grid_ptr grid;
	};

	std::vector<tresolution> resolutions;

private:
	std::string id_;
	std::string app_;
};

/**
 * Builds a window.
 */
twindow *build(CVideo &video, const twindow_builder::tresolution *res, const unsigned explicit_x, const unsigned explicit_y);

} // namespace gui2

#endif

