/* GENIUS Calculator
 * Copyright (C) 1997-2002 George Lebl
 *
 * Author: George Lebl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the  Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include "config.h"

#include <gnome.h>

#include <string.h>
#include <math.h>
#include <glib.h>
#include <limits.h>
#include "mpwrap.h"
#include "eval.h"
#include "dict.h"
#include "funclib.h"
#include "calc.h"
#include "matrix.h"
#include "matrixw.h"
#include "matop.h"
#include "geloutput.h"

extern int got_eof;
extern calcstate_t calcstate;

GelEFunc *_internal_ln_function = NULL;
GelEFunc *_internal_exp_function = NULL;

/*maximum number of primes to precalculate and store*/
#define MAXPRIMES 30000
GArray *primes = NULL;
int numprimes = 0;

static mpw_t e_cache;
static int e_iscached = FALSE;
static mpw_t golden_ratio_cache;
static int golden_ratio_iscached = FALSE;

/* As calculated by Thomas Papanikolaou taken from
 * http://www.cecm.sfu.ca/projects/ISC/dataB/isc/C/gamma.txt */
const char *euler_constant =
"0.5772156649015328606065120900824024310421593359399235988057672348848677267776\
646709369470632917467495146314472498070824809605040144865428362241739976449235\
362535003337429373377376739427925952582470949160087352039481656708532331517766\
115286211995015079847937450857057400299213547861466940296043254215190587755352\
673313992540129674205137541395491116851028079842348775872050384310939973613725\
530608893312676001724795378367592713515772261027349291394079843010341777177808\
815495706610750101619166334015227893586796549725203621287922655595366962817638\
879272680132431010476505963703947394957638906572967929601009015125195950922243\
501409349871228247949747195646976318506676129063811051824197444867836380861749\
455169892792301877391072945781554316005002182844096053772434203285478367015177\
394398700302370339518328690001558193988042707411542227819716523011073565833967\
348717650491941812300040654693142999297779569303100503086303418569803231083691\
640025892970890985486825777364288253954925873629596133298574739302373438847070\
370284412920166417850248733379080562754998434590761643167103146710722370021810\
745044418664759134803669025532458625442225345181387912434573501361297782278288\
148945909863846006293169471887149587525492366493520473243641097268276160877595\
088095126208404544477992299157248292516251278427659657083214610298214617951957\
959095922704208989627971255363217948873764210660607065982561990102880756125199\
137511678217643619057058440783573501580056077457934213144988500786415171615194\
565706170432450750081687052307890937046143066848179164968425491504967243121837\
838753564894950868454102340601622508515583867234944187880440940770106883795111\
307872023426395226920971608856908382511378712836820491178925944784861991185293\
910293099059255266917274468920443869711147174571574573203935209122316085086827\
558890109451681181016874975470969366671210206304827165895049327314860874940207\
006742590918248759621373842311442653135029230317517225722162832488381124589574\
386239870375766285513033143929995401853134141586212788648076110030152119657800\
681177737635016818389733896639868957932991456388644310370608078174489957958324\
579418962026049841043922507860460362527726022919682995860988339013787171422691\
788381952984456079160519727973604759102510995779133515791772251502549293246325\
028747677948421584050759929040185576459901862692677643726605711768133655908815\
548107470000623363725288949554636971433012007913085552639595497823023144039149\
740494746825947320846185246058776694882879530104063491722921858008706770690427\
926743284446968514971825678095841654491851457533196406331199373821573450874988\
325560888873528019019155089688554682592454445277281730573010806061770113637731\
824629246600812771621018677446849595142817901451119489342288344825307531187018\
609761224623176749775564124619838564014841235871772495542248201615176579940806\
296834242890572594739269638633838743805471319676429268372490760875073785283702\
304686503490512034227217436689792848629729088926789777032624623912261888765300\
577862743606094443603928097708133836934235508583941126709218734414512187803276\
150509478055466300586845563152454605315113252818891079231491311032344302450933\
450003076558648742229717700331784539150566940159988492916091140029486902088485\
381697009551566347055445221764035862939828658131238701325358800625686626926997\
767737730683226900916085104515002261071802554659284938949277595897540761559933\
782648241979506418681437881718508854080367996314239540091964388750078900000627\
997942809886372992591977765040409922037940427616817837156686530669398309165243\
227059553041766736640116792959012930537449718308004275848635083808042466735093\
559832324116969214860649892763624432958854873789701489713343538448002890466650\
902845376896223983048814062730540879591189670574938544324786914808533770264067\
758081275458731117636478787430739206642011251352727499617545053085582356683068\
322917676677041035231535032510124656386156706449847132695969330167866138333333\
441657900605867497103646895174569597181553764078377650184278345991842015995431\
449047725552306147670165993416390660912054005322158902091340802782251533852899\
511665452245869185993671220132150144801424230986254604488672569343148870491593\
044640189164502022405495386291847586293077889350643771596606909604681243702305\
465703160679992587166675247219409777980186362625633582526279422393254860132693\
530701388937436923842878938512764740856548650281563067740442203064403756826309\
102917514572234441050369317711452170888907446416048688701083862311426128441425\
960956370400619200579335034155242624026206465693543061258526583452192121497771\
878069586608516334922104836737994592594340379560002192785418379417760203365594\
673078879838084816314678241492354649148876683368407492893865281863048589820354\
818624383848175997635849075180791480634943916284705482200754945348986133827235\
730922190030740096800337666844932505567654937530318112516410552492384077645149\
842395762012781552322944928854557853820248918942441857095919558208100071578384\
039627479985817880888865716830699436060735990421068511427913169699596792300828\
988156097538338059109360341252998656790389568795673455083362907823862638563490\
747319275278740166557531190111543470018186256971261120126852923129937161403906\
965112224816615082353643982396620532633322248505191593682690715004315589871802\
783353845448309107249498057880961717996337167036554180041464667538719586948483\
331543583330641935929487420951478832347748481418149776871694413640056645156936\
116524161555734141935424721373067468333849054426626038372788217552709930958141\
026136979500786465876771608630804460749802801576962675913897794772214337515470\
829345879123898433055067223474969984942486706721502569273529585065869588997486\
535562186958043997125168976654169862653862891977542187721939605817001104236414\
158780810386172101557551923711160049880682291618097732421958328974869227183979\
190467716542668138893379296036815457939611339621922245430151580631743708405608\
536416031384982969518566952612822123716939368130321296561939718710207098007948\
833910197535104307441823448833331796978277332091143324514305086573457500687391\
475470777577559918467118308583660159437193718449039061770232536567977596744475\
747511584195746700997345002454428406585024508585646392791246119879093693072019\
804029303603738838430742162821201635386466226097198958436799430572030149638050\
832232365825557724534237187737439818333306454662906993311125973721950274646899\
065457155440303917835419756434315739034883866750542742161831050060550464223545\
708427393549359051762717479299472398908632970101905610107742690926475235740304\
630159243442464900834188630859320685522507790910195858895314328799817570981916\
829315940453005632543314488517357302698256937253469964013440871580108145287865\
790408663637945071108505104241797691911292615132010316363498086606948624407800\
668400671696221463718114777268341846646364242734053003138077349611998146861768\
585463120816316479893796426373835661893831371098328956490521148813402974238886\
863154313297876579912545424333856347200268129048994955042698088213026726358153\
248067538790323057421040330149788786752377860705468861472100992632942510887801\
970284117922402591091466584809257857192786282147667074087863519714256292427867\
028407703241437569931883243331559002433304769111009247979118006286202213707800\
621725732904735994398883139279927969397063567628116694054128859081982023838277\
035483496879734048888293016736770941584654400954862465146101353913496855912040\
236361872150992980651905861682815302875042754525860533196343259577747881343723\
939499124380614375449859068607518563142725525564259396701498041425981823785257\
682943639596562438852065654807103884546394453770191784571874101186223227802525\
194362657438242256093567692582387749116073775945140144703190224153559112506138\
178297421264982641618724606313340891926702359795802365841631755679233566210123\
133584549459059006998420067226025116774384736482438571540714626594564239112717\
078030637141692638644010057131095896063264963755295676936468941051795200061645\
202188435340473018243930514881984593076296404445687762416528716207276731860632\
540801428874571198657307471701886603687970364770854852871670003622928528837468\
246605881411754047446061676354303739923756596593696708792316774468569310838210\
783048315919643002144125970228906320317410114936648095290301171633453191792293\
924242877283787234956992923213609223494722645824375509451533552011761289751733\
951371782933287158609438662701179184155458726489825139255594379519731786744876\
992532617942338129994127939860264424519600605436818664670986594436593015437629\
148697959769499653352721002002096791043948254724411334224487005463765684086676\
215337362746159120547008629057169825735370523976123123841256434941178949861582\
859702097109703919635216812258224756271951938272828520914718237534365525402074\
620306673047695247009441381456178282666319613967359367257264603388494642489472\
448978481596715306153846712236987128231978476931105716623637326207595971180401\
514389623170601958570982381392466613527913695637655904861676205129740998314965\
597860253410129454399867288300624408440186181511750687088764671609799292017769\
624963301575829259618849485872002292248060628812177873383145882551293953651088\
191865120044923154983947731473578688973142047310945381464822632102331079439597\
462852354129575191063558792300195618131294612030157576340159785681751749842377\
882447375398247457599686770740854557432942402678193848220354096210606072192499\
082510485400318496319986322156908909761405310716511312932685349852440286448293\
470591608586995981988903959955918110764134466588145254256588153545028884739975\
324327809021522569521844145298650835512983980226238264974881811581525034599749\
";


void
gel_break_fp_caches (void)
{
	if (e_iscached) {
		e_iscached = FALSE;
		mpw_clear (e_cache);
	}
	if (golden_ratio_iscached) {
		golden_ratio_iscached = FALSE;
		mpw_clear (golden_ratio_cache);
	}
}

static int
get_nonnegative_integer (mpw_ptr z, const char *funcname)
{
	long i;
	i = mpw_get_long(z);
	if (error_num != 0) {
		error_num = 0;
		return -1;
	}
	if (i <= 0) {
		char *s = g_strdup_printf (_("%s: argument can't be negative or 0"), funcname);
		(*errorout)(s);
		g_free (s);
		return -1;
	}
	if (i > INT_MAX) {
		char *s = g_strdup_printf (_("%s: argument too large"), funcname);
		(*errorout)(s);
		g_free (s);
		return -1;
	}
	return i;
}

static GelETree *
manual_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GString *str;
	FILE *fp;

	str = g_string_new (NULL);

	fp = fopen ("../doc/manual.txt", "r");
	if (fp == NULL)
		fp = fopen (LIBRARY_DIR "/manual.txt", "r");

	if (fp != NULL) {
		char buf[256];
		while (fgets (buf, sizeof(buf), fp) != NULL) {
			g_string_append (str, buf);
		}

		fclose (fp);
	} else {
		g_string_append (str,
				 _("Cannot locate the manual"));
	}

	(*infoout) (str->str);
	error_num = IGNORE_ERROR;
	if(exception) *exception = TRUE; /*raise exception*/

	g_string_free (str, TRUE);

	return NULL;
}

static GelETree *
warranty_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	char *p;
	p = g_strdup_printf(_("Genius %s\n"
		    "%s\n\n"
		    "    This program is free software; you can redistribute it and/or modify\n"
		    "    it under the terms of the GNU General Public License as published by\n"
		    "    the Free Software Foundation; either version 2 of the License , or\n"
		    "    (at your option) any later version.\n"
		    "\n"
		    "    This program is distributed in the hope that it will be useful,\n"
		    "    but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		    "    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		    "    GNU General Public License for more details.\n"
		    "\n"
		    "    You should have received a copy of the GNU General Public License\n"
		    "    along with this program. If not, write to the Free Software\n"
		    "    Foundation,  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,\n"
		    "    USA.\n"), 
			    VERSION,
			    COPYRIGHT_STRING);
	(*infoout)(p);
	g_free(p);
	error_num = IGNORE_ERROR;
	if(exception) *exception = TRUE; /*raise exception*/
	return NULL;
}

