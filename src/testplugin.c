#include "config.h"
#include <stdio.h>
#include <gtk/gtk.h>
#include "plug_api.h"
#include "calc.h"
#include "geloutput.h"

static void
open(void)
{
	GelOutput *gelo;
	printf("testplugin open()\n");
	puts("trying to evaluate 2+2");

	gelo = gel_output_new();
	
	evalexp("2+2", NULL, gelo, NULL, TRUE, NULL);

	gel_output_unref(gelo);
	
	puts("ending...");
}

static int
save_state(char *prefix)
{
	printf("testplugin save_state(%s)\n",prefix);
	return FALSE;
}

static void
restore_state(char *prefix)
{
	printf("testplugin restore_state(%s)\n",prefix);
}

static GelPluginInfo info = {
	open,
	save_state,
	restore_state
};

GelPluginInfo *
init_func(void)
{
	return &info;
}
