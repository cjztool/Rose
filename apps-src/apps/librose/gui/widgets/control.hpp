/* $Id: control.hpp 54584 2012-07-05 18:33:49Z mordante $ */
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

#ifndef GUI_WIDGETS_CONTROL_HPP_INCLUDED
#define GUI_WIDGETS_CONTROL_HPP_INCLUDED

#include "gui/auxiliary/widget_definition.hpp"
#include "gui/auxiliary/formula.hpp"
#include "gui/auxiliary/window_builder.hpp"
#include "gui/widgets/widget.hpp"
#include "integrate.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_control;
} // namespace implementation

class tfloat_widget 
{
	friend class twindow;
public:
	tfloat_widget(twindow& window, const twindow_builder::tfloat_widget& builder, tcontrol& widget)
		: window(window)
		, widget(std::unique_ptr<tcontrol>(&widget))
		, ref_(builder.ref)
		, x(builder.x)
		, y(builder.y)
		, ref_widget_(nullptr)
		, need_layout(true)
	{}

	void set_visible(bool visible);
	void set_ref_widget(tcontrol* widget);
	void set_ref(const std::string& ref);

	twindow& window;
	std::vector<std::pair<std::string, int> > x;
	std::vector<std::pair<std::string, int> > y;
	texture buf;
	texture canvas;
	std::vector<SDL_Rect> buf_rects;
	std::unique_ptr<tcontrol> widget;
	bool need_layout;

private:
	tcontrol* ref_widget_; // if isn't null, ref_widget is priori to  ref.
	std::string ref_;
};

/** Base class for all visible items. */
class tcontrol : public twidget
{
public:
	static bool force_add_to_dirty_list;

	class ttext_maximum_width_lock
	{
	public:
		ttext_maximum_width_lock(tcontrol& widget, int text_maximum_width2)
			: widget_(widget)
			, original_(widget.text_maximum_width_)
		{
			widget_.text_maximum_width_ = text_maximum_width2 - widget.config_->text_extra_width;
		}
		~ttext_maximum_width_lock()
		{
			widget_.text_maximum_width_ = original_;
		}

	private:
		tcontrol& widget_;
		int original_;
	};

	class tadd_to_dirty_list_lock
	{
	public:
		tadd_to_dirty_list_lock()
			: force_add_to_dirty_list_(force_add_to_dirty_list)
		{
			force_add_to_dirty_list = true;
		}
		~tadd_to_dirty_list_lock()
		{
			force_add_to_dirty_list = force_add_to_dirty_list_;
		}

	private:
		bool force_add_to_dirty_list_;
	};

	/** @deprecated Used the second overload. */
	explicit tcontrol(const unsigned canvas_count);

	virtual ~tcontrol();

	/**
	 * Sets the members of the control.
	 *
	 * The map contains named members it can set, controls inheriting from us
	 * can add additional members to set by this function. The following
	 * members can by the following key:
	 *  * label_                  label
	 *  * tooltip_                tooltip
	 *
	 *
	 * @param data                Map with the key value pairs to set the members.
	 */
	virtual void set_members(const string_map& data);

	/***** ***** ***** ***** State handling ***** ***** ***** *****/

	/**
	 * Sets the control's state.
	 *
	 *  Sets the control in the active state, when inactive a control can't be
	 *  used and doesn't react to events. (Note read-only for a ttext_ is a
	 *  different state.)
	 */
	virtual void set_active(const bool active) = 0;

	/** Gets the active state of the control. */
	virtual bool get_active() const = 0;

	int insert_animation(const ::config& cfg);
	void erase_animation(int id);

	void set_canvas_variable(const std::string& name, const variant& value);
	void set_restrict_width() { restrict_width_ = true; }
	void set_size_is_max() { size_is_max_ = true; }
	void set_text_font_size(int size) { text_font_size_ = size; }
	int get_text_font_size() const;
	void set_text_color_tpl(int tpl) { text_color_tpl_ = tpl; }
	int get_text_color_tpl() const { return text_color_tpl_; }

	virtual texture get_canvas_tex();

protected:
	/** Returns the id of the state.
	 *
	 * The current state is also the index canvas_.
	 */
	virtual unsigned get_state() const = 0;

	tpoint best_size_from_builder() const;

public:

	/***** ***** ***** ***** Easy close handling ***** ***** ***** *****/

	/**
	 * Inherited from twidget.
	 *
	 * The default behavious is that a widget blocks easy close, if not it
	 * hould override this function.
	 */
	bool disable_click_dismiss() const;

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/**
	 * Gets the default size as defined in the config.
	 *
	 * @pre                       config_ !=  NULL
	 *
	 * @returns                   The size.
	 */
	tpoint get_config_min_size() const;