static GelETree *
exit_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	got_eof = TRUE;
	if(exception) *exception = TRUE; /*raise exception*/
	return NULL;
}

static GelETree *
ni_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	(*infoout)("We are the Knights Who Say... Ni!");
	if(exception) *exception = TRUE; /*raise exception*/
	error_num = IGNORE_ERROR;
	return NULL;
}

static GelETree *
shrubbery_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	(*infoout)("Then, when you have found the shrubbery, you must\n"
		   "cut down the mightiest tree in the forest... with...\n"
		   "A HERRING!");
	if(exception) *exception = TRUE; /*raise exception*/
	error_num = IGNORE_ERROR;
	return NULL;
}
	
/*error printing function*/
static GelETree *
error_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		(*errorout)(a[0]->str.str);
	else {
		GelOutput *gelo = gel_output_new();
		char *s;
		gel_output_setup_string (gelo, 0, NULL);
		pretty_print_etree(gelo, a[0]);
		s = gel_output_snarf_string(gelo);
		gel_output_unref(gelo);
		(*errorout)(s?s:"");
	}
	return gel_makenum_null();
}
/*print function*/
static GelETree *
print_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (a[0]->type==STRING_NODE) {
		gel_output_printf_full (main_out, FALSE, "%s\n", a[0]->str.str);
	} else {
		/* FIXME: whack limit */
		pretty_print_etree (main_out, a[0]);
		gel_output_string (main_out,"\n");
	}
	gel_output_flush (main_out);
	return gel_makenum_null();
}
/*print function*/
static GelETree *
chdir_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (a[0]->type != STRING_NODE) {
		(*errorout)(_("chdir: argument must be string!"));
		return NULL;
	}
	return gel_makenum_si (chdir (a[0]->str.str));
}
/*print function*/
static GelETree *
printn_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		gel_output_printf(main_out, "%s", a[0]->str.str);
	else
		print_etree(main_out, a[0], TRUE);
	gel_output_flush(main_out);
	return gel_makenum_null();
}
/*print function*/
static GelETree *
display_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=STRING_NODE) {
		(*errorout)(_("display: first argument must be string!"));
		return NULL;
	}
	gel_output_printf(main_out, "%s: ", a[0]->str.str);
	pretty_print_etree(main_out, a[1]);
	gel_output_string(main_out, "\n");
	gel_output_flush(main_out);
	return gel_makenum_null();
}

/*set function*/
static GelETree *
set_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelToken *id;
	GelEFunc *func;
	if (a[0]->type != IDENTIFIER_NODE &&
	    a[0]->type != STRING_NODE) {
		(*errorout)(_("set: first argument must be an identifier or string!"));
		return NULL;
	}
	if (a[0]->type == IDENTIFIER_NODE) {
		id = a[0]->id.id;
	} else /* STRING_NODE */ {
		id = d_intern (a[0]->str.str);
	}

	if (id->protected) {
		(*errorout)(_("set: trying to set a protected id!"));
		return NULL;
	}
	if (id->parameter) {
		/* FIXME: fix this, this should just work too */
		(*errorout)(_("set: trying to set a parameter, use the equals sign"));
		return NULL;
	}

	func = d_makevfunc (id, copynode (a[1]));
	/* make function global */
	func->context = 0;
	d_addfunc_global (func);

	return copynode (a[1]);
}

/*rand function*/
static GelETree *
rand_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int args;

	args = 0;
	while (a != NULL && a[args] != NULL)
		args++;

	if (args > 2) {
		(*errorout)(_("rand: Too many arguments, should be at most two"));
		return NULL;
	}

	if (args == 0) {
		mpw_t fr; 
		mpw_init (fr);
		mpw_rand (fr);

		return gel_makenum_use (fr);
	} else if (args == 1) {
		GelETree *n;
		GelMatrix *m;
		int size, i;

		if (a[0]->type != VALUE_NODE ||
		    mpw_is_complex(a[0]->val.value) ||
		     ! mpw_is_integer (a[0]->val.value)) {
			(*errorout)(_("rand: arguments must be integers"));
			return NULL;
		}

		size = get_nonnegative_integer (a[0]->val.value, "rand");
		if (size < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, size, 1, FALSE /* padding */);
		for (i = 0; i < size; i++) {
			mpw_t fr; 
			mpw_init (fr);
			mpw_rand (fr);

			gel_matrix_index (m, i, 0) = gel_makenum_use (fr);
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = 0;

		return n;
	} else /* args == 2 */ {
		GelETree *n;
		GelMatrix *m;
		int sizex, sizey, i, j;

		if (a[0]->type != VALUE_NODE ||
		    a[1]->type != VALUE_NODE ||
		    mpw_is_complex(a[0]->val.value) ||
		    mpw_is_complex(a[1]->val.value) ||
		    ! mpw_is_integer (a[0]->val.value) ||
		    ! mpw_is_integer (a[1]->val.value)) {
			(*errorout)(_("rand: arguments must be integers"));
			return NULL;
		}

		sizey = get_nonnegative_integer (a[0]->val.value, "rand");
		if (sizey < 0)
			return NULL;
		sizex = get_nonnegative_integer (a[1]->val.value, "rand");
		if (sizex < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, sizex, sizey, FALSE /* padding */);
		for (i = 0; i < sizex; i++) {
			for (j = 0; j < sizey; j++) {
				mpw_t fr; 
				mpw_init (fr);
				mpw_rand (fr);

				gel_matrix_index (m, i, j) = gel_makenum_use (fr);
			}
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = 0;

		return n;
	}
}

/*rand function*/
static GelETree *
randint_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	int args;

	args = 0;
	while (a[args] != NULL)
		args++;

	if (args > 3) {
		(*errorout)(_("randint: Too many arguments, should be at most two"));
		return NULL;
	}

	if (args == 1) {
		mpw_t fr; 

		if (a[0]->type != VALUE_NODE ||
		    mpw_is_complex(a[0]->val.value) ||
		    ! mpw_is_integer (a[0]->val.value)) {
			(*errorout)(_("randint: arguments must be integers"));
			return NULL;
		}

		mpw_init (fr);
		mpw_randint (fr, a[0]->val.value);
		if (error_num != 0) {
			mpw_clear (fr);
			return NULL;
		}

		return gel_makenum_use (fr);
	} else if (args == 2) {
		GelETree *n;
		GelMatrix *m;
		int size, i;

		if (a[0]->type != VALUE_NODE ||
		    a[1]->type != VALUE_NODE ||
		    mpw_is_complex (a[0]->val.value) ||
		    mpw_is_complex (a[1]->val.value) ||
		    ! mpw_is_integer (a[0]->val.value) ||
		    ! mpw_is_integer (a[1]->val.value)) {
			(*errorout)(_("randint: arguments must be integers"));
			return NULL;
		}

		size = get_nonnegative_integer (a[1]->val.value, "randint");
		if (size < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, size, 1, FALSE /* padding */);
		for (i = 0; i < size; i++) {
			mpw_t fr;
			mpw_init (fr);
			mpw_randint (fr, a[0]->val.value);
			if (error_num != 0) {
				mpw_clear (fr);
				/* This can only happen if a[0]->val.value is
				 * evil, in which case we have not set any
				 * elements yet.  So we don't have to free any
				 * elements yet */
				g_assert (i == 0);
				gel_matrix_free (m);
				return NULL;
			}

			gel_matrix_index (m, i, 0) = gel_makenum_use (fr);
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = 0;

		return n;
	} else /* args == 3 */ {
		GelETree *n;
		GelMatrix *m;
		int sizex, sizey, i, j;

		if (a[0]->type != VALUE_NODE ||
		    a[1]->type != VALUE_NODE ||
		    a[2]->type != VALUE_NODE ||
		    mpw_is_complex (a[0]->val.value) ||
		    mpw_is_complex (a[1]->val.value) ||
		    mpw_is_complex (a[2]->val.value) ||
		    ! mpw_is_integer (a[0]->val.value) ||
		    ! mpw_is_integer (a[1]->val.value) ||
		    ! mpw_is_integer (a[2]->val.value)) {
			(*errorout)(_("randint: arguments must be integers"));
			return NULL;
		}

		sizey = get_nonnegative_integer (a[1]->val.value, "randint");
		if (sizey < 0)
			return NULL;
		sizex = get_nonnegative_integer (a[2]->val.value, "randint");
		if (sizex < 0)
			return NULL;

		m = gel_matrix_new ();
		gel_matrix_set_size (m, sizex, sizey, FALSE /* padding */);
		for (i = 0; i < sizex; i++) {
			for (j = 0; j < sizey; j++) {
				mpw_t fr;
				mpw_init (fr);
				mpw_randint (fr, a[0]->val.value);
				if (error_num != 0) {
					mpw_clear (fr);
					/* This can only happen if a[0]->val.value is
					 * evil, in which case we have not set any
					 * elements yet.  So we don't have to free any
					 * elements yet */
					g_assert (i == 0 && j == 0);
					gel_matrix_free (m);
					return NULL;
				}

				gel_matrix_index (m, i, j) = gel_makenum_use (fr);
			}
		}

		GET_NEW_NODE (n);
		n->type = MATRIX_NODE;
		n->mat.matrix = gel_matrixw_new_with_matrix (m);
		n->mat.quoted = 0;

		return n;
	}
}

static GelETree *
apply_func_to_matrixen(GelCtx *ctx, GelETree *mat1, GelETree *mat2,
		       GelETree * (*function)(GelCtx *ctx, GelETree **a,int *exception),
		       char *ident)
{
	GelMatrixW *m1 = NULL;
	GelMatrixW *m2 = NULL;
	GelMatrixW *new;
	GelETree *re_node = NULL;
	int reverse = FALSE;
	GelETree *n;
	int i,j;
	int quote = 0;

	if(mat1->type == MATRIX_NODE &&
	   mat2->type == MATRIX_NODE) {
		m1 = mat1->mat.matrix;
		m2 = mat2->mat.matrix;
		quote = mat1->mat.quoted || mat2->mat.quoted;
	} else if(mat1->type == MATRIX_NODE) {
		m1 = mat1->mat.matrix;
		quote = mat1->mat.quoted;
		re_node = mat2;
	} else /*if(mat2->type == MATRIX_NODE)*/ {
		m1 = mat2->mat.matrix;
		quote = mat2->mat.quoted;
		re_node = mat1;
		reverse = TRUE;
	}
	
	if(m2 && (gel_matrixw_width(m1) != gel_matrixw_width(m2) ||
		  gel_matrixw_height(m1) != gel_matrixw_height(m2))) {
		(*errorout)(_("Cannot apply function to two differently sized matrixes"));
		return NULL;
	}
	
	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	new = n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = quote;
	gel_matrixw_set_size(new,gel_matrixw_width(m1),gel_matrixw_height(m1));

	for(i=0;i<gel_matrixw_width(m1);i++) {
		for(j=0;j<gel_matrixw_height(m1);j++) {
			GelETree *t[2];
			GelETree *e;
			int ex = FALSE;
			if(!reverse) {
				t[0] = gel_matrixw_index(m1,i,j);
				t[1] = m2?gel_matrixw_index(m2,i,j):re_node;
			} else {
				t[0] = m2?gel_matrixw_index(m2,i,j):re_node;
				t[1] = gel_matrixw_index(m1,i,j);
			}
			e = (*function)(ctx, t,&ex);
			/*FIXME: handle exceptions*/
			if(!e) {
				GelETree *nn;
				GelETree *ni;
				GET_NEW_NODE(ni);
				ni->type = IDENTIFIER_NODE;
				ni->id.id = d_intern(ident);

				GET_NEW_NODE(nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_CALL;
				nn->op.nargs = 3;
				nn->op.args = ni;
				nn->op.args->any.next = copynode(t[0]);
				nn->op.args->any.next->any.next = copynode(t[1]);
				nn->op.args->any.next->any.next->any.next = NULL;

				gel_matrixw_set_index(new,i,j) = nn;
			} else {
				gel_matrixw_set_index(new,i,j) = e;
			}
		}
	}
	return n;
}

static GelETree *
apply_func_to_matrix (GelCtx *ctx, GelETree *mat, 
		      GelETree * (*function)(GelCtx *ctx, GelETree **a,int *exception),
		      char *ident)
{
	GelMatrixW *m;
	GelMatrixW *new;
	GelETree *n;
	int i,j;

	m = mat->mat.matrix;
	
	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	new = n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = mat->mat.quoted;
	gel_matrixw_set_size(new,gel_matrixw_width(m),gel_matrixw_height(m));

	for(i=0;i<gel_matrixw_width(m);i++) {
		for(j=0;j<gel_matrixw_height(m);j++) {
			GelETree *t[1];
			GelETree *e;
			int ex = FALSE;
			t[0] = gel_matrixw_index(m,i,j);
			e = (*function)(ctx,t,&ex);
			/*FIXME: handle exceptions*/
			if(!e) {
				GelETree *nn;
				GelETree *ni;
				GET_NEW_NODE(nn);
				nn->type = OPERATOR_NODE;
				nn->op.oper = E_CALL;
				nn->op.args = NULL;
				nn->op.nargs = 2;
				
				GET_NEW_NODE(ni);
				ni->type = IDENTIFIER_NODE;
				ni->id.id = d_intern(ident);
				
				nn->op.args = ni;
				nn->op.args->any.next = copynode(t[0]);
				nn->op.args->any.next->any.next = NULL;

				gel_matrixw_set_index(new,i,j) = nn;
			} else if (e->type == VALUE_NODE &&
				   ! mpw_is_complex (e->val.value) &&
				   mpw_is_integer (e->val.value) &&
				   mpw_sgn (e->val.value) == 0) {
				gel_freetree (e);
				gel_matrixw_set_index(new,i,j) = NULL;
			} else {
				gel_matrixw_set_index(new,i,j) = e;
			}
		}
	}
	return n;
}

/* expand matrix function*/
static GelETree *
ExpandMatrix_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;

	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("ExpandMatrix: argument not a matrix"));
		return NULL;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy (a[0]->mat.matrix);
	gel_expandmatrix (n);
	n->mat.quoted = 0;
	return n;
}

