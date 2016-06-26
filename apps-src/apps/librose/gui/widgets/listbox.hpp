/* $Id: listbox.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_LISTBOX_HPP_INCLUDED
#define GUI_WIDGETS_LISTBOX_HPP_INCLUDED

#include "gui/widgets/scroll_container.hpp"
#include "gui/widgets/toggle_panel.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_listbox;
}

/** The listbox class. */
class tlistbox: public tscroll_container
{
	friend struct implementation::tbuilder_listbox;
public:
	class tgrid3: public tgrid
	{
	public:
		tgrid3(tlistbox& listbox)
			: listbox_(listbox)
		{}


		void set_origin(const tpoint& origin) override;	
		void place(const tpoint& origin, const tpoint& size) override
		{
			listbox_.list_place(origin, size);
		}

		tpoint calculate_best_size() const override
		{
			return listbox_.list_calculate_best_size();
		}

		void listbox_init();
		int listbox_insert_child(twidget& widget, int at);
		void listbox_erase_child(int at);
		void validate_children_continuous() const;
	private:
		tlistbox& listbox_;
	};

	struct tgc_row {
		tgc_row(const std::map<std::string, string_map>& data)
			: data(data)
			, distance(twidget::npos)
			, height(twidget::npos)
		{}
		
		std::map<std::string, string_map> data;
		int distance; // distance to 0.
		int height; // this row's hight.
	};

	/**
	 * Constructor.
	 *
	 * @param has_minimum         Does the listbox need to have one item
	 *                            selected.
	 * @param has_maximum         Can the listbox only have one item
	 *                            selected.
	 * @param placement           How are the items placed.
	 * @param select              Select an item when selected, if false it
	 *                            changes the visible state instead.
	 */
	tlistbox(bool gc);
	~tlistbox();

	/***** ***** ***** ***** Row handling. ***** ***** ****** *****/
	/**
	 * Adds single row to the grid.
	 *
	 * This function expect a row to have multiple widgets (either multiple
	 * columns or one column with multiple widgets).
	 *
	 *
	 * @param data                The data to send to the set_members of the
	 *                            widgets. If the member id is not an empty
	 *                            string it is only send to the widget that has
	 *                            the wanted id (if any). If the member id is an
	 *                            empty string, it is send to all members.
	 *                            Having both empty and non-empty id's gives
	 *                            undefined behaviour.
	 * @param index               The item before which to add the new item,
	 *                            0 == begin, -1 == end.
	 */
	ttoggle_panel& add_row(const std::map<std::string /* widget id */, string_map>& data, const int index = npos);
	void insert_row_gc(const std::map<std::string /* widget id */, string_map>& data, const int index = npos);

	/**
	 * Removes a row in the listbox.
	 *
	 * @param row                 The row to remove, when not in
	 *                            range the function is ignored.
	 * @param count               The number of rows to remove, 0 means all
	 *                            rows (starting from row).
	 */
	void remove_row(int row, int count = 1);

	/** Removes all the rows in the listbox, clearing it. */
	void clear();

	/** Sort all items. */
	void sort(void* caller, bool (*callback)(void*, twidget&, twidget&));

	/** Returns the number of items in the listbox. */
	int get_item_count() const;

	/**
	 * Makes a row active or inactive.
	 *
	 * NOTE this doesn't change the select status of the row.
	 *
	 * @param row                 The row to (de)activate.
	 * @param active              true activate, false deactivate.
	 */
	void set_row_active(const unsigned row, const bool active);

	/**
	 * Makes a row visible or invisible.
	 *
	 * @param row                 The row to show or hide.
	 * @param shown               true visible, false invisible.
	 */
	void set_row_shown(const int row, const bool visible);

	/**
	 * Makes a row visible or invisible.
	 *
	 * Use this version if you want to show hide multiple items since it's
	 * optimized for that purpose, for one it calls the selection changed
	 * callback only once instead of several times.
	 *
	 * @param shown               A vector with the show hide status for every
	 *                            row. The number of items in the vector must
	 *                            be equal to the number of items in the
	 *                            listbox.
	 */
	void set_row_shown(const std::vector<bool>& shown);

	/**
	 * Returns the panel of the wanted row.
	 *
	 * There's only a const version since allowing callers to modify the grid
	 * behind our backs might give problems. We return a pointer instead of a
	 * reference since dynamic casting of pointers is easier (no try catch
	 * needed).
	 *
	 * @param row                 The row to get the grid from, the caller has
	 *                            to make sure the row is a valid row.
	 * @returns                   The panel of the wanted row.
	 */
	ttoggle_panel* get_row_panel(const unsigned row) const;

	/**
	 * Selectes a row.
	 *
	 * @param row                 The row to select.
	 * @param select              Select or deselect the row.
	 */
	bool select_row(const int row, const bool from_ui = false);


	/**
	 * Returns the first selected row
	 *
	 * @returns                   The first selected row.
	 * @retval -1                 No row selected.
	 */
	ttoggle_panel* cursel() const;