	/**
	 * Returns the number of characters per line.
	 *
	 * This value is used to call @ref ttext::set_characters_per_line
	 * (indirectly).
	 *
	 * @returns                   The characters per line. This implementation
	 *                            always returns 0.
	 */
	virtual unsigned get_characters_per_line() const;

	/** Inherited from twidget. */
	void layout_init();

	void refresh_locator_anim(std::vector<tintegrate::tlocator>& locator);
	void set_integrate_default_color(const SDL_Color& color);

	void set_blits(const std::vector<image::tblit>& blits);
	void set_blits(const image::tblit& blit);
	const std::vector<image::tblit>& get_blits() const { return blits_; }

	/** Inherited from twidget. */
	tpoint calculate_best_size() const override;

	// bool drag_detect_started() const { return drag_detect_started_; }
	// void control_drag_detect(bool start, int x = npos, int y = npos, const tdrag_direction type = drag_none);
	// void set_drag_coordinate(int x, int y);
	// int drag_satisfied();
	void set_enable_drag_draw_coordinate(bool enable) { enable_drag_draw_coordinate_ = enable; }

	void set_draw_offset(int x, int y);
	const tpoint& draw_offset() const { return draw_offset_; }

	void set_float_widget() { float_widget_ = true; }
	void associate_float_widget(tfloat_widget& item, bool associate);

protected:
	virtual bool exist_anim();
	virtual void calculate_integrate();

	tintegrate* integrate_;
	SDL_Color integrate_default_color_;
	std::vector<tintegrate::tlocator> locator_;

	/**
	 * Surface of all in state.
	 *
	 * If it is blits style button, blits_ will not empty.
	 */
	std::vector<image::tblit> blits_;
	std::set<tfloat_widget*> associate_float_widgets_;
	bool float_widget_;

public:

	/** Inherited from twidget. */
	void place(const tpoint& origin, const tpoint& size) override;

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/**
	 * Uses the load function.
	 *
	 * @note This doesn't look really clean, but the final goal is refactor
	 * more code and call load_config in the ctor, removing the use case for
	 * the window. That however is a longer termine refactoring.
	 */
	// friend class twindow;

public:
	/** Inherited from twidget. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active)
	{
		return (twidget::find_at(coordinate, must_be_active)
			&& (!must_be_active || get_active())) ? this : NULL;
	}


	/** Inherited from twidget.*/
	twidget* find(const std::string& id, const bool must_be_active)
	{
		return (twidget::find(id, must_be_active)
			&& (!must_be_active || get_active())) ? this : NULL;
	}

	/** Inherited from twidget.*/
	const twidget* find(const std::string& id,
			const bool must_be_active) const
	{
		return (twidget::find(id, must_be_active)
			&& (!must_be_active || get_active())) ? this : NULL;
	}

	/**
	 * Sets the definition.
	 *
	 * This function sets the definition of a control and should be called soon
	 * after creating the object since a lot of internal functions depend on the
	 * definition.
	 *
	 * This function should be called one time only!!!
	 */
	void set_definition(const std::string& definition);

	/***** ***** ***** setters / getters for members ***** ****** *****/
	const std::string& label() const { return label_; }
	virtual void set_label(const std::string& label);

	virtual void set_text_editable(bool editable);
	bool get_text_editable() const { return text_editable_; }

	const t_string& tooltip() const { return tooltip_; }
	// Note setting the tooltip_ doesn't dirty an object.
	void set_tooltip(const t_string& tooltip)
		{ tooltip_ = tooltip; set_wants_mouse_hover(!tooltip_.empty()); }

	// const versions will be added when needed
	std::vector<tcanvas>& canvas() { return canvas_; }
	tcanvas& canvas(const unsigned index)
		{ assert(index < canvas_.size()); return canvas_[index]; }
	const tcanvas& canvas(const unsigned index) const
		{ assert(index < canvas_.size()); return canvas_[index]; }

	tresolution_definition_ptr config() { return config_; }
	tresolution_definition_const_ptr config() const { return config_; }

	// limit width of text when calculate_best_size.
	void set_text_maximum_width(int maximum);
	void clear_label_size_cache();

	void set_callback_place(boost::function<void (tcontrol*, const SDL_Rect&)> callback)
		{ callback_place_ = callback; }

	void set_callback_pre_impl_draw_children(boost::function<void (tcontrol*, texture&, int, int)> callback)
		{ callback_pre_impl_draw_children_ = callback; }