static GelETree *
RowsOf_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;

	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("RowsOf: argument not a matrix"));
		return NULL;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_rowsof (a[0]->mat.matrix);
	n->mat.quoted = 0;
	return n;
}

static GelETree *
ColumnsOf_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;

	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("ColumnsOf: argument not a matrix"));
		return NULL;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_columnsof (a[0]->mat.matrix);
	n->mat.quoted = 0;
	return n;
}

static GelETree *
DiagonalOf_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;

	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("DiagonalOf: argument not a matrix"));
		return NULL;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_diagonalof (a[0]->mat.matrix);
	n->mat.quoted = 0;
	return n;
}

/*ComplexConjugate function*/
static GelETree *
ComplexConjugate_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if (a[0]->type == MATRIX_NODE)
		return apply_func_to_matrix (ctx, a[0], ComplexConjugate_op, "ComplexConjugate");

	if (a[0]->type != VALUE_NODE) {
		(*errorout)(_("ComplexConjugate: argument not a number"));
		return NULL;
	}

	mpw_init (fr);

	mpw_conj (fr, a[0]->val.value);

	return gel_makenum_use (fr);
}

/*sin function*/
static GelETree *
sin_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sin_op,"sin");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("sin: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_sin(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*sinh function*/
static GelETree *
sinh_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sinh_op,"sinh");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("sinh: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_sinh(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*cos function*/
static GelETree *
cos_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],cos_op,"cos");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("cos: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_cos(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*cosh function*/
static GelETree *
cosh_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],cosh_op,"cosh");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("cosh: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_cosh(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}

/*tan function*/
static GelETree *
tan_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;
	mpw_t fr2;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],tan_op,"tan");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value)) {
		(*errorout)(_("tan: argument not a real number"));
		return NULL;
	}

	mpw_init(fr);
	mpw_set(fr,a[0]->val.value);

	/*is this algorithm always precise??? sin/cos*/
	mpw_init(fr2);
	mpw_cos(fr2,fr);
	mpw_sin(fr,fr);
	mpw_div(fr,fr,fr2);
	mpw_clear(fr2);

	return gel_makenum_use(fr);
}

/*atan (arctan) function*/
static GelETree *
atan_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],atan_op,"atan");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("atan: argument not a number"));
		return NULL;
	}

	mpw_init(fr);

	mpw_arctan(fr,a[0]->val.value);

	return gel_makenum_use(fr);
}
	

/*e function (or e variable actually)*/
static GelETree *
e_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (e_iscached)
		return gel_makenum (e_cache);

	mpw_init (e_cache);
	mpw_set_ui (e_cache,1);
	mpw_exp (e_cache, e_cache);
	e_iscached = TRUE;
	return gel_makenum (e_cache);
}

/* Free fall accelleration */
static GelETree *
g_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t g;
	mpw_init (g);
	mpw_set_d (g, 9.80665);
	return gel_makenum_use (g);
}

/* EulerConstant */
static GelETree *
EulerConstant_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t e;
	mpw_init (e);
	mpw_set_str_float (e, euler_constant, 10);
	return gel_makenum_use (e);
}

/*pi function (or pi variable or whatever)*/
static GelETree *
pi_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr; 
	mpw_init (fr);
	mpw_pi (fr);

	return gel_makenum_use (fr);
}

static GelETree *
GoldenRatio_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if (golden_ratio_iscached)
		return gel_makenum (golden_ratio_cache);

	mpw_init (golden_ratio_cache);
	mpw_set_ui (golden_ratio_cache, 5);
	mpw_sqrt (golden_ratio_cache, golden_ratio_cache);
	mpw_add_ui (golden_ratio_cache, golden_ratio_cache, 1);
	mpw_div_ui (golden_ratio_cache, golden_ratio_cache, 2);
	golden_ratio_iscached = TRUE;
	return gel_makenum (golden_ratio_cache);
}

static GelETree *
IsNull_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==NULL_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsValue_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==VALUE_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsString_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==STRING_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsMatrix_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsFunction_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==FUNCTION_NODE)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsFunctionRef_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==OPERATOR_NODE &&
	   a[0]->op.oper == E_REFERENCE) {
		GelETree *arg = a[0]->op.args;
		g_assert(arg);
		if(arg->type==IDENTIFIER_NODE &&
		   d_lookup_global(arg->id.id))
			return gel_makenum_ui(1);
	}
	return gel_makenum_ui(0);
}
static GelETree *
IsComplex_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsReal_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE)
		return gel_makenum_ui(0);
	else if(mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else
		return gel_makenum_ui(1);
}
static GelETree *
IsMatrixReal_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("IsMatrixReal: argument not a matrix"));
		return NULL;
	}

	if (gel_is_matrix_value_only_real (a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsInteger_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_integer(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsMatrixInteger_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("IsMatrixInteger: argument not a matrix"));
		return NULL;
	}

	if (gel_is_matrix_value_only_integer (a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsRational_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_rational(a[0]->val.value) ||
		mpw_is_integer(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsMatrixRational_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if (a[0]->type != MATRIX_NODE) {
		(*errorout)(_("IsMatrixRational: argument not a matrix"));
		return NULL;
	}

	if (gel_is_matrix_value_only_rational (a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
IsFloat_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex(a[0]->val.value))
		return gel_makenum_ui(0);
	else if(mpw_is_float(a[0]->val.value))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
trunc_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],trunc_op,"trunc");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("trunc: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_trunc(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
floor_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],floor_op,"floor");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("floor: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_floor(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
ceil_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],ceil_op,"ceil");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("ceil: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_ceil(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
round_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],round_op,"round");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("round: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_round(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}
static GelETree *
float_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],float_op,"float");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("float: argument not a number"));
		return NULL;
	}
	mpw_init_set(fr,a[0]->val.value);
	mpw_make_float(fr);
	return gel_makenum_use(fr);
}

static GelETree *
Numerator_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Numerator_op,"Numerator");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Numerator: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_numerator(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
Denominator_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Denominator_op,"Denominator");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Denominator: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_denominator(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

static GelETree *
Re_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Re_op,"Re");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Re: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_re(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
Im_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Im_op,"Im");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("Im: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_im(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
sqrt_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],sqrt_op,"sqrt");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("sqrt: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_sqrt(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
exp_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE) {
		if(gel_matrixw_width(a[0]->mat.matrix) !=
		   gel_matrixw_height(a[0]->mat.matrix)) {
			(*errorout)(_("exp: matrix argument is not square"));
			return NULL;
		}
		return funccall(ctx,_internal_exp_function,a,1);
	}

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("exp: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_exp(fr,a[0]->val.value);
	return gel_makenum_use(fr);
}

static GelETree *
ln_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t fr;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],ln_op,"ln");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("ln: argument not a number"));
		return NULL;
	}
	mpw_init(fr);
	mpw_ln(fr,a[0]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(fr);
		return NULL;
	}
	return gel_makenum_use(fr);
}

/*gcd function*/
static GelETree *
gcd2_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],gcd2_op,"gcd");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("gcd: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_gcd(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*gcd function*/
static GelETree *
gcd_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *gcd;
	int i;

	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			mpw_t gcd;
			if ( ! gel_is_matrix_value_only_integer (a[0]->mat.matrix)) {
				(*errorout)(_("gcd: matrix argument must be integer only"));
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			mpw_init (gcd);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (i == 0 && j == 0) {
						mpw_abs (gcd, n->val.value);
					} else {
						mpw_gcd (gcd, gcd, n->val.value);
					}
				}
			}
			return gel_makenum_use (gcd);
		} else if (a[0]->type == VALUE_NODE) {
			mpw_t tmp;
			if (mpw_is_complex (a[0]->val.value) ||
			    ! mpw_is_integer (a[0]->val.value)) {
				(*errorout)(_("gcd: argument must be an integer"));
				return NULL;
			}
			mpw_init (tmp);
			mpw_abs (tmp, a[0]->val.value);
			return gel_makenum_use (tmp);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	gcd = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { gcd, a[i] };
		GelETree *res;
		res = gcd2_op (ctx, argv, exception);
		if (res == NULL) {
			if (gcd != a[0])
				gel_freetree (gcd);
			return NULL;
		}
		if (gcd != a[0])
			gel_freetree (gcd);
		gcd = res;
	}
	if (gcd == a[0]) {
		mpw_t tmp;
		mpw_init (tmp);
		mpw_abs (tmp, a[0]->val.value);
		return gel_makenum_use (tmp);
	} else {
		return gcd;
	}

}