	/**
	 * Scroll to row.
	 */
	void scroll_to_row(const unsigned row);

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/)  {}

	/***** ***** ***** ***** inherited ***** ***** ****** *****/

	/** Inherited from tscroll_container. */
	void place(const tpoint& origin, const tpoint& size) override;

	/** Inherited from tscroll_container. */
	void adjust_offset(int& x_offset, int& y_offset);
	void mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_origin) override;
	void mini_set_content_grid_visible_area(const SDL_Rect& area) override;
	
	int mini_handle_gc(const int x_offset, const int y_offset) override;

	/***** ***** ***** setters / getters for members ***** ****** *****/
	void set_drag_started(const boost::function<void (tlistbox& list, ttoggle_panel& widget, tgrid& drag_grid, const int drag_at)>& callback)
		{ drag_started_ = callback; }

	void set_did_row_focus_changed(const boost::function<void (tlistbox&, ttoggle_panel&, const bool)>& callback)
	{
		did_row_focus_changed_ = callback;
	}
	void set_did_row_pre_change(const boost::function<bool (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_pre_change_ = callback;
	}
	void set_did_row_changed(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_changed_ = callback;
	}
	void set_did_row_click(const boost::function<void (tlistbox& list, ttoggle_panel& widget, const int type)>& callback)
	{
		did_row_click_ = callback;
	}
	void set_did_row_double_click(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_double_click_ = callback;
	}
	void set_did_row_right_click(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_row_right_click_ = callback;
	}

	void set_did_allocated_gc(const boost::function<void (tlistbox& list, ttoggle_panel& widget)>& callback)
	{
		did_allocated_gc_ = callback;
	}

	void set_did_more_rows_gc(const boost::function<void (tlistbox& list)>& callback)
	{
		did_more_rows_gc_ = callback;
	}

	void set_list_builder(tbuilder_grid_ptr list_builder);

	tgrid* left_drag_grid() const { return left_drag_grid_; }
	int drag_at() const { return drag_at_; }
	void cancel_drag();

	void set_row_align(bool val) { row_align_ = val; }

	const std::vector<std::unique_ptr<tgc_row> >& gc_rows() const { return gc_rows_; }

protected:

	/** Inherited from tcontainer_. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;

	/** Inherited from tcontainer_. */
	void child_populate_dirty_list(twindow& caller,	const std::vector<twidget*>& call_stack);

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tscroll_container. */
	void handle_key_up_arrow(SDL_Keymod modifier, bool& handled);

	/** Inherited from tscroll_container. */
	void handle_key_down_arrow(SDL_Keymod modifier, bool& handled);

	bool list_grid_handle_key_up_arrow();
	bool list_grid_handle_key_down_arrow();

	/** Function to call after the user clicked on a row. */
	void did_focus_changed(ttoggle_panel& widget, const bool);
	bool did_pre_change(ttoggle_panel& widget);
	void did_changed(ttoggle_panel& widget);
	void did_click(ttoggle_panel& widget, const int type);
	void did_double_click(ttoggle_panel& widget);

private:
	ttoggle_panel& insert_row_internal(const std::map<std::string /* widget id */, string_map>& data, const int index);
	int calculate_total_height_gc() const;
	int which_precise_row_gc(const int distance) const;
	int which_children_row_gc(const int distance) const;

	void select_row_internal(twidget* widget);

	/**
	 * @todo A listbox must have the following config parameters in the
	 * instanciation:
	 * - fixed row height?
	 * - fixed column width?
	 * and if so the following ways to set them
	 * - fixed depending on header ids
	 * - fixed depending on footer ids
	 * - fixed depending on first row ids
	 * - fixed depending on list (the user has to enter a list of ids)
	 *
	 * For now it's always fixed width depending on the first row.
	 */


	/**
	 * Finishes the building initialization of the widget.
	 *
	 * @param header              Builder for the header.
	 * @param footer              Builder for the footer.
	 * @param list_data           The initial data to fill the listbox with.
	 */
	void finalize(
			tbuilder_grid_const_ptr header,
			tbuilder_grid_const_ptr footer,
			const std::vector<string_map>& list_data);

	void list_place(const tpoint& origin, const tpoint& size);
	tpoint list_calculate_best_size();

	ttoggle_panel* next_selectable_row(int start_at, bool invert);
	bool callback_control_drag_detect(tcontrol* control, bool start, const tdrag_direction type);
	void callback_pre_impl_draw_children(tcontrol* control, texture& frame_buffer, int x_offset, int y_offset);
	bool callback_set_drag_coordinate(tcontrol* control, const tpoint& first, const tpoint& last);

	/**
	 * Contains a pointer to the generator.
	 *
	 * The pointer is not owned by this class, it's stored in the content_grid_
	 * of the tscroll_container super class and freed when it's grid is
	 * freed.
	 */
	tgrid3* list_grid_;

	/** Contains the builder for the new items. */
	tbuilder_grid_const_ptr list_builder_;

	ttoggle_panel* cursel_;
	int drag_at_;
	bool row_align_;

	bool gc_;
	std::vector<std::unique_ptr<tgc_row> > gc_rows_;
	int gc_cursel_at_;
	int gc_next_precise_at_;

	tgrid* left_drag_grid_;
	tpoint left_drag_grid_size_;

	boost::function<void (tlistbox& list, ttoggle_panel& panel, tgrid& drag_grid, const int drag_at)> drag_started_;

	/**
	 * This callback is called when the value in the listbox changes.
	 *
	 * @todo the implementation of the callback hasn't been tested a lot and
	 * there might be too many calls. That might happen if an arrow up didn't
	 * change the selected item.
	 */

	boost::function<void (tlistbox& list, ttoggle_panel& widget, const bool)> did_row_focus_changed_;
	boost::function<bool (tlistbox& list, ttoggle_panel& widget)> did_row_pre_change_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_row_changed_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget, const int type)> did_row_click_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_row_double_click_;
	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_row_right_click_;

	boost::function<void (tlistbox& list, ttoggle_panel& widget)> did_allocated_gc_;
	boost::function<void (tlistbox& list)> did_more_rows_gc_;

	/** Inherited from tscroll_container. */
	void mini_place_content_grid(const tpoint& content_origin, const tpoint& content_size, const tpoint& desire_origin) override;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;
};

} // namespace gui2

#endif