	void set_best_size(const std::string& width, const std::string& height);

protected:
	void set_config(tresolution_definition_ptr config) { config_ = config; }

	/***** ***** ***** ***** miscellaneous ***** ***** ***** *****/

	/**
	 * Updates the canvas(ses).
	 *
	 * This function should be called if either the size of the widget changes
	 * or the text on the widget changes.
	 */
	virtual void update_canvas();

	/**
	 * Returns the maximum width available for the text.
	 *
	 * This value makes sense after the widget has been given a size, since the
	 * maximum width is based on the width of the widget.
	 */
	int get_text_maximum_width() const;

	/**
	 * Returns the maximum height available for the text.
	 *
	 * This value makes sense after the widget has been given a size, since the
	 * maximum height is based on the height of the widget.
	 */
	int get_text_maximum_height() const;

	void clear_texture();

protected:
	/** Contain the non-editable text associated with control. */
	std::string label_;

	/** Read only for the label? */
	bool text_editable_;
	int text_font_size_;
	int text_color_tpl_;

	// bool drag_detect_started_;
	// tpoint first_drag_coordinate_;
	// tpoint last_drag_coordinate_;
	bool enable_drag_draw_coordinate_;

	tpoint draw_offset_;

	boost::function<void (tcontrol*, texture& frame_buffer, int x_offset, int y_offset)> callback_pre_impl_draw_children_;

	/** When we're used as a fixed size item, this holds the best size. */
	tformula<unsigned> best_width_;
	tformula<unsigned> best_height_;
	bool hdpi_off_width_;
	bool hdpi_off_height_;

	/**
	 * Contains the pointer to the configuration.
	 *
	 * Every control has a definition of how it should look, this contains a
	 * pointer to the definition. The definition is resolution dependant, where
	 * the resolution is the size of the Wesnoth application window. Depending
	 * on the resolution widgets can look different, use different fonts.
	 * Windows can use extra scrollbars use abbreviations as text etc.
	 */
	tresolution_definition_ptr config_;

private:
	/**
	 * The definition is the id of that widget class.
	 *
	 * Eg for a button it [button_definition]id. A button can have multiple
	 * definitions which all look different but for the engine still is a
	 * button.
	 */
	std::string definition_;

	mutable std::pair<int, tpoint> label_size_;

	std::vector<int> post_anims_;

	// call when after place.
	boost::function<void (tcontrol*, const SDL_Rect&)> callback_place_;

	/**
	 * Tooltip text.
	 *
	 * The hovering event can cause a small tooltip to be shown, this is the
	 * text to be shown. At the moment the tooltip is a single line of text.
	 */
	t_string tooltip_;

	/**
	 * Holds all canvas objects for a control.
	 *
	 * A control can have multiple states, which are defined in the classes
	 * inheriting from us. For every state there is a separate canvas, which is
	 * stored here. When drawing the state is determined and that canvas is
	 * drawn.
	 */
	std::vector<tcanvas> canvas_;

	/**
	 * Load class dependant config settings.
	 *
	 * load_config will call this method after loading the config, by default it
	 * does nothing but classes can override it to implement custom behaviour.
	 */
	virtual void load_config_extra() {}

protected:
	/** Inherited from twidget. */
	void impl_draw_background(
			  texture& frame_buffer
			, int x_offset
			, int y_offset);

	/** Inherited from twidget. */
	void impl_draw_foreground(
			  texture& /*frame_buffer*/
			, int /*x_offset*/
			, int /*y_offset*/)
	{ /* do nothing */ }

	void child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack);

private:

	/**
	 * Gets the best size for a text.
	 *
	 * @param minimum_size        The minimum size of the text.
	 * @param maximum_size        The wanted maximum size of the text, if not
	 *                            possible it's ignored. A value of 0 means
	 *                            that it's ignored as well.
	 *
	 * @returns                   The best size.
	 */
	tpoint get_best_text_size(const tpoint& maximum_size) const;

	virtual tpoint mini_get_best_text_size() const { return tpoint(0, 0); }

	/** The maximum width for the text in a control. has git ride of text_extra_width */
	int text_maximum_width_;
	bool restrict_width_;
	bool size_is_max_;

	/** The alignment of the text in a control. */
	PangoAlignment text_alignment_;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_show_tooltip(
			  const event::tevent event
			, bool& handled
			, const tpoint& location);

	void signal_handler_notify_remove_tooltip(
			  const event::tevent event
			, bool& handled);
};

} // namespace gui2

#endif