/*lcm function*/
static GelETree *
lcm2_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],lcm2_op,"lcm");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("lcm: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_lcm(tmp,
		a[0]->val.value,
		a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*lcm function*/
static GelETree *
lcm_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *lcm;
	int i;

	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			mpw_t lcm;
			if ( ! gel_is_matrix_value_only_integer (a[0]->mat.matrix)) {
				(*errorout)(_("lcm: matrix argument must be integer only"));
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			mpw_init (lcm);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (i == 0 && j == 0) {
						mpw_set (lcm, n->val.value);
					} else {
						mpw_lcm (lcm, lcm, n->val.value);
					}
				}
			}
			return gel_makenum_use (lcm);
		} else if (a[0]->type == VALUE_NODE) {
			mpw_t tmp;
			if (mpw_is_complex (a[0]->val.value) ||
			    ! mpw_is_integer (a[0]->val.value)) {
				(*errorout)(_("lcm: argument must be an integer"));
				return NULL;
			}
			mpw_init (tmp);
			mpw_abs (tmp, a[0]->val.value);
			return gel_makenum_use (tmp);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	lcm = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { lcm, a[i] };
		GelETree *res;
		res = lcm2_op (ctx, argv, exception);
		if (res == NULL) {
			if (lcm != a[0])
				gel_freetree (lcm);
			return NULL;
		}
		if (lcm != a[0])
			gel_freetree (lcm);
		lcm = res;
	}
	if (lcm == a[0]) {
		mpw_t tmp;
		mpw_init (tmp);
		mpw_abs (tmp, a[0]->val.value);
		return gel_makenum_use (tmp);
	} else {
		return lcm;
	}
}

/*jacobi function*/
static GelETree *
Jacobi_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],Jacobi_op,"Jacobi");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("Jacobi: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_jacobi(tmp,
		   a[0]->val.value,
		   a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*kronecker function*/
static GelETree *
JacobiKronecker_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1], JacobiKronecker_op, "JacobiKronecker");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("JacobiKronecker: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_kronecker(tmp,
		      a[0]->val.value,
		      a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*legendre function*/
static GelETree *
Legendre_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t tmp;

	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],Legendre_op,"Legendre");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("Legendre: arguments must be numbers"));
		return NULL;
	}

	mpw_init(tmp);
	mpw_legendre(tmp,
		     a[0]->val.value,
		     a[1]->val.value);
	if(error_num) {
		error_num = 0;
		mpw_clear(tmp);
		return NULL;
	}

	return gel_makenum_use(tmp);
}

/*perfect square testing function*/
static GelETree *
IsPerfectSquare_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],IsPerfectSquare_op,"IsPerfectSquare");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("IsPerfectSquare: argument must be a number"));
		return NULL;
	}

	if(mpw_perfect_square(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}


/*perfect square testing function*/
static GelETree *
IsPerfectPower_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],IsPerfectPower_op,"IsPerfectPower");

	if(a[0]->type!=VALUE_NODE) {
		(*errorout)(_("IsPerfectPower: argument must be a number"));
		return NULL;
	}

	if(mpw_perfect_power(a[0]->val.value)) {
		return gel_makenum_ui(1);
	} else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum_ui(0);
	}
}

/*max function for two elements */
static GelETree *
max2_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],max2_op,"max");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("max: arguments must be numbers"));
		return NULL;
	}

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)<0)
		return gel_makenum(a[1]->val.value);
	else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum(a[0]->val.value);
	}
}

/*max function*/
static GelETree *
max_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *max = NULL;
	int i;
	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			if ( ! gel_is_matrix_value_only (a[0]->mat.matrix)) {
				(*errorout)(_("max: matrix argument must be value only"));
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (max == NULL) {
						max = n;
					} else if (max != n) {
						if (mpw_cmp (n->val.value, max->val.value) > 0)
							max = n;
					}
				}
			}
			g_assert (max != NULL);
			return gel_makenum (max->val.value);
		} else if (a[0]->type == VALUE_NODE) {
			return copynode (a[0]);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	max = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { max, a[i] };
		GelETree *res;
		res = max2_op (ctx, argv, exception);
		if (res == NULL) {
			if (max != a[0])
				gel_freetree (max);
			return NULL;
		}
		if (max != a[0])
			gel_freetree (max);
		max = res;
	}
	if (max == a[0])
		return copynode (a[0]);
	else
		return max;
}

/*min function*/
static GelETree *
min2_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type==MATRIX_NODE ||
	   a[1]->type==MATRIX_NODE)
		return apply_func_to_matrixen(ctx,a[0],a[1],min2_op,"min");

	if(a[0]->type!=VALUE_NODE ||
	   a[1]->type!=VALUE_NODE) {
		(*errorout)(_("min: arguments must be numbers"));
		return NULL;
	}

	if(mpw_cmp(a[0]->val.value,a[1]->val.value)>0)
		return gel_makenum(a[1]->val.value);
	else {
		if(error_num) {
			error_num = 0;
			return NULL;
		}
		return gel_makenum(a[0]->val.value);
	}
}

/*min function*/
static GelETree *
min_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *min = NULL;
	int i;
	if (a[1] == NULL) {
		if (a[0]->type == MATRIX_NODE) {
			int j, w, h;
			if ( ! gel_is_matrix_value_only (a[0]->mat.matrix)) {
				(*errorout)(_("min: matrix argument must be value only"));
				return NULL;
			}
			w = gel_matrixw_width (a[0]->mat.matrix);
			h = gel_matrixw_height (a[0]->mat.matrix);
			for (i = 0; i < w; i++) {
				for (j = 0; j < h; j++) {
					GelETree *n = gel_matrixw_index (a[0]->mat.matrix, i, j);
					if (min == NULL) {
						min = n;
					} else if (min != n) {
						if (mpw_cmp (n->val.value, min->val.value) < 0)
							min = n;
					}
				}
			}
			g_assert (min != NULL);
			return gel_makenum (min->val.value);
		} else if (a[0]->type == VALUE_NODE) {
			return copynode (a[0]);
		}
	}

	/* FIXME: optimize value only case */

	/* kind of a quick hack follows */
	min = a[0];
	for (i = 1; a[i] != NULL; i++) {
		/* at least ONE iteration will be run */
		GelETree *argv[2] = { min, a[i] };
		GelETree *res;
		res = min2_op (ctx, argv, exception);
		if (res == NULL) {
			if (min != a[0])
				gel_freetree (min);
			return NULL;
		}
		if (min != a[0])
			gel_freetree (min);
		min = res;
	}
	if (min == a[0])
		return copynode (a[0]);
	else
		return min;
}

static GelETree *
IsValueOnly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("IsValueOnly: argument not a matrix"));
		return NULL;
	}
	
	if(gel_is_matrix_value_only(a[0]->mat.matrix))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
I_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i,j;
	static int cached_size = -1;
	static GelMatrixW *cached_m = NULL;

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   ! mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("I: argument not an integer"));
		return NULL;
	}

	size = get_nonnegative_integer (a[0]->val.value, "I");
	if (size < 0)
		return NULL;

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.quoted = 0;

	if (cached_size == size) {
		n->mat.matrix = gel_matrixw_copy (cached_m);
	} else {
		if (cached_m != NULL)
			gel_matrixw_free (cached_m);
		n->mat.matrix = gel_matrixw_new();
		gel_matrixw_set_size(n->mat.matrix,size,size);

		for(i=0;i<size;i++)
			for(j=0;j<size;j++)
				if(i==j)
					gel_matrixw_set_index(n->mat.matrix,i,j) =
						gel_makenum_ui(1);

		cached_m = gel_matrixw_copy (n->mat.matrix);
		cached_size = -1;
	}

	return n;
}

static GelETree *
zeros_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long rows, cols;

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value) ||
	   a[1]->type!=VALUE_NODE ||
	   mpw_is_complex (a[1]->val.value) ||
	   !mpw_is_integer(a[1]->val.value)) {
		(*errorout)(_("zeros: arguments not an integers"));
		return NULL;
	}

	rows = get_nonnegative_integer (a[0]->val.value, "zeros");
	if (rows < 0)
		return NULL;
	cols = get_nonnegative_integer (a[1]->val.value, "zeros");
	if (cols < 0)
		return NULL;

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = 0;
	gel_matrixw_set_size(n->mat.matrix,cols,rows);
	
	return n;
}

static GelETree *
ones_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long rows, cols;
	int i, j;

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value) ||
	   a[1]->type!=VALUE_NODE ||
	   mpw_is_complex (a[1]->val.value) ||
	   !mpw_is_integer(a[1]->val.value)) {
		(*errorout)(_("ones: argument not an integer"));
		return NULL;
	}

	rows = get_nonnegative_integer (a[0]->val.value, "ones");
	if (rows < 0)
		return NULL;
	cols = get_nonnegative_integer (a[1]->val.value, "ones");
	if (cols < 0)
		return NULL;

	/*make us a new empty node*/
	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new();
	n->mat.quoted = 0;
	gel_matrixw_set_size(n->mat.matrix,cols,rows);
	
	for(i=0;i<cols;i++)
		for(j=0;j<rows;j++)
			gel_matrixw_set_index(n->mat.matrix,i,j) =
				gel_makenum_ui(1);

	return n;
}

static GelETree *
rows_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("rows: argument not a matrix"));
		return NULL;
	}
	return gel_makenum_ui(gel_matrixw_height(a[0]->mat.matrix));
}
static GelETree *
columns_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("columns: argument not a matrix"));
		return NULL;
	}
	return gel_makenum_ui(gel_matrixw_width(a[0]->mat.matrix));
}
static GelETree *
elements_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("elements: argument not a matrix"));
		return NULL;
	}
	return gel_makenum_ui (gel_matrixw_width (a[0]->mat.matrix) *
			       gel_matrixw_height (a[0]->mat.matrix));
}
static GelETree *
IsMatrixSquare_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=MATRIX_NODE) {
		(*errorout)(_("IsMatrixSquare: argument not a matrix"));
		return NULL;
	}
	if (gel_matrixw_width (a[0]->mat.matrix) == gel_matrixw_height (a[0]->mat.matrix))
		return gel_makenum_ui (1);
	else
		return gel_makenum_ui (0);
}
static GelETree *
SetMatrixSize_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long w,h;
	if(a[0]->type!=MATRIX_NODE ||
	   a[1]->type!=VALUE_NODE ||
	   a[2]->type!=VALUE_NODE) {
		(*errorout)(_("SetMatrixSize: wrong argument type"));
		return NULL;
	}

	w = get_nonnegative_integer (a[1]->val.value, "SetMatrixSize");
	if (w < 0)
		return NULL;
	h = get_nonnegative_integer (a[2]->val.value, "SetMatrixSize");
	if (h < 0)
		return NULL;

	n = copynode(a[0]);
	gel_matrixw_set_size (n->mat.matrix, h, w);
	return n;
}

static GelETree *
IndexComplement_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	GelMatrix *nm;
	GelMatrixW *m;
	int nml;
	char *index;
	int i, ii, ml;
	int len;
	if ((a[0]->type != MATRIX_NODE &&
	     a[0]->type != VALUE_NODE) ||
	    a[1]->type != VALUE_NODE) {
		(*errorout)(_("IndexComplement: wrong argument type"));
		return NULL;
	}

	len = get_nonnegative_integer (a[1]->val.value, "IndexComplement");
	if (len < 0)
		return NULL;
	if (a[0]->type == MATRIX_NODE) {
		index = g_new0 (char, len);

		m = a[0]->mat.matrix;
		ml = gel_matrixw_elements (m);
		nml = len;
		for (i = 0; i < ml; i++) {
			GelETree *t = gel_matrixw_vindex (m, i);
			int elt;
			if (t->type != VALUE_NODE) {
				(*errorout)(_("IndexComplement: vector argument not value only"));
				g_free (index);
				return NULL;
			}
			elt = get_nonnegative_integer (t->val.value, "IndexComplement");
			if (elt < 0) {
				g_free (index);
				return NULL;
			}
			elt--;
			if (elt >= len) {
				(*errorout)(_("IndexComplement: vector argument has too large entries"));
				g_free (index);
				return NULL;
			}

			if (index[elt] == 0) {
				nml --;
				index[elt] = 1;
			}
		}

		if (nml <= 0)
			return gel_makenum_null ();

		nm = gel_matrix_new ();
		gel_matrix_set_size (nm, nml, 1, FALSE /* padding */);
		ii = 0;
		for (i = 0; i < len; i++) {
			if (index[i] == 0) {
				gel_matrix_index (nm, ii++, 0) = gel_makenum_ui (i+1);
			}
		}

		g_free (index);
	} else {
		int elt = get_nonnegative_integer (a[0]->val.value, "IndexComplement");
		if (elt < 0)
			return NULL;
		if (elt > len) {
			(*errorout)(_("IndexComplement: vector argument has too large entries"));
			return NULL;
		}
		if (len == 1 && elt == 1)
			return gel_makenum_null ();

		nm = gel_matrix_new ();
		gel_matrix_set_size (nm, len-1, 1, FALSE /* padding */);
		ii = 0;
		for (i = 1; i <= len; i++) {
			if (i != elt)
				gel_matrix_index (nm, ii++, 0) = gel_makenum_ui (i);
		}
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (nm);
	if (a[0]->type == MATRIX_NODE)
		n->mat.quoted = a[0]->mat.quoted;
	else
		n->mat.quoted = 1;

	return n;
}

