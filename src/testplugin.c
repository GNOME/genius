#include "config.h"
#include <stdio.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include "plug_api.h"
#include "eval.h"
#include "calc.h"
#include "dict.h"
#include "geloutput.h"

static GelETree *
TestPluginFunction_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	gel_output_printf (main_out, _("This is the test-plugin function\n"));

	/* return a null */
	return gel_makenum_null ();
}

static void
open (void)
{
	GelEFunc *f;

	gel_output_printf (main_out, _("You have opened test plugin!\n\n"
				       "Will evaluate 2+2 as a demonstration\n"
				       "2+2 = "));

	evalexp("2+2", NULL, main_out, NULL, TRUE, NULL);

	gel_output_printf (main_out, _("For my next trick I will add a "
				       "function named "
				       "TestPluginFunction\n\n"));

	f = d_addfunc (d_makebifunc (d_intern ("TestPluginFunction"),
				     TestPluginFunction_op, 0));
	d_add_named_args (f, "");
	add_category ("TestPluginFunction" , "misc");
	add_description ("TestPluginFunction",
			 "This is a test function added by the test plugin");

	gel_output_printf (main_out, _("That's it, isn't this fun\n\n"));
}

static gboolean
save_state (const char *prefix)
{
	printf("testplugin save_state(%s)\n", prefix);
	return FALSE;
}

static void
restore_state (const char *prefix)
{
	printf ("testplugin restore_state(%s)\n", prefix);
}

static GelPluginInfo info = {
	open,
	save_state,
	restore_state
};

GelPluginInfo *
init_func (void)
{
	return &info;
}
