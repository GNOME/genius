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
#include <locale.h>

#include <string.h>
#include <glib.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "calc.h"
#include "util.h"

#include "mpwrap.h"

#if 0
/* Context sutff */
struct _MpwCtx {
	int ref_count;

	MpwErrorFunc errorout;
	int error_num;

	int default_mpf_prec;
	gboolean double_math; /* instead of gmp, use doubles */
	
	gpointer data;
};
#endif

#ifndef HAVE_MPFR
/* As calculated by Thomas Papanikolaou taken from
 * http://www.cecm.sfu.ca/projects/ISC/dataB/isc/C/gamma.txt */
/* We only need this if not compiling with MPFR since MPFR does this
 * for us at any precision we want */
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
#endif /* ! HAVE_MPFR */

static MpwRealNum *free_reals = NULL;
static int free_reals_n = 0;

static MpwRealNum *zero = NULL;
static MpwRealNum *one = NULL;

static mpf_ptr pi_mpf = NULL;

static int default_mpf_prec = 0;

#define MAKE_CPLX_OPS(THE_op,THE_r,THE_i) {		\
	if(rop==THE_op) {				\
		THE_r = g_new0(MpwRealNum,1);		\
		THE_i = g_new0(MpwRealNum,1);		\
		mpwl_init_type(THE_r,MPW_FLOAT);	\
		mpwl_init_type(THE_i,MPW_FLOAT);	\
		mpwl_set(THE_r,THE_op->r);		\
		mpwl_set(THE_i,THE_op->i);		\
	} else {					\
		THE_r = THE_op->r;			\
		THE_i = THE_op->i;			\
	}						\
}
#define BREAK_CPLX_OPS(THE_op,THE_r,THE_i) {		\
	if(rop==THE_op) {				\
		mpwl_free(THE_i,TRUE);			\
		mpwl_free(THE_r,TRUE);			\
	}						\
}


#define GET_NEW_REAL(n) {				\
	if(!free_reals) {				\
		n = g_new0(MpwRealNum,1);		\
	} else {					\
		n = free_reals;				\
		free_reals = free_reals->alloc.next;	\
		free_reals_n--;				\
	}						\
}
#define MAKE_COPY(n) {					\
	if(n->alloc.usage>1) {				\
		MpwRealNum *m;				\
		GET_NEW_REAL(m);			\
		m->alloc.usage = 1;			\
		mpwl_init_type(m,n->type);		\
		mpwl_set(m,n);				\
		n->alloc.usage --;			\
		n = m;					\
	}						\
}
#define MAKE_REAL(n) {					\
	if(n->type==MPW_COMPLEX) {			\
		n->type=MPW_REAL;			\
		if(n->i != zero) {			\
			n->i->alloc.usage--;		\
			if(n->i->alloc.usage==0)	\
				mpwl_free(n->i,FALSE);	\
			n->i = zero;			\
			zero->alloc.usage++;		\
		}					\
	}						\
}
#define MAKE_IMAG(n) {					\
	if(n->type==MPW_COMPLEX) {			\
		if(n->r != zero) {			\
			n->r->alloc.usage--;		\
			if(n->r->alloc.usage==0)	\
				mpwl_free(n->r,FALSE);	\
			n->r = zero;			\
			zero->alloc.usage++;		\
		}					\
	}						\
}

#if 0
/*************************************************************************/
/*cache system                                                           */
/*************************************************************************/
typedef struct _MpwCache MpwCache;
struct _MpwCache {
	int prec;
	int use_count;
	mpf_ptr pi_mpf;
};

static MpwCache *mpw_chache_get(int prec);
static void mpw_chache_unget(MpwCache *mc);
static GHashTable *mpw_cache_ht = NULL;
#endif

/*************************************************************************/
/*low level stuff prototypes                                             */
/*************************************************************************/

#ifndef HAVE_MPFR
/*get sin*/
static void mympf_sin(mpf_t rop, mpf_t op, gboolean hyperbolic, gboolean reduceop);
/*get cos*/
static void mympf_cos(mpf_t rop, mpf_t op, gboolean hyperbolic, gboolean reduceop);
/*get arctan*/
static void mympf_arctan(mpf_ptr rop, mpf_ptr op);
#endif
/*get pi*/ /* This will use MPFR if needed and does the caching */
/* FIXME: this should be done differently */
static void mympf_pi(mpf_ptr rop);

/*my own power function for floats, very simple :) */
static void mympf_pow_z(mpf_t rop,mpf_t op,mpz_t e);

/*my own power function for ints, very simple :) */
static void mympz_pow_z(mpz_t rop,mpz_t op,mpz_t e);

static gboolean mympq_perfect_square_p (mpq_t op);

#ifndef HAVE_MPFR
/*simple exponential function*/
static void mympf_exp(mpf_t rop,mpf_t op);

/*ln function*/
static gboolean mympf_ln(mpf_t rop,mpf_t op);
static gboolean mympf_log2(mpf_t rop,mpf_t op);
static gboolean mympf_log10(mpf_t rop,mpf_t op);
#endif /* ! HAVE_MPFR */

#ifdef HAVE_MPFR
static void mympz_set_fr (mpz_ptr z, mpfr_srcptr fr);
static void mympq_set_fr (mpq_ptr q, mpfr_srcptr fr);
static int mympfr_cmp_d (mpfr_srcptr a, double b);
/* FIXME: an UGLY UGLY HACK */
#undef mpz_set_f
#undef mpq_set_f
#undef mpf_cmp_d
#define mpz_set_f mympz_set_fr
#define mpq_set_f mympq_set_fr
#define mpf_cmp_d mympfr_cmp_d
#endif

/*clear extra variables of type type, if type=op->type nothing is done*/
static void mpwl_clear_extra_type(MpwRealNum *op,int type);

/*only set the type, don't free it, and don't set the type variable
  create an extra type of the variable for temporary use*/
static void mpwl_make_extra_type (MpwRealNum *op, int type);
/*static void mpwl_make_extra_type_no_convert (MpwRealNum *op, int type);*/

static void mpwl_make_type(MpwRealNum *op,int type);

/*this only adds a value of that type but does nto clear the old one!
  retuns the new extra type set*/
static int mpwl_make_same_extra_type(MpwRealNum *op1,MpwRealNum *op2);
/*
static int mpwl_make_same_extra_type_3(MpwRealNum *op1,MpwRealNum *op2);
*/

/*make new type and clear the old one*/
static void mpwl_make_same_type(MpwRealNum *op1,MpwRealNum *op2);

static void mpwl_clear(MpwRealNum *op);

static void mpwl_init_type(MpwRealNum *op,int type);

static void mpwl_free(MpwRealNum *op, int local);

static int mpwl_sgn(MpwRealNum *op);

static void mpwl_set_si(MpwRealNum *rop,signed long int i);
static void mpwl_set_ui(MpwRealNum *rop,unsigned long int i);
static void mpwl_set_d(MpwRealNum *rop,double d);