static GelETree *
det_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t ret;
	if(a[0]->type!=MATRIX_NODE ||
	   !gel_is_matrix_value_only(a[0]->mat.matrix)) {
		(*errorout)(_("det: argument not a value only matrix"));
		return NULL;
	}
	mpw_init(ret);
	if(!gel_value_matrix_det(ret,a[0]->mat.matrix)) {
		mpw_clear(ret);
		return NULL;
	}
	return gel_makenum_use(ret);
}
static GelETree *
ref_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	if(a[0]->type!=MATRIX_NODE ||
	   !gel_is_matrix_value_only(a[0]->mat.matrix)) {
		(*errorout)(_("ref: argument not a value only matrix"));
		return NULL;
	}

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss(n->mat.matrix, FALSE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = 0;
	return n;
}
static GelETree *
rref_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	if(a[0]->type!=MATRIX_NODE ||
	   !gel_is_matrix_value_only(a[0]->mat.matrix)) {
		(*errorout)(_("rref: argument not a value only matrix"));
		return NULL;
	}

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	gel_value_matrix_gauss(n->mat.matrix, TRUE, FALSE, FALSE, NULL, NULL);
	n->mat.quoted = 0;
	return n;
}

/* this is utterly stupid, but only used for small primes
 * where it's all ok */
static gboolean
is_prime_small (unsigned int testnum)
{
	int i;
	unsigned int s = (unsigned int)sqrt(testnum);
	
	for(i=0;g_array_index(primes,unsigned int,i)<=s && i<numprimes;i++) {
		if((testnum%g_array_index(primes,unsigned int,i))==0) {
			return FALSE;
		}
	}
	return TRUE;
}

static GelETree *
Prime_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	long num;
	unsigned int i;
	unsigned int last_prime;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],Prime_op,"prime");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("Prime: argument not an integer"));
		return NULL;
	}

	num = get_nonnegative_integer (a[0]->val.value, "Prime");
	if (num < 0)
		return NULL;
	
	if(!primes) {
		unsigned int b;
		primes = g_array_new(FALSE,FALSE,sizeof(unsigned int));
		b = 2;
		primes = g_array_append_val(primes,b);
		b = 3;
		primes = g_array_append_val(primes,b);
		b = 5;
		primes = g_array_append_val(primes,b);
		b = 7;
		primes = g_array_append_val(primes,b);
		numprimes = 4;
	}
	
	if(num-1 < numprimes)
		return gel_makenum_ui(g_array_index(primes,unsigned int,num-1));

	last_prime = g_array_index (primes, unsigned int, numprimes-1);
	primes = g_array_set_size(primes,num);
	for(i=g_array_index(primes,unsigned int,numprimes-1)+2;
	    numprimes<=num-1 && numprimes <= MAXPRIMES && i<=UINT_MAX-1;i+=2) {
		if (is_prime_small (i)) {
			g_array_index(primes,unsigned int,numprimes++) = i;
			last_prime = i;
		}
	}

	if (numprimes <= num-1) {
		mpw_t prime;
		mpw_init (prime);
		mpw_set_ui (prime, last_prime);
		for (i = numprimes; i <= num-1; i++) {
			mpw_nextprime (prime, prime);
		}
		return gel_makenum_use (prime);
	}
	return gel_makenum_ui(g_array_index(primes,unsigned int,num-1));
}

static GelETree *
NextPrime_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t ret;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],NextPrime_op,"NextPrime");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("NextPrime: argument not an integer"));
		return NULL;
	}

	mpw_init (ret);
	mpw_nextprime (ret, a[0]->val.value);
	if (error_num != NO_ERROR) {
		mpw_clear (ret);
		/* eek! should not happen */
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
LucasNumber_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t ret;

	if(a[0]->type==MATRIX_NODE)
		return apply_func_to_matrix(ctx,a[0],LucasNumber_op,"LucasNumber");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("LucasNumber: argument not an integer"));
		return NULL;
	}

	mpw_init (ret);
	mpw_lucnum (ret, a[0]->val.value);
	if (error_num != NO_ERROR) {
		mpw_clear (ret);
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static GelETree *
IsPrimeProbability_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	int ret;

	if (a[0]->type == MATRIX_NODE ||
	    a[1]->type == MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1],
					       IsPrimeProbability_op,
					       "IsPrimeProbability");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("IsPrimeProbability: argument not an integer"));
		return NULL;
	}

	if(a[1]->type!=VALUE_NODE ||
	   mpw_is_complex (a[1]->val.value) ||
	   !mpw_is_integer(a[1]->val.value)) {
		(*errorout)(_("IsPrimeProbability: argument not an integer"));
		return NULL;
	}

	ret = mpw_probab_prime_p (a[0]->val.value, a[1]->val.value);
	if (error_num != NO_ERROR) {
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_ui (ret);
}

static GelETree *
ModInvert_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	mpw_t ret;

	if (a[0]->type == MATRIX_NODE ||
	    a[1]->type == MATRIX_NODE)
		return apply_func_to_matrixen (ctx, a[0], a[1],
					       ModInvert_op,
					       "ModInvert");

	if(a[0]->type!=VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   !mpw_is_integer(a[0]->val.value)) {
		(*errorout)(_("ModInvert: argument not an integer"));
		return NULL;
	}

	if(a[1]->type!=VALUE_NODE ||
	   mpw_is_complex (a[1]->val.value) ||
	   !mpw_is_integer(a[1]->val.value)) {
		(*errorout)(_("ModInvert: argument not an integer"));
		return NULL;
	}

	mpw_init (ret);
	mpw_invert (ret, a[0]->val.value, a[1]->val.value);
	if (error_num != NO_ERROR) {
		mpw_clear (ret);
		error_num = NO_ERROR;
		return NULL;
	}
	return gel_makenum_use (ret);
}

static void
poly_cut_zeros(GelMatrixW *m)
{
	int i;
	int cutoff;
	for(i=gel_matrixw_width(m)-1;i>=1;i--) {
		GelETree *t = gel_matrixw_index(m,i,0);
	       	if(mpw_sgn(t->val.value)!=0)
			break;
	}
	cutoff = i+1;
	if(cutoff==gel_matrixw_width(m))
		return;
	gel_matrixw_set_size(m,cutoff,1);
}

static int
check_poly(GelETree * *a, int args, char *func, int complain)
{
	int i,j;

	for(j=0;j<args;j++) {
		if(a[j]->type!=MATRIX_NODE ||
		   gel_matrixw_height(a[j]->mat.matrix)!=1) {
			char buf[256];
			if(!complain) return FALSE;
			g_snprintf(buf,256,_("%s: arguments not horizontal vectors"),func);
			(*errorout)(buf);
			return FALSE;
		}

		for(i=0;i<gel_matrixw_width(a[j]->mat.matrix);i++) {
			GelETree *t = gel_matrixw_index(a[j]->mat.matrix,i,0);
			if(t->type != VALUE_NODE) {
				char buf[256];
				if(!complain) return FALSE;
				g_snprintf(buf,256,_("%s: arguments not numeric only vectors"),func);
				(*errorout)(buf);
				return FALSE;
			}
		}
	}
	return TRUE;
}

