/* This is included in funclib.c and other files that want to define functions */

#define RAISE_EXCEPTION(e) { if ((e) != NULL) *(e) = TRUE; }

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

static inline gboolean
check_argument_integer (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != VALUE_NODE ||
		       mpw_is_complex(a[argnum]->val.value) ||
		       ! mpw_is_integer (a[argnum]->val.value)) {
		gel_errorout (_("%s: argument number %d not an integer"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_integer_or_matrix (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY ((a[argnum]->type != VALUE_NODE ||
			mpw_is_complex(a[argnum]->val.value) ||
			! mpw_is_integer (a[argnum]->val.value)) &&
		       a[argnum]->type != MATRIX_NODE) {
		gel_errorout (_("%s: argument number %d not an integer or a matrix"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}


static inline gboolean
check_argument_positive_integer (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != VALUE_NODE ||
		       mpw_is_complex(a[argnum]->val.value) ||
		       ! mpw_is_integer (a[argnum]->val.value) ||
		       mpw_sgn (a[argnum]->val.value) < 0) {
		gel_errorout (_("%s: argument number %d not a positive integer"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_number (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type!=VALUE_NODE) {
		gel_errorout (_("%s: argument number %d not a number"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_bool (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != VALUE_NODE &&
		       a[argnum]->type != BOOL_NODE) {
		gel_errorout (_("%s: argument number %d not a boolean"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_real_number (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type!=VALUE_NODE ||
		       mpw_is_complex(a[argnum]->val.value)) {
		gel_errorout (_("%s: argument number %d not a real number"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_matrix (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != MATRIX_NODE) {
		gel_errorout (_("%s: argument number %d not a matrix"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_matrix_or_null (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != MATRIX_NODE &&
		       a[argnum]->type != NULL_NODE) {
		gel_errorout (_("%s: argument number %d not a matrix or null node"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_value_only_matrix (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != MATRIX_NODE ||
		       ! gel_is_matrix_value_only (a[argnum]->mat.matrix)) {
		gel_errorout (_("%s: argument number %d not a value only matrix"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_value_only_vector (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != MATRIX_NODE ||
		       ! gel_is_matrix_value_only (a[argnum]->mat.matrix) ||
		       ! (gel_matrixw_width (a[argnum]->mat.matrix) == 1 ||
			  gel_matrixw_height (a[argnum]->mat.matrix) == 1)) {
		gel_errorout (_("%s: argument number %d not a value only vector"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_string (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != STRING_NODE) {
		gel_errorout (_("%s: argument number %d not a string"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_string_or_identifier (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != STRING_NODE &&
		       a[argnum]->type != IDENTIFIER_NODE) {
		gel_errorout (_("%s: argument number %d not a string"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

static inline gboolean
check_argument_function_or_identifier (GelETree **a, int argnum, const char *funcname)
{
	if G_UNLIKELY (a[argnum]->type != FUNCTION_NODE &&
		       a[argnum]->type != IDENTIFIER_NODE) {
		gel_errorout (_("%s: argument number %d not a function or identifier"), funcname, argnum+1);
		return FALSE;
	}
	return TRUE;
}

