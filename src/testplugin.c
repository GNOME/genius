#include "config.h"
#include <stdio.h>
#include <glib.h>
#include "calc.h"
#include "plug_api.h"
#include "eval.h"
#include "dict.h"
#include "geloutput.h"

static GelETree *
TestPluginFunction_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	gel_output_printf (gel_main_out, _("This is the test-plugin function\n"));

	/* return a null */
	return gel_makenum_null ();
}

static void
open (void)
{
	GelEFunc *f;

	gel_output_printf (gel_main_out, _("You have opened test plugin!\n\n"
				       "Will evaluate 2+2 as a demonstration\n"
				       "2+2 = "));

	gel_evalexp ("2+2", NULL, gel_main_out, NULL, TRUE, NULL);

	gel_output_printf (gel_main_out, _("For my next trick I will add a "
				       "function named "
				       "TestPluginFunction\n\n"));

	f = d_addfunc (d_makebifunc (d_intern ("TestPluginFunction"),
				     TestPluginFunction_op, 0));
	d_add_named_args (f, "");
	gel_add_category ("TestPluginFunction" , "misc");
	gel_add_description ("TestPluginFunction",
			 "This is a test function added by the test plugin");

	gel_output_printf (gel_main_out, _("That's it, isn't this fun\n\n"));
}

static gboolean
save_state (const char *unique_id)
{
	printf("testplugin save_state(%s)\n", unique_id);
	return FALSE;
}

static void
restore_state (const char *unique_id)
{
	printf ("testplugin restore_state(%s)\n", unique_id);

	/* we open on restore */
	open ();
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