static GelETree *
AddPoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i;
	GelMatrixW *m1,*m2,*mn;
	
	if(!check_poly(a,2,"AddPoly",TRUE))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	size = MAX(gel_matrixw_width(m1), gel_matrixw_width(m2));
	gel_matrixw_set_size(mn,size,1);
	
	for(i=0;i<size;i++) {
		if(i<gel_matrixw_width(m1) &&
		   i<gel_matrixw_width(m2)) {
			GelETree *l,*r;
			mpw_t t;
			mpw_init(t);
			l = gel_matrixw_index(m1,i,0);
			r = gel_matrixw_index(m2,i,0);
			mpw_add(t,l->val.value,r->val.value);
			gel_matrixw_set_index(mn,i,0) = gel_makenum_use(t);
		} else if(i<gel_matrixw_width(m1)) {
			gel_matrixw_set_index(mn,i,0) =
				copynode(gel_matrixw_set_index(m1,i,0));
		} else /*if(i<gel_matrixw_width(m2)*/ {
			gel_matrixw_set_index(mn,i,0) =
				copynode(gel_matrixw_set_index(m2,i,0));
		}
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
SubtractPoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i;
	GelMatrixW *m1,*m2,*mn;
	
	if(!check_poly(a,2,"SubtractPoly",TRUE))
		return NULL;

	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	size = MAX(gel_matrixw_width(m1), gel_matrixw_width(m2));
	gel_matrixw_set_size(mn,size,1);

	for(i=0;i<size;i++) {
		if(i<gel_matrixw_width(m1) &&
		   i<gel_matrixw_width(m2)) {
			GelETree *l,*r;
			mpw_t t;
			mpw_init(t);
			l = gel_matrixw_index(m1,i,0);
			r = gel_matrixw_index(m2,i,0);
			mpw_sub(t,l->val.value,r->val.value);
			gel_matrixw_set_index(mn,i,0) = gel_makenum_use(t);
		} else if(i<gel_matrixw_width(m1)) {
			gel_matrixw_set_index(mn,i,0) =
				copynode(gel_matrixw_set_index(m1,i,0));
		} else /*if(i<gel_matrixw_width(m2))*/ {
			GelETree *nn,*r;
			r = gel_matrixw_index(m2,i,0);
			nn = gel_makenum_ui(0);
			mpw_neg(nn->val.value,r->val.value);
			gel_matrixw_set_index(mn,i,0) = nn;
		}
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
MultiplyPoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	long size;
	int i,j;
	mpw_t accu;
	GelMatrixW *m1,*m2,*mn;
	
	if(!check_poly(a,2,"MultiplyPoly",TRUE))
		return NULL;
	m1 = a[0]->mat.matrix;
	m2 = a[1]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	size = gel_matrixw_width(m1) + gel_matrixw_width(m2);
	gel_matrixw_set_size(mn,size,1);
	
	mpw_init(accu);
		
	for(i=0;i<gel_matrixw_width(m1);i++) {
		for(j=0;j<gel_matrixw_width(m2);j++) {
			GelETree *l,*r,*nn;
			l = gel_matrixw_index(m1,i,0);
			r = gel_matrixw_index(m2,j,0);
			if(mpw_sgn(l->val.value)==0 ||
			   mpw_sgn(r->val.value)==0)
				continue;
			mpw_mul(accu,l->val.value,r->val.value);
			nn = gel_matrixw_set_index(mn,i+j,0);
			if(nn)
				mpw_add(nn->val.value,nn->val.value,accu);
			else 
				gel_matrixw_set_index(mn,i+j,0) =
					gel_makenum(accu);
		}
	}

	mpw_clear(accu);
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
PolyDerivative_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if(!check_poly(a,1,"PolyDerivative",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	if(gel_matrixw_width(m)==1) {
		gel_matrixw_set_size(mn,1,1);
		return n;
	}
	gel_matrixw_set_size(mn,gel_matrixw_width(m)-1,1);
	
	for(i=1;i<gel_matrixw_width(m);i++) {
		GelETree *r;
		mpw_t t;
		mpw_init(t);
		r = gel_matrixw_index(m,i,0);
		mpw_mul_ui(t,r->val.value,i);
		gel_matrixw_set_index(mn,i-1,0) = gel_makenum_use(t);
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
Poly2ndDerivative_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	int i;
	GelMatrixW *m,*mn;
	
	if(!check_poly(a,1,"Poly2ndDerivative",TRUE))
		return NULL;

	m = a[0]->mat.matrix;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = mn = gel_matrixw_new();
	n->mat.quoted = 0;
	if(gel_matrixw_width(m)<=2) {
		gel_matrixw_set_size(mn,1,1);
		return n;
	}
	gel_matrixw_set_size(mn,gel_matrixw_width(m)-2,1);
	
	for(i=2;i<gel_matrixw_width(m);i++) {
		GelETree *r;
		mpw_t t;
		r = gel_matrixw_index(m,i,0);
		mpw_init(t);
		mpw_mul_ui(t,r->val.value,i*(i-1));
		gel_matrixw_set_index(mn,i-2,0) = gel_makenum_use(t);
	}
	
	poly_cut_zeros(mn);

	return n;
}

static GelETree *
TrimPoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	
	if(!check_poly(a,1,"TrimPoly",TRUE))
		return NULL;

	GET_NEW_NODE(n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_copy(a[0]->mat.matrix);
	n->mat.quoted = 0;
	
	poly_cut_zeros(n->mat.matrix);

	return n;
}

static GelETree *
IsPoly_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(check_poly(a,1,"IsPoly",FALSE))
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
PolyToString_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	int i;
	GString *gs;
	int any = FALSE;
	GelMatrixW *m;
	char *var;
	GelOutput *gelo;
	char *r;
	
	if(!check_poly(a,1,"PolyToString",TRUE))
		return NULL;

	if (a[1] == NULL) {
		var = "x";
	} else if (a[1]->type!=STRING_NODE) {
		(*errorout)(_("PolyToString: 2nd argument not a string"));
		return NULL;
	} else {
		if (a[2] != NULL) {
			(*errorout)(_("PolyToString: too many arguments"));
			return NULL;
		}
		var = a[1]->str.str;
	}
	
	m = a[0]->mat.matrix;
	
	gs = g_string_new("");

	gelo = gel_output_new();
	gel_output_setup_string(gelo, 0, NULL);
	gel_output_set_gstring(gelo, gs);

	for(i=gel_matrixw_width(m)-1;i>=0;i--) {
		GelETree *t;
		t = gel_matrixw_index(m,i,0);
		if(mpw_sgn(t->val.value)==0)
			continue;
		/*positive*/
		if(mpw_sgn(t->val.value)>0) {
			if(any) g_string_append(gs," + ");
			if(i==0)
				print_etree(gelo,t,FALSE);
			else if(mpw_cmp_ui(t->val.value,1)!=0) {
				print_etree(gelo,t,FALSE);
				g_string_append_c(gs,'*');
			}
			/*negative*/
		} else {
			if(any) g_string_append(gs," - ");
			else g_string_append_c(gs,'-');
			mpw_neg(t->val.value,t->val.value);
			if(i==0)
				print_etree(gelo,t,FALSE);
			else if(mpw_cmp_ui(t->val.value,1)!=0) {
				print_etree(gelo,t,FALSE);
				g_string_append_c(gs,'*');
			}
			mpw_neg(t->val.value,t->val.value);
		}
		if(i==1)
			g_string_sprintfa(gs,"%s",var);
		else if(i>1)
			g_string_sprintfa(gs,"%s^%d",var,i);
		any = TRUE;
	}
	if(!any)
		g_string_append(gs,"0");

	r = gel_output_snarf_string (gelo);
	gel_output_unref (gelo);

	GET_NEW_NODE(n);
	n->type = STRING_NODE;
	n->str.str = r;
	
	return n;
}

static GelETree *
ptf_makenew_power(GelToken *id, int power)
{
	GelETree *n;
	GelETree *tokn;
	GET_NEW_NODE(tokn);
	tokn->type = IDENTIFIER_NODE;
	tokn->id.id = id;

	if(power == 1)
		return tokn;

	GET_NEW_NODE(n);
	n->type = OPERATOR_NODE;
	n->op.oper = E_EXP;
	n->op.args = tokn;
	n->op.args->any.next = gel_makenum_ui(power);
	n->op.args->any.next->any.next = NULL;
	n->op.nargs = 2;

	return n;
}

static GelETree *
ptf_makenew_term(mpw_t mul, GelToken *id, int power)
{
	GelETree *n;
	
	/* we do the zero power the same as >1 so
	 * that we get an x^0 term.  This may seem
	 * pointless but it allows evaluating matrices
	 * as it will make the constant term act like
	 * c*I(n) */
	if (mpw_cmp_ui(mul,1)==0) {
		n = ptf_makenew_power(id,power);
	} else {
		GET_NEW_NODE(n);
		n->type = OPERATOR_NODE;
		n->op.oper = E_MUL;
		n->op.args = gel_makenum(mul);
		n->op.args->any.next = ptf_makenew_power(id,power);
		n->op.args->any.next->any.next = NULL;
		n->op.nargs = 2;
	}
	return n;
}

static GelETree *
PolyToFunction_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *n;
	GelETree *nn = NULL;
	int i;
	GelMatrixW *m;

	static GelToken *var = NULL;
	
	if(!check_poly(a,1,"PolyToFunction",TRUE))
		return NULL;
	
	if(!var)
		var = d_intern("x");
	
	m = a[0]->mat.matrix;

	for(i=gel_matrixw_width(m)-1;i>=0;i--) {
		GelETree *t;
		t = gel_matrixw_index(m,i,0);
		if(mpw_sgn(t->val.value)==0)
			continue;
		
		if(!nn)
			nn = ptf_makenew_term(t->val.value,var,i);
		else {
			GelETree *nnn;
			GET_NEW_NODE(nnn);
			nnn->type = OPERATOR_NODE;
			nnn->op.oper = E_PLUS;
			nnn->op.args = nn;
			nnn->op.args->any.next =
				ptf_makenew_term(t->val.value,var,i);
			nnn->op.args->any.next->any.next = NULL;
			nnn->op.nargs = 2;
			nn = nnn;
		}
	}
	if(!nn)
		nn = gel_makenum_ui(0);

	GET_NEW_NODE(n);
	n->type = FUNCTION_NODE;
	n->func.func = d_makeufunc(NULL,nn,g_slist_append(NULL,var),1, NULL);
	n->func.func->context = -1;

	return n;
}

static GelETree *
SetHelp_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=STRING_NODE ||
	   a[1]->type!=STRING_NODE ||
	   a[2]->type!=STRING_NODE) {
		(*errorout)(_("SetHelp: arguments must be strings (function name, category, help text)"));
		return NULL;
	}
	
	add_category (a[0]->str.str, a[1]->str.str);
	add_description (a[0]->str.str, a[2]->str.str);

	return gel_makenum_null();
}

static GelETree *
SetHelpAlias_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	if(a[0]->type!=STRING_NODE ||
	   a[1]->type!=STRING_NODE) {
		(*errorout)(_("SetHelpAlias: arguments must be strings (function name, alias)"));
		return NULL;
	}
	
	add_alias (a[0]->str.str, a[1]->str.str);

	return gel_makenum_null();
}

static GelETree *
etree_out_of_int_vector (int *vec, int len)
{
	GelMatrix *mm;
	int i;
	GelETree *n;

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, len, 1, FALSE /* padding */);

	for (i = 0; i < len; i++) {
		gel_matrix_index (mm, i, 0) = gel_makenum_si (vec[i]);
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (mm);
	n->mat.quoted = 0;

	return n;
}

static GelETree *
etree_out_of_etree_list (GSList *list, int len)
{
	GelMatrix *mm;
	GSList *li;
	int i;
	GelETree *n;

	mm = gel_matrix_new ();
	gel_matrix_set_size (mm, len, 1, FALSE /* padding */);

	li = list;
	for (i = 0; i < len; i++) {
		gel_matrix_index (mm, i, 0) = li->data;
		li = li->next;
	}

	GET_NEW_NODE (n);
	n->type = MATRIX_NODE;
	n->mat.matrix = gel_matrixw_new_with_matrix (mm);
	n->mat.quoted = 0;

	return n;
}

static gboolean
comb_get_next_combination (int *vec, int len, int n)
{
	int i = len;
	int j;

	/* do you like my gel -> C porting? */

	while (i > 0 && vec[i-1] == n-(len-i)) {
		i--;
	}
	if (i == 0) {
		return FALSE;
	} else {
		vec[i-1] ++;
		for (j = i+1; j <= len; j++)
			vec[j-1] = vec[j-2]+1;
	}
	return TRUE;
}

static gboolean
perm_is_pos_mobile (int *perm, char *arrow, int pos, int n)
{
	if (arrow[pos]=='L' && pos==0)
		return FALSE;
	else if (arrow[pos]=='R' && pos==n-1)
		return FALSE;
	else if (arrow[pos]=='L' && perm[pos] > perm[pos-1])
		return TRUE;
	else if (arrow[pos]=='R' && perm[pos] > perm[pos+1])
		return TRUE;
	else
		return FALSE;
}

static int
perm_get_highest_mobile (int *perm, char *arrow, int n)
{
	int highest = -1;
	int i;
	for (i = 0; i < n; i++) {
		if (perm_is_pos_mobile (perm, arrow, i, n) &&
		    (highest == -1 || perm[highest] < perm[i]))
			highest = i;
	}
	return highest;
}

static void
perm_move_pos (int *perm, char *arrow, int pos, int n)
{
	if (arrow[pos] == 'L') {
		char t;
		g_assert (pos > 0);
		t = perm[pos];
		perm[pos] = perm[pos-1];
		perm[pos-1] = t;
		t = arrow[pos];
		arrow[pos] = arrow[pos-1];
		arrow[pos-1] = t;
	} else {
		char t;
		g_assert (pos < n-1);
		t = perm[pos];
		perm[pos] = perm[pos+1];
		perm[pos+1] = t;
		t = arrow[pos];
		arrow[pos] = arrow[pos+1];
		arrow[pos+1] = t;
	}
}

static void
perm_switch_all_above (int *perm, char *arrow, int pos, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		if (perm[i] > perm[pos]) {
			if (arrow[i] == 'L')
				arrow[i] = 'R';
			else
				arrow[i] = 'L';
		}
	}
}

