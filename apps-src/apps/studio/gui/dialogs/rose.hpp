/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_ROSE_HPP_INCLUDED
#define GUI_DIALOGS_ROSE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "lobby.hpp"
#include "build.hpp"
#include "theme.hpp"

class display;
class hero_map;
class hero;
class tapp_copier;
class tfile;

namespace gui2 {

class ttree_view_node;
class ttoggle_button;

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class trose : public tdialog, public tlobby::thandler, public tbuild, public rtc::MessageHandler
{
public:
	trose(display& disp, hero& player_hero);
	~trose();

	/**
	 * Values for the menu-items of the main menu.
	 *
	 * @todo Evaluate the best place for these items.
	 */
	enum tresult {
			EDIT_DIALOG = 1     /**< Let user select a campaign to play */
			, PLAYER
			, CHANGE_LANGUAGE
			, MESSAGE
			, EDIT_PREFERENCES
			, DESIGN
			, QUIT_GAME
			, NOTHING             /**<
			                       * Default, nothing done, no redraw needed
			                       */
			};

	bool handle(int tag, tsock::ttype type, const config& data) override;
	bool walk_dir2(const std::string& dir, const SDL_dirent2* dirent, ttree_view_node* root);
	const std::map<std::string, std::string>& get_app_tdomains() const { return tdomains; }

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show(CVideo& video, twindow& window) override;

	void set_retval(twindow& window, int retval);
	void icon_toggled(ttoggle_button& widget);

	bool compare_sort(const ttree_view_node& a, const ttree_view_node& b);
	void fill_items(twindow& window);

	void pre_base(twindow& window);

	void check_build_toggled(twidget& widget);
	void set_working_dir(twindow& window);
	void do_refresh(twindow& window);
	void do_normal_build(twindow& window, const bool refresh_listbox);

	void do_build(twindow& window);
	void set_build_active(twindow& window);

	void app_work_start() override;
	void app_work_done() override;
	void app_handle_desc(const bool started, const int at, const bool ret) override;

	void on_change_working_dir(twindow& window, const std::string& dir);
	void build_on_app_changed(const std::string& app, twindow& window, const bool remove);

	void right_click_explorer(ttree_view_node& node, bool& handled, bool& halt);
	void new_app(const tapp_capabilities& capabilities);
	void do_new_window(tapp_copier& copier, const std::string& id, bool scene, bool unit_files = false);
	void do_remove_dialog(tapp_copier& copier, const std::string& id, bool scene);
	void do_new_theme(tapp_copier& copier, const std::string& id, const theme::tfields& result);
	void export_app(tapp_copier& copier);
	bool copy_android_res(const tapp_copier& copier, bool silent);
	bool export_ios_kit(const tapp_copier& copier);
	void import_apps(const std::string& import_res);

	ttree_view_node& add_explorer_node(const std::string& dir, ttree_view_node& parent, const std::string& name, bool isdir);

	void validater_res(const std::set<std::string>& apps);
	void refresh_explorer_tree(twindow& window);

	void generate_gui_app_main_cfg(const std::string& res_path, const std::set<std::string>& apps) const;
	void generate_app_cfg(const std::string& res_path, const std::set<std::string>& apps) const;
	void generate_app_cfg(const std::string& res_path, const std::string& app) const
	{
		std::set<std::string> apps;
		apps.insert(app);
		generate_app_cfg(res_path, apps);
	}
	void generate_capabilities_cfg(const std::string& res_path) const;
	void generate_capabilities_cfg2(const std::string& res_path, const tapp_capabilities& copier) const;
	void reload_mod_configs(display& disp);

	bool collect_app(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& apps);
	bool collect_app2(const std::string& dir, const SDL_dirent2* dirent, std::vector<std::unique_ptr<tapp_copier> >& app_copiers);
	bool browse_import_res_changed(const std::string& path, const std::string& terminate);

	enum {build_normal, build_export, build_import, build_new, build_remove, build_new_dialog, build_ios_kit};
	class build_msg_data: public rtc::MessageData {
	public:
		build_msg_data(int _type, const std::string& _app) 
		{ 
			set(_type, _app); 
		}
		void set(int _type, const std::string& _app) 
		{
			type = _type;
			app = _app;
		}
		void set(int _type, const std::set<std::string>& _apps) 
		{
			type = _type;
			apps = _apps;
		}
		void set(int _type, const std::string& _app, const std::string& val) 
		{
			type = _type;
			app = _app;
			apps.clear();
			apps.insert(val);
		}

		int type;
		std::string app;
		std::set<std::string> apps;
	};

	enum {
		MSG_BUILD_FINISHED
	};
	void OnMessage(rtc::Message* msg) override;

	void test_gc_listbox() const;
private:
	display& disp_;
	hero& player_hero_;
	twindow* window_;

	std::string app_bin_id_;
	build_msg_data build_msg_data_;

	const tapp_copier* current_copier_;

	struct tcookie {
		tcookie(std::string dir, std::string name, bool isdir)
			: dir(dir)
			, name(name)
			, isdir(isdir)
		{}

		std::string dir;
		std::string name;
		bool isdir;
	};
	std::map<const ttree_view_node*, tcookie> cookies_;

	// why use std::vector<std::unique_ptr<tapp_copier> > insteal with std::vector<tapp_copier>.
	// deriver class from ttask will use tapp_copier instance, so tapp_copier must not make another one.
	std::vector<std::unique_ptr<tapp_copier> > app_copiers;
	std::unique_ptr<tnewer> newer;
	std::unique_ptr<timporter> importer;
	std::unique_ptr<tremover> remover;
	std::unique_ptr<tnew_dialog> new_dialog;
	std::unique_ptr<tnew_scene> new_scene;
	std::unique_ptr<tvalidater> validater;
	std::unique_ptr<tsave_theme> save_theme;
	std::unique_ptr<tremove_theme> remove_theme;

	config generate_cfg;
};

} // namespace gui2

#endif
