#define GETTEXT_DOMAIN "studio-lib"

#include "gui/dialogs/simple.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

namespace gui2 {

REGISTER_DIALOG(studio, simple)

tsimple::tsimple()
{
}

void tsimple::pre_show(CVideo& video, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));
	find_widget<tlabel>(&window, "title", false).set_label(_("Hello World"));
}

void tsimple::post_show(twindow& window)
{
}

} // namespace gui2