static GelETree *
Combinations_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	long k, n;
	int *comb;
	int i;
	GSList *list;
	int len;
	GelETree *r;

	if (a[0]->type != VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   ! mpw_is_integer (a[0]->val.value) ||
	   a[1]->type != VALUE_NODE ||
	   mpw_is_complex (a[1]->val.value) ||
	   ! mpw_is_integer (a[1]->val.value)) {
		(*errorout)(_("Combinations: arguments not an integers"));
		return NULL;
	}

	error_num = 0;
	k = mpw_get_long(a[0]->val.value);
	if (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	n = mpw_get_long(a[1]->val.value);
	if (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	if (n < 1 || n > INT_MAX || k < 1 || k > n) {
		(*errorout)(_("Combinations: arguments out of range"));
		return NULL;
	}

	list = NULL;
	len = 0;

	comb = g_new (int, k);
	for (i = 0; i < k; i++)
		comb[i] = i+1;

	do {
		list = g_slist_prepend (list, etree_out_of_int_vector (comb, k));
		len++;
	} while (comb_get_next_combination (comb, k, n));

	g_free (comb);

	list = g_slist_reverse (list);

	r = etree_out_of_etree_list (list, len);

	g_slist_free (list);

	return r;
}

static GelETree *
Permutations_op (GelCtx *ctx, GelETree * * a, int *exception)
{
	GelETree *r;
	GSList *list;
	long k, n, len;
	int *comb;
	int *perm;
	char *arrow;
	int i;

	if (a[0]->type != VALUE_NODE ||
	   mpw_is_complex (a[0]->val.value) ||
	   ! mpw_is_integer (a[0]->val.value) ||
	   a[1]->type != VALUE_NODE ||
	   mpw_is_complex (a[1]->val.value) ||
	   ! mpw_is_integer (a[1]->val.value)) {
		(*errorout)(_("Permutations: arguments not an integers"));
		return NULL;
	}

	error_num = 0;
	k = mpw_get_long(a[0]->val.value);
	if (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	n = mpw_get_long(a[1]->val.value);
	if (error_num != 0) {
		error_num = 0;
		return NULL;
	}
	if (n < 1 || n > INT_MAX || k < 1 || k > n) {
		(*errorout)(_("Permutations: arguments out of range"));
		return NULL;
	}

	arrow = g_new (char, k);
	perm = g_new (int, k);
	comb = g_new (int, k);

	for (i = 0; i < k; i++)
		comb[i] = i+1;

	list = NULL;
	len = 0;

	do {
		for (i = 0; i < k; i++)
			perm[i] = comb[i];
		for (i = 0; i < k; i++)
			arrow[i] = 'L';
		for (;;) {
			int m;

			list = g_slist_prepend (list, etree_out_of_int_vector (perm, k));
			len++;

			m = perm_get_highest_mobile (perm, arrow, k);
			if (m == -1)
				break;
			perm_switch_all_above (perm, arrow, m, k);
			perm_move_pos (perm, arrow, m, k);
		}
	} while (comb_get_next_combination (comb, k, n));

	g_free (comb);
	g_free (perm);
	g_free (arrow);

	list = g_slist_reverse (list);

	r = etree_out_of_etree_list (list, len);

	g_slist_free (list);

	return r;
}

static GelETree *
protect_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelToken *tok;

	if(a[0]->type!=STRING_NODE) {
		(*errorout)(_("protect: argument must be a string"));
		return NULL;
	}
	
	tok = d_intern(a[0]->str.str);
	tok->protected = 1;

	return gel_makenum_null();
}

static GelETree *
unprotect_op(GelCtx *ctx, GelETree * * a, int *exception)
{
	GelToken *tok;

	if(a[0]->type!=STRING_NODE) {
		(*errorout)(_("unprotect: argument must be a string"));
		return NULL;
	}
	
	tok = d_intern(a[0]->str.str);
	tok->protected = 0;

	return gel_makenum_null();
}

static GelETree *
set_FloatPrecision (GelETree * a)
{
	long bits;

	if(a->type!=VALUE_NODE ||
	   mpw_is_complex(a->val.value) ||
	   !mpw_is_integer(a->val.value)) {
		(*errorout)(_("FloatPrecision: argument not an integer"));
		return NULL;
	}

	bits = mpw_get_long(a->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(bits<60 || bits>16384) {
		(*errorout)(_("FloatPrecision: argument should be between 60 and 16384"));
		return NULL;
	}
	
	if(calcstate.float_prec != bits) {
		calcstate.float_prec = bits;
		mpw_set_default_prec(calcstate.float_prec);
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.float_prec);
}

static GelETree *
get_FloatPrecision (void)
{
	return gel_makenum_ui(calcstate.float_prec);
}

static GelETree *
set_MaxDigits (GelETree * a)
{
	long digits;

	if(a->type!=VALUE_NODE ||
	   mpw_is_complex(a->val.value) ||
	   !mpw_is_integer(a->val.value)) {
		(*errorout)(_("MaxDigits: argument not an integer"));
		return NULL;
	}

	digits = mpw_get_long(a->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(digits<0 || digits>256) {
		(*errorout)(_("MaxDigits: argument should be between 0 and 256"));
		return NULL;
	}
	
	if(calcstate.max_digits != digits) {
		calcstate.max_digits = digits;
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.max_digits);
}

static GelETree *
get_MaxDigits (void)
{
	return gel_makenum_ui(calcstate.max_digits);
}

static GelETree *
set_ResultsAsFloats (GelETree * a)
{
	if(a->type!=VALUE_NODE) {
		(*errorout)(_("ResultsAsFloats: argument not a value"));
		return NULL;
	}
	calcstate.results_as_floats = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.results_as_floats)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_ResultsAsFloats (void)
{
	if(calcstate.results_as_floats)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
set_ScientificNotation (GelETree * a)
{
	if(a->type!=VALUE_NODE) {
		(*errorout)(_("ScientificNotation: argument not a value"));
		return NULL;
	}
	calcstate.scientific_notation = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.scientific_notation)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_ScientificNotation (void)
{
	if(calcstate.scientific_notation)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
set_FullExpressions (GelETree * a)
{
	if(a->type!=VALUE_NODE) {
		(*errorout)(_("FullExpressions: argument not a value"));
		return NULL;
	}
	calcstate.full_expressions = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.full_expressions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_FullExpressions (void)
{
	if(calcstate.full_expressions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
set_OutputStyle (GelETree * a)
{
	const char *token;
	GelOutputStyle output_style = GEL_OUTPUT_NORMAL;

	if (a->type != STRING_NODE &&
	    a->type != IDENTIFIER_NODE) {
		(*errorout)(_("OutputStyle: argument not a string"));
		return NULL;
	}

	if (a->type == STRING_NODE)
		token = a->str.str;
	else
		token = a->id.id->token;

	if (token != NULL && strcmp (token, "normal") == 0) {
		output_style = GEL_OUTPUT_NORMAL;
	} else if (token != NULL && strcmp (token, "troff") == 0) {
		output_style = GEL_OUTPUT_TROFF;
	} else if (token != NULL && strcmp (token, "latex") == 0) {
		output_style = GEL_OUTPUT_LATEX;
	} else {
		(*errorout)(_("set_output_style: argument not one of normal, troff or latex"));
		return NULL;
	}

	calcstate.output_style = output_style;
	if (statechange_hook)
		(*statechange_hook)(calcstate);

	return gel_makenum_string (token);
}

static GelETree *
get_OutputStyle (void)
{
	const char *token;

	token = "normal";
	if (calcstate.output_style == GEL_OUTPUT_TROFF)
		token = "troff";
	else if (calcstate.output_style == GEL_OUTPUT_LATEX)
		token = "latex";

	return gel_makenum_string (token);
}

static GelETree *
set_MaxErrors (GelETree * a)
{
	long errors;

	if(a->type!=VALUE_NODE ||
	   mpw_is_complex(a->val.value) ||
	   !mpw_is_integer(a->val.value)) {
		(*errorout)(_("MaxErrors: argument not an integer"));
		return NULL;
	}

	errors = mpw_get_long(a->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(errors<0) {
		(*errorout)(_("MaxErrors: argument should be larger or equal to 0"));
		return NULL;
	}
	
	if(calcstate.max_errors != errors) {
		calcstate.max_errors = errors;
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.max_errors);
}

static GelETree *
get_MaxErrors (void)
{
	return gel_makenum_ui(calcstate.max_errors);
}

static GelETree *
set_MixedFractions (GelETree * a)
{
	if(a->type!=VALUE_NODE) {
		(*errorout)(_("MixedFractions: argument not a value"));
		return NULL;
	}
	calcstate.mixed_fractions = mpw_sgn(a->val.value)!=0;
	if(statechange_hook)
		(*statechange_hook)(calcstate);

	if(calcstate.mixed_fractions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}
static GelETree *
get_MixedFractions (void)
{
	if(calcstate.mixed_fractions)
		return gel_makenum_ui(1);
	else
		return gel_makenum_ui(0);
}

static GelETree *
set_IntegerOutputBase (GelETree * a)
{
	long base;

	if(a->type!=VALUE_NODE ||
	   mpw_is_complex(a->val.value) ||
	   !mpw_is_integer(a->val.value)) {
		(*errorout)(_("IntegerOutputBase: argument not an integer"));
		return NULL;
	}

	base = mpw_get_long(a->val.value);
	if(error_num) {
		error_num = 0;
		return NULL;
	}
	if(base<2 || base>36) {
		(*errorout)(_("IntegerOutputBase: argument should be between 2 and 36"));
		return NULL;
	}
	
	if(calcstate.integer_output_base != base) {
		calcstate.integer_output_base = base;
		if(statechange_hook)
			(*statechange_hook)(calcstate);
	}

	return gel_makenum_ui(calcstate.integer_output_base);
}

static GelETree *
get_IntegerOutputBase (void)
{
	return gel_makenum_ui(calcstate.integer_output_base);
}

/*add the routines to the dictionary*/
void
gel_funclib_addall(void)
{
	GelEFunc *f;
	GelToken *id;

	new_category ("basic", _("Basic"));
	new_category ("parameters", _("Parameters"));
	new_category ("constants", _("Constants"));
	new_category ("numeric", _("Numeric"));
	new_category ("trigonometry", _("Trigonometry"));
	new_category ("number_theory", _("Number Theory"));
	new_category ("matrix", _("Matrix Manipulation"));
	new_category ("linear_algebra", _("Linear Algebra"));
	new_category ("combinatorics", _("Combinatorics"));
	new_category ("calculus", _("Calculus"));
	new_category ("functions", _("Functions"));
	new_category ("equation_solving", _("Equation Solving"));
	new_category ("statistics", _("Statistics"));
	new_category ("polynomial", _("Polynomials"));
	new_category ("misc", _("Miscellaneous"));

	/* FIXME: add more help fields */
#define FUNC(name,args,argn,category,desc) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), name ## _op, args)); \
	d_add_named_args (f, argn); \
	add_category ( #name , category); \
	add_description ( #name , desc);
#define VFUNC(name,args,argn,category,desc) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), name ## _op, args)); \
	d_add_named_args (f, argn); \
	f->vararg = TRUE; \
	add_category ( #name , category); \
	add_description ( #name , desc);
#define ALIAS(name,args,aliasfor) \
	d_addfunc (d_makebifunc (d_intern ( #name ), aliasfor ## _op, args)); \
	add_alias ( #aliasfor , #name );
#define VALIAS(name,args,aliasfor) \
	f = d_addfunc (d_makebifunc (d_intern ( #name ), aliasfor ## _op, args)); \
	f->vararg = TRUE; \
	add_alias ( #aliasfor , #name );
#define PARAMETER(name,desc) \
	id = d_intern ( #name ); \
	id->parameter = 1; \
	id->built_in_parameter = 1; \
	id->data1 = set_ ## name; \
	id->data2 = get_ ## name; \
	add_category ( #name , "parameters"); \
	add_description ( #name , desc); \
	/* bogus value */ \
	d_addfunc_global (d_makevfunc (id, gel_makenum_null()));


	FUNC (manual, 0, "", "basic", _("Displays the user manual"));
	FUNC (warranty, 0, "", "basic", _("Gives the warranty information"));
	FUNC (exit, 0, "", "basic", _("Exits the program"));
	ALIAS (quit, 0, exit);
	FUNC (error, 1, "str", "basic", _("Prints a string to the error stream"));
	FUNC (print, 1, "str", "basic", _("Prints an expression"));
	FUNC (chdir, 1, "dir", "basic", _("Changes current directory"));
	FUNC (printn, 1, "str", "basic", _("Prints an expression without a trailing newline"));
	FUNC (display, 2, "str,expr", "basic", _("Display a string and an expression"));
	FUNC (set, 2, "id,val", "basic", _("Set a global variable"));

	FUNC (SetHelp, 3, "id,category,desc", "basic", _("Set the category and help description line for a function"));
	FUNC (SetHelpAlias, 2, "id,alias", "basic", _("Sets up a help alias"));

	VFUNC (rand, 1, "size", "numeric", _("Generate random float"));
	VFUNC (randint, 2, "max,size", "numeric", _("Generate random integer"));

	PARAMETER (FloatPrecision, _("Floating point precision"));
	PARAMETER (MaxDigits, _("Maximum digits to display"));
	PARAMETER (MaxErrors, _("Maximum errors to display"));
	PARAMETER (OutputStyle, _("Output style: normal, latex or troff"));
	PARAMETER (IntegerOutputBase, _("Integer output base"));
	PARAMETER (MixedFractions, _("If true, mixed fractions are printed"));
	PARAMETER (FullExpressions, _("Print full expressions, even if more then a line"));
	PARAMETER (ResultsAsFloats, _("Convert all results to floats before printing"));
	PARAMETER (ScientificNotation, _("Use scientific notation"));

	/* secret functions */
	d_addfunc(d_makebifunc(d_intern("ni"),ni_op,0));
	d_addfunc(d_makebifunc(d_intern("shrubbery"),shrubbery_op,0));

	FUNC (ExpandMatrix, 1, "M", "matrix", _("Expands a matrix just like we do on unquoted matrix input"));
	FUNC (RowsOf, 1, "M", "matrix", _("Gets the rows of a matrix as a vertical vector"));
	FUNC (ColumnsOf, 1, "M", "matrix", _("Gets the columns of a matrix as a horizontal vector"));
	FUNC (DiagonalOf, 1, "M", "matrix", _("Gets the diagonal entries of a matrix as a horizontal vector"));

	FUNC (ComplexConjugate, 1, "M", "numeric", _("Calculates the conjugate"));
	ALIAS (conj, 1, ComplexConjugate);
	ALIAS (Conj, 1, ComplexConjugate);

	FUNC (sin, 1, "x", "trigonometry", _("Calculates the sine function"));
	FUNC (cos, 1, "x", "trigonometry", _("Calculates the cossine function"));
	FUNC (sinh, 1, "x", "trigonometry", _("Calculates the hyperbolic sine function"));
	FUNC (cosh, 1, "x", "trigonometry", _("Calculates the hyperbolic cosine function"));
	FUNC (tan, 1, "x", "trigonometry", _("Calculates the tan function"));
	FUNC (atan, 1, "x", "trigonometry", _("Calculates the arctan function"));
	ALIAS (arctan, 1, atan);

	FUNC (pi, 0, "", "constants", _("The number pi"));
	FUNC (e, 0, "", "constants", _("The natural number e"));
	FUNC (GoldenRatio, 0, "", "constants", _("The Golden Ratio"));
	FUNC (g, 0, "", "constants", _("Free fall acceleration"));
	FUNC (EulerConstant, 0, "", "constants",
	      _("Euler's Constant gamma good up to about precision of 9516 digits"));
	ALIAS (gamma, 0, EulerConstant);

	FUNC (sqrt, 1, "x", "numeric", _("The square root"));
	ALIAS (SquareRoot, 1, sqrt);
	FUNC (exp, 1, "x", "numeric", _("The exponential function"));
	FUNC (ln, 1, "x", "numeric", _("The natural logarithm"));
	FUNC (round, 1, "x", "numeric", _("Round a number"));
	ALIAS (Round, 1, round);
	FUNC (floor, 1, "x", "numeric", _("Get the highest integer less then or equal to n"));
	ALIAS (Floor, 1, floor);
	FUNC (ceil, 1, "x", "numeric", _("Get the lowest integer more then or equal to n"));
	ALIAS (Ceiling, 1, ceil);
	FUNC (trunc, 1, "x", "numeric", _("Truncate number to an integer (return the integer part)"));
	ALIAS (Truncate, 1, trunc);
	ALIAS (IntegerPart, 1, trunc);
	FUNC (float, 1, "x", "numeric", _("Make number a float"));
	FUNC (Numerator, 1, "x", "numeric", _("Get the numerator of a rational number"));
	FUNC (Denominator, 1, "x", "numeric", _("Get the denominator of a rational number"));

	VFUNC (gcd, 2, "a,args", "number_theory", _("Greatest common divisor"));
	ALIAS (GCD, 2, gcd);
	VFUNC (lcm, 2, "a,args", "number_theory", _("Least common multiplier"));
	ALIAS (LCM, 2, lcm);
	FUNC (IsPerfectSquare, 1, "n", "number_theory", _("Check a number for being a perfect square"));
	FUNC (IsPerfectPower, 1, "n", "number_theory", _("Check a number for being any perfect power (a^b)"));
	FUNC (Prime, 1, "n", "number_theory", _("Return the n'th prime (up to a limit)"));
	ALIAS (prime, 1, Prime);

	FUNC (NextPrime, 1, "n", "number_theory", _("Returns the least prime greater than n (if n is positive)"));
	FUNC (LucasNumber, 1, "n", "number_theory", _("Returns the n'th Lucas number"));
	FUNC (IsPrimeProbability, 2, "n,reps", "number_theory", _("Returns 0 if composite, 1 if probably prime, 2 if definately prime"));
	FUNC (ModInvert, 2, "n,m", "number_theory", _("Returns inverse of n mod m"));

	VFUNC (max, 2, "a,args", "numeric", _("Returns the maximum of arguments or matrix"));
	VALIAS (Max, 2, max);
	VALIAS (Maximum, 2, max);
	VFUNC (min, 2, "a,args", "numeric", _("Returns the minimum of arguments or matrix"));
	VALIAS (Min, 2, min);
	VALIAS (Minimum, 2, min);

	FUNC (Jacobi, 2, "a,b", "number_theory", _("Calculate the Jacobi symbol (a/b) (b should be odd)"));
	ALIAS (JacobiSymbol, 2, Jacobi);
	FUNC (JacobiKronecker, 2, "a,b", "number_theory", _("Calculate the Jacobi symbol (a/b) with the Kronecker extension (a/2)=(2/a) when a odd, or (a/2)=0 when a even"));
	ALIAS (JacobiKroneckerSymbol, 2, JacobiKronecker);
	FUNC (Legendre, 2, "a,p", "number_theory", _("Calculate the Legendre symbol (a/p)"));
	ALIAS (LegendreSymbol, 2, Legendre);

	FUNC (Re, 1, "z", "numeric", _("Get the real part of a complex number"));
	ALIAS (RealPart, 1, Re);
	FUNC (Im, 1, "z", "numeric", _("Get the imaginary part of a complex number"));
	ALIAS (ImaginaryPart, 1, Im);

	FUNC (I, 1, "n", "matrix", _("Make an identity matrix of a given size"));
	ALIAS (eye, 1, I);
	FUNC (zeros, 2, "rows,columns", "matrix", _("Make an matrix of all zeros"));
	FUNC (ones, 2, "rows,columns", "matrix", _("Make an matrix of all ones"));

	FUNC (rows, 1, "M", "matrix", _("Get the number of rows of a matrix"));
	FUNC (columns, 1, "M", "matrix", _("Get the number of columns of a matrix"));
	FUNC (IsMatrixSquare, 1, "M", "matrix", _("Is a matrix square"));
	FUNC (elements, 1, "M", "matrix", _("Get the number of elements of a matrix"));

	FUNC (ref, 1, "M", "linear_algebra", _("Get the row echelon form of a matrix"));
	ALIAS (REF, 1, ref);
	ALIAS (RowEchelonForm, 1, ref);
	FUNC (rref, 1, "M", "linear_algebra", _("Get the reduced row echelon form of a matrix"));
	ALIAS (RREF, 1, rref);
	ALIAS (ReducedRowEchelonForm, 1, rref);

	FUNC (det, 1, "M", "linear_algebra", _("Get the determinant of a matrix"));
	ALIAS (Determinant, 1, det);

	FUNC (SetMatrixSize, 3, "M,rows,columns", "matrix", _("Make new matrix of given size from old one"));
	FUNC (IndexComplement, 2, "vec,msize", "matrix", _("Return the index complement of a vector of indexes"));

	FUNC (IsValueOnly, 1, "M", "matrix", _("Check if a matrix is a matrix of numbers"));
	FUNC (IsMatrixInteger, 1, "M", "matrix", _("Check if a matrix is an integer (non-complex) matrix"));
	FUNC (IsMatrixRational, 1, "M", "matrix", _("Check if a matrix is a rational (non-complex) matrix"));
	FUNC (IsMatrixReal, 1, "M", "matrix", _("Check if a matrix is a real (non-complex) matrix"));

	FUNC (IsNull, 1, "arg", "basic", _("Check if argument is a null"));
	FUNC (IsValue, 1, "arg", "basic", _("Check if argument is a number"));
	FUNC (IsString, 1, "arg", "basic", _("Check if argument is a text string"));
	FUNC (IsMatrix, 1, "arg", "basic", _("Check if argument is a matrix"));
	FUNC (IsFunction, 1, "arg", "basic", _("Check if argument is a function"));
	FUNC (IsFunctionRef, 1, "arg", "basic", _("Check if argument is a function reference"));

	FUNC (IsComplex, 1, "num", "numeric", _("Check if argument is a complex (non-real) number"));
	FUNC (IsReal, 1, "num", "numeric", _("Check if argument is a real number"));
	FUNC (IsInteger, 1, "num", "numeric", _("Check if argument is an integer (non-complex)"));
	FUNC (IsRational, 1, "num", "numeric", _("Check if argument is a rational number (non-complex)"));
	FUNC (IsFloat, 1, "num", "numeric", _("Check if argument is a floating point number (non-complex)"));

	FUNC (AddPoly, 2, "p1,p2", "polynomial", _("Add two polynomials (vectors)"));
	FUNC (SubtractPoly, 2, "p1,p2", "polynomial", _("Subtract two polynomials (as vectors)"));
	FUNC (MultiplyPoly, 2, "p1,p2", "polynomial", _("Multiply two polynomials (as vectors)"));
	FUNC (PolyDerivative, 1, "p", "polynomial", _("Take polynomial (as vector) derivative"));
	FUNC (Poly2ndDerivative, 1, "p", "polynomial", _("Take second polynomial (as vector) derivative"));
	FUNC (TrimPoly, 1, "p", "polynomial", _("Trim zeros from a polynomial (as vector)"));
	FUNC (IsPoly, 1, "p", "polynomial", _("Check if a vector is usable as a polynomial"));
	VFUNC (PolyToString, 2, "p,var", "polynomial", _("Make string out of a polynomial (as vector)"));
	FUNC (PolyToFunction, 1, "p", "polynomial", _("Make function out of a polynomial (as vector)"));

	FUNC (Combinations, 2, "k,n", "combinatorics", _("Get all combinations of k numbers from 1 to n as a vector of vectors"));
	FUNC (Permutations, 2, "k,n", "combinatorics", _("Get all permutations of k numbers from 1 to n as a vector of vectors"));

	FUNC (protect, 1, "id", "basic", _("Protect a variable from being modified"));
	FUNC (unprotect, 1, "id", "basic", _("Unprotect a variable from being modified"));

	/*temporary until well done internal functions are done*/
	_internal_ln_function = d_makeufunc(d_intern("<internal>ln"),
					    /*FIXME:this is not the correct 
					      function*/
					    gel_parseexp("error(\"ln not finished\")",
							 NULL, FALSE, FALSE,
							 NULL, NULL),
					    g_slist_append(NULL,d_intern("x")),1,
					    NULL);
	_internal_exp_function = d_makeufunc(d_intern("<internal>exp"),
					     gel_parseexp
					       ("s = float(x^0); "
						"fact = 1; "
						"for i = 1 to 100 do "
						"(fact = fact * x / i; "
						"s = s + fact) ; s",
						NULL, FALSE, FALSE,
						NULL, NULL),
					     g_slist_append(NULL,d_intern("x")),1,
					     NULL);
	/*protect EVERYthing up to this point*/
	d_protect_all();
}