static void mpwl_move(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_set(MpwRealNum *rop,MpwRealNum *op);

static int mympn_add(long *res, long op1, long op2);
static int mympn_sub(long *res, long op1, long op2);

static void mpwl_add(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_add_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i);
static void mpwl_sub(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_sub_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i);
static void mpwl_ui_sub(MpwRealNum *rop,unsigned long i,MpwRealNum *op);

static void mpwl_mul(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_mul_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i);

static void mpwl_div(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_div_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i);
static void mpwl_ui_div(MpwRealNum *rop,unsigned long int i,MpwRealNum *op);

static void mpwl_mod(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static gboolean mpwl_invert (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2);
static void mpwl_gcd(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_jacobi(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_legendre(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_kronecker(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);
static void mpwl_lucnum (MpwRealNum *rop, MpwRealNum *op);
static void mpwl_nextprime (MpwRealNum *rop, MpwRealNum *op);
static gboolean mpwl_perfect_square(MpwRealNum *op);
static gboolean mpwl_perfect_power(MpwRealNum *op);
static gboolean mpwl_even_p(MpwRealNum *op);
static gboolean mpwl_odd_p(MpwRealNum *op);

static void mpwl_neg(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_fac_ui (MpwRealNum *rop, unsigned int op);
static void mpwl_fac (MpwRealNum *rop, MpwRealNum *op);

static void mpwl_dblfac_ui (MpwRealNum *rop, unsigned int op);
static void mpwl_dblfac (MpwRealNum *rop, MpwRealNum *op);

static gboolean mpwl_pow_q(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

/*power to an unsigned long and optionaly invert the answer*/
static void mpwl_pow_ui(MpwRealNum *rop,MpwRealNum *op1,unsigned int e,
			gboolean reverse);

static void mpwl_pow_z(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static gboolean mpwl_pow_f(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static gboolean mpwl_pow(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2);

static void mpwl_powm(MpwRealNum *rop,
		      MpwRealNum *op1,
		      MpwRealNum *op2,
		      MpwRealNum *mod);
static void mpwl_powm_ui(MpwRealNum *rop,
			 MpwRealNum *op,
			 unsigned long int e,
			 MpwRealNum *mod);

static gboolean mpwl_sqrt(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_exp(MpwRealNum *rop,MpwRealNum *op);
static gboolean mpwl_ln(MpwRealNum *rop,MpwRealNum *op);

static void mpwl_sin(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_cos(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_sinh(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_cosh(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_arctan(MpwRealNum *rop,MpwRealNum *op);
static void mpwl_pi (MpwRealNum *rop);
static void mpwl_euler_constant (MpwRealNum *rop);
static void mpwl_rand (MpwRealNum *rop);
static void mpwl_randint (MpwRealNum *rop, MpwRealNum *op);

static int mpwl_cmp(MpwRealNum *op1, MpwRealNum *op2);

static int mpwl_cmp_ui(MpwRealNum *op, unsigned long int i);

static void mpwl_make_int(MpwRealNum *rop);

/*make number into a float, this might be neccessary for unprecise
  calculations*/
static void mpwl_make_float(MpwRealNum *rop);

/*round to an integer*/
static void mpwl_round(MpwRealNum *rop);
static void mpwl_ceil(MpwRealNum *rop);
static void mpwl_floor(MpwRealNum *rop);

static void mpwl_set_str_float(MpwRealNum *rop,const char *s,int base);
static void mpwl_set_str_int(MpwRealNum *rop,const char *s,int base);

static void mpwl_numerator(MpwRealNum *rop, MpwRealNum *op);
static void mpwl_denominator(MpwRealNum *rop, MpwRealNum *op);

/**************/
/*output stuff*/

enum {
	MPWL_EXCEPTION_NO_EXCEPTION = 0,
	MPWL_EXCEPTION_CONVERSION_ERROR,
	MPWL_EXCEPTION_NUMBER_TOO_LARGE,
};

/*get a long if possible*/
static long mpwl_get_long(MpwRealNum *op, int *ex);
static double mpwl_get_double(MpwRealNum *op, int *ex);

/*round off the number at some digits*/
static void str_make_max_digits (char *s, int digits, long *exponent);
/*trim trailing zeros*/
static void str_trim_trailing_zeros(char *s);

/*formats a floating point with mantissa in p and exponent in e*/
static char * str_format_float(char *p,long int e,int scientific_notation);

static char * str_getstring_n(long num, int max_digits,
			      int scientific_notation,
			      int integer_output_base,
			      const char *postfix);
static char * str_getstring_z(mpz_t num, int max_digits,
			      int scientific_notation,
			      int integer_output_base,
			      const char *postfix);
static char * str_getstring_q(mpq_t num, int max_digits,
			      int scientific_notation,
			      int mixed_fractions,
			      GelOutputStyle style,
			      const char *postfix);
static char * str_getstring_f(mpf_t num, int max_digits,
			      int scientific_notation,
			      const char *postfix);

static char * mpwl_getstring(MpwRealNum * num, int max_digits,
			     gboolean scientific_notation,
			     gboolean results_as_floats,
			     gboolean mixed_fractions,
			     GelOutputStyle style,
			     int integer_output_base,
			     const char *postfix);
/*************************************************************************/
/*low level stuff                                                        */
/*************************************************************************/

#ifdef HAVE_MPFR
static void
mympz_set_fr (mpz_ptr z, mpfr_srcptr fr)
{
	long exp;
	int sgn = mpfr_sgn (fr);
	
	exp = mpfr_get_z_exp (z, fr);
	if (exp > 0)
		mpz_mul_2exp (z, z, exp);
	else if (exp < 0)
		mpz_div_2exp (z, z, -exp);
	if (sgn < 0)
		mpz_neg (z, z);
}

static void
mympq_set_fr (mpq_ptr q, mpfr_srcptr fr)
{
	long exp;
	int sgn = mpfr_sgn (fr);
	
	/* FIXME: check if correct */
	mpz_set_ui (mpq_denref (q), 1);
	exp = mpfr_get_z_exp (mpq_numref (q), fr);
	if (exp > 0)
		mpq_mul_2exp (q, q, exp);
	else if (exp < 0)
		mpq_div_2exp (q, q, -exp);
	if (sgn < 0)
		mpq_neg (q, q);
}

static int
mympfr_cmp_d (mpfr_srcptr a, double b)
{
	mpfr_t fr;
	int ret;
	mpfr_init_set_d (fr, b, GMP_RNDN);
	ret = mpfr_cmp (a, fr);
	mpfr_clear (fr);
	return ret;
}
#endif

#ifndef HAVE_MPFR
/*get sin*/
static void
mympf_sin(mpf_t rop, mpf_t op, gboolean hyperbolic, gboolean reduceop)
{
	mpf_t top;
	mpf_t bottom;
	mpf_t fres;
	mpf_t foldres;
	mpf_t xsq;
	mpz_t q;
	unsigned long int i;
	gboolean negate = TRUE;

	/*special case*/
	if(mpf_cmp_ui(op,0)==0) {
		mpf_set_ui(rop,0);
		return;
	}
	
	if(!hyperbolic && reduceop) {
		if(!pi_mpf)
			mympf_pi(NULL);

		/*make xsq be op but in the range of -pi*2<xsq<pi*2 */
		mpf_init(xsq);
		mpf_mul_ui(xsq,pi_mpf,2);
		mpf_div(xsq,op,xsq);
		mpz_init(q);
		mpz_set_f(q,xsq);
		mpf_set_z(xsq,q);
		mpz_clear(q);
		mpf_mul(xsq,xsq,pi_mpf);
		mpf_mul_ui(xsq,xsq,2);
		mpf_sub(xsq,op,xsq);
	} else 
		mpf_init_set(xsq,op);
	
	mpf_init_set(top,xsq);
	mpf_init(bottom);
	mpf_set_ui(bottom,1);

	mpf_init(fres);
	mpf_init(foldres);
	mpf_set(foldres,top);

	mpf_mul(xsq,xsq,xsq);

	for(i=2;;i+=2) {
		mpf_mul(top,top,xsq);
		/*this assumes that SHRT_MAX^2 can fit in an ulong*/
		if(i<SHRT_MAX) {
			mpf_mul_ui(bottom,bottom,i*(i+1));
		} else {
			mpf_mul_ui(bottom,bottom,i);
			mpf_mul_ui(bottom,bottom,i+1);
		}
		mpf_div(fres,top,bottom);
		if(!hyperbolic && negate)
			mpf_sub(fres,foldres,fres);
		else
			mpf_add(fres,foldres,fres);
		negate= !negate;

		if(mpf_cmp(foldres,fres)==0)
			break;
		mpf_set(foldres,fres);
	}
	
	mpf_clear(top);
	mpf_clear(bottom);
	mpf_clear(foldres);
	mpf_clear(xsq);

	mpf_set(rop,fres);

	mpf_clear(fres);
}

/*get cos*/
static void
mympf_cos(mpf_t rop, mpf_t op, gboolean hyperbolic, gboolean reduceop)
{
	mpf_t top;
	mpf_t bottom;
	mpf_t fres;
	mpf_t foldres;
	mpf_t xsq;
	mpz_t q;
	unsigned long int i;
	gboolean negate = TRUE;
	int prec;
	int old_prec;
	
	old_prec = mpf_get_prec(op);
	prec = 6*4 + old_prec;

	mpf_set_default_prec(prec);

	/*special case*/
	if(mpf_cmp_ui(op,0)==0) {
		mpf_set_ui(rop,1);
		return;
	}

	if(!hyperbolic && reduceop) {
		if(!pi_mpf)
			mympf_pi(NULL);

		/*make xsq be op but in the range of -pi*2<xsq<pi*2 */
		mpf_init(xsq);
		mpf_mul_ui(xsq,pi_mpf,2);
		mpf_div(xsq,op,xsq);
		mpz_init(q);
		mpz_set_f(q,xsq);
		mpf_set_z(xsq,q);
		mpz_clear(q);
		mpf_mul(xsq,xsq,pi_mpf);
		mpf_mul_ui(xsq,xsq,2);
		mpf_sub(xsq,op,xsq);
	} else
		mpf_init_set(xsq,op);
	
	mpf_init(top);
	mpf_set_ui(top,1);
	mpf_init(bottom);
	mpf_set_ui(bottom,1);

	mpf_init(fres);
	mpf_init(foldres);
	mpf_set_ui(foldres,1);

	mpf_mul(xsq,xsq,xsq);

	for(i=1;i<ULONG_MAX;i+=2) {
		mpf_mul(top,top,xsq);
		/*this assumes that SHRT_MAX^2 can fit in an ulong*/
		if(i<SHRT_MAX) {
			mpf_mul_ui(bottom,bottom,i*(i+1));
		} else {
			mpf_mul_ui(bottom,bottom,i);
			mpf_mul_ui(bottom,bottom,i+1);
		}
		mpf_div(fres,top,bottom);
		if(!hyperbolic && negate)
			mpf_sub(fres,foldres,fres);
		else
			mpf_add(fres,foldres,fres);
		negate= !negate;

		if(mpf_eq(foldres,fres,old_prec))
			break;
		mpf_set(foldres,fres);
	}
	
	mpf_clear(top);
	mpf_clear(bottom);
	mpf_clear(foldres);
	mpf_clear(xsq);

	mpf_set(rop,fres);

	mpf_clear(fres);

	mpf_set_default_prec(default_mpf_prec);
}
#endif /* ! HAVE_MPFR */

/*get the value for pi*/
void
mympf_pi(mpf_ptr rop)
{
#ifndef HAVE_MPFR
	mpf_t foldres;
	mpf_t bottom;
	mpf_t bottom2;
	mpf_t top;
	gboolean negate = TRUE;
	unsigned long i;
#endif

	if(pi_mpf) {
		if(rop) {mpf_set(rop,pi_mpf);}
		return;
	}
	pi_mpf = g_new(__mpf_struct,1);
	mpf_init(pi_mpf);

#ifdef HAVE_MPFR
	mpfr_const_pi (pi_mpf, GMP_RNDN);
#else

	default_mpf_prec += 6*4;
	mpf_set_default_prec(default_mpf_prec);

	mpf_init(bottom);
	mpf_set_ui(bottom,1);
	mpf_init(bottom2);
	mpf_set_ui(bottom2,1);
	
	mpf_init(top);
	mpf_sqrt_ui(top,3);
	mpf_mul_ui(top,top,2);

	mpf_init(foldres);
	mpf_set(foldres,top);

	for(i=1;i<ULONG_MAX;i+=2) {
		mpf_add_ui(bottom,bottom,2);
		mpf_mul_ui(bottom2,bottom2,3);
		mpf_mul(pi_mpf,bottom,bottom2);
		mpf_div(pi_mpf,top,pi_mpf);
		if(negate)
			mpf_sub(pi_mpf,foldres,pi_mpf);
		else
			mpf_add(pi_mpf,foldres,pi_mpf);
		negate= !negate;

		if(mpf_cmp(foldres,pi_mpf)==0)
			break;
		mpf_set(foldres,pi_mpf);
	}
	
	mpf_clear(top);
	mpf_clear(bottom);
	mpf_clear(bottom2);
	mpf_clear(foldres);

	default_mpf_prec -= 6*4;
	mpf_set_default_prec(default_mpf_prec);
#endif

	if(rop) mpf_set(rop,pi_mpf);
}


#ifndef HAVE_MPFR
/*exponential function uses the fact that e^x == (e^(x/2))^2*/
/*precision is OFF ... bc mathlib defines it as "scale = 6 + scale + .44*x"*/
static void
mympf_exp(mpf_t rop,mpf_t op)
{
	mpf_t x;
	mpf_t fres;
	mpf_t foldres;
	mpf_t fact;
	mpf_t top;
	mpf_t cmp;
	unsigned long int i;
	unsigned long int f=0;
	unsigned long int prec;
	unsigned long int old_prec;

	int sgn;
	
	sgn = mpf_sgn(op);
	if(sgn == 0) {
		mpf_set_ui(rop,1);
		return;
	}

	/* 4bits is about 1 digit I guess */
	old_prec = mpf_get_prec(op);
	prec = 6*4 + old_prec;

	mpf_init2(x,prec);
	mpf_set(x,op);

	if(sgn<0)
		mpf_neg(x,x);
	
	mpf_init_set_d(cmp,1.0);
	while(mpf_cmp(x,cmp)>0) {
		mpf_div_2exp(x,x,1);
		f++;

		if G_UNLIKELY (f == ULONG_MAX) {
			mpf_clear(x);
			mpf_clear(cmp);
			(*errorout)(_("Number too large to compute exponential!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
	}
	mpf_clear(cmp);
	
	mpf_init2(fres,prec);
	mpf_init2(foldres,prec);
	mpf_set_d(foldres,1.0);

	mpf_init2(fact,prec);
	mpf_set_d(fact,1.0);

	mpf_init2(top,prec);
	mpf_set_d(top,1.0);

	for(i=1;;i++) {
		mpf_mul_ui(fact,fact,i);
		mpf_mul(top,top,x);
		mpf_div(fres,top,fact);
		
		mpf_add(fres,foldres,fres);

		if(mpf_eq(foldres,fres,old_prec+1))
			break;
		mpf_set(foldres,fres);
	}
	
	mpf_clear(fact);
	mpf_clear(x);
	mpf_clear(top);
	mpf_clear(foldres);
	
	while(f--)
		mpf_mul(fres,fres,fres);
	
	if(sgn<0)
		mpf_ui_div(fres,1,fres);

	mpf_set(rop,fres);

	mpf_clear(fres);
}

/*ln function using 2*ln(x) == ln(x^2)*/
static gboolean
mympf_ln(mpf_t rop,mpf_t op)
{
	gboolean neg = TRUE;

	mpf_t x;
	mpf_t top;
	mpf_t fres;
	mpf_t foldres;
	mpf_t cmp;
	unsigned long int i;
	unsigned long int f = 0;
	unsigned long int prec;
	unsigned long int old_prec;
	gboolean inverse = FALSE;

	if(mpf_cmp_ui(op,0)<=0)
		inverse = TRUE;

	/* 4bits is about 1 digit I guess */
	old_prec = mpf_get_prec(op);
	prec = 6*4 + old_prec;
	
	mpf_init2(x,prec);
	if(inverse)
		mpf_neg(x,op);
	else
		mpf_set(x,op);
	
	/*make x be 1/2 < x < 2 and set 2^f to the approriate multiplier*/

	mpf_init_set_d(cmp,0.5);
	while(mpf_cmp(x,cmp)<0) {
		f++;
		mpf_sqrt(x,x);
	}

	mpf_set_d(cmp,2.0);
	while(mpf_cmp(x,cmp)>=0) {
		f++;
		mpf_sqrt(x,x);
	}
	mpf_clear(cmp);
	
	/*we must work with -1 < x < 1*/
	mpf_sub_ui(x,x,1);
	
	mpf_init2(fres,prec);
	mpf_init2(foldres,prec);

	mpf_init2(top,prec);
	mpf_set_ui(top,1);

	/* x = op-1 */
	/* sum from 1 to infinity of (-1)^i * x^i / i*/
	for(i=1;;i++) {
		mpf_mul(top,top,x);
		mpf_div_ui(fres,top,i);
		
		if(neg)
			mpf_sub(fres,foldres,fres);
		else
			mpf_add(fres,foldres,fres);
		neg = !neg;

		if(mpf_eq(foldres,fres,old_prec))
			break;
		mpf_set(foldres,fres);
	}

	mpf_clear(foldres);
	mpf_clear(top);
	mpf_clear(x);

	mpf_mul_2exp(fres,fres,f);

	mpf_neg(fres,fres);

	mpf_set(rop,fres);

	mpf_clear(fres);
	
	return inverse?FALSE:TRUE;
}

/* an evil and bad function, you should really install mpfr */
static gboolean
mympf_log2(mpf_t rop,mpf_t op)
{
	mpf_t x;
	mpf_t two;
	gboolean inverse = FALSE;

	if (mpf_cmp_ui (op, 0)<=0)
		inverse = TRUE;

	mpf_init (x);
	if (inverse)
		mpf_neg (x, op);
	else
		mpf_set (x, op);
	mpf_init (two);
	mpf_set_ui (two, 2);

	mympf_ln (two, two);
	mympf_ln (x, x);

	mpf_div (rop, x, two);

	mpf_clear (two);
	mpf_clear (x);

	return inverse?FALSE:TRUE;
}

/* an evil and bad function, you should really install mpfr */
static gboolean
mympf_log10(mpf_t rop,mpf_t op)
{
	mpf_t x;
	mpf_t ten;
	gboolean inverse = FALSE;

	if (mpf_cmp_ui (op, 0)<=0)
		inverse = TRUE;

	mpf_init (x);
	if (inverse)
		mpf_neg (x, op);
	else
		mpf_set (x, op);
	mpf_init (ten);
	mpf_set_ui (ten, 10);

	mympf_ln (ten, ten);
	mympf_ln (x, x);

	mpf_div (rop, x, ten);

	mpf_clear (ten);
	mpf_clear (x);

	return inverse?FALSE:TRUE;
}

/*these functions may be more precise but slow as HELL*/
#if 0
/*ln function for ranges op>=1/2*/
static void
mympf_ln_top(mpf_t rop,mpf_t op)
{
	mpf_t x;
	mpf_t top;
	mpf_t bottom;
	mpf_t fres;
	mpf_t foldres;
	unsigned long int i;
	
	mpf_init_set(x,op);
	mpf_sub_ui(x,x,1);
	
	mpf_init(fres);
	mpf_init(foldres);

	mpf_init(top);
	mpf_set_ui(top,1);
	mpf_init(bottom);
	mpf_set_ui(bottom,1);

	/* x = op-1 */
	/* sum from 1 to infinity of x^i / ( i * op^i ) */
	/* top = x^i, bottom = op^i */
	for(i=1;;i++) {
		mpf_mul(top,top,x);

		mpf_mul(bottom,bottom,op);
		mpf_mul_ui(fres,bottom,i);

		mpf_div(fres,top,fres);
		
		mpf_add(fres,fres,foldres);

		if(mpf_cmp(foldres,fres)==0)
			break;
		mpf_set(foldres,fres);
	}
	
	mpf_clear(foldres);
	mpf_clear(top);
	mpf_clear(bottom);
	mpf_clear(x);

	mpf_set(rop,fres);

	mpf_clear(fres);
}
#endif

/* Following function stolen from internet post by:
 * Guillermo Ballester Valor <gbv@oxixares.com> */

/*arctan function*/
static void
mympf_arctan(mpf_ptr rop,mpf_ptr op)
{
	mpf_t halfpi;
  mpf_t aux,sum,num,op2,limit;
  unsigned long int n=3;

  if(!pi_mpf)
	  mympf_pi(NULL);

  mpf_init(halfpi);
  mpf_div_ui (halfpi, pi_mpf, 2);
  
  /* 
     Trying to avoid the danger op == 1 
     which make a slooooow convergence
  */
  mpf_init(sum);
  mpf_abs(sum,op);
  mpf_add(sum,sum,sum);/* An error in GMP ? */
  if( (mpf_cmp_ui(sum,1) > 0) && (mpf_cmp_ui( sum,4) < 0) )
    {
      mpf_init(aux);
      mpf_mul(aux,op,op);
      mpf_add_ui(aux,aux,1U);
      mpf_sqrt(aux,aux);
      mpf_sub_ui(aux,aux,1U);
      mpf_div(aux,aux,op);
      /* recursive call */
      mympf_arctan(sum,aux);
     
      mpf_mul_ui(rop,sum,2U);
      mpf_clear(sum);
      mpf_clear(aux);
      return;
    }
  
  mpf_set_ui(sum,0U);
  mpf_init(op2); mpf_init(aux);
  mpf_init(num); mpf_init(limit);
  
  mpf_mul(op2, op, op);
  mpf_set(num, op);
  if(mpf_cmp_ui(op2,1U) > 0) 
    {
      mpf_ui_div(op2, 1U, op2);
      mpf_set(sum,halfpi);
      if(mpf_cmp_si(op, 0L) < 0)  mpf_neg(sum,sum);
      mpf_ui_div(num,1U,num);
      mpf_neg(num,num);
    }
  mpf_neg(op2, op2);
  mpf_add(sum,sum,num);

  do {
    mpf_set(limit,sum);
    mpf_mul(num,num,op2);
    mpf_div_ui(aux,num,n);
    mpf_add(sum,sum,aux);

    n+=2;
    /*mpf_out_str(stdout, 10, 0, sum);
      fprintf(stdout,"\n");*/

  } while (mpf_cmp(sum,limit));
  mpf_set(rop,sum);
  mpf_clear(sum); mpf_clear(aux); mpf_clear(num);
  mpf_clear(op2); mpf_clear(limit);
  mpf_clear(halfpi);
}
#endif /* ! HAVE_MPFR */

/*my own power function for floats, very simple :) */
static void
mympf_pow_z(mpf_t rop,mpf_t op,mpz_t e)
{
	int esgn = mpz_sgn (e);
	gboolean neg = FALSE;
	if (esgn == 0) {
		mpf_set_ui (rop, 1);
		return;
	} else if (esgn < 0) {
		neg = TRUE;
		mpz_neg (e, e);
	}

	if (mpz_fits_ulong_p (e)) {
		unsigned int exp = mpz_get_ui (e);
		mpf_pow_ui (rop, op, exp);
		if (neg) {
			mpf_ui_div (rop, 1, rop);
			mpz_neg (e, e);
		}
	} else {
		mpf_t fe;

		/* we don't need a negative here */
		if (neg) {
			mpz_neg (e, e);
		}

		mpf_init (fe);
		mpf_set_z (fe, e);
#ifdef HAVE_MPFR
		mpfr_pow (rop, op, fe, GMP_RNDN);
#else
		mympf_ln (rop, op);
		mpf_mul (rop, rop, fe);
		mympf_exp (rop, rop);
#endif
		mpf_clear (fe);
	}
}


/*my own power function for ints, very simple :), assumes that
 * e is positive */
static void
mympz_pow_z(mpz_t rop,mpz_t op,mpz_t e)
{
	if (mpz_sgn (e) == 0) {
		mpz_set_ui (rop, 1);
		return;
	}

	if G_LIKELY (mpz_fits_ulong_p (e)) {
		unsigned int exp = mpz_get_ui (e);
		mpz_pow_ui (rop, op, exp);
	} else {
		(*errorout)(_("Integer exponent too large to compute"));
		error_num=NUMERICAL_MPW_ERROR;
		mpz_set_ui (rop, 1);
	}
}

/*clear extra variables of type type, if type=op->type nothing is done*/
static void
mpwl_clear_extra_type(MpwRealNum *op,int type)
{
	if(op->type==type)
		return;
	switch(type) {
	case MPW_INTEGER:
		if(op->data.ival) {
			mpz_clear(op->data.ival);
			g_free(op->data.ival);
			op->data.ival = NULL;
		}
		break;
	case MPW_RATIONAL:
		if(op->data.rval) {
			mpq_clear(op->data.rval);
			g_free(op->data.rval);
			op->data.rval = NULL;
		}
		break;
	case MPW_FLOAT:
		if(op->data.fval) {
			mpf_clear(op->data.fval);
			g_free(op->data.fval);
			op->data.fval = NULL;
		}
		break;
	}
}

static gboolean
mympq_perfect_square_p (mpq_t op)
{
	return mpz_perfect_square_p (mpq_numref(op)) &&
		mpz_perfect_square_p (mpq_denref(op));
}

/*only set the type, don't free it, and don't set the type variable
  create an extra type of the variable for temporary use*/
static void
mpwl_make_extra_type(MpwRealNum *op,int type)
{
	if(op->type==type)
		return;
	switch(type) {
	case MPW_NATIVEINT:
		if(op->type==MPW_INTEGER)
			op->data.nval = mpz_get_si(op->data.ival);
		else if(op->type==MPW_RATIONAL)
			op->data.nval = mpq_get_d(op->data.rval);
		else if(op->type==MPW_FLOAT)
			op->data.nval = mpf_get_d(op->data.fval);
		break;
	case MPW_INTEGER:
		if(!op->data.ival)
			op->data.ival = g_new(__mpz_struct,1);
		mpz_init(op->data.ival);
		if(op->type==MPW_FLOAT)
			mpz_set_f(op->data.ival,op->data.fval);
		else if(op->type==MPW_RATIONAL)
			mpz_set_q(op->data.ival,op->data.rval);
		else if(op->type==MPW_NATIVEINT)
			mpz_set_si(op->data.ival,op->data.nval);
		break;
	case MPW_RATIONAL:
		if(!op->data.rval)
			op->data.rval = g_new(__mpq_struct,1);
		mpq_init(op->data.rval);
		if(op->type==MPW_INTEGER)
			mpq_set_z(op->data.rval,op->data.ival);
		else if(op->type==MPW_FLOAT)
			mpq_set_f(op->data.rval,op->data.fval);
		else if(op->type==MPW_NATIVEINT)
			mpq_set_si(op->data.rval,op->data.nval,1);
		break;
	case MPW_FLOAT:
		if(!op->data.fval)
			op->data.fval = g_new(__mpf_struct,1);
		mpf_init(op->data.fval);
		if(op->type==MPW_INTEGER)
			mpf_set_z(op->data.fval,op->data.ival);
		else if(op->type==MPW_RATIONAL) {
			mpf_set_q(op->data.fval,op->data.rval);
			/* XXX: a hack!!
			 * get around a mpf_set_q bug*/
			if(mpq_sgn(op->data.rval)<0 &&
			   mpf_sgn(op->data.fval)>0) {
				mpf_neg(op->data.fval,op->data.fval);
			}
		} else if(op->type==MPW_NATIVEINT)
			mpf_set_si(op->data.fval,op->data.nval);
		break;
	}
}

/*only set the type, don't free it, and don't set the type variable
  create an extra type of the variable for temporary use*/

#define mpwl_make_extra_type_no_convert mpwl_make_extra_type
	/* FIXME: something is very wrong, see test:
	   Numerator (3i/7)
	 */
/*
static void
mpwl_make_extra_type_no_convert (MpwRealNum *op, int type)
{
	if(op->type==type)
		return;
	switch(type) {
	case MPW_NATIVEINT:
		break;
	case MPW_INTEGER:
		if(!op->data.ival)
			op->data.ival = g_new(__mpz_struct,1);
		mpz_init(op->data.ival);
		break;
	case MPW_RATIONAL:
		if(!op->data.rval)
			op->data.rval = g_new(__mpq_struct,1);
		mpq_init(op->data.rval);
		break;
	case MPW_FLOAT:
		if(!op->data.fval)
			op->data.fval = g_new(__mpf_struct,1);
		mpf_init(op->data.fval);
		break;
	}
}
*/

static void
mpwl_make_type(MpwRealNum *op,int type)
{
	int t;

	if(op->type==type)
		return;
	t=op->type;
	mpwl_make_extra_type(op,type);
	op->type=type;
	mpwl_clear_extra_type(op,t);
}

/*this only adds a value of that type but does nto clear the old one!
  retuns the new extra type set*/
static int
mpwl_make_same_extra_type(MpwRealNum *op1,MpwRealNum *op2)
{
	if(op1->type==op2->type)
		return op1->type;
	else if(op1->type > op2->type) {
		mpwl_make_extra_type(op2,op1->type);
		return op1->type;
	} else { /*if(op1->type < op2->type)*/
		mpwl_make_extra_type(op1,op2->type);
		return op2->type;
	}
}

#if 0
/*this only adds a value of that type but does nto clear the old one!
  retuns the new extra type set*/
static int
mpwl_make_same_extra_type_3 (MpwRealNum *op1, MpwRealNum *op2, MpwRealNum *op3)
{
	int maxtype = MAX (MAX (op1->type, op2->type), op3->type);
	if (op1->type < maxtype)
		mpwl_make_extra_type (op1, maxtype);
	if (op2->type < maxtype)
		mpwl_make_extra_type (op2, maxtype);
	if (op3->type < maxtype)
		mpwl_make_extra_type (op3, maxtype);
	return maxtype;
}
#endif

/*make new type and clear the old one*/
static void
mpwl_make_same_type(MpwRealNum *op1,MpwRealNum *op2)
{
	if(op1->type==op2->type)
		return;
	else if(op1->type > op2->type)
		mpwl_make_type(op2,op1->type);
	else /*if(op1->type < op2->type)*/
		mpwl_make_type(op1,op2->type);
}

static void
mpwl_clear(MpwRealNum *op)
{
	if(!op) return;

	if(op->data.ival) {
		mpz_clear(op->data.ival);
		g_free(op->data.ival);
		op->data.ival = NULL;
	}
	if(op->data.rval) {
		mpq_clear(op->data.rval);
		g_free(op->data.rval);
		op->data.rval = NULL;
	}
	if(op->data.fval) {
		mpf_clear(op->data.fval);
		g_free(op->data.fval);
		op->data.fval = NULL;
	}
}

static void
mpwl_init_type(MpwRealNum *op,int type)
{
	if(!op) return;

	op->type=type;

	switch(type) {
	case MPW_NATIVEINT:
		op->data.nval = 0;
		break;
	case MPW_INTEGER:
		if(!op->data.ival)
			op->data.ival = g_new(__mpz_struct,1);
		mpz_init(op->data.ival);
		break;
	case MPW_RATIONAL:
		if(!op->data.rval)
			op->data.rval = g_new(__mpq_struct,1);
		mpq_init(op->data.rval);
		break;
	case MPW_FLOAT:
		if(!op->data.fval)
			op->data.fval = g_new(__mpf_struct,1);
		mpf_init(op->data.fval);
		break;
	default: ;
	}
}

static void
mpwl_free(MpwRealNum *op, int local)
{
	if(!op) return;
	mpwl_clear(op);
	if(local) return;
	/*FIXME: the 2000 should be settable*/
	/*if we want to store this so that we don't allocate new one
	  each time, up to a limit of 2000, unless it was some local
	  var in which case it can't be freed nor put on the free
	  stack*/
	if(free_reals_n>2000) {
		g_free(op);
	} else {
		op->alloc.next = free_reals;
		free_reals = op;
		free_reals_n++;
	}
}

static int
mpwl_sgn(MpwRealNum *op)
{
	switch(op->type) {
	case MPW_NATIVEINT: 
		if(op->data.nval>0)
			return 1;
		else if(op->data.nval<0)
			return -1;
		else
			return 0;
	case MPW_FLOAT: return mpf_sgn(op->data.fval);
	case MPW_RATIONAL: return mpq_sgn(op->data.rval);
	case MPW_INTEGER: return mpz_sgn(op->data.ival);
	}
	return 0;
}

static int
mpwl_cmp(MpwRealNum *op1, MpwRealNum *op2)
{
	int r=0;
	int t;

	t=mpwl_make_same_extra_type(op1,op2);
	switch(t) {
	case MPW_NATIVEINT:
		if(op1->data.nval > op2->data.nval)
			return 1;
		else if(op1->data.nval < op2->data.nval)
			return -1;
		else
			return 0;
	case MPW_FLOAT:
		r=mpf_cmp(op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		r=mpq_cmp(op1->data.rval,op2->data.rval);
		break;
	case MPW_INTEGER:
		r=mpz_cmp(op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	return r;
}

static int
mpwl_cmp_ui(MpwRealNum *op, unsigned long int i)
{
	switch(op->type) {
	case MPW_NATIVEINT:
		if(op->data.nval > i)
			return 1;
		else if(op->data.nval < i)
			return -1;
		else
			return 0;
	case MPW_FLOAT: return mpf_cmp_ui(op->data.fval,i);
	case MPW_RATIONAL: return mpq_cmp_ui(op->data.rval,i,1);
	case MPW_INTEGER: return mpz_cmp_ui(op->data.ival,i);
	}
	return 0;
}

static void
mpwl_set_d(MpwRealNum *rop,double d)
{
	switch(rop->type) {
	case MPW_FLOAT:
		mpf_set_d(rop->data.fval,d);
		break;
	case MPW_RATIONAL:
	case MPW_INTEGER:
		mpwl_clear(rop);
	case MPW_NATIVEINT:
		mpwl_init_type(rop,MPW_FLOAT);
		mpf_set_d(rop->data.fval,d);
		break;
	}
}

static void
mpwl_set_si(MpwRealNum *rop,signed long int i)
{
	switch(rop->type) {
	case MPW_FLOAT:
		mpwl_clear(rop);
		break;
	case MPW_RATIONAL:
		mpwl_clear(rop);
		break;
	case MPW_INTEGER:
		if(i==LONG_MIN) {
			mpz_set_si(rop->data.ival,i);
			return;
		}
		mpwl_clear(rop);
		break;
	}
	if(i==LONG_MIN) {
		mpwl_init_type(rop,MPW_INTEGER);
		mpz_set_si(rop->data.ival,i);
		return;
	}
	rop->type = MPW_NATIVEINT;
	rop->data.nval = i;
}

static void
mpwl_set_ui(MpwRealNum *rop,unsigned long int i)
{
	if(i>LONG_MAX) {
		switch(rop->type) {
		case MPW_FLOAT:
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_INTEGER);
			mpz_set_ui(rop->data.ival,i);
			break;
		case MPW_RATIONAL:
			mpq_set_ui(rop->data.rval,i,1);
			break;
		case MPW_NATIVEINT:
			mpwl_init_type(rop,MPW_INTEGER);
		case MPW_INTEGER:
			mpz_set_ui(rop->data.ival,i);
			break;
		}
	} else {
		switch(rop->type) {
		case MPW_FLOAT:
			mpwl_clear(rop);
			break;
		case MPW_RATIONAL:
			mpwl_clear(rop);
			break;
		case MPW_INTEGER:
			mpwl_clear(rop);
			break;
		}
		rop->type = MPW_NATIVEINT;
		rop->data.nval = i;
	}
}

/*the original op should be a local or not be used anymore*/
static void
mpwl_move(MpwRealNum *rop,MpwRealNum *op)
{
	if(rop==op)
		return;
	
	if(rop->data.ival) {
		mpz_clear(rop->data.ival);
		g_free(rop->data.ival);
	}
	if(rop->data.rval) {
		mpq_clear(rop->data.rval);
		g_free(rop->data.rval);
	}
	if(rop->data.fval) {
		mpf_clear(rop->data.fval);
		g_free(rop->data.fval);
	}
	memcpy(rop,op,sizeof(MpwRealNum));
	rop->alloc.usage=1;
	/* not necessary 
	op->type = MPW_NATIVEINT;
	op->data.nval = 0;
	op->data.ival = NULL;
	op->data.rval = NULL;
	op->data.fval = NULL;*/
}

static void
mpwl_set(MpwRealNum *rop,MpwRealNum *op)
{
	if(rop==op)
		return;
	else if(rop->type==op->type) {
		switch(op->type) {
		case MPW_NATIVEINT:
			rop->data.nval = op->data.nval;
			break;
		case MPW_FLOAT:
			mpf_set(rop->data.fval,op->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_set(rop->data.rval,op->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_set(rop->data.ival,op->data.ival);
			break;
		}
	} else {
		mpwl_clear(rop);
		mpwl_init_type(rop,op->type);
		mpwl_set(rop,op);
	}
}

static int
mympn_add(long *res, long op1, long op2)
{
	long r;
	if(op1>=0 && op2>=0) {
		r = op1+op2;
		if(r<0)
			return FALSE;
	} else if (op1<0 && op2<0) {
		r = op1+op2;
		if(r>=0 || r==LONG_MIN)
			return FALSE;
	} else {
		/*we would get a one off error!*/
		if(op1 == LONG_MIN ||
		   op2 == LONG_MIN)
			return FALSE;
		r = op1+op2;
	}
	*res = r;

	return TRUE;
}

static void
mpwl_add(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};
	
	/*special case*/
	if(op1->type==op2->type && rop!=op1 && rop!=op2) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_NATIVEINT:
			if(!mympn_add(&rop->data.nval,op1->data.nval,op2->data.nval)) {
				mpwl_init_type(rop,MPW_INTEGER);
				mpz_set_si(rop->data.ival,op1->data.nval);
				if(op2->data.nval>=0)
					mpz_add_ui(rop->data.ival,rop->data.ival,
						   op2->data.nval);
				else
					mpz_sub_ui(rop->data.ival,rop->data.ival,
						   -(op2->data.nval));

			}
			break;
		case MPW_FLOAT:
			mpf_add(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_add(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_add(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	mpwl_init_type(&r,t);

	switch(t) {
	case MPW_NATIVEINT:
		if(!mympn_add(&r.data.nval,op1->data.nval,op2->data.nval)) {
			mpwl_init_type(&r,MPW_INTEGER);
			mpz_set_si(r.data.ival,op1->data.nval);
			if(op2->data.nval>=0)
				mpz_add_ui(r.data.ival,r.data.ival,op2->data.nval);
			else
				mpz_sub_ui(r.data.ival,r.data.ival,-(op2->data.nval));

		}
		break;
	case MPW_FLOAT:
		mpf_add(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpq_add(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpz_add(r.data.ival,op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
}

static void
mpwl_add_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i)
{
	switch(op->type) {
	case MPW_NATIVEINT:
		/*this assumes that SHRT_MAX+SHRT_MAX can fit in an long*/
		if(op->data.nval>=SHRT_MAX ||
		   i>=SHRT_MAX) {
			long val = op->data.nval;
			if(rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			mpz_set_si(rop->data.ival,val);
			mpz_add_ui(rop->data.ival,rop->data.ival,i);
		} else {
			if(rop->type != MPW_NATIVEINT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_NATIVEINT);
			}
			rop->data.nval = op->data.nval+i;
		}
		break;
	case MPW_FLOAT:
		if(rop->type != MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_add_ui(rop->data.fval,op->data.fval,i);
		break;
	case MPW_RATIONAL:
		{
			mpq_t tmp;
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_init(tmp);
			mpq_set_ui(tmp,i,1);
			mpq_add(rop->data.rval,op->data.rval,tmp);
			mpq_clear(tmp);
			mpwl_make_int(rop);
		}
		break;
	case MPW_INTEGER:
		mpz_add_ui(rop->data.ival,op->data.ival,i);
		break;
	}
}

static int
mympn_sub(long *res, long op1, long op2)
{
	long r;
	if(op1>=0 && op2<0) {
		r = op1-op2;
		if(r<0)
			return FALSE;
	} else if (op1<0 && op2>=0) {
		r = op1-op2;
		if(r>=0 || r==LONG_MIN)
			return FALSE;
	} else {
		/*we would get a one off error!*/
		if(op1 == LONG_MIN ||
		   op2 == LONG_MIN)
			return FALSE;
		r = op1-op2;
	}
	*res = r;

	return TRUE;
}

static void
mpwl_sub(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	/*special case*/
	if(op1->type==op2->type && rop!=op1 && rop!=op2) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_NATIVEINT:
			if(!mympn_sub(&rop->data.nval,op1->data.nval,op2->data.nval)) {
				mpwl_init_type(rop,MPW_INTEGER);
				mpz_set_si(rop->data.ival,op1->data.nval);
				if(op2->data.nval>=0)
					mpz_add_ui(rop->data.ival,rop->data.ival,op2->data.nval);
				else
					mpz_sub_ui(rop->data.ival,rop->data.ival,-(op2->data.nval));

			}
			break;
		case MPW_FLOAT:
			mpf_sub(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_sub(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			mpz_sub(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	mpwl_init_type(&r,t);

	switch(t) {
	case MPW_NATIVEINT:
		if(!mympn_sub(&r.data.nval,op1->data.nval,op2->data.nval)) {
			mpwl_init_type(&r,MPW_INTEGER);
			mpz_set_si(r.data.ival,op1->data.nval);
			if(op2->data.nval>=0)
				mpz_sub_ui(r.data.ival,r.data.ival,op2->data.nval);
			else
				mpz_add_ui(r.data.ival,r.data.ival,-(op2->data.nval));

		}
		break;
	case MPW_FLOAT:
		mpf_sub(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpq_sub(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpz_sub(r.data.ival,op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
}

static void
mpwl_sub_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long i)
{
	switch(op->type) {
	case MPW_NATIVEINT:
		/*this assumes that SHRT_MIN-SHRT_MAX can fit in an long*/
		if(op->data.nval<=SHRT_MIN ||
		   i>=SHRT_MAX) {
			long val = op->data.nval;
			if(rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			mpz_set_si(rop->data.ival,val);
			mpz_sub_ui(rop->data.ival,rop->data.ival,i);
		} else {
			if(rop->type != MPW_NATIVEINT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_NATIVEINT);
			}
			rop->data.nval = op->data.nval-i;
		}
		break;
	case MPW_FLOAT:
		if(rop->type != MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_sub_ui(rop->data.fval,op->data.fval,i);
		break;
	case MPW_RATIONAL:
		{
			mpq_t tmp;
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_init(tmp);
			mpq_set_ui(tmp,i,1);
			mpq_neg(tmp,tmp);
			mpq_add(rop->data.rval,op->data.rval,tmp);
			mpq_clear(tmp);
			mpwl_make_int(rop);
		}
		break;
	case MPW_INTEGER:
		mpz_sub_ui(rop->data.ival,op->data.ival,i);
		break;
	}
}

static void
mpwl_ui_sub(MpwRealNum *rop, unsigned long i, MpwRealNum *op)
{
	switch(op->type) {
	case MPW_NATIVEINT:
		/*this assumes that SHRT_MIN-SHRT_MAX can fit in an long*/
		if(op->data.nval>=SHRT_MAX ||
		   i<=SHRT_MIN) {
			long val = op->data.nval;
			if(rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			mpz_set_si(rop->data.ival,val);
			mpz_neg(rop->data.ival,rop->data.ival);
			mpz_add_ui(rop->data.ival,rop->data.ival,i);
		} else {
			if(rop->type != MPW_NATIVEINT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_NATIVEINT);
			}
			rop->data.nval = i-op->data.nval;
		}
		break;
	case MPW_FLOAT:
		if(rop->type != MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_ui_sub(rop->data.fval,i,op->data.fval);
		break;
	case MPW_RATIONAL:
		{
			mpq_t tmp;
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_init(tmp);
			mpq_set_ui(tmp,i,1);
			mpq_sub(rop->data.rval,op->data.rval,tmp);
			mpq_clear(tmp);
			mpwl_make_int(rop);
		}
		break;
	case MPW_INTEGER:
		mpz_sub_ui(rop->data.ival,op->data.ival,i);
		mpz_neg(rop->data.ival,rop->data.ival);
		break;
	}
}

static void
mpwl_mul(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	/*special case*/
	if(op1->type==op2->type && rop!=op1 && rop!=op2) {
		switch(op1->type) {
		case MPW_NATIVEINT:
			if(rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			mpz_set_si(rop->data.ival,op1->data.nval);
			if(op2->data.nval>=0)
				mpz_mul_ui(rop->data.ival,rop->data.ival,op2->data.nval);
			else {
				mpz_mul_ui(rop->data.ival,rop->data.ival,-(op2->data.nval));
				mpz_neg(rop->data.ival,rop->data.ival);
			}
			break;
		case MPW_FLOAT:
			if(rop->type != MPW_FLOAT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_FLOAT);
			}
			mpf_mul(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			if(rop->type != MPW_RATIONAL) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_RATIONAL);
			}
			mpq_mul(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		case MPW_INTEGER:
			if(rop->type != MPW_INTEGER) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			mpz_mul(rop->data.ival,op1->data.ival,op2->data.ival);
			break;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	switch(t) {
	case MPW_NATIVEINT:
		mpwl_init_type(&r,MPW_INTEGER);
		mpz_set_si(r.data.ival,op1->data.nval);
		if(op2->data.nval>=0)
			mpz_mul_ui(r.data.ival,r.data.ival,op2->data.nval);
		else {
			mpz_mul_ui(r.data.ival,r.data.ival,-(op2->data.nval));
			mpz_neg(r.data.ival,r.data.ival);
		}
		break;
	case MPW_FLOAT:
		mpwl_init_type(&r,t);
		mpf_mul(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpwl_init_type(&r,t);
		mpq_mul(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpwl_init_type(&r,t);
		mpz_mul(r.data.ival,op1->data.ival,op2->data.ival);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
}

static void
mpwl_mul_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i)
{
	switch(op->type) {
	case MPW_NATIVEINT:
		if(rop!=op) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_INTEGER);
			mpz_set_si(rop->data.ival,op->data.nval);
			mpz_mul_ui(rop->data.ival,rop->data.ival,i);
		} else {
			mpwl_make_type(rop,MPW_INTEGER);
			mpz_mul_ui(rop->data.ival,rop->data.ival,i);
		}
		break;
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_mul_ui(rop->data.fval,op->data.fval,i);
		break;
	case MPW_RATIONAL:
		if(rop->type!=MPW_RATIONAL) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_RATIONAL);
		}
		mpz_mul_ui(mpq_numref(rop->data.rval),
			   mpq_numref(op->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		if(rop->type!=MPW_INTEGER) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_INTEGER);
		}
		mpz_mul_ui(rop->data.ival,op->data.ival,i);
		break;
	}
}

static void
mpwl_div(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	int t;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY (mpwl_sgn(op2)==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	/*special case*/
	if(op1->type > MPW_INTEGER && op1->type==op2->type && rop!=op2) {
		if(rop->type != op1->type) {
			mpwl_clear(rop);
			mpwl_init_type(rop,op1->type);
		}
		switch(op1->type) {
		case MPW_FLOAT:
			mpf_div(rop->data.fval,op1->data.fval,op2->data.fval);
			break;
		case MPW_RATIONAL:
			mpq_div(rop->data.rval,op1->data.rval,op2->data.rval);
			mpwl_make_int(rop);
			break;
		default: ;
		}
		return;
	}

	t=mpwl_make_same_extra_type(op1,op2);

	switch(t) {
	case MPW_FLOAT:
		mpwl_init_type(&r,t);
		mpf_div(r.data.fval,op1->data.fval,op2->data.fval);
		break;
	case MPW_RATIONAL:
		mpwl_init_type(&r,t);
		mpq_div(r.data.rval,op1->data.rval,op2->data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_INTEGER:
		mpwl_init_type(&r,MPW_RATIONAL);
		mpq_set_z(r.data.rval,op1->data.ival);
		mpz_set(mpq_denref(r.data.rval),
			op2->data.ival);
		mpwl_make_int(&r);
		break;
	case MPW_NATIVEINT:
		if(op1->data.nval%op2->data.nval == 0) {
			mpwl_init_type(&r,MPW_NATIVEINT);
			r.data.nval = op1->data.nval/op2->data.nval;
			break;
		}
		mpwl_init_type(&r,MPW_RATIONAL);
		if(op2->data.nval>0)
			mpq_set_si(r.data.rval,
				   op1->data.nval,
				   op2->data.nval);
		else
			mpq_set_si(r.data.rval,
				   -(op1->data.nval),
				   -(op2->data.nval));
		mpwl_make_int(&r);
		break;
	}
	mpwl_clear_extra_type(op1,t);
	mpwl_clear_extra_type(op2,t);
	mpwl_move(rop,&r);
}

static void
mpwl_div_ui(MpwRealNum *rop,MpwRealNum *op,unsigned long int i)
{
	int t;
	if G_UNLIKELY (i==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_div_ui(rop->data.fval,op->data.fval,i);
		break;
	case MPW_RATIONAL:
		if(rop->type!=MPW_RATIONAL) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_RATIONAL);
		}
		mpz_mul_ui(mpq_denref(rop->data.rval),
			   mpq_denref(op->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		t = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_RATIONAL);
		rop->type = MPW_RATIONAL;
		mpq_set_z(rop->data.rval,op->data.ival);
		mpz_set_ui(mpq_denref(rop->data.rval),i);
		mpwl_clear_extra_type(rop,t);
		break;
	case MPW_NATIVEINT:
		if(op->data.nval%i == 0) {
			long n = op->data.nval;
			mpwl_init_type(rop,MPW_NATIVEINT);
			rop->data.nval = n/i;
			break;
		}
		t = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_RATIONAL);
		rop->type = MPW_RATIONAL;
		mpq_set_si(rop->data.rval,op->data.nval,i);
		mpwl_clear_extra_type(rop,t);
		break;
	}
}

static void
mpwl_ui_div(MpwRealNum *rop,unsigned long int i,MpwRealNum *op)
{
	int t;
	if G_UNLIKELY (mpwl_sgn(op)==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	switch(op->type) {
	case MPW_FLOAT:
		if(rop->type!=MPW_FLOAT) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_FLOAT);
		}
		mpf_ui_div(rop->data.fval,i,op->data.fval);
		break;
	case MPW_RATIONAL:
		if(rop->type!=MPW_RATIONAL) {
			mpwl_clear(rop);
			mpwl_init_type(rop,MPW_RATIONAL);
		}
		mpq_inv(rop->data.rval,op->data.rval);
		mpz_mul_ui(mpq_numref(rop->data.rval),
			   mpq_numref(rop->data.rval),i);
		mpwl_make_int(rop);
		break;
	case MPW_INTEGER:
		t = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_RATIONAL);
		rop->type = MPW_RATIONAL;
		mpz_set_ui(mpq_numref(rop->data.rval),i);
		mpz_set(mpq_denref(rop->data.rval),op->data.ival);
		mpwl_clear_extra_type(rop,t);
		break;
	case MPW_NATIVEINT:
		if(i%op->data.nval == 0) {
			long n = op->data.nval;
			mpwl_init_type(rop,MPW_NATIVEINT);
			rop->data.nval = i/n;
			break;
		}
		t = rop->type;
		mpwl_make_extra_type_no_convert (rop,MPW_RATIONAL);
		rop->type = MPW_RATIONAL;
		if(op->data.nval>0)
			mpq_set_ui(rop->data.rval,i,
				   op->data.nval);
		else
			mpq_set_ui(rop->data.rval,-i,
				   -(op->data.nval));
		mpwl_clear_extra_type(rop,t);
		break;
	}
}

static void
mpwl_mod(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_UNLIKELY (mpwl_sgn(op2)==0) {
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}

	if G_LIKELY (op1->type<=MPW_INTEGER && op2->type<=MPW_INTEGER) {
		int t=mpwl_make_same_extra_type(op1,op2);
		int t1;

		switch(t) {
		case MPW_INTEGER:
			t1 = rop->type;
			mpwl_make_extra_type_no_convert (rop,MPW_INTEGER);
			rop->type = MPW_INTEGER;
			mpz_mod(rop->data.ival,op1->data.ival,op2->data.ival);
			mpwl_clear_extra_type(rop,t1);
			break;
		case MPW_NATIVEINT:
			if(rop->type!=MPW_NATIVEINT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_INTEGER);
			}
			rop->data.nval = op1->data.nval % op2->data.nval;
			break;
		}
		mpwl_clear_extra_type(op1,t);
		mpwl_clear_extra_type(op2,t);
	} else {
		(*errorout)(_("Can't do modulo of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_gcd(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type<=MPW_INTEGER && op2->type<=MPW_INTEGER) {
		int t=mpwl_make_same_extra_type(op1,op2);
		int t1;
		long a,b;

		switch(t) {
		case MPW_INTEGER:
			t1 = rop->type;
			mpwl_make_extra_type_no_convert (rop,MPW_INTEGER);
			rop->type = MPW_INTEGER;
			mpz_gcd(rop->data.ival,op1->data.ival,op2->data.ival);
			mpwl_clear_extra_type(rop,t1);
			break;
		case MPW_NATIVEINT:
			if(rop->type!=MPW_NATIVEINT) {
				mpwl_clear(rop);
				mpwl_init_type(rop,MPW_NATIVEINT);
			}
			a = op1->data.nval;
		        b = op2->data.nval;
			while(b!=0) {
				long tmp = a%b;
				a = b;
				b = tmp;
			}
			rop->data.nval = a>0?a:-a;
			break;
		}
		mpwl_clear_extra_type(op1,t);
		mpwl_clear_extra_type(op2,t);
	} else {
		(*errorout)(_("Can't do GCD of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static gboolean
mpwl_invert (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	if G_LIKELY (op1->type <= MPW_INTEGER && op2->type <= MPW_INTEGER) {
		int t = mpwl_make_same_extra_type (op1, op2);
		gboolean suc = FALSE;
		mpz_t ret;
		mpz_t i1, i2;

		mpz_init (ret);

		switch(t) {
		case MPW_INTEGER:
			suc = mpz_invert (ret, op1->data.ival, op2->data.ival);
			break;
		case MPW_NATIVEINT:
			mpz_init_set_si (i1, op1->data.nval);
			mpz_init_set_si (i2, op2->data.nval);
			suc = mpz_invert (ret, i1, i2);
			mpz_clear (i1);
			mpz_clear (i2);
			break;
		}
		mpwl_clear_extra_type (op1, t);
		mpwl_clear_extra_type (op2, t);
		if (suc) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
			mpz_set (rop->data.ival, ret);
		}
		mpz_clear (ret);

		return suc;
	} else {
		(*errorout)(_("Can't modulo invert non integers!"));
		error_num=NUMERICAL_MPW_ERROR;

		return FALSE;
	}
}

static void
mpwl_jacobi(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type<=MPW_INTEGER && op2->type<=MPW_INTEGER) {
		int t=mpwl_make_same_extra_type(op1,op2);
		int ret = 0;
		mpz_t i1,i2;

		switch(t) {
		case MPW_INTEGER:
			ret = mpz_jacobi(op1->data.ival,op2->data.ival);
			break;
		case MPW_NATIVEINT:
			mpz_init_set_si(i1,op1->data.nval);
			mpz_init_set_si(i2,op2->data.nval);
			ret = mpz_jacobi(i1,i2);
			mpz_clear(i1);
			mpz_clear(i2);
			break;
		}
		mpwl_clear_extra_type(op1,t);
		mpwl_clear_extra_type(op2,t);
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_NATIVEINT);
		rop->data.nval = ret;
	} else {
		(*errorout)(_("Can't get jacobi symbols of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_legendre(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	if G_LIKELY (op1->type<=MPW_INTEGER && op2->type<=MPW_INTEGER) {
		int t=mpwl_make_same_extra_type(op1,op2);
		int ret = 0;
		mpz_t i1,i2;

		switch(t) {
		case MPW_INTEGER:
			ret = mpz_legendre(op1->data.ival,op2->data.ival);
			break;
		case MPW_NATIVEINT:
			mpz_init_set_si(i1,op1->data.nval);
			mpz_init_set_si(i2,op2->data.nval);
			ret = mpz_legendre(i1,i2);
			mpz_clear(i1);
			mpz_clear(i2);
			break;
		}
		mpwl_clear_extra_type(op1,t);
		mpwl_clear_extra_type(op2,t);
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_NATIVEINT);
		rop->data.nval = ret;
	} else {
		(*errorout)(_("Can't get legendre symbols of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_kronecker (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	if G_LIKELY (op1->type<=MPW_INTEGER && op2->type<=MPW_INTEGER) {
		int t=mpwl_make_same_extra_type(op1,op2);
		int ret = 0;
		mpz_t i1,i2;

		switch(t) {
		case MPW_INTEGER:
			ret = mpz_kronecker(op1->data.ival,op2->data.ival);
			break;
		case MPW_NATIVEINT:
			mpz_init_set_si(i1,op1->data.nval);
			mpz_init_set_si(i2,op2->data.nval);
			ret = mpz_kronecker(i1,i2);
			mpz_clear(i1);
			mpz_clear(i2);
			break;
		}
		mpwl_clear_extra_type(op1,t);
		mpwl_clear_extra_type(op2,t);
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_NATIVEINT);
		rop->data.nval = ret;
	} else {
		(*errorout)(_("Can't get jacobi symbol with Kronecker extension of floats or rationals!"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

static void
mpwl_lucnum (MpwRealNum *rop, MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER && op->type!=MPW_NATIVEINT) {
		(*errorout)(_("Lucas must get an integer argument!"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}
	if(op->type==MPW_INTEGER) {
		if G_UNLIKELY (mpz_cmp_ui(op->data.ival,ULONG_MAX)>0) {
			(*errorout)(_("Number too large to compute lucas number!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
			(*errorout)(_("No such thing as negative lucas numbers!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpz_lucnum_ui (rop->data.ival, mpz_get_ui (op->data.ival));
	} else {
		if G_UNLIKELY (op->data.nval<0) {
			(*errorout)(_("No such thing as negative lucas numbers!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpz_lucnum_ui (rop->data.ival, mpz_get_ui (op->data.ival));
	}
}

static void
mpwl_nextprime (MpwRealNum *rop, MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER && op->type!=MPW_NATIVEINT) {
		(*errorout)(_("Cannot get next prime after non-integer!"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}
	if(op->type==MPW_INTEGER) {
		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpz_nextprime (rop->data.ival, op->data.ival);
	} else {
		mpz_t num;
		mpz_init (num);
		mpz_set_si (num, op->data.nval);
		if (rop->type != MPW_INTEGER) {
			mpwl_clear (rop);
			mpwl_init_type (rop, MPW_INTEGER);
		}
		mpz_nextprime (rop->data.ival, num);
		mpz_clear (num);
	}
}

static gboolean
mpwl_perfect_square(MpwRealNum *op)
{
	if (op->type == MPW_NATIVEINT) {
		int ret;
		mpz_t i;
		mpz_init_set_si(i,op->data.nval);
		ret = mpz_perfect_square_p(i);
		mpz_clear(i);
		return ret;
	} else if (op->type == MPW_INTEGER) {
		return mpz_perfect_square_p (op->data.ival);
	} else if (op->type == MPW_RATIONAL) {
		return mympq_perfect_square_p (op->data.rval);
	} else {
		(*errorout)(_("perfect_square: can't work on non-integers!"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static gboolean
mpwl_perfect_power (MpwRealNum *op)
{
	if(op->type==MPW_NATIVEINT) {
		int ret;
		mpz_t i;
		mpz_init_set_si(i,op->data.nval);
		ret = mpz_perfect_power_p(i);
		mpz_clear(i);
		return ret;
	} else if(op->type==MPW_INTEGER) {
		return mpz_perfect_power_p(op->data.ival);
	} else {
		(*errorout)(_("perfect_power: can't work on non-integers!"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static gboolean
mpwl_even_p (MpwRealNum *op)
{
	if (op->type == MPW_NATIVEINT) {
		if (op->data.nval & 0x1)
			return FALSE;
		else
			return TRUE;
	} else if(op->type == MPW_INTEGER) {
		return mpz_even_p (op->data.ival);
	} else {
		(*errorout)(_("even_p: can't work on non-integers!"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static gboolean
mpwl_odd_p (MpwRealNum *op)
{
	if (op->type == MPW_NATIVEINT) {
		if (op->data.nval & 0x1)
			return TRUE;
		else
			return FALSE;
	} else if(op->type == MPW_INTEGER) {
		return mpz_odd_p (op->data.ival);
	} else {
		(*errorout)(_("odd_p: can't work on non-integers!"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
}

static void
mpwl_neg(MpwRealNum *rop,MpwRealNum *op)
{
	if(rop->type!=op->type) {
		mpwl_clear(rop);
		mpwl_init_type(rop,op->type);
	}

	switch(op->type) {
	case MPW_FLOAT:
		mpf_neg(rop->data.fval,op->data.fval);
		break;
	case MPW_RATIONAL:
		mpq_neg(rop->data.rval,op->data.rval);
		break;
	case MPW_INTEGER:
		mpz_neg(rop->data.ival,op->data.ival);
		break;
	case MPW_NATIVEINT:
		rop->data.nval = -(op->data.nval);
		break;
	}
}

static void
mpwl_fac_ui(MpwRealNum *rop,unsigned int op)
{
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	mpz_fac_ui(rop->data.ival,op);
}

static void
mpwl_fac(MpwRealNum *rop,MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER && op->type!=MPW_NATIVEINT) {
		(*errorout)(_("Can't do factorials of rationals or floats!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if(op->type==MPW_INTEGER) {
		if G_UNLIKELY (mpz_cmp_ui(op->data.ival,ULONG_MAX)>0) {
			(*errorout)(_("Number too large to compute factorial!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
			(*errorout)(_("Can't do factorials of negative numbers!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		mpwl_fac_ui(rop,mpz_get_ui(op->data.ival));
	} else {
		if G_UNLIKELY (op->data.nval<0) {
			(*errorout)(_("Can't do factorials of negative numbers!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		mpwl_fac_ui(rop,op->data.nval);
	}
}

static void
mpwl_dblfac_ui(MpwRealNum *rop,unsigned int op)
{
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	mpz_set_ui (rop->data.ival, 1);
	if (op == 0)
		return;
	for (;;) {
		mpz_mul_ui (rop->data.ival, rop->data.ival, op);
		if (op <= 2)
			break;
		op -= 2;
	}
}

static void
mpwl_dblfac (MpwRealNum *rop,MpwRealNum *op)
{
	if G_UNLIKELY (op->type!=MPW_INTEGER && op->type!=MPW_NATIVEINT) {
		(*errorout)(_("Can't do factorials of rationals or floats!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if(op->type==MPW_INTEGER) {
		if G_UNLIKELY (mpz_cmp_ui(op->data.ival,ULONG_MAX)>0) {
			(*errorout)(_("Number too large to compute factorial!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		if G_UNLIKELY (mpz_sgn(op->data.ival)<0) {
			(*errorout)(_("Can't do factorials of negative numbers!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		mpwl_dblfac_ui(rop,mpz_get_ui(op->data.ival));
	} else {
		if G_UNLIKELY (op->data.nval<0) {
			(*errorout)(_("Can't do factorials of negative numbers!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		mpwl_dblfac_ui(rop,op->data.nval);
	}
}

static gboolean
mpwl_pow_q(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	mpf_t fr;
	mpf_t fr2;
	mpf_t frt;
	mpf_t de;
	mpz_t des;
	unsigned long int den;
	int t;
	gboolean reverse = FALSE;

	if G_UNLIKELY (op2->type!=MPW_RATIONAL) {
		error_num=INTERNAL_MPW_ERROR;
		return FALSE;
	}

	if G_UNLIKELY (mpwl_sgn (op1) < 0 &&
			mpz_even_p (mpq_denref(op2->data.rval))) {
		/*it's gonna be complex*/
		error_num=NUMERICAL_MPW_ERROR;
		return TRUE;
	}

	if (mpz_cmp_ui (mpq_denref (op2->data.rval), ULONG_MAX) <= 0 &&
	    op1->type <= MPW_RATIONAL) {
		den = mpz_get_ui (mpq_denref (op2->data.rval));
		/* We can do square root, perhaps symbolically */
		if (den == 2 || den == 4 || den == 8 || den == 16) {
			MpwRealNum n = {{NULL}};
			mpwl_init_type (&n, MPW_INTEGER);
			mpz_set (n.data.ival, mpq_numref (op2->data.rval));
			mpwl_sqrt (rop, op1);
			if (den > 2) {
				mpwl_sqrt (rop, rop);
				if (den > 4) {
					mpwl_sqrt (rop, rop);
					if (den > 8)
						mpwl_sqrt (rop, rop);
				}
			}
			mpwl_pow_z (rop, rop, &n);
			mpwl_clear (&n);

			return FALSE;
		} else if (op1->type <= MPW_INTEGER) {
			mpz_t z;
			mpwl_make_extra_type (op1, MPW_INTEGER);

			mpz_init (z);
			if (mpz_root (z, op1->data.ival, den) != 0) {
				mpwl_clear_extra_type (op1, MPW_INTEGER);

				mympz_pow_z (z, z,
					     mpq_numref (op2->data.rval));
				mpwl_clear (rop);
				mpwl_init_type (rop, MPW_INTEGER);
				mpz_set (rop->data.ival, z);
				mpz_clear (z);

				return FALSE;
			}
			mpz_clear (z);

			mpwl_clear_extra_type (op1, MPW_INTEGER);
		} else if (op1->type == MPW_RATIONAL) {
			mpz_t n;
			mpz_t d;
			mpz_init (n);
			mpz_init (d);
			if (mpz_root (n, mpq_numref (op1->data.rval),
				      den) != 0 &&
			    mpz_root (d, mpq_denref (op1->data.rval),
				      den) != 0) {
				mympz_pow_z (n, n,
					     mpq_numref (op2->data.rval));
				mympz_pow_z (d, d,
					     mpq_numref (op2->data.rval));
				mpwl_clear (rop);
				mpwl_init_type (rop, MPW_RATIONAL);
				mpz_set (mpq_numref (rop->data.rval), n);
				mpz_set (mpq_denref (rop->data.rval), d);
				mpq_canonicalize (rop->data.rval);
				mpz_clear (n);
				mpz_clear (d);

				return FALSE;
			}
			mpz_clear (d);
			mpz_clear (n);
		}
	}

	mpz_init_set(des,mpq_denref(op2->data.rval));
	mpz_sub_ui(des,des,1);

	mpf_init(de);
	mpf_set_z(de,mpq_denref(op2->data.rval));


	if(mpq_sgn(op2->data.rval)<0)
		reverse=TRUE;

	mpwl_make_extra_type(op1,MPW_FLOAT);
	t=MPW_FLOAT;


	/*
	 * Newton's method: Xn+1 = Xn - f(Xn)/f'(Xn)
	 */
	
	mpf_init(fr);
	mpf_init(fr2);
	mpf_init(frt);
	mpf_div_ui(fr,op1->data.fval,2); /*use half the value
					     as an initial guess*/
	for(;;) {
		mympf_pow_z(fr2,fr,mpq_denref(op2->data.rval));
		mpf_sub(fr2,fr2,op1->data.fval);

		mympf_pow_z(frt,fr,des);
		mpf_mul(frt,frt,de);
		mpf_div(fr2,fr2,frt);
		mpf_neg(fr2,fr2);
		mpf_add(fr2,fr2,fr);

		
		if(mpf_cmp(fr2,fr)==0)
			break;
		mpf_set(fr,fr2);
	}
	mpf_clear(fr2);
	mpf_clear(frt);
	mpz_clear(des);
	mpf_clear(de);

	if(reverse) {
		/*numerator will be negative*/
		mpz_neg(mpq_numref(op2->data.rval),mpq_numref(op2->data.rval));
		mympf_pow_z(fr,fr,mpq_numref(op2->data.rval));
		mpz_neg(mpq_numref(op2->data.rval),mpq_numref(op2->data.rval));

		mpf_ui_div(fr,1,fr);
	} else
		mympf_pow_z(fr,fr,mpq_numref(op2->data.rval));

	/*op1 might have equaled rop so clear extra type here*/
	mpwl_clear_extra_type(op1,t);

	mpwl_clear(rop);
	mpwl_init_type(rop,MPW_FLOAT);
	mpf_set(rop->data.fval,fr);
	return FALSE;
}

/*power to an unsigned long and optionaly invert the answer*/
static void
mpwl_pow_ui(MpwRealNum *rop,MpwRealNum *op1,unsigned int e, gboolean reverse)
{
	MpwRealNum r = {{NULL}};

	switch(op1->type) {
	case MPW_NATIVEINT:
		if(!reverse) {
			mpwl_init_type(&r,MPW_INTEGER);
			mpz_set_si(r.data.ival,op1->data.nval);
			mpz_pow_ui(r.data.ival, r.data.ival,e);
		} else {
			mpwl_init_type(&r,MPW_RATIONAL);
			mpz_set_si(mpq_denref(r.data.rval),op1->data.nval);
			mpz_pow_ui(mpq_denref(r.data.rval),
				   mpq_denref(r.data.rval),e);
			mpz_set_ui(mpq_numref(r.data.rval),1);
			mpwl_make_int(&r);
		}
		break;
	case MPW_INTEGER:
		if(!reverse) {
			mpwl_init_type(&r,MPW_INTEGER);
			mpz_pow_ui(r.data.ival,
				   op1->data.ival,e);
		} else {
			mpwl_init_type(&r,MPW_RATIONAL);
			mpz_pow_ui(mpq_denref(r.data.rval),
				   op1->data.ival,e);
			mpz_set_ui(mpq_numref(r.data.rval),1);
			mpwl_make_int(&r);
		}
		break;
	case MPW_RATIONAL:
		mpwl_init_type(&r,MPW_RATIONAL);
		mpz_pow_ui(mpq_numref(r.data.rval),
			   mpq_numref(op1->data.rval),e);
		mpz_pow_ui(mpq_denref(r.data.rval),
			   mpq_denref(op1->data.rval),e);
		/*the exponent was negative! reverse the result!*/
		if(reverse)
			mpq_inv(r.data.rval,r.data.rval);
		mpwl_make_int(&r);
		break;
	case MPW_FLOAT:
		mpwl_init_type(&r,MPW_FLOAT);
		mpf_pow_ui (r.data.fval, op1->data.fval, e);

		if(reverse)
			mpf_ui_div(r.data.fval,1,r.data.fval);
		break;
	}
	mpwl_move(rop,&r);
}

static void
mpwl_pow_z(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	gboolean reverse = FALSE;;
	if G_UNLIKELY (op2->type!=MPW_INTEGER) {
		error_num=INTERNAL_MPW_ERROR;
		return;
	}
	
	reverse = FALSE;
	if(mpz_sgn(op2->data.ival)<0) {
		reverse = TRUE;
		mpz_neg(op2->data.ival,op2->data.ival);
	}

	if(mpz_cmp_ui(op2->data.ival,ULONG_MAX)>0) {
		MpwRealNum r = {{NULL}};

		switch(op1->type) {
		case MPW_NATIVEINT:
			if(!reverse) {
				mpwl_init_type(&r,MPW_INTEGER);
				mpz_set_si(r.data.ival,op1->data.nval);
				mympz_pow_z(r.data.ival, r.data.ival,
					    op2->data.ival);
			} else {
				mpwl_init_type(&r,MPW_RATIONAL);
				mpz_set_si(mpq_denref(r.data.rval),op1->data.nval);
				mympz_pow_z(mpq_denref(r.data.rval),
					    mpq_denref(r.data.rval),
					    op2->data.ival);
				mpz_set_ui(mpq_numref(r.data.rval),1);
				mpwl_make_int(&r);
			}
			break;
		case MPW_INTEGER:
			if(!reverse) {
				mpwl_init_type(&r,MPW_INTEGER);
				mympz_pow_z(r.data.ival,
					    op1->data.ival,
					    op2->data.ival);
			} else {
				mpwl_init_type(&r,MPW_RATIONAL);
				mympz_pow_z(mpq_denref(r.data.rval),
					    op1->data.ival,
					    op2->data.ival);
				mpz_set_ui(mpq_numref(r.data.rval),1);
				mpwl_make_int(&r);
			}
			break;
		case MPW_RATIONAL:
			mpwl_init_type(&r,MPW_RATIONAL);
			mympz_pow_z(mpq_numref(r.data.rval),
				    mpq_numref(op1->data.rval),
				    op2->data.ival);
			mympz_pow_z(mpq_denref(r.data.rval),
				    mpq_denref(op1->data.rval),
				    op2->data.ival);
			/*the exponent was negative! reverse the result!*/
			if(reverse)
				mpq_inv(r.data.rval,r.data.rval);
			mpwl_make_int(&r);
			break;
		case MPW_FLOAT:
			mpwl_init_type(&r,MPW_FLOAT);
			mympf_pow_z(r.data.fval,op1->data.fval,
				    op2->data.ival);

			if(reverse)
				mpf_ui_div(r.data.fval,1,r.data.fval);
			break;
		}
		mpwl_move(rop,&r);
	} else {
		if(mpz_sgn(op2->data.ival)==0)
			mpwl_set_ui(rop,1);
		else 
			mpwl_pow_ui(rop,op1,mpz_get_ui(op2->data.ival),reverse);
	}

	if(reverse)
		mpz_neg(op2->data.ival,op2->data.ival);
}

static gboolean
mpwl_pow_f(MpwRealNum *rop,MpwRealNum *op1,MpwRealNum *op2)
{
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY (op2->type!=MPW_FLOAT) {
		error_num=INTERNAL_MPW_ERROR;
		return FALSE;
	}
	if(mpwl_sgn(op1)<=0)
		return TRUE;
	
	mpwl_make_extra_type(op1,MPW_FLOAT);
	
	mpwl_init_type(&r,MPW_FLOAT);

#ifdef HAVE_MPFR
	mpfr_pow (r.data.fval, op1->data.fval, op2->data.fval, GMP_RNDN);
#else
	mympf_ln(r.data.fval,op1->data.fval);
	mpf_mul(r.data.fval,r.data.fval,op2->data.fval);
	mympf_exp(r.data.fval,r.data.fval);
#endif

	mpwl_clear_extra_type(op1,MPW_FLOAT);

	mpwl_move(rop,&r);
	return FALSE;
}

static gboolean
mpwl_pow (MpwRealNum *rop, MpwRealNum *op1, MpwRealNum *op2)
{
	int sgn1 = mpwl_sgn(op1);
	int sgn2 = mpwl_sgn(op2);
	if (sgn2 == 0) {
		mpwl_set_ui(rop,1);
		return FALSE;
	} else if (sgn1 == 0) {
		mpwl_set_ui(rop,0);
		return FALSE;
	}

	switch(op2->type) {
	case MPW_NATIVEINT:
		if(op2->data.nval == 0)
			mpwl_set_ui(rop,1);
		else if(op2->data.nval>0)
			mpwl_pow_ui(rop,op1,op2->data.nval,FALSE);
		else
			mpwl_pow_ui(rop,op1,-(op2->data.nval),TRUE);
		break;
	case MPW_FLOAT: return mpwl_pow_f(rop,op1,op2);
	case MPW_RATIONAL: return mpwl_pow_q(rop,op1,op2);
	case MPW_INTEGER: mpwl_pow_z(rop,op1,op2); break;
	}
	return FALSE;
}

static void
mpwl_powm (MpwRealNum *rop,
	   MpwRealNum *op1,
	   MpwRealNum *op2,
	   MpwRealNum *mod)
{
	int sgn1, sgn2;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY ((op1->type != MPW_INTEGER && op1->type != MPW_NATIVEINT) ||
		       (op2->type != MPW_INTEGER && op2->type != MPW_NATIVEINT) ||
		       (mod->type != MPW_INTEGER && mod->type != MPW_NATIVEINT)) {
		(*errorout) (_("powm: Bad types for mod power"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	sgn1 = mpwl_sgn(op1);
	sgn2 = mpwl_sgn(op2);
	if (sgn2 == 0) {
		mpwl_set_ui(rop,1);
		return;
	} else if (sgn1 == 0) {
		mpwl_set_ui(rop,0);
		return;
	}

	mpwl_make_extra_type (mod, MPW_INTEGER);
	mpwl_make_extra_type (op1, MPW_INTEGER);

	mpwl_init_type (&r, MPW_INTEGER);

	switch(op2->type) {
	case MPW_NATIVEINT:
		if (op2->data.nval > 0) {
			mpz_powm_ui (r.data.ival,
				     op1->data.ival,
				     op2->data.nval,
				     mod->data.ival);
		} else {
			mpz_powm_ui (r.data.ival,
				     op1->data.ival,
				     -(op2->data.nval),
				     mod->data.ival);
			if G_UNLIKELY ( ! mpz_invert (r.data.ival,
						      r.data.ival,
						      mod->data.ival)) {
				(*errorout)(_("Can't invert in powm"));
				error_num = NUMERICAL_MPW_ERROR;
				mpwl_clear (&r);
				return;
			}
		}
		break;
	case MPW_INTEGER:
		if (sgn2 > 0) {
			mpz_powm (r.data.ival,
				     op1->data.ival,
				     op2->data.ival,
				     mod->data.ival);
		} else {
			mpz_neg (op2->data.ival, op2->data.ival);
			mpz_powm (r.data.ival,
				  op1->data.ival,
				  op2->data.ival,
				  mod->data.ival);
			mpz_neg (op2->data.ival, op2->data.ival);
			if G_UNLIKELY ( ! mpz_invert (r.data.ival,
						      r.data.ival,
						      mod->data.ival)) {
				(*errorout)(_("Can't invert in powm"));
				error_num = NUMERICAL_MPW_ERROR;
				mpwl_clear (&r);
				return;
			}
		}
		break;
	case MPW_FLOAT: 
	case MPW_RATIONAL:
		g_assert_not_reached ();
		break;
	}

	mpwl_clear_extra_type (op1, MPW_INTEGER);
	mpwl_clear_extra_type (mod, MPW_INTEGER);

	mpwl_move (rop, &r);
}

static void
mpwl_powm_ui (MpwRealNum *rop,
	      MpwRealNum *op,
	      unsigned long int e,
	      MpwRealNum *mod)
{
	int sgn;
	MpwRealNum r = {{NULL}};

	if G_UNLIKELY ((op->type != MPW_INTEGER && op->type != MPW_NATIVEINT) ||
		       (mod->type != MPW_INTEGER && mod->type != MPW_NATIVEINT)) {
		(*errorout) (_("powm: Bad types for mod power"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	sgn = mpwl_sgn (op);
	if (e == 0) {
		mpwl_set_ui (rop, 1);
		return;
	} else if (sgn == 0) {
		mpwl_set_ui (rop, 0);
		return;
	}

	mpwl_make_extra_type (mod, MPW_INTEGER);
	mpwl_make_extra_type (op, MPW_INTEGER);

	mpwl_init_type (&r, MPW_INTEGER);

	mpz_powm_ui (r.data.ival,
		     op->data.ival,
		     e,
		     mod->data.ival);

	mpwl_clear_extra_type (op, MPW_INTEGER);
	mpwl_clear_extra_type (mod, MPW_INTEGER);

	mpwl_move (rop, &r);
}

static gboolean
mpwl_sqrt (MpwRealNum *rop, MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};
	gboolean is_complex = FALSE;

	if (mpwl_sgn (op) < 0) {
		is_complex = TRUE;
		mpwl_neg (op, op);
	}
	if ((op->type == MPW_NATIVEINT ||
	     op->type == MPW_INTEGER) &&
	    mpwl_perfect_square (op)) {
		mpwl_init_type (&r, MPW_INTEGER);
		mpwl_make_extra_type (op, MPW_INTEGER);
		mpz_sqrt (r.data.ival, op->data.ival);
		mpwl_clear_extra_type (op, MPW_INTEGER);
	} else if (op->type == MPW_RATIONAL &&
		   mpwl_perfect_square (op)) {
		mpwl_init_type (&r, MPW_RATIONAL);
		mpz_sqrt (mpq_numref (r.data.rval), mpq_numref (op->data.rval));
		mpz_sqrt (mpq_denref (r.data.rval), mpq_denref (op->data.rval));
	} else if (op->type == MPW_NATIVEINT) {
		mpwl_init_type (&r, MPW_FLOAT);
		mpf_sqrt_ui (r.data.fval, op->data.nval);
	} else {
		mpwl_init_type (&r, MPW_FLOAT);
		mpwl_make_extra_type (op, MPW_FLOAT);
		mpf_sqrt (r.data.fval, op->data.fval);
		mpwl_clear_extra_type (op,MPW_FLOAT);
	}
	if (is_complex)
		mpwl_neg (op, op);

	mpwl_move (rop, &r);
	return is_complex;
}

static void
mpwl_exp(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_exp (r.data.fval, op->data.fval, GMP_RNDN);
#else
	mympf_exp(r.data.fval,op->data.fval);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}

static gboolean
mpwl_ln(MpwRealNum *rop,MpwRealNum *op)
{
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	if (mpfr_sgn (op->data.fval) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op->data.fval, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log (r.data.fval, op->data.fval, GMP_RNDN);
		ret = TRUE;
	}
#else
	ret = mympf_ln(r.data.fval,op->data.fval);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
	
	return ret;
}

static gboolean
mpwl_log2(MpwRealNum *rop,MpwRealNum *op)
{
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	if (mpfr_sgn (op->data.fval) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op->data.fval, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log2 (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log2 (r.data.fval, op->data.fval, GMP_RNDN);
		ret = TRUE;
	}
#else
	ret = mympf_log2 (r.data.fval,op->data.fval);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
	
	return ret;
}

static gboolean
mpwl_log10(MpwRealNum *rop,MpwRealNum *op)
{
	gboolean ret;
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	if (mpfr_sgn (op->data.fval) < 0) {
		mpfr_t f;
		mpfr_init_set (f, op->data.fval, GMP_RNDN);
		mpfr_neg (f, f, GMP_RNDN);
		mpfr_log10 (r.data.fval, f, GMP_RNDN);
		mpfr_clear (f);
		ret = FALSE;
	} else {
		mpfr_log10 (r.data.fval, op->data.fval, GMP_RNDN);
		ret = TRUE;
	}
#else
	ret = mympf_log10 (r.data.fval,op->data.fval);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
	
	return ret;
}

static void
mpwl_cos(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_cos (r.data.fval, op->data.fval, GMP_RNDN);
#else
	mympf_cos(r.data.fval,op->data.fval,FALSE,TRUE);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_sin(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_sin (r.data.fval, op->data.fval, GMP_RNDN);
#else
	mympf_sin(r.data.fval,op->data.fval,FALSE,TRUE);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_cosh(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_cosh (r.data.fval, op->data.fval, GMP_RNDN);
#else
	mympf_cos(r.data.fval,op->data.fval,TRUE,FALSE);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_sinh(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_sinh (r.data.fval, op->data.fval, GMP_RNDN);
#else
	mympf_sin(r.data.fval,op->data.fval,TRUE,FALSE);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}
static void
mpwl_arctan(MpwRealNum *rop,MpwRealNum *op)
{
	MpwRealNum r = {{NULL}};

	mpwl_init_type(&r,MPW_FLOAT);
	mpwl_make_extra_type(op,MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_atan (r.data.fval, op->data.fval, GMP_RNDN);
#else
	mympf_arctan(r.data.fval,op->data.fval);
#endif
	mpwl_clear_extra_type(op,MPW_FLOAT);

	mpwl_move(rop,&r);
}

static void
mpwl_pi (MpwRealNum *rop)
{
	mpwl_clear(rop);
	mpwl_init_type(rop,MPW_FLOAT);
	mympf_pi(rop->data.fval);
}

static void
mpwl_euler_constant (MpwRealNum *rop)
{
	mpwl_clear (rop);
	mpwl_init_type (rop, MPW_FLOAT);
#ifdef HAVE_MPFR
	mpfr_const_euler (rop->data.fval, GMP_RNDN);
#else
	mpf_set_str (rop->data.fval, euler_constant, 10);
#endif
}

/* Random state stuff: FIXME: this is evil */
static unsigned long randstate_seed = 0;

static void
mpwl_rand (MpwRealNum *rop)
{
	gmp_randstate_t rand_state;
	gmp_randinit_default (rand_state);
	randstate_seed += (unsigned long)time (NULL) * (unsigned long) rand ();
	gmp_randseed_ui (rand_state, randstate_seed);
	mpwl_clear (rop);
	mpwl_init_type (rop, MPW_FLOAT);
	mpf_urandomb (rop->data.fval, rand_state, default_mpf_prec);
	gmp_randclear (rand_state);
}

static void
mpwl_randint (MpwRealNum *rop, MpwRealNum *op)
{
	gmp_randstate_t rand_state;

	if G_UNLIKELY (op->type != MPW_NATIVEINT &&
		       op->type != MPW_INTEGER) {
		(*errorout)(_("Can't make random integer from a non-integer"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}
	if G_UNLIKELY (mpwl_sgn (op) <= 0) {
		(*errorout)(_("Range for random integer must be positive"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	gmp_randinit_default (rand_state);
	randstate_seed += (unsigned long)time (NULL) * (unsigned long) rand ();
	gmp_randseed_ui (rand_state, randstate_seed);

	mpwl_clear (rop);
	mpwl_init_type (rop, MPW_INTEGER);
	if (op->type == MPW_INTEGER) {
		mpz_urandomm (rop->data.ival, rand_state, op->data.ival);
	} else {
		mpz_t z;
		mpz_init (z);
		mpz_set_si (z, op->data.nval);
		mpz_urandomm (rop->data.ival, rand_state, z);
		mpz_clear (z);
	}
	gmp_randclear (rand_state);
}

static void
mpwl_make_int(MpwRealNum *rop)
{
	switch(rop->type) {
		case MPW_NATIVEINT:
		case MPW_INTEGER:
		case MPW_FLOAT: return;
		case MPW_RATIONAL:
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				if(!rop->data.ival)
					rop->data.ival = g_new(__mpz_struct,1);
				mpz_init_set(rop->data.ival,
					mpq_numref(rop->data.rval));
				mpq_clear(rop->data.rval);
				g_free(rop->data.rval);
				rop->data.rval = NULL;
				rop->type=MPW_INTEGER;
			}
			break;
	}

}

/*make number into a float, this might be neccessary for unprecise
  calculations*/
static void
mpwl_make_float(MpwRealNum *rop)
{
	mpwl_make_type(rop,MPW_FLOAT);
}

static void
mpwl_round(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER) {
		if(rop->type == MPW_FLOAT) {
			mpf_t tmp;
			mpf_init_set_d(tmp,0.5);
			if(mpf_sgn(rop->data.fval)<0)
				mpf_sub(rop->data.fval,rop->data.fval,tmp);
			else
				mpf_add(rop->data.fval,rop->data.fval,tmp);
			mpf_clear(tmp);
		} else /*MPW_RATIONAL*/ {
			mpq_t tmp;
			mpq_init(tmp);
			mpq_set_ui(tmp,1,2);
			if(mpq_sgn(rop->data.rval)<0)
				mpq_sub(rop->data.rval,rop->data.rval,tmp);
			else
				mpq_add(rop->data.rval,rop->data.rval,tmp);
			mpq_clear(tmp);
		}
		mpwl_make_type(rop,MPW_INTEGER);
	}
}

static void
mpwl_ceil(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER) {
		if(rop->type == MPW_FLOAT) {
			mpf_t fr;
			rop->type=MPW_INTEGER;
			if(!rop->data.ival)
				rop->data.ival = g_new(__mpz_struct,1);
			mpz_init(rop->data.ival);
			mpz_set_f(rop->data.ival,rop->data.fval);
			mpf_init(fr);
			mpf_set_z(fr,rop->data.ival);
			if(mpf_cmp(fr,rop->data.fval)!=0) {
				if(mpf_sgn(rop->data.fval)>0)
					mpz_add_ui(rop->data.ival,
						   rop->data.ival,1);
			}
			g_free(rop->data.fval);
			rop->data.fval = NULL;
			mpf_clear(fr);
		} else /*MPW_RATIONAL*/ {
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				rop->type=MPW_INTEGER;
				if(!rop->data.ival)
					rop->data.ival = g_new(__mpz_struct,1);
				mpz_init_set(rop->data.ival,
					     mpq_numref(rop->data.rval));
				mpq_clear(rop->data.rval);
				g_free(rop->data.rval);
				rop->data.rval = NULL;
			} else {
				if(mpq_sgn(rop->data.rval)>0) {
					mpwl_make_type(rop,MPW_INTEGER);
					mpz_add_ui(rop->data.ival,
						   rop->data.ival,1);
				} else 
					mpwl_make_type(rop,MPW_INTEGER);
			}
		}
	}
}

static void
mpwl_floor(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER) {
		if(rop->type == MPW_FLOAT) {
			mpf_t fr;
			rop->type=MPW_INTEGER;
			if(!rop->data.ival)
				rop->data.ival = g_new(__mpz_struct,1);
			mpz_init(rop->data.ival);
			mpz_set_f(rop->data.ival,rop->data.fval);
			mpf_init(fr);
			mpf_set_z(fr,rop->data.ival);
			if(mpf_cmp(fr,rop->data.fval)!=0) {
				if(mpf_sgn(rop->data.fval)<0)
					mpz_sub_ui(rop->data.ival,
						   rop->data.ival,1);
			}
			mpf_clear(rop->data.fval);
			g_free(rop->data.fval);
			rop->data.fval = NULL;
			mpf_clear(fr);
		} else /*MPW_RATIONAL*/ {
			mpq_canonicalize(rop->data.rval);
			if(mpz_cmp_ui(mpq_denref(rop->data.rval),1)==0) {
				rop->type=MPW_INTEGER;
				if(!rop->data.ival)
					rop->data.ival = g_new(__mpz_struct,1);
				mpz_init_set(rop->data.ival,
					     mpq_numref(rop->data.rval));
				mpq_clear(rop->data.rval);
				g_free(rop->data.rval);
				rop->data.rval = NULL;
			} else {
				if(mpq_sgn(rop->data.rval)<0) {
					mpwl_make_type(rop,MPW_INTEGER);
					mpz_sub_ui(rop->data.ival,
						   rop->data.ival,1);
				} else 
					mpwl_make_type(rop,MPW_INTEGER);
			}
		}
	}
}

static void
mpwl_trunc(MpwRealNum *rop)
{
	if(rop->type > MPW_INTEGER)
		mpwl_make_type(rop,MPW_INTEGER);
}

static void
mpwl_numerator(MpwRealNum *rop, MpwRealNum *op)
{
	if(op->type == MPW_INTEGER ||
	   op->type == MPW_NATIVEINT) {
		if(rop != op)
			mpwl_set(rop, op);
	} else if G_UNLIKELY (op->type == MPW_FLOAT) {
		(*errorout)(_("Can't get numerator of floating types"));
		error_num=NUMERICAL_MPW_ERROR;
	} else { /* must be rational */
		if(rop != op) {
			mpwl_clear(rop);
			mpwl_init_type(rop, MPW_INTEGER);
			mpz_set(rop->data.ival, mpq_numref(op->data.rval));
		} else {
			if(!rop->data.ival)
				rop->data.ival = g_new(__mpz_struct,1);
			mpz_init_set(rop->data.ival,
				     mpq_numref(rop->data.rval));
			mpq_clear(rop->data.rval);
			g_free(rop->data.rval);
			rop->data.rval = NULL;
			rop->type = MPW_INTEGER;
		}
	}
}

static void
mpwl_denominator(MpwRealNum *rop, MpwRealNum *op)
{
	if(op->type == MPW_INTEGER ||
	   op->type == MPW_NATIVEINT) {
		mpwl_set_si(rop, 1);
	} else if G_UNLIKELY (op->type == MPW_FLOAT) {
		(*errorout)(_("Can't get numerator of floating types"));
		error_num=NUMERICAL_MPW_ERROR;
	} else { /* must be rational */
		if(rop != op) {
			mpwl_clear(rop);
			mpwl_init_type(rop, MPW_INTEGER);
			mpz_set(rop->data.ival, mpq_denref(op->data.rval));
		} else {
			if(!rop->data.ival)
				rop->data.ival = g_new(__mpz_struct,1);
			mpz_init_set(rop->data.ival,
				     mpq_denref(rop->data.rval));
			mpq_clear(rop->data.rval);
			g_free(rop->data.rval);
			rop->data.rval = NULL;
			rop->type = MPW_INTEGER;
		}
	}
}

static void
mpwl_set_str_float(MpwRealNum *rop,const char *s,int base)
{
	char *old_locale = setlocale (LC_NUMERIC, NULL);
	if (old_locale != NULL &&
	    strcmp (old_locale, "C") != 0) {
		old_locale = g_strdup (old_locale);
		setlocale (LC_NUMERIC, "C");
	} else {
		old_locale = NULL;
	}

	if(rop->type!=MPW_FLOAT) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_FLOAT);
	}
	mpf_set_str(rop->data.fval,s,base);

	if (old_locale != NULL) {
		setlocale (LC_NUMERIC, old_locale);
		g_free (old_locale);
	}
}

static void
mpwl_set_str_int(MpwRealNum *rop,const char *s,int base)
{
	if(rop->type!=MPW_INTEGER) {
		mpwl_clear(rop);
		mpwl_init_type(rop,MPW_INTEGER);
	}
	mpz_set_str(rop->data.ival,s,base);
	if(mpz_cmp_si(rop->data.ival,LONG_MAX)>0 ||
	   mpz_cmp_si(rop->data.ival,LONG_MIN+1)<0)
		return;
	mpwl_make_type(rop,MPW_NATIVEINT);
}


/**************/
/*output stuff*/

/*get a long if possible*/
static long
mpwl_get_long (MpwRealNum *op, int *ex)
{
	if G_UNLIKELY (op->type > MPW_INTEGER) {
		*ex = MPWL_EXCEPTION_CONVERSION_ERROR;
		return 0;
	} else if(op->type == MPW_NATIVEINT) {
		return op->data.nval;
	} else { /*real integer*/
		if G_UNLIKELY (mpz_cmp_si (op->data.ival, LONG_MAX) > 0 ||
			       mpz_cmp_si (op->data.ival, LONG_MIN) < 0) {
			*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
			return 0;
		} else
			return mpz_get_si(op->data.ival);
	}

}

/*get a double if possible*/
static double
mpwl_get_double (MpwRealNum *op, int *ex)
{
	double d;

	mpwl_make_extra_type (op, MPW_FLOAT);

	if G_UNLIKELY (mpf_cmp_d (op->data.fval, DBL_MAX) > 0 ||
		       mpf_cmp_d (op->data.fval, -DBL_MAX) < 0) {
		*ex = MPWL_EXCEPTION_NUMBER_TOO_LARGE;
		return 0;
	}

	d = mpf_get_d (op->data.fval);

	mpwl_clear_extra_type (op, MPW_FLOAT);

	return d;
}

/*round off the number at some digits*/
static void
str_make_max_digits (char *s, int digits, long *exponent)
{
	int i;
	int sd=0; /*digit where the number starts*/

	if(s[0]=='-')
		sd=1;

	if(!s || digits<=0)
		return;

	digits+=sd;

	if(strlen(s)<=digits)
		return;

	if(s[digits]<'5') {
		s[digits]='\0';
		return;
	}
	s[digits]='\0';

	for(i=digits-1;i>=sd;i--) {
		if(s[i]<'9') {
			s[i]++;
			return;
		}
		s[i]='\0';
	}
	shiftstr (s, 1);
	s[sd]='1';
	/* if we add a digit in front increase exponent */
	(*exponent) ++;
}

/*trim trailing zeros*/
static void
str_trim_trailing_zeros(char *s)
{
	char *p,*pp;

	p = strrchr(s,'.');
	if(!p) return;
	for(pp=p+1;*pp;pp++) {
		if(*pp!='0')
			p = pp+1;
	}
	*p = '\0';
}

/*formats a floating point with mantissa in p and exponent in e*/
static char *
str_format_float(char *p,long int e,int scientific_notation)
{
	long int len;
	int i;
	if(((e-1)<-8 || (e-1)>8) || scientific_notation) {
		if(e!=0)
			p=my_realloc(p,strlen(p)+1,
				strlen(p)+1+((int)log10(abs(e))+2)+1);
		else
			p=my_realloc(p,strlen(p)+1,strlen(p)+3);
			
		if(p[0]=='-') {
			if(strlen(p)>2) {
				shiftstr(p+2,1);
				p[2]='.';
			}
		} else {
			if(strlen(p)>1) {
				shiftstr(p+1,1);
				p[1]='.';
			}
		}
		str_trim_trailing_zeros(p);
		/* look above to see why this is one sprintf which is in
		   fact safe */
		sprintf(p,"%se%ld",p,e-1);
	} else if(e>0) {
		len=strlen(p);
		if(p[0]=='-')
			len--;
		if(e>len) {
			p=my_realloc(p,strlen(p)+1,
				strlen(p)+1+e-len);
			for(i=0;i<e-len;i++)
				strcat(p,"0");
		} else if(e<len) {
			if(p[0]=='-') {
				shiftstr(p+1+e,1);
				p[e+1]='.';
			} else {
				shiftstr(p+e,1);
				p[e]='.';
			}
		}
		str_trim_trailing_zeros(p);
	} else { /*e<=0*/
		if(strlen(p)==0) {
			p=g_strdup("0");
		} else {
			p=my_realloc(p,strlen(p)+1,
				strlen(p)+1+(-e)+2);
			if(p[0]=='-') {
				shiftstr(p+1,2+(-e));
				p[1]='0';
				p[2]='.';
				for(i=0;i<(-e);i++)
					p[i+3]='0';
			} else {
				shiftstr(p,2+(-e));
				p[0]='0';
				p[1]='.';
				for(i=0;i<(-e);i++)
					p[i+2]='0';
			}
		}
		str_trim_trailing_zeros(p);
	}
	return p;
}

static char *
str_getstring_n(long num, int max_digits,int scientific_notation,
	       	int integer_output_base, const char *postfix)
{
	char *p,*p2;
	mpf_t fr;

	g_return_val_if_fail(integer_output_base>=2,NULL);

	if(integer_output_base == 10) {
		p = g_strdup_printf("%ld",num);
	} else if(integer_output_base == 8) {
		p = g_strdup_printf("%s0%lo",num<0?"-":"",num<0?-num:num);
	} else if(integer_output_base == 16) {
		p = g_strdup_printf("%s0x%lx",num<0?"-":"",num<0?-num:num);
	} else {
		GString *gs = g_string_new("");
		long x;
		x = num;
		if(num<0) x=-x;
		while(x>0) {
			int digit = x % integer_output_base;
			x = x / integer_output_base;
			if(digit<=9)
				g_string_prepend_c(gs,'0'+digit);
			else
				g_string_prepend_c(gs,'a'+(digit-10));
		}
		if(num<0)
			g_string_prepend_c(gs,'-');
		else if(num == 0)
			/* if num is 0 then we haven't added any digits yet */
			g_string_prepend_c(gs,'0');
		p = g_strdup_printf("%d\\%s",integer_output_base,gs->str);
		g_string_free(gs,TRUE);
	}
	if(max_digits>0 && max_digits<strlen(p)) {
		mpf_init(fr);
		mpf_set_si(fr,num);
		p2=str_getstring_f(fr,max_digits,scientific_notation,
				   postfix);
		mpf_clear(fr);
		if(strlen(p2)>=strlen(p)) {
			g_free(p2);
			return p;
		} else  {
			g_free(p);
			return p2;
		}
	}
	p=appendstr(p,postfix);
	return p;
}

static char *
str_getstring_z(mpz_t num, int max_digits,int scientific_notation,
		int integer_output_base, const char *postfix)
{
	char *p,*p2;
	mpf_t fr;

	p=mpz_get_str(NULL,integer_output_base,num);
	if(integer_output_base==16) {
		p2 = g_strconcat("0x",p,NULL);
		g_free(p);
		p = p2;
	} else if(integer_output_base==8) {
		p2 = g_strconcat("0",p,NULL);
		g_free(p);
		p = p2;
	} else if(integer_output_base!=10) {
		p2 = g_strdup_printf("%d\\%s",integer_output_base,p);
		g_free(p);
		p = p2;
	}
	if(max_digits>0 && max_digits<strlen(p)) {
		mpf_init(fr);
		mpf_set_z(fr,num);
		p2=str_getstring_f(fr,max_digits,scientific_notation,postfix);
		mpf_clear(fr);
		if(strlen(p2)>=strlen(p)) {
			g_free(p2);
			return p;
		} else  {
			g_free(p);
			return p2;
		}
	}
	p=appendstr(p,postfix);
	return p;
}

static char *
get_frac (mpz_t num,
	  mpz_t den,
	  GelOutputStyle style,
	  const char *postfix,
	  int *dig)
{
	char *p, *p2;
	int digits = 0;

	if (style == GEL_OUTPUT_LATEX) {
		int l;
		p=mpz_get_str(NULL,10,num);
		digits = strlen(p);
		p=prependstr(p,"\\frac{");
		p=appendstr(p,"}{");
		p2=mpz_get_str(NULL,10,den);
		p=appendstr(p,p2);
		l = strlen (p2);
		if (l > digits)
			digits = l;
		g_free(p2);
		p=appendstr(p,"}");
		p=appendstr(p,postfix);
	} else if (style == GEL_OUTPUT_TROFF) {
		int l;
		p=mpz_get_str(NULL,10,num);
		digits = strlen(p);
		p=prependstr(p,"{");
		p=appendstr(p,"} over {");
		p2=mpz_get_str(NULL,10,den);
		p=appendstr(p,p2);
		l = strlen (p2);
		if (l > digits)
			digits = l;
		g_free(p2);
		p=appendstr(p,"}");
		p=appendstr(p,postfix);
	} else {
		p=mpz_get_str(NULL,10,num);
		p=appendstr(p,postfix);
		p=appendstr(p,"/");
		p2=mpz_get_str(NULL,10,den);
		p=appendstr(p,p2);
		g_free(p2);
		digits = strlen(p) - 1; /* don't count the / */
		digits -= strlen (postfix); /* don't count the i */
	}
	
	*dig += digits;

	return p;
}	

static char *
str_getstring_q(mpq_t num,
		int max_digits,
		int scientific_notation,
		int mixed_fractions,
		GelOutputStyle style,
		const char *postfix)
{
	char *p,*p2;
	mpf_t fr;
	int digits = 0;
	
	if(mixed_fractions) {
		if(mpq_sgn(num)>0) {
			if(mpz_cmp(mpq_numref(num),mpq_denref(num))<0)
				mixed_fractions = FALSE;
		} else {
			mpz_neg(mpq_numref(num),mpq_numref(num));
			if(mpz_cmp(mpq_numref(num),mpq_denref(num))<0)
				mixed_fractions = FALSE;
			mpz_neg(mpq_numref(num),mpq_numref(num));
		}
	}

	if(!mixed_fractions) {
		p = get_frac (mpq_numref (num), mpq_denref (num),
			      style, postfix, &digits);
	} else {
		int d;
		mpz_t tmp1,tmp2;
		mpz_init(tmp1);
		mpz_init(tmp2);
		mpz_tdiv_qr(tmp1,tmp2,mpq_numref(num),mpq_denref(num));
		if(mpz_sgn(tmp2)<0)
			mpz_neg(tmp2,tmp2);
		p=mpz_get_str(NULL,10,tmp1);
		digits = strlen (p);

		if (postfix != NULL &&
		    *postfix != '\0') {
			if (style == GEL_OUTPUT_LATEX)
				p = prependstr (p, "\\left(");
			else if (style == GEL_OUTPUT_TROFF)
				p = prependstr (p, " left ( ");
			else
				p = prependstr (p, "(");
		}

		p=appendstr(p," ");

		p2 = get_frac (tmp2, mpq_denref (num),
			       style, "", &d);
		p=appendstr(p,p2);
		g_free(p2);

		if (postfix != NULL &&
		    *postfix != '\0') {
			if (style == GEL_OUTPUT_LATEX)
				p = appendstr (p, "\\right)");
			else if (style == GEL_OUTPUT_TROFF)
				p = appendstr (p, " right )~");
			else
				p = appendstr (p, ")");
			p = appendstr (p, postfix);
		}
	}
	if (max_digits > 0 && max_digits < digits) {
		mpf_init(fr);
		mpf_set_q(fr,num);
		p2=str_getstring_f(fr,max_digits,scientific_notation,
				   postfix);
		mpf_clear(fr);
		if(strlen(p2)>=strlen(p)) {
			g_free(p2);
			return p;
		} else  {
			g_free(p);
			return p2;
		}
	}
	return p;
}

static char *
str_getstring_f(mpf_t num, int max_digits,int scientific_notation,
		const char *postfix)
{
	char *p;
	long e;

	p=mpf_get_str(NULL,&e,10,0,num);
	str_make_max_digits (p, max_digits, &e);
	p=str_format_float(p,e,scientific_notation);
	p=appendstr(p,postfix);

	return p;
}

static char *
mpwl_getstring(MpwRealNum * num, int max_digits,
	       gboolean scientific_notation,
	       gboolean results_as_floats,
	       gboolean mixed_fractions,
	       GelOutputStyle style,
	       int integer_output_base,
	       const char *postfix)
{
	mpf_t fr;
	char *p;
	switch(num->type) {
	case MPW_RATIONAL:
		if(results_as_floats) {
			mpf_init(fr);
			mpf_set_q(fr,num->data.rval);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation, postfix);
			mpf_clear(fr);
			return p;
		}
		return str_getstring_q(num->data.rval,
				       max_digits,
				       scientific_notation,
				       mixed_fractions,
				       style,
				       postfix);
	case MPW_NATIVEINT:
		if(results_as_floats) {
			mpf_init(fr);
			mpf_set_si(fr,num->data.nval);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation,
					  postfix);
			mpf_clear(fr);
			return p;
		}
		return str_getstring_n(num->data.nval,max_digits,
				       scientific_notation,
				       integer_output_base,
				       postfix);
	case MPW_INTEGER:
		if(results_as_floats) {
			mpf_init(fr);
			mpf_set_z(fr,num->data.ival);
			p=str_getstring_f(fr,max_digits,
					  scientific_notation,
					  postfix);
			mpf_clear(fr);
			return p;
		}
		return str_getstring_z(num->data.ival,max_digits,
				       scientific_notation,
				       integer_output_base,
				       postfix);
	case MPW_FLOAT:
		return str_getstring_f(num->data.fval,max_digits,
				       scientific_notation,
				       postfix);
	}
	/*something bad happened*/
	return NULL;
}

#define mpw_uncomplex(rop)					\
{								\
	if(mpwl_sgn(rop->i)==0) {				\
		rop->type=MPW_REAL;				\
		if(rop->i->type!=MPW_NATIVEINT) {		\
			mpwl_clear(rop->i);			\
			mpwl_init_type(rop->i,MPW_NATIVEINT);	\
		}						\
	}							\
}

/*************************************************************************/
/*high level stuff                                                       */
/*************************************************************************/

/*set default precision*/
void
mpw_set_default_prec(unsigned long int i)
{
	mpf_set_default_prec(i);
	if(pi_mpf) {
		mpf_clear(pi_mpf);
		g_free(pi_mpf);
		pi_mpf = NULL;
	}
	default_mpf_prec = i;
}

/*initialize a number*/
void
mpw_init (mpw_ptr op)
{
	op->type=MPW_REAL;
	op->r = zero;
	zero->alloc.usage++;
	op->i = zero;
	zero->alloc.usage++;
}

void
mpw_init_set(mpw_ptr rop,mpw_ptr op)
{
	rop->type=op->type;
	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
	mpw_uncomplex(rop);
}

/*clear memory held by number*/
void
mpw_clear(mpw_ptr op)
{
	op->r->alloc.usage--;
	op->i->alloc.usage--;
	if(op->r->alloc.usage==0)
		mpwl_free(op->r,FALSE);
	if(op->i->alloc.usage==0)
		mpwl_free(op->i,FALSE);
	op->type=0;
}

/*make them the same type without loosing information*/
void
mpw_make_same_type(mpw_ptr op1,mpw_ptr op2)
{
	MAKE_COPY(op1->r);
	MAKE_COPY(op2->r);
	mpwl_make_same_type(op1->r,op2->r);
	if(op1->type==MPW_COMPLEX || op2->type==MPW_COMPLEX) {
		MAKE_COPY(op1->i);
		MAKE_COPY(op2->i);
		mpwl_make_same_type(op1->i,op2->i);
	}
}

void
mpw_set(mpw_ptr rop,mpw_ptr op)
{
	rop->type=op->type;
	rop->r = op->r;
	rop->r->alloc.usage++;
	rop->i = op->i;
	rop->i->alloc.usage++;
	mpw_uncomplex(rop);
}

void
mpw_set_d(mpw_ptr rop,double d)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_set_d(rop->r,d);
}

void
mpw_set_si(mpw_ptr rop,signed long int i)
{
	MAKE_REAL(rop);
	if(i==0) {
		if(rop->r != zero) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = zero;
			zero->alloc.usage++;
		}
	} else if(i==1) {
		if(rop->r != one) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = one;
			one->alloc.usage++;
		}
	} else {
		MAKE_COPY(rop->r);
		mpwl_set_si(rop->r,i);
	}
}

void
mpw_set_ui(mpw_ptr rop,unsigned long int i)
{
	MAKE_REAL(rop);
	if(i==0) {
		if(rop->r != zero) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = zero;
			zero->alloc.usage++;
		}
	} else if(i==1) {
		if(rop->r != one) {
			rop->r->alloc.usage--;
			if(rop->r->alloc.usage==0)
				mpwl_free(rop->r,FALSE);
			rop->r = one;
			one->alloc.usage++;
		}
	} else {
		MAKE_COPY(rop->r);
		mpwl_set_ui(rop->r,i);
	}
}

void
mpw_set_mpz_use (mpw_ptr rop, mpz_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r,FALSE);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_INTEGER;
	rop->r->alloc.usage = 1;
	rop->r->data.ival = g_new (__mpz_struct, 1);
	memcpy (rop->r->data.ival, op, sizeof (__mpz_struct));
}

void
mpw_set_mpq_use (mpw_ptr rop, mpq_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r,FALSE);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_RATIONAL;
	rop->r->alloc.usage = 1;
	rop->r->data.rval = g_new (__mpq_struct, 1);
	memcpy (rop->r->data.rval, op, sizeof (__mpq_struct));
}

void
mpw_set_mpf_use (mpw_ptr rop, mpf_ptr op)
{
	MAKE_REAL(rop);
	rop->r->alloc.usage--;
	if(rop->r->alloc.usage==0)
		mpwl_free(rop->r,FALSE);
	GET_NEW_REAL (rop->r);
	rop->r->type = MPW_FLOAT;
	rop->r->alloc.usage = 1;
	rop->r->data.fval = g_new (__mpf_struct, 1);
	memcpy (rop->r->data.fval, op, sizeof (__mpf_struct));
}

mpz_ptr
mpw_peek_real_mpz (mpw_ptr op)
{
	if (op->r->type == MPW_INTEGER)
		return op->r->data.ival;
	else
		return NULL;
}

mpq_ptr
mpw_peek_real_mpq (mpw_ptr op)
{
	if (op->r->type == MPW_RATIONAL)
		return op->r->data.rval;
	else
		return NULL;
}

mpf_ptr
mpw_peek_real_mpf (mpw_ptr op)
{
	if (op->r->type == MPW_FLOAT)
		return op->r->data.fval;
	else
		return NULL;
}

mpz_ptr
mpw_peek_imag_mpz (mpw_ptr op)
{
	if (op->type == MPW_COMPLEX &&
	    op->i->type == MPW_INTEGER)
		return op->i->data.ival;
	else
		return NULL;
}

mpq_ptr
mpw_peek_imag_mpq (mpw_ptr op)
{
	if (op->type == MPW_COMPLEX &&
	    op->i->type == MPW_RATIONAL)
		return op->i->data.rval;
	else
		return NULL;
}

mpf_ptr
mpw_peek_imag_mpf (mpw_ptr op)
{
	if (op->type == MPW_COMPLEX &&
	    op->i->type == MPW_FLOAT)
		return op->i->data.fval;
	else
		return NULL;
}

int
mpw_sgn(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_sgn(op->r);
	} else {
		(*errorout)(_("Can't compare complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
	}
	return 0;
}

void
mpw_abs(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if(mpwl_sgn(op->r)<0)
			mpw_neg(rop,op);
		else
			mpw_set(rop,op);
	} else {
		MpwRealNum t = {{NULL}};

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		
		mpwl_init_type(&t,MPW_NATIVEINT);
		
		mpwl_mul(rop->r,op->r,op->r);
		mpwl_mul(&t,op->i,op->i);
		mpwl_add(rop->r,rop->r,&t);
		
		mpwl_free(&t,TRUE);

		mpwl_sqrt(rop->r,rop->r);
	}
}

void
mpw_neg(mpw_ptr rop,mpw_ptr op)
{
	MAKE_COPY(rop->r);
	mpwl_neg(rop->r,op->r);
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
	} else {
		MAKE_COPY(rop->i);
		mpwl_neg(rop->i,op->i);
	}
	rop->type = op->type;
}

void
mpw_add(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_add(rop->r,op1->r,op2->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_add(rop->r,op1->r,op2->r);
		mpwl_add(rop->i,op1->i,op2->i);

		mpw_uncomplex(rop);
	}
}

void
mpw_add_ui(mpw_ptr rop,mpw_ptr op, unsigned long i)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_add_ui(rop->r,op->r,i);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_add_ui(rop->r,op->r,i);
		mpwl_set(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_sub(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sub(rop->r,op1->r,op2->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_sub(rop->r,op1->r,op2->r);
		mpwl_sub(rop->i,op1->i,op2->i);

		mpw_uncomplex(rop);
	}
}

void
mpw_sub_ui(mpw_ptr rop,mpw_ptr op, unsigned long i)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sub_ui(rop->r,op->r,i);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_sub_ui(rop->r,op->r,i);
		mpwl_set(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_ui_sub(mpw_ptr rop,unsigned long i, mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_ui_sub(rop->r,i,op->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		rop->type = MPW_COMPLEX;
		mpwl_ui_sub(rop->r,i,op->r);
		mpwl_neg(rop->i,op->i);

		/* it shouldn't need uncomplexing*/
		/* mpw_uncomplex(rop);*/
	}
}

void
mpw_mul(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mul(rop->r,op1->r,op2->r);
	} else {
		MpwRealNum tr = {{NULL}};
		MpwRealNum ti = {{NULL}};
		MpwRealNum *r1;
		MpwRealNum *i1;
		MpwRealNum *r2;
		MpwRealNum *i2;
		
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op1,r1,i1);
		MAKE_CPLX_OPS(op2,r2,i2);

		mpwl_mul(rop->r,r1,r2);
		mpwl_mul(rop->i,i1,r2);

		mpwl_init_type(&tr,i1->type);
		mpwl_init_type(&ti,r1->type);

		/* tmp->type = MPW_COMPLEX; */
		mpwl_mul(&tr,i1,i2);
		mpwl_neg(&tr,&tr);
		mpwl_mul(&ti,r1,i2);
		
		mpwl_add(rop->r,rop->r,&tr);
		mpwl_add(rop->i,rop->i,&ti);

		mpwl_free(&tr,TRUE);
		mpwl_free(&ti,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op1,r1,i1);
		BREAK_CPLX_OPS(op2,r2,i2);
	}
}

void
mpw_mul_ui(mpw_ptr rop,mpw_ptr op, unsigned int i)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mul_ui(rop->r,op->r,i);
	} else {
		rop->type = MPW_COMPLEX;
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		mpwl_mul_ui(rop->r,op->r,i);
		mpwl_mul_ui(rop->i,op->i,i);

		mpw_uncomplex(rop);
	}
}

void
mpw_div(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op2->r)==0) {
			(*errorout)(_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_div(rop->r,op1->r,op2->r);
	} else {
		MpwRealNum t1 = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		MpwRealNum *r1;
		MpwRealNum *i1;
		MpwRealNum *r2;
		MpwRealNum *i2;
		if G_UNLIKELY (mpwl_sgn(op2->r)==0 && mpwl_sgn(op2->i)==0) {
			(*errorout)(_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op1,r1,i1);
		MAKE_CPLX_OPS(op2,r2,i2);

		mpwl_init_type(&t1,MPW_INTEGER);
		mpwl_init_type(&t2,MPW_INTEGER);

		/*real part (r1r2 + i1i2)/(r2r2 + i2i2)*/
		mpwl_mul(rop->r,r1,r2);
		mpwl_mul(&t1,i1,i2);
		mpwl_add(rop->r,rop->r,&t1);

		mpwl_mul(&t1,r2,r2);
		mpwl_mul(&t2,i2,i2);
		mpwl_add(&t2,&t2,&t1);

		mpwl_div(rop->r,rop->r,&t2);


		/*imaginary part (i1r2 - r1i2)/(r2r2 + i2i2)*/
		mpwl_mul(rop->i,i1,r2);
		mpwl_mul(&t1,r1,i2);
		mpwl_neg(&t1,&t1);
		mpwl_add(rop->i,rop->i,&t1);

		/*t2 is calculated above*/

		mpwl_div(rop->i,rop->i,&t2);

		mpwl_free(&t1,TRUE);
		mpwl_free(&t2,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op1,r1,i1);
		BREAK_CPLX_OPS(op2,r2,i2);
	}
}

void
mpw_div_ui(mpw_ptr rop,mpw_ptr op, unsigned int i)
{
	if G_UNLIKELY (i==0) {
		(*errorout)(_("Division by zero!"));
		error_num=NUMERICAL_MPW_ERROR;
		return;
	}
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_div_ui(rop->r,op->r,i);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		mpwl_div_ui(rop->r,op->r,i);
		mpwl_div_ui(rop->i,op->i,i);

		mpw_uncomplex(rop);
	}
}

void
mpw_ui_div(mpw_ptr rop,unsigned int in,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			(*errorout)(_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_ui_div(rop->r,in,op->r);
	} else {
		MpwRealNum t1 = {{NULL}};
		MpwRealNum t2 = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		if G_UNLIKELY (mpwl_sgn(op->r)==0 && mpwl_sgn(op->i)==0) {
			(*errorout)(_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t1,MPW_NATIVEINT);
		mpwl_init_type(&t2,MPW_NATIVEINT);

		/*real part (r1r2)/(r2r2 + i2i2)*/
		mpwl_mul_ui(rop->r,r,in);

		mpwl_mul(&t1,r,r);
		mpwl_mul(&t2,i,i);
		mpwl_add(&t2,&t2,&t1);

		mpwl_div(rop->r,rop->r,&t2);

		/*imaginary part (- r1i2)/(r2r2 + i2i2)*/
		mpwl_mul_ui(rop->i,i,in);
		mpwl_neg(rop->i,rop->i);

		/*t2 is calculated above*/

		mpwl_div(rop->i,rop->i,&t2);

		mpwl_free(&t1,TRUE);
		mpwl_free(&t2,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_mod (mpw_ptr rop, mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op2->r)==0) {
			(*errorout)(_("Division by zero!"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mod(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't modulo complex numbers"));
	}
}

void
mpw_invert (mpw_ptr rop, mpw_ptr op1, mpw_ptr mod)
{
	if  G_LIKELY (op1->type == MPW_REAL && mod->type == MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		if G_UNLIKELY ( ! mpwl_invert (rop->r, op1->r, mod->r)) {
			error_num = NUMERICAL_MPW_ERROR;
			/* FIXME: give the numbers */
			(*errorout)(_("No modulo inverse found!"));
		}
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't do modulo invert on complex numbers"));
	}
}

void
mpw_gcd(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_gcd(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't GCD complex numbers"));
	}
}
void
mpw_lcm (mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MpwRealNum gcd = {{NULL}};
		mpwl_init_type (&gcd, MPW_NATIVEINT);

		mpwl_gcd (&gcd, op1->r, op2->r);
		if G_UNLIKELY (error_num == NUMERICAL_MPW_ERROR)
			return;

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_mul (rop->r, op1->r, op2->r);
		mpwl_div (rop->r, rop->r, &gcd);
		mpwl_clear (&gcd);
		if (mpwl_sgn (rop->r) < 0)
			mpwl_neg (rop->r, rop->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't LCM complex numbers"));
	}
}

void
mpw_jacobi(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_jacobi(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't get jacobi symbols of complex numbers"));
	}
}
void
mpw_legendre(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_legendre(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't get legendre symbols complex numbers"));
	}
}
void
mpw_kronecker(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_kronecker(rop->r,op1->r,op2->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("Can't get jacobi symbol with Kronecker extension for complex numbers"));
	}
}
void
mpw_lucnum (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (op->type == MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		mpwl_lucnum (rop->r, op->r);
	} else {
		error_num = NUMERICAL_MPW_ERROR;
		(*errorout) (_("Can't get lucas number for complex numbers"));
	}
}
void
mpw_nextprime (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (op->type == MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		mpwl_nextprime (rop->r, op->r);
	} else {
		error_num = NUMERICAL_MPW_ERROR;
		(*errorout) (_("Can't get next prime for complex numbers"));
	}
}
gboolean
mpw_perfect_square(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_perfect_square(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("perfect_square: can't work on complex numbers"));
		return FALSE;
	}
}
gboolean
mpw_perfect_power(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_perfect_power(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("perfect_power: can't work on complex numbers"));
		return FALSE;
	}
}
gboolean
mpw_even_p(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_even_p(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("even_p: can't work on complex numbers"));
		return FALSE;
	}
}
gboolean
mpw_odd_p(mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_odd_p(op->r);
	} else {
		error_num=NUMERICAL_MPW_ERROR;
		(*errorout)(_("odd_p: can't work on complex numbers"));
		return FALSE;
	}
}

void
mpw_pow(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2)
{
	if(op1->type==MPW_REAL && op2->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(mpwl_pow(rop->r,op1->r,op2->r)) {
			mpw_t tmp;
			mpw_init(tmp);
			mpw_ln(tmp,op1);
			mpw_mul(tmp,tmp,op2);
			mpw_exp(tmp,tmp);
			mpw_set(rop,tmp);
			mpw_clear(tmp);
		}
	} else {
		mpw_t tmp;
		mpw_init(tmp);
		mpw_ln(tmp,op1);
		mpw_mul(tmp,tmp,op2);
		mpw_exp(tmp,tmp);
		mpw_set(rop,tmp);
		mpw_clear(tmp);
	}
}

void
mpw_powm(mpw_ptr rop,mpw_ptr op1, mpw_ptr op2, mpw_ptr mod)
{
	if G_UNLIKELY (op1->type != MPW_REAL ||
		       op2->type != MPW_REAL ||
		       mod->type != MPW_REAL) {
		(*errorout) (_("powm: Bad types for mod power"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	MAKE_REAL (rop);
	MAKE_COPY (rop->r);

	mpwl_powm (rop->r, op1->r, op2->r, mod->r);
}

void
mpw_powm_ui (mpw_ptr rop,mpw_ptr op, unsigned long int e, mpw_ptr mod)
{
	if G_UNLIKELY (op->type != MPW_REAL ||
		       mod->type != MPW_REAL) {
		(*errorout) (_("powm: Bad types for mod power"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}

	MAKE_REAL (rop);
	MAKE_COPY (rop->r);

	mpwl_powm_ui (rop->r, op->r, e, mod->r);
}

void
mpw_sqrt(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(mpwl_sqrt(rop->r,op->r)) {
			MpwRealNum *t;
			t = rop->r;
			rop->r = rop->i;
			rop->i = t;
			rop->type=MPW_COMPLEX;
		}
	} else {
		mpw_ln(rop,op);
		mpw_div_ui(rop,rop,2);
		mpw_exp(rop,rop);
	}
}

void
mpw_exp(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_exp(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_exp(rop->r,r);
		mpwl_set(rop->i,rop->r);
		
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sin(&t,i);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_ln(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			(*errorout)(_("ln: can't take logarithm of 0"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(!mpwl_ln(rop->r,op->r)) {
			rop->type = MPW_COMPLEX;
			MAKE_COPY(rop->i);
			mpwl_pi(rop->i);
		}
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		if(mpwl_sgn(op->r)==0) {
			g_assert(mpwl_sgn(op->i)!=0);
			/*don't set the pi before the ln, for the case
			  rop==op*/
			if(mpwl_ln(rop->r,op->i)) {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
			} else {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
				mpwl_neg(rop->i,rop->i);
			}
			return;
		}

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_mul(rop->r,r,r);
		mpwl_mul(&t,i,i);
		mpwl_add(rop->r,rop->r,&t);
		mpwl_ln(rop->r,rop->r);
		mpwl_div_ui(rop->r,rop->r,2);

		mpwl_div(rop->i,i,r);
		mpwl_arctan(rop->i,rop->i);

		if(mpwl_sgn(r)<0) {
			mpwl_pi(&t);
			if(mpwl_sgn(i)<0)
				mpwl_sub(rop->i,rop->i,&t);
			else
				mpwl_add(rop->i,rop->i,&t);
		}

		mpw_uncomplex(rop);

		mpwl_free(&t,TRUE);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_log2(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			(*errorout)(_("log2: can't take logarithm of 0"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(!mpwl_log2(rop->r,op->r)) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;
			MAKE_COPY(rop->i);
			mpwl_pi (rop->i);
			/* FIXME: implement caching or use the const log2
			   function from mpfr */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_ui (&t, 2);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
		}
	} else {
		mpw_t two;
		if (mpwl_sgn(op->r)==0) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;

			MAKE_COPY(rop->r);
			MAKE_COPY(rop->i);

			g_assert(mpwl_sgn(op->i)!=0);
			/*don't set the pi before the ln, for the case
			  rop==op*/
			if(mpwl_log2(rop->r,op->i)) {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
			} else {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
				mpwl_neg(rop->i,rop->i);
			}
			/* FIXME: implement caching or use the const log2
			   function from mpfr */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_ui (&t, 2);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
			return;
		}
		/* this is stupid, but simple to implement for now */
		mpw_init (two);
		/* FIXME: implement caching or use the const log2
		   function from mpfr */
		mpw_set_ui (two, 2);
		mpw_ln (two, two);
		mpw_ln (rop, op);
		mpw_div (rop, rop, two);
		mpw_clear (two);
	}
}

void
mpw_log10(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		if G_UNLIKELY (mpwl_sgn(op->r)==0) {
			(*errorout)(_("log10: can't take logarithm of 0"));
			error_num=NUMERICAL_MPW_ERROR;
			return;
		}

		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		if(!mpwl_log10(rop->r,op->r)) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;
			MAKE_COPY(rop->i);
			mpwl_pi (rop->i);
			/* FIXME: implement caching */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_ui (&t, 10);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
		}
	} else {
		mpw_t ten;
		if (mpwl_sgn(op->r)==0) {
			MpwRealNum t = {{NULL}};
			rop->type = MPW_COMPLEX;

			MAKE_COPY(rop->r);
			MAKE_COPY(rop->i);

			g_assert(mpwl_sgn(op->i)!=0);
			/*don't set the pi before the ln, for the case
			  rop==op*/
			if(mpwl_log10(rop->r,op->i)) {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
			} else {
				mpwl_pi(rop->i);
				mpwl_div_ui(rop->i,rop->i,2);
				mpwl_neg(rop->i,rop->i);
			}
			/* FIXME: implement caching */
			mpwl_init_type (&t, MPW_FLOAT);
			mpwl_set_ui (&t, 10);
			mpwl_ln (&t, &t);
			mpwl_div (rop->i, rop->i, &t);
			mpwl_free (&t,TRUE);
			return;
		}
		/* this is stupid, but simple to implement for now */
		mpw_init (ten);
		/* FIXME: implement caching */
		mpw_set_ui (ten, 10);
		mpw_ln (ten, ten);
		mpw_ln (rop, op);
		mpw_div (rop, rop, ten);
		mpw_clear (ten);
	}
}

void
mpw_sin(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sin(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_sin(rop->r,r);
		mpwl_cosh(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_cos(rop->i,i);
		mpwl_sinh(&t,r);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_cos(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_cos(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_cos(rop->r,r);
		mpwl_cosh(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sin(rop->i,i);
		mpwl_neg(rop->i,rop->i);
		mpwl_sinh(&t,r);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_sinh(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_sinh(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_sinh(rop->r,r);
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_cosh(rop->i,i);
		mpwl_sin(&t,r);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_cosh(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_cosh(rop->r,op->r);
	} else {
		MpwRealNum t = {{NULL}};
		MpwRealNum *r;
		MpwRealNum *i;
		rop->type = MPW_COMPLEX;

		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);

		MAKE_CPLX_OPS(op,r,i);

		mpwl_init_type(&t,MPW_FLOAT);
		
		mpwl_cosh(rop->r,r);
		mpwl_cos(&t,i);
		mpwl_mul(rop->r,rop->r,&t);

		mpwl_sinh(rop->i,i);
		mpwl_sin(&t,r);
		mpwl_mul(rop->i,rop->i,&t);
		
		mpwl_free(&t,TRUE);

		mpw_uncomplex(rop);

		BREAK_CPLX_OPS(op,r,i);
	}
}

void
mpw_arctan(mpw_ptr rop,mpw_ptr op)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_arctan(rop->r,op->r);
	} else {
		mpw_t tmp1;
		mpw_t tmp2;
		mpw_t ai;
		MpwRealNum *t;
		
		/*multiply op by i and put it into ai*/
		mpw_init_set(ai,op);
		MAKE_COPY(ai->r);
		MAKE_COPY(ai->i);
		
		t = ai->i;
		ai->i = ai->r;
		ai->r = t;
		
		mpwl_neg(ai->r,ai->r);
		
		error_num = 0;
		
		mpw_init(tmp1);
		mpw_set_ui(tmp1,1);
		mpw_add(tmp1,tmp1,ai);
		
		mpw_init(tmp2);
		mpw_set_ui(tmp2,1);
		mpw_sub(tmp2,tmp2,ai);
		mpw_clear(ai);

		mpw_div(tmp1,tmp1,tmp2);
		mpw_clear(tmp2);
		
		if G_UNLIKELY (error_num) {
			mpw_clear(tmp1);
			return;
		}
		
		mpw_ln(tmp1,tmp1);

		if G_UNLIKELY (error_num) {
			mpw_clear(tmp1);
			return;
		}

		/*divide by 2i*/
		tmp1->type = MPW_COMPLEX;
		MAKE_COPY(tmp1->r);
		MAKE_COPY(tmp1->i);
		
		t = tmp1->i;
		tmp1->i = tmp1->r;
		tmp1->r = t;
		
		mpwl_neg(tmp1->i,tmp1->i);
		mpwl_div_ui(tmp1->r,tmp1->r,2);
		mpwl_div_ui(tmp1->i,tmp1->i,2);
		
		mpw_uncomplex(tmp1);
		
		mpw_set(rop,tmp1);
		mpw_clear(tmp1);
	}
}

void
mpw_pi (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_pi (rop->r);
}

void
mpw_euler_constant (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_euler_constant (rop->r);
}

void
mpw_rand (mpw_ptr rop)
{
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_rand (rop->r);
}

void
mpw_randint (mpw_ptr rop, mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		(*errorout)(_("Can't make random integer out of a complex number"));
		error_num = NUMERICAL_MPW_ERROR;
		return;
	}
	MAKE_REAL (rop);
	MAKE_COPY (rop->r);
	mpwl_randint (rop->r, op->r);
}

void
mpw_i (mpw_ptr rop)
{
	mpw_clear (rop);

	rop->type = MPW_COMPLEX;
	rop->r = zero;
	zero->alloc.usage++;
	rop->i = one;
	one->alloc.usage++;
}

void
mpw_conj (mpw_ptr rop, mpw_ptr op)
{
	if (op->type == MPW_REAL) {
		if (rop != op) {
			MAKE_REAL (rop);
			rop->r->alloc.usage--;
			if (rop->r->alloc.usage==0)
				mpwl_free (rop->r, FALSE);
			rop->r = op->r;
			op->r->alloc.usage++;
		}
	} else {
		if (rop != op) {
			rop->type = MPW_COMPLEX;
			rop->r->alloc.usage--;
			if (rop->r->alloc.usage==0)
				mpwl_free (rop->r, FALSE);
			rop->r = op->r;
			op->r->alloc.usage++;

			MAKE_COPY(rop->i);
			mpwl_neg (rop->i, op->i);
		} else {
			MAKE_COPY(rop->i);
			mpwl_neg(rop->i,op->i);
		}
	}
}

void
mpw_pow_ui(mpw_ptr rop,mpw_ptr op, unsigned long int e)
{
	if(op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_pow_ui(rop->r,op->r,e,FALSE);
	} else {
		mpw_ln(rop,op);
		mpw_mul_ui(rop,rop,e);
		mpw_exp(rop,rop);
	}
}

int
mpw_cmp(mpw_ptr op1, mpw_ptr op2)
{
	if G_LIKELY (op1->type==MPW_REAL && op2->type==MPW_REAL) {
		return mpwl_cmp(op1->r,op2->r);
	} else {
		(*errorout)(_("Can't compare complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
}

int
mpw_cmp_ui(mpw_ptr op, unsigned long int i)
{
	if G_LIKELY (op->type==MPW_REAL) {
		return mpwl_cmp_ui(op->r,i);
	} else {
		(*errorout)(_("Can't compare complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
}

int
mpw_eql(mpw_ptr op1, mpw_ptr op2)
{
	return (mpwl_cmp(op1->r,op2->r)==0 && mpwl_cmp(op1->i,op2->i)==0);
}

int
mpw_eql_ui(mpw_ptr op, unsigned long int i)
{
	if(op->type==MPW_REAL) {
		return mpwl_cmp_ui(op->r,i);
	} else {
		return FALSE;
	}
}

void
mpw_fac_ui(mpw_ptr rop,unsigned long int i)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_fac_ui(rop->r,i);
}

void
mpw_fac(mpw_ptr rop,mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_fac(rop->r,op->r);
	} else {
		(*errorout)(_("Can't make factorials of complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

void
mpw_dblfac (mpw_ptr rop, mpw_ptr op)
{
	if G_LIKELY (op->type==MPW_REAL) {
		MAKE_REAL (rop);
		MAKE_COPY (rop->r);
		mpwl_dblfac (rop->r, op->r);
	} else {
		(*errorout)(_("Can't make factorials of complex numbers"));
		error_num=NUMERICAL_MPW_ERROR;
	}
}

/*make a number int if possible*/
void
mpw_make_int(mpw_ptr rop)
{
	if(rop->type==MPW_REAL) {
		mpwl_make_int(rop->r);
	} else {
		mpwl_make_int(rop->r);
		mpwl_make_int(rop->i);
	}
}

/*make number into a float, this might be neccessary for unprecise
  calculations*/
void
mpw_make_float(mpw_ptr rop)
{
	if(rop->type==MPW_REAL) {
		MAKE_COPY(rop->r);
		mpwl_make_float(rop->r);
	} else {
		MAKE_COPY(rop->r);
		MAKE_COPY(rop->i);
		mpwl_make_float(rop->r);
		mpwl_make_float(rop->i);
	}
}

/*init the mp stuff*/
void
mpw_init_mp(void)
{
	static gboolean done = FALSE;
	if (done)
		return;

	mp_set_memory_functions(my_malloc,my_realloc,my_free);
	GET_NEW_REAL(zero);
	mpwl_init_type(zero,MPW_NATIVEINT);
	mpwl_set_ui(zero,0);
	zero->alloc.usage = 1;
	GET_NEW_REAL(one);
	mpwl_init_type(one,MPW_NATIVEINT);
	mpwl_set_ui(one,1);
	one->alloc.usage = 1;
	done = TRUE;
}

char *
mpw_getstring(mpw_ptr num, int max_digits,
	      gboolean scientific_notation,
	      gboolean results_as_floats,
	      gboolean mixed_fractions,
	      /* GelOutputStyle */ int style,
	      int integer_output_base,
	      gboolean add_parenths)
{
	mpw_uncomplex(num);
	if(num->type==MPW_REAL) {
		return mpwl_getstring(num->r,max_digits,scientific_notation,
			results_as_floats,mixed_fractions,style,
			integer_output_base,"");
	} else {
		char *p1 = NULL, *p2, *r;
		int justreal = mpwl_sgn(num->r) == 0;
		if (! justreal)
			p1 = mpwl_getstring(num->r,
					    max_digits,
					    scientific_notation,
					    results_as_floats,
					    mixed_fractions, 
					    style,
					    integer_output_base,
					    "" /* postfix */);
		p2 = mpwl_getstring(num->i,
				    max_digits,
				    scientific_notation,
				    results_as_floats,
				    mixed_fractions, 
				    style,
				    integer_output_base,
				    "i" /* postfix */);
		if (justreal) {
			r = p2;
			p2 = NULL;
		} else if (mpwl_sgn(num->i)>=0) {
			if (add_parenths)
				r = g_strconcat("(",p1,"+",p2,")",NULL);
			else
				r = g_strconcat(p1,"+",p2,NULL);
		} else {
			if (add_parenths)
				r = g_strconcat("(",p1,p2,")",NULL);
			else
				r = g_strconcat(p1,p2,NULL);
		}
		g_free(p1);
		g_free(p2);
		return r;
	}
}

void
mpw_set_str_float (mpw_ptr rop, const char *s, int base)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_set_str_float(rop->r,s,base);
}

void
mpw_set_str_int (mpw_ptr rop, const char *s, int base)
{
	MAKE_REAL(rop);
	MAKE_COPY(rop->r);
	mpwl_set_str_int(rop->r,s,base);
}

void
mpw_set_str_complex_int(mpw_ptr rop,const char *s,int base)
{
	char *p;
	int size;

	rop->type = MPW_COMPLEX;

	p = g_strdup(s);
	size = strlen(p);
	if(p[size-1] == 'i')
		p[size-1] = '\0';
	MAKE_COPY(rop->i);
	mpwl_set_str_int(rop->i,p,base);

	g_free(p);

	mpw_uncomplex(rop);
}

void
mpw_set_str_complex(mpw_ptr rop,const char *s,int base)
{
	char *p;
	int size;

	rop->type = MPW_COMPLEX;

	p = g_strdup(s);
	size = strlen(p);
	if(p[size-1] == 'i')
		p[size-1] = '\0';
	MAKE_COPY(rop->i);
	mpwl_set_str_float(rop->i,p,base);

	g_free(p);

	mpw_uncomplex(rop);
}

static gboolean
looks_like_float (const char *s)
{
	/*floats, FIXME: this is pretty hackish */
	if (strchr(s,'.') != NULL)
		return TRUE;
	return (strchr(s,'e') != NULL ||
		strchr(s,'E') != NULL) &&
		strncmp(s,"0x",2) != 0 &&
		strchr(s,'\\') == NULL;
}

/*set one element (space separated)*/
static void
mpw_set_str_one(mpw_ptr rop,const char *s,int base)
{
	/*rationals*/
	if(strchr(s,'/')) {
		char *p = g_strdup(s);
		char *pp;
		mpw_t tmp;

		mpw_init(tmp);

		/* numerator */
		pp = strtok(p,"/");
		if (strchr (pp, 'i') == NULL)
			mpw_set_str_int(rop,pp,base);
		else
			mpw_set_str_complex_int(rop,pp,base);

		/* denominator */
		pp = strtok(NULL,"/");
		if (strchr (pp, 'i') == NULL)
			mpw_set_str_int(tmp,pp,base);
		else
			mpw_set_str_complex_int(tmp,pp,base);

		g_free(p);
		
		mpw_div(rop,rop,tmp);
	/*complex*/
	} else if(strchr(s,'i')) {
		char *p = g_strdup(s);
		char *pp;
		mpw_t tmp;

		for(pp=p;*pp;pp++) {
			if(*pp=='+') {
				*pp='\0';
				pp++;
				break;
			} else if(*pp=='-') {
				if(pp>p && *(pp-1)!='e') {
					*pp='\0';
					pp++;
					break;
				}
			}
		}
		/*must be a pure imaginary*/
		if(!*pp) {
			g_free(p);
			if (looks_like_float (s))
				mpw_set_str_complex(rop,s,base);
			else
				mpw_set_str_complex_int(rop,s,base);
			return;
		}
		mpw_set_str(rop,p,base);
		mpw_init(tmp);
		if (looks_like_float (pp))
			mpw_set_str_complex(tmp,pp,base);
		else
			mpw_set_str_complex_int(tmp,pp,base);
		mpw_add(rop,rop,tmp);
		mpw_clear(tmp);
	} else if (looks_like_float (s)) {
		mpw_set_str_float(rop,s,base);
	} else {
		mpw_set_str_int(rop,s,base);
	}
}

void
mpw_set_str(mpw_ptr rop,const char *s,int base)
{
	char *p;
	char *d;
	mpw_t tmp;
	p = strchr(s,' ');
	if(!p) {
		mpw_set_str_one(rop,s,base);
		return;
	}
	mpw_init(tmp);
	mpw_set_ui(rop,0);
	d = g_strdup(s);
	p = strtok(d," ");
	while(p) {
		mpw_set_str_one(tmp,p,base);
		mpw_add(rop,rop,tmp);
		p = strtok(NULL," ");
	}
	mpw_clear(tmp);
	g_free(d);
}

gboolean
mpw_is_complex(mpw_ptr op)
{
	mpw_uncomplex(op);
	return op->type == MPW_COMPLEX;
}

gboolean
mpw_is_integer(mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		(*errorout)(_("Can't determine type of a complex number"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_INTEGER || op->r->type == MPW_NATIVEINT;
}

gboolean
mpw_is_complex_integer(mpw_ptr op)
{
	if(op->type == MPW_COMPLEX) {
		return op->r->type <= MPW_INTEGER &&
		       op->i->type <= MPW_INTEGER;
	} else {
		return op->r->type == MPW_INTEGER || op->r->type == MPW_NATIVEINT;
	}
}

gboolean
mpw_is_rational(mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		(*errorout)(_("Can't determine type of a complex number"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_RATIONAL;
}

gboolean
mpw_is_complex_rational_or_integer(mpw_ptr op)
{
	if(op->type == MPW_COMPLEX) {
		return op->r->type <= MPW_RATIONAL &&
			op->i->type <= MPW_RATIONAL;
	} else {
		return op->r->type <= MPW_RATIONAL;
	}
}

gboolean
mpw_is_float(mpw_ptr op)
{
	if G_UNLIKELY (op->type == MPW_COMPLEX) {
		(*errorout)(_("Can't determine type of a complex number"));
		error_num=NUMERICAL_MPW_ERROR;
		return FALSE;
	}
	return op->r->type == MPW_FLOAT;
}

gboolean
mpw_is_complex_float(mpw_ptr op)
{
	if(op->type == MPW_COMPLEX) {
		return op->r->type == MPW_FLOAT ||
			op->i->type == MPW_FLOAT;
	} else {
		return op->r->type == MPW_FLOAT;
	}
}

void
mpw_im(mpw_ptr rop, mpw_ptr op)
{
	MAKE_REAL(rop);
	rop->r=op->i;
	op->i->alloc.usage++;
}

void
mpw_re(mpw_ptr rop, mpw_ptr op)
{
	MAKE_REAL(rop);
	rop->r=op->r;
	op->r->alloc.usage++;
}

void
mpw_round(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_round(rop->r);
	if(op->type==MPW_COMPLEX) {
		MAKE_COPY(rop->i);
		mpwl_round(rop->i);
		mpw_uncomplex(rop);
	}
}

void
mpw_floor(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_floor(rop->r);
	if(op->type==MPW_COMPLEX) {
		MAKE_COPY(rop->i);
		mpwl_floor(rop->i);
		mpw_uncomplex(rop);
	}
}

void
mpw_ceil(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_ceil(rop->r);
	if(op->type==MPW_COMPLEX) {
		MAKE_COPY(rop->i);
		mpwl_ceil(rop->i);
		mpw_uncomplex(rop);
	}
}

void
mpw_trunc(mpw_ptr rop, mpw_ptr op)
{
	mpw_set(rop,op);
	MAKE_COPY(rop->r);
	mpwl_trunc(rop->r);
	if(op->type==MPW_COMPLEX) {
		MAKE_COPY(rop->i);
		mpwl_trunc(rop->i);
		mpw_uncomplex(rop);
	}
}

long
mpw_get_long(mpw_ptr op)
{
	long r;
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	if G_UNLIKELY (op->type==MPW_COMPLEX) {
		(*errorout)(_("Can't convert complex number into integer"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_long(op->r,&ex);
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		(*errorout)(_("Can't convert real number to integer"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} else if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		(*errorout)(_("Integer too large for this operation"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

double
mpw_get_double (mpw_ptr op)
{
	double r;
	int ex = MPWL_EXCEPTION_NO_EXCEPTION;
	if G_UNLIKELY (op->type==MPW_COMPLEX) {
		(*errorout)(_("Can't convert complex number into integer"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} 
	r = mpwl_get_double (op->r, &ex);
	/* currently there is no conversion error exception for
	   get_double */
#if 0
	if G_UNLIKELY (ex == MPWL_EXCEPTION_CONVERSION_ERROR) {
		(*errorout)(_("Can't convert real number to double"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	} else
#endif
	if G_UNLIKELY (ex == MPWL_EXCEPTION_NUMBER_TOO_LARGE) {
		(*errorout)(_("Number too large for this operation"));
		error_num=NUMERICAL_MPW_ERROR;
		return 0;
	}
	return r;
}

void
mpw_denominator(mpw_ptr rop, mpw_ptr op)
{
	if(op->type==MPW_COMPLEX) {
		MpwRealNum r1 = {{NULL}};
		MpwRealNum r2 = {{NULL}};

		mpwl_init_type (&r1, MPW_NATIVEINT);
		mpwl_init_type (&r1, MPW_NATIVEINT);

		mpwl_denominator (&r1, op->r);
		mpwl_denominator (&r2, op->i);

		if G_UNLIKELY (error_num != NO_ERROR) {
			mpwl_clear (&r1);
			mpwl_clear (&r2);
			return;
		}

		MAKE_REAL (rop);
		MAKE_COPY (rop->r);

		mpwl_mul (rop->r, &r1, &r2);
		mpwl_gcd (&r1, &r1, &r2);
		mpwl_div (rop->r, rop->r, &r1);

		mpwl_clear (&r1);
		mpwl_clear (&r2);
	} else {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_denominator(rop->r, op->r);
	}
}

void
mpw_numerator(mpw_ptr rop, mpw_ptr op)
{
	if(op->type==MPW_COMPLEX) {
		MpwRealNum r1 = {{NULL}};
		MpwRealNum r2 = {{NULL}};
		MpwRealNum n1 = {{NULL}};
		MpwRealNum n2 = {{NULL}};

		mpwl_init_type (&r1, MPW_NATIVEINT);
		mpwl_init_type (&r1, MPW_NATIVEINT);
		mpwl_init_type (&n1, MPW_NATIVEINT);
		mpwl_init_type (&n1, MPW_NATIVEINT);

		mpwl_denominator (&r1, op->r);
		mpwl_denominator (&r2, op->i);
		mpwl_numerator (&n1, op->r);
		mpwl_numerator (&n2, op->i);

		if G_UNLIKELY (error_num != NO_ERROR) {
			mpwl_clear (&r1);
			mpwl_clear (&r2);
			mpwl_clear (&n1);
			mpwl_clear (&n2);
			return;
		}

		rop->type = MPW_COMPLEX;
		MAKE_COPY (rop->r);
		MAKE_COPY (rop->i);

		mpwl_mul (rop->r, &n1, &r2);
		mpwl_mul (rop->i, &n2, &r1);

		mpwl_gcd (&r1, &r1, &r2);

		mpwl_div (rop->r, rop->r, &r1);
		mpwl_div (rop->i, rop->i, &r1);

		mpwl_clear (&r1);
		mpwl_clear (&r2);
		mpwl_clear (&n1);
		mpwl_clear (&n2);

		mpw_uncomplex (rop);
	} else {
		MAKE_REAL(rop);
		MAKE_COPY(rop->r);
		mpwl_numerator(rop->r, op->r);
	}
}

#if 0
/*************************************************************************/
/*conext level stuff                                                     */
/*************************************************************************/

/* make new context with a refcount of 1 */
MpwCtx *
mpw_ctx_new(MpwErrorFunc errorout,
	    int default_mpf_prec,
	    gboolean double_math,
	    gpointer data)
{
	MpwCtx *mctx = g_new0(MpwCtx,1);
	mctx->errorout = errorout;
	mctx->default_mpf_prec = default_mpf_prec;
	mctx->double_math = double_math;
	mctx->data = data;

	return mctx;
}

void
mpw_ctx_set_errorout(MpwCtx *mctx, MpwErrorFunc errorout)
{
	mctx->errorout = errorout;
}

void
mpw_ctx_set_default_mpf_prec(MpwCtx *mctx, int default_mpf_prec)
{
	mctx->default_mpf_prec = default_mpf_prec;
}

void
mpw_ctx_set_double_math(MpwCtx *mctx, gboolean double_math)
{
	mctx->double_math = double_math;
}

void
mpw_ctx_set_data(MpwCtx *mctx, gpointer data)
{
	mctx->data = data;
}

MpwErrorFunc
mpw_ctx_get_errorout(MpwCtx *mctx)
{
	return mctx->errorout;
}

int
mpw_ctx_get_default_mpf_prec(MpwCtx *mctx)
{
	return mctx->default_mpf_prec;
}

gboolean
mpw_ctx_get_double_math(MpwCtx *mctx)
{
	return mctx->double_math;
}

gpointer
mpw_ctx_get_data(MpwCtx *mctx)
{
	return mctx->data;
}

void
mpw_ctx_ref(MpwCtx *mctx)
{
	mctx->ref_count++;
}

void
mpw_ctx_unref(MpwCtx *mctx)
{
	mctx->ref_count--;
	if(mctx->ref_count <= 0) {
		g_free(mctx);
	}
}
#endif

#if 0
/*************************************************************************/
/*cache system                                                           */
/*************************************************************************/
static MpwCache *
mpw_chache_get(int prec)
{
	MpwCache *mc;
	if(!mpw_cache_ht)
		mpw_cache_ht = g_hash_table_new(NULL, NULL);
	mc = g_hash_table_lookup(mpw_cache_ht, GINT_TO_POINTER(prec));
	if(mc) {
		mc->use_count++;
	} else {
		mc = g_new0(MpwCache,1);
		mc->use_count = 1;
		g_hash_table_insert(mpw_cache_ht,
				    GINT_TO_POINTER(prec), mc);
	}
	return mc;
}

static void
mpw_chache_unget(MpwCache *mc)
{
	mc->use_count--;
	/* FIXME: clear on 0 */
}
#endif
