/* Copyright 2015 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ofc/sema.h"

static const char* ofc_sema_intrinsics__reserved_list[]=
{
	"AdjustL",
	"AdjustR",
	"All",
	"Allocated",
	"Any",
	"Associated",
	"Ceiling",
	"Count",
	"CShift",
	"Digits",
	"Dot_Product",
	"EOShift",
	"Epsilon",
	"Exponent",
	"Floor",
	"Fraction",
	"Huge",
	"Kind",
	"LBound",
	"Logical",
	"MatMul",
	"MaxExponent",
	"MaxLoc",
	"MaxVal",
	"Merge",
	"MinExponent",
	"MinLoc",
	"MinVal",
	"Modulo",
	"Nearest",
	"Pack",
	"Precision",
	"Present",
	"Product",
	"Radix",
	"Random_Number",
	"Random_Seed",
	"Range",
	"Repeat",
	"Reshape",
	"RRSpacing",
	"Scale",
	"Scan",
	"Selected_Int_Kind",
	"Selected_Real_Kind",
	"Set_Exponent",
	"Shape",
	"Spacing",
	"Spread",
	"Sum",
	"Tiny",
	"Transfer",
	"Transpose",
	"Trim",
	"UBound",
	"Unpack",
	"Verify",

	NULL
};

bool ofc_sema_intrinsic_name_reserved(const char* name)
{
    unsigned i = 0;

	while (ofc_sema_intrinsics__reserved_list[i])
	{
		if (strcasecmp(ofc_sema_intrinsics__reserved_list[i], name) == 0)
			return true;
		i++;
	}

	return false;
}

typedef enum
{
	OFC_SEMA_INTRINSIC_OP,
	OFC_SEMA_INTRINSIC_FUNC,
	OFC_SEMA_INTRINSIC_SUBR,

	OFC_SEMA_INTRINSIC_INVALID
} ofc_sema_intrinsic_e;

typedef enum
{
	OFC_SEMA_INTRINSIC__TYPE_NORMAL = 0,
	OFC_SEMA_INTRINSIC__TYPE_ANY,
	OFC_SEMA_INTRINSIC__TYPE_SCALAR,

	/* Same as argument(s) */
	OFC_SEMA_INTRINSIC__TYPE_SAME,

	/* Return type calculated in callback */
	OFC_SEMA_INTRINSIC__TYPE_CALLBACK,
} ofc_sema_intrinsic__type_e;

typedef struct
{
	ofc_sema_intrinsic__type_e type_type;
	ofc_sema_type_e            type;
	ofc_sema_kind_e            kind;
	unsigned                   size;
	bool                       intent_in;
	bool                       intent_out;
} ofc_sema_intrinsic__param_t;

static const ofc_sema_intrinsic__param_t ofc_sema_intrinsic__param[] =
{
	{ OFC_SEMA_INTRINSIC__TYPE_ANY     , OFC_SEMA_KIND_NONE, 0, 0, 1, 0 }, /* ANY  */
	{ OFC_SEMA_INTRINSIC__TYPE_SAME    , OFC_SEMA_KIND_NONE, 0, 0, 1, 0 }, /* SAME */
	{ OFC_SEMA_INTRINSIC__TYPE_SCALAR  , OFC_SEMA_KIND_NONE, 0, 0, 1, 0 }, /* SCALAR */
	{ OFC_SEMA_INTRINSIC__TYPE_CALLBACK, OFC_SEMA_KIND_NONE, 0, 0, 1, 0 }, /* CALLBACK */

	{ 0, OFC_SEMA_TYPE_LOGICAL  , OFC_SEMA_KIND_NONE, 0, 1, 0 }, /* LOGICAL */
	{ 0, OFC_SEMA_TYPE_INTEGER  , OFC_SEMA_KIND_NONE, 0, 1, 0 }, /* INTEGER */
	{ 0, OFC_SEMA_TYPE_REAL     , OFC_SEMA_KIND_NONE, 0, 1, 0 }, /* REAL */
	{ 0, OFC_SEMA_TYPE_COMPLEX  , OFC_SEMA_KIND_NONE, 0, 1, 0 }, /* COMPLEX */
	{ 0, OFC_SEMA_TYPE_CHARACTER, OFC_SEMA_KIND_NONE, 0, 1, 0 }, /* CHARACTER */

	{ 0, OFC_SEMA_TYPE_CHARACTER, OFC_SEMA_KIND_NONE, 1, 1, 0 }, /* CHARACTER_1 */

	{ 0, OFC_SEMA_TYPE_LOGICAL, OFC_SEMA_KIND_DEFAULT, 0, 1, 0 }, /* DEF_LOGICAL */
	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_DEFAULT, 0, 1, 0 }, /* DEF_INTEGER */
	{ 0, OFC_SEMA_TYPE_REAL   , OFC_SEMA_KIND_DEFAULT, 0, 1, 0 }, /* DEF_REAL */
	{ 0, OFC_SEMA_TYPE_COMPLEX, OFC_SEMA_KIND_DEFAULT, 0, 1, 0 }, /* DEF_COMPLEX */

	{ 0, OFC_SEMA_TYPE_REAL   , OFC_SEMA_KIND_DOUBLE, 0, 1, 0 }, /* DEF_DOUBLE */
	{ 0, OFC_SEMA_TYPE_COMPLEX, OFC_SEMA_KIND_DOUBLE, 0, 1, 0 }, /* DEF_DOUBLE_COMPLEX */

	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_HALF, 0, 1, 0 }, /* DEF_HALF_INTEGER */

	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_1_BYTE, 0, 1, 0 }, /* INTEGER_1 */
	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_2_BYTE, 0, 1, 0 }, /* INTEGER_2 */
	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_4_BYTE, 0, 1, 0 }, /* INTEGER_4 */

	{ 0, OFC_SEMA_TYPE_REAL, OFC_SEMA_KIND_DEFAULT, 2, 1, 0 }, /* DEF_REAL_A2 */
	{ 0, OFC_SEMA_TYPE_REAL, OFC_SEMA_KIND_DEFAULT, 2, 0, 1 }, /* DEF_REAL_A2_OUT */

	{ 0, OFC_SEMA_TYPE_CHARACTER, OFC_SEMA_KIND_NONE, 0, 0, 1 }, /* CHARACTER_OUT */
	{ 0, OFC_SEMA_TYPE_INTEGER  , OFC_SEMA_KIND_NONE, 0, 0, 1 }, /* INTEGER_OUT */
	{ 0, OFC_SEMA_TYPE_REAL     , OFC_SEMA_KIND_NONE, 0, 0, 1 }, /* REAL_OUT */

	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_NONE,  3, 0, 1 }, /* INTEGER_A3_OUT */
	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_NONE, 13, 1, 0 }, /* INTEGER_A13 */
	{ 0, OFC_SEMA_TYPE_INTEGER, OFC_SEMA_KIND_NONE, 13, 0, 1 }, /* INTEGER_A13_OUT */
};


typedef enum
{
	IP_ANY = 0,  /* Any type */
	IP_SAME,     /* Same as argument */
	IP_SCALAR,   /* Any scalar type */
	IP_CALLBACK, /* Use a callback to determine return type. */

	IP_LOGICAL,
	IP_INTEGER,
	IP_REAL,
	IP_COMPLEX,
	IP_CHARACTER,

	IP_CHARACTER_1,

	IP_DEF_LOGICAL,
	IP_DEF_INTEGER,
	IP_DEF_REAL,
	IP_DEF_COMPLEX,

	IP_DEF_DOUBLE,
	IP_DEF_DOUBLE_COMPLEX,

	IP_DEF_HALF_INTEGER,

	IP_INTEGER_1,
	IP_INTEGER_2,
	IP_INTEGER_4,

	IP_DEF_REAL_A2,
	IP_DEF_REAL_A2_OUT,

	IP_INTEGER_OUT,
	IP_REAL_OUT,
	IP_CHARACTER_OUT,

	IP_INTEGER_A3_OUT,
	IP_INTEGER_A13,
	IP_INTEGER_A13_OUT,

	IP_COUNT
} ofc_sema_intrinsic__param_e;


static ofc_sema_typeval_t* ofc_sema_intrinsic_op__constant_cast(
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	if (!intrinsic || !args
		|| (args->count != 1)
		|| !ofc_sema_expr_is_constant(args->expr[0]))
		return NULL;

	return ofc_sema_typeval_cast(
		ofc_sema_expr_constant(args->expr[0]),
		ofc_sema_intrinsic_type(intrinsic, args));
}

typedef struct
{
	const char*                 name;
	unsigned                    arg_min, arg_max;
	ofc_sema_intrinsic__param_e return_type;
	ofc_sema_intrinsic__param_e arg_type;

	ofc_sema_typeval_t* (*constant)(
		const ofc_sema_intrinsic_t*,
		const ofc_sema_expr_list_t*);
} ofc_sema_intrinsic_op_t;

static const ofc_sema_intrinsic_op_t ofc_sema_intrinsic__op_list[] =
{
	/* Casts */
	{ "INT"   , 1, 1, IP_DEF_INTEGER        , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "IFIX"  , 1, 1, IP_DEF_INTEGER        , IP_DEF_REAL          , ofc_sema_intrinsic_op__constant_cast },
	{ "IDINT" , 1, 1, IP_DEF_INTEGER        , IP_DEF_DOUBLE        , ofc_sema_intrinsic_op__constant_cast },
	{ "HFIX"  , 1, 1, IP_DEF_HALF_INTEGER   , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "INT1"  , 1, 1, IP_INTEGER_1          , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "INT2"  , 1, 1, IP_INTEGER_2          , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "INT4"  , 1, 1, IP_INTEGER_4          , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "INTC"  , 1, 1, IP_INTEGER_2          , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "JFIX"  , 1, 1, IP_INTEGER_4          , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "REAL"  , 1, 1, IP_DEF_REAL           , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "FLOAT" , 1, 1, IP_DEF_REAL           , IP_DEF_INTEGER       , ofc_sema_intrinsic_op__constant_cast },
	{ "SNGL"  , 1, 1, IP_DEF_REAL           , IP_DEF_DOUBLE        , ofc_sema_intrinsic_op__constant_cast },
	{ "DREAL" , 1, 1, IP_DEF_DOUBLE         , IP_DEF_DOUBLE_COMPLEX, ofc_sema_intrinsic_op__constant_cast },
	{ "DBLE"  , 1, 1, IP_DEF_DOUBLE         , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "DFLOAT", 1, 1, IP_DEF_DOUBLE         , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "CMPLX" , 1, 2, IP_DEF_COMPLEX        , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },
	{ "DCMPLX", 1, 2, IP_DEF_DOUBLE_COMPLEX , IP_ANY               , ofc_sema_intrinsic_op__constant_cast },

	/* Truncation */
	{ "AINT", 1, 1, IP_SAME, IP_REAL      , NULL },
	{ "DINT", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	/* Rounding */
	{ "ANINT" , 1, 1, IP_SAME       , IP_REAL      , NULL },
	{ "DNINT" , 1, 1, IP_SAME       , IP_DEF_DOUBLE, NULL },
	{ "NINT"  , 1, 1, IP_DEF_INTEGER, IP_REAL      , NULL },
	{ "IDNINT", 1, 1, IP_DEF_INTEGER, IP_DEF_DOUBLE, NULL },

	{ "ABS" , 1, 1, IP_SCALAR  , IP_ANY        , NULL },
	{ "IABS", 1, 1, IP_SAME    , IP_DEF_INTEGER, NULL },
	{ "DABS", 1, 1, IP_SAME    , IP_DEF_DOUBLE , NULL },
	{ "CABS", 1, 1, IP_DEF_REAL, IP_DEF_COMPLEX, NULL },

	{ "MOD"   , 2, 2, IP_SAME, IP_SCALAR    , NULL },
	{ "AMOD"  , 2, 2, IP_SAME, IP_DEF_REAL  , NULL },
	{ "DMOD"  , 2, 2, IP_SAME, IP_DEF_DOUBLE, NULL },
	{ "MODULO", 2, 2, IP_SAME, IP_SCALAR    , NULL },

	{ "FLOOR"  , 1, 1, IP_SAME, IP_REAL, NULL },
	{ "CEILING", 1, 1, IP_SAME, IP_REAL, NULL },

	/* Transfer of sign */
	{ "SIGN" , 2, 2, IP_SAME, IP_SCALAR     , NULL },
	{ "ISIGN", 2, 2, IP_SAME, IP_DEF_INTEGER, NULL },
	{ "DSIGN", 2, 2, IP_SAME, IP_DEF_DOUBLE , NULL },

	/* Positive difference */
	{ "DIM" , 2, 2, IP_SAME, IP_SCALAR     , NULL },
	{ "IDIM", 2, 2, IP_SAME, IP_DEF_INTEGER, NULL },
	{ "DDIM", 2, 2, IP_SAME, IP_DEF_DOUBLE , NULL },

	/* Inner product */
	{ "DRPOD", 2, 2, IP_DEF_DOUBLE, IP_DEF_REAL, NULL },

	{ "MAX"  , 2, 0, IP_SAME       , IP_SCALAR     , NULL },
	{ "MAX0" , 2, 0, IP_SAME       , IP_DEF_INTEGER, NULL },
	{ "AMAX1", 2, 0, IP_SAME       , IP_DEF_REAL   , NULL },
	{ "DMAX1", 2, 0, IP_SAME       , IP_DEF_DOUBLE , NULL },
	{ "AMAX0", 2, 0, IP_DEF_REAL   , IP_DEF_INTEGER, NULL },
	{ "MAX1" , 2, 0, IP_DEF_INTEGER, IP_DEF_REAL   , NULL },
	{ "MIN"  , 2, 0, IP_SAME       , IP_SCALAR     , NULL },
	{ "MIN0" , 2, 0, IP_SAME       , IP_DEF_INTEGER, NULL },
	{ "AMIN1", 2, 0, IP_SAME       , IP_DEF_REAL   , NULL },
	{ "DMIN1", 2, 0, IP_SAME       , IP_DEF_DOUBLE , NULL },
	{ "AMIN0", 2, 0, IP_DEF_REAL   , IP_DEF_INTEGER, NULL },
	{ "MIN1" , 2, 0, IP_DEF_INTEGER, IP_DEF_REAL   , NULL },

	{ "AIMG" , 1, 1, IP_SCALAR, IP_COMPLEX, NULL },
	{ "CONJG", 1, 1, IP_SCALAR, IP_COMPLEX, NULL },

	{ "SQRT" , 1, 1, IP_SAME, IP_ANY        , NULL },
	{ "DSQRT", 1, 1, IP_SAME, IP_DEF_DOUBLE , NULL },
	{ "CSQRT", 1, 1, IP_SAME, IP_DEF_COMPLEX, NULL },

	{ "EXP" , 1, 1, IP_SAME, IP_ANY, NULL },
	{ "DEXP", 1, 1, IP_SAME, IP_ANY, NULL },
	{ "CEXP", 1, 1, IP_SAME, IP_ANY, NULL },

	{ "LOG" , 1, 1, IP_SAME, IP_ANY        , NULL },
	{ "ALOG", 1, 1, IP_SAME, IP_DEF_REAL   , NULL },
	{ "DLOG", 1, 1, IP_SAME, IP_DEF_DOUBLE , NULL },
	{ "CLOG", 1, 1, IP_SAME, IP_DEF_COMPLEX, NULL },

	{ "LOG10" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "ALOG10", 1, 1, IP_SAME, IP_DEF_REAL  , NULL },
	{ "DLOG10", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "SIN" , 1, 1, IP_SAME, IP_ANY        , NULL },
	{ "DSIN", 1, 1, IP_SAME, IP_DEF_DOUBLE , NULL },
	{ "CSIN", 1, 1, IP_SAME, IP_DEF_COMPLEX, NULL },

	{ "COS" , 1, 1, IP_SAME, IP_ANY        , NULL },
	{ "DCOS", 1, 1, IP_SAME, IP_DEF_DOUBLE , NULL },
	{ "CCOS", 1, 1, IP_SAME, IP_DEF_COMPLEX, NULL },

	{ "TAN" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DTAN", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "ASIN" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DASIN", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "ACOS" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DACOS", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "ATAN"  , 1, 2, IP_SAME, IP_ANY       , NULL },
	{ "DATAN" , 1, 2, IP_SAME, IP_DEF_DOUBLE, NULL },
	{ "ATAN2" , 2, 2, IP_SAME, IP_ANY       , NULL },
	{ "DATAN2", 2, 2, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "SINH" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DSINH", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "COSH" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DCOSH", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "TANH"  , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DTANH" , 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "ASINH" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DASINH", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "ACOSH" , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DACOSH", 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "ATANH"  , 1, 1, IP_SAME, IP_ANY       , NULL },
	{ "DATANH" , 1, 1, IP_SAME, IP_DEF_DOUBLE, NULL },

	{ "IAND", 2, 2, IP_SAME, IP_INTEGER, NULL },
	{ "IEOR", 2, 2, IP_SAME, IP_INTEGER, NULL },
	{ "IOR" , 2, 2, IP_SAME, IP_INTEGER, NULL },
	{ "NOT" , 1, 1, IP_SAME, IP_INTEGER, NULL },

	{ NULL, 0, 0, 0, 0, NULL }
};



static const ofc_sema_type_t* ofc_sema_intrinsic__char_rt(
	const ofc_sema_expr_list_t* args)
{
	ofc_sema_kind_e kind = OFC_SEMA_KIND_DEFAULT;
	if (args->count >= 2)
	{
		const ofc_sema_type_t* type
			= ofc_sema_expr_type(args->expr[1]);
		if (!type) return NULL;
		kind = type->kind;
	}

	return ofc_sema_type_create_character(
		kind, 1, false);
}

static const ofc_sema_type_t* ofc_sema_intrinsic__ichar_rt(
	const ofc_sema_expr_list_t* args)
{
	ofc_sema_kind_e kind = OFC_SEMA_KIND_DEFAULT;
	if (args->count >= 2)
	{
		const ofc_sema_type_t* type
			= ofc_sema_expr_type(args->expr[1]);
		if (!type) return NULL;
		kind = type->kind;
	}

	return ofc_sema_type_create_primitive(
		OFC_SEMA_TYPE_INTEGER, kind);
}

static const ofc_sema_type_t* ofc_sema_intrinsic__transfer_rt(
	const ofc_sema_expr_list_t* args)
{
	if (args->count < 2)
		return NULL;

	if (args->count >= 3)
	{
		/* TODO - INTRINSIC - Use 3rd parameter as array size if present. */
		ofc_sparse_ref_error(args->expr[2]->src,
			"TRANSFER SIZE argument not yet supported");
		return NULL;
	}

	return ofc_sema_expr_type(args->expr[1]);
}

static ofc_sema_typeval_t* ofc_sema_intrinsic__transfer_tv(
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	(void)intrinsic;

	/* TODO - INTRINSIC - Use 3rd parameter as array size if present. */
	if ((args->count < 2)
		|| (args->count >= 3))
		return NULL;

	const ofc_sema_typeval_t* atv
		= ofc_sema_expr_constant(args->expr[1]);
	if (!atv) return NULL;

	const ofc_sema_type_t* atype = atv->type;
	if (!atype) return NULL;

	const ofc_sema_type_t* rtype
		= ofc_sema_intrinsic__transfer_rt(args);

	unsigned asize, rsize;
	if (!ofc_sema_type_size(atype, &asize)
		|| !ofc_sema_type_size(rtype, &rsize))
		return NULL;

	uint8_t buff[asize];
	memset(buff, 0x00, asize);

	const void* src = buff;
	switch (atv->type->type)
	{
		case OFC_SEMA_TYPE_LOGICAL:
			buff[0] = (atv->logical ? 1 : 0);
			break;

		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_BYTE:
			if (asize > 8)
				return NULL;
			src = &atv->integer;
			break;

		case OFC_SEMA_TYPE_REAL:
		{
			if (asize == 4)
			{
				float f32 = atv->real;
				memcpy(buff, &f32, 4);
			}
			else if (asize == 8)
			{
				double f64 = atv->real;
				memcpy(buff, &f64, 8);
			}
			else if (asize == 10)
			{
				long double f80 = atv->real;
				memcpy(buff, &f80, 10);
			}
			else
			{
				return NULL;
			}
			break;
		}

		case OFC_SEMA_TYPE_COMPLEX:
		{
			if (asize == 8)
			{
				float f32[2] =
				{
					atv->complex.real,
					atv->complex.imaginary
				};
				memcpy(buff, f32, 8);
			}
			else if (asize == 16)
			{
				double f64[2] =
				{
					atv->complex.real,
					atv->complex.imaginary
				};
				memcpy(buff, f64, 16);
			}
			else if (asize == 20)
			{
				long double f80[2] =
				{
					atv->complex.real,
					atv->complex.imaginary
				};
				memcpy(buff, f80, 20);
			}
			else
			{
				return NULL;
			}
			break;
		}

		case OFC_SEMA_TYPE_CHARACTER:
			src = atv->character;
			if (!src) return NULL;
			break;

		default:
			return NULL;
	}

	/* TODO - INTRINSIC - Retain location in typeval. */
	ofc_sema_typeval_t* rtv
		= ofc_sema_typeval_create_integer(
			0, OFC_SEMA_KIND_DEFAULT,
			OFC_SPARSE_REF_EMPTY);
	if (!rtv) return NULL;
	rtv->type = rtype;

	if (ofc_sema_type_is_character(rtype))
	{
		rtv->character = (char*)malloc(rsize);
		if (!rtv->character)
		{
			ofc_sema_typeval_delete(rtv);
			return NULL;
		}

		if (asize > rsize)
			asize = rsize;

		memset(rtv->character, 0x00, rsize);
		memcpy(rtv->character, src, asize);
	}
	else
	{
		switch (rtype->type)
		{
			case OFC_SEMA_TYPE_LOGICAL:
			{
				uint8_t zero[asize];
				memset(zero, 0x00, asize);
				rtv->logical = (memcmp(src, zero, asize) != 0);
				break;
			}

			case OFC_SEMA_TYPE_INTEGER:
			case OFC_SEMA_TYPE_BYTE:
				/* We can't fold constants this large. */
				if (rsize > 8)
				{
					ofc_sema_typeval_delete(rtv);
					return NULL;
				}

				if (asize < rsize)
					rsize = asize;

				rtv->integer = 0;
				memcpy(&rtv->integer, src, rsize);
				break;

			case OFC_SEMA_TYPE_REAL:
			{
				if (asize > rsize)
					asize = rsize;

				if (rsize == 4)
				{
					float f32 = 0.0f;
					memcpy(&f32, src, asize);
					rtv->real = f32;
				}
				else if (rsize == 8)
				{
					double f64 = 0.0;
					memcpy(&f64, src, asize);
					rtv->real = f64;
				}
				else if (rsize == 10)
				{
					long double f80 = 0.0;
					memcpy(&f80, src, asize);
					rtv->real = f80;
				}
				else
				{
					/* We can't fold obscure float constants. */
					ofc_sema_typeval_delete(rtv);
					return NULL;
				}

				break;
			}

			case OFC_SEMA_TYPE_COMPLEX:
			{
				if (asize > rsize)
					asize = rsize;

				if (rsize == 8)
				{
					float f32[2] = { 0.0f, 0.0f };
					memcpy(f32, src, asize);
					rtv->complex.real = f32[0];
					rtv->complex.imaginary = f32[1];
				}
				else if (rsize == 16)
				{
					double f64[2] = { 0.0, 0.0 };
					memcpy(f64, src, asize);
					rtv->complex.real = f64[0];
					rtv->complex.imaginary = f64[1];
				}
				else if (rsize == 20)
				{
					long double f80[2] = { 0.0, 0.0 };
					memcpy(f80, src, asize);
					rtv->complex.real = f80[0];
					rtv->complex.imaginary = f80[1];
				}
				else
				{
					/* We can't fold obscure float constants. */
					ofc_sema_typeval_delete(rtv);
					return NULL;
				}

				break;
			}

			default:
				ofc_sema_typeval_delete(rtv);
				return NULL;
		}
	}

	return rtv;
}



typedef struct
{
	const char*                 name;
	unsigned                    arg_min, arg_max;
	ofc_sema_intrinsic__param_e return_type;
	ofc_sema_intrinsic__param_e arg_type[3];

	const ofc_sema_type_t* (*return_type_callback)(const ofc_sema_expr_list_t*);

	ofc_sema_typeval_t* (*constant)(
		const ofc_sema_intrinsic_t*,
		const ofc_sema_expr_list_t*);
} ofc_sema_intrinsic_func_t;

static const ofc_sema_intrinsic_func_t ofc_sema_intrinsic__func_list[] =
{
	{ "MClock",  0, 0, IP_INTEGER_1, { 0 }, NULL, NULL },
	{ "MClock8", 0, 0, IP_INTEGER_2, { 0 }, NULL, NULL },
	{ "FDate",   0, 0, IP_CHARACTER, { 0 }, NULL, NULL },
	{ "Second",  0, 0, IP_DEF_REAL,  { 0 }, NULL, NULL },

	{ "Loc",      1, 1, IP_DEF_INTEGER, { IP_ANY           }, NULL, NULL },
	{ "IRand",    0, 1, IP_DEF_INTEGER, { IP_INTEGER       }, NULL, NULL },
	{ "LnBlnk",   1, 1, IP_DEF_INTEGER, { IP_CHARACTER     }, NULL, NULL },
	{ "IsaTty",   1, 1, IP_LOGICAL,     { IP_INTEGER       }, NULL, NULL },
	{ "Len",      1, 1, IP_INTEGER_1,   { IP_CHARACTER     }, NULL, NULL },
	{ "AImag",    1, 1, IP_REAL,        { IP_DEF_COMPLEX   }, NULL, NULL },
	{ "Len_Trim", 1, 1, IP_DEF_INTEGER, { IP_CHARACTER     }, NULL, NULL },
	{ "BesJ0",    1, 1, IP_REAL,        { IP_REAL          }, NULL, NULL },
	{ "BesJ1",    1, 1, IP_REAL,        { IP_REAL          }, NULL, NULL },
	{ "BesJN",    1, 1, IP_DEF_INTEGER, { IP_REAL          }, NULL, NULL },
	{ "BesY0",    1, 1, IP_REAL,        { IP_REAL          }, NULL, NULL },
	{ "BesY1",    1, 1, IP_REAL,        { IP_REAL          }, NULL, NULL },
	{ "CTime",    1, 1, IP_CHARACTER,   { IP_INTEGER       }, NULL, NULL },
	{ "DErF",     1, 1, IP_DEF_DOUBLE,  { IP_DEF_DOUBLE    }, NULL, NULL },
	{ "DErFC",    1, 1, IP_DEF_DOUBLE,  { IP_DEF_DOUBLE    }, NULL, NULL },
	{ "ErF",      1, 1, IP_REAL,        { IP_REAL          }, NULL, NULL },
	{ "ErFC",     1, 1, IP_REAL,        { IP_REAL          }, NULL, NULL },
	{ "ETime",    1, 1, IP_DEF_REAL,    { IP_DEF_REAL_A2   }, NULL, NULL },
	{ "FTell",    1, 1, IP_DEF_INTEGER, { IP_INTEGER       }, NULL, NULL },
	{ "GetCWD",   1, 1, IP_DEF_INTEGER, { IP_CHARACTER_OUT }, NULL, NULL },
	{ "HostNm",   1, 1, IP_DEF_INTEGER, { IP_CHARACTER_OUT }, NULL, NULL },
	{ "TtyNam",   1, 1, IP_CHARACTER,   { IP_INTEGER       }, NULL, NULL },

	{ "Stat",   2, 2, IP_DEF_INTEGER, { IP_CHARACTER, IP_INTEGER_A13_OUT }, NULL, NULL },
	{ "LStat",  2, 2, IP_DEF_INTEGER, { IP_CHARACTER, IP_INTEGER_A13_OUT }, NULL, NULL },
	{ "FStat",  2, 2, IP_DEF_INTEGER, { IP_INTEGER  , IP_INTEGER_A13_OUT }, NULL, NULL },
	{ "Access", 2, 2, IP_DEF_INTEGER, { IP_CHARACTER, IP_CHARACTER       }, NULL, NULL },
	{ "LGe",    2, 2, IP_LOGICAL,     { IP_CHARACTER, IP_CHARACTER       }, NULL, NULL },
	{ "LGt",    2, 2, IP_LOGICAL,     { IP_CHARACTER, IP_CHARACTER       }, NULL, NULL },
	{ "LLe",    2, 2, IP_LOGICAL,     { IP_CHARACTER, IP_CHARACTER       }, NULL, NULL },
	{ "LLt",    2, 2, IP_LOGICAL,     { IP_CHARACTER, IP_CHARACTER       }, NULL, NULL },
	{ "LShift", 2, 2, IP_DEF_INTEGER, { IP_INTEGER  , IP_INTEGER         }, NULL, NULL },
	{ "IShft",  2, 2, IP_DEF_INTEGER, { IP_INTEGER  , IP_INTEGER         }, NULL, NULL },
	{ "BesYN",  2, 2, IP_REAL,        { IP_INTEGER  , IP_REAL            }, NULL, NULL },
	{ "BTest",  2, 2, IP_LOGICAL,     { IP_INTEGER  , IP_INTEGER         }, NULL, NULL },

	{ "IShftC", 3, 3, IP_INTEGER, { IP_INTEGER, IP_INTEGER, IP_INTEGER }, NULL, NULL },

	{ "Char" , 1, 2, IP_CALLBACK, { IP_INTEGER    , IP_INTEGER },
		ofc_sema_intrinsic__char_rt, NULL },
	{ "AChar", 1, 2, IP_CALLBACK, { IP_INTEGER    , IP_INTEGER },
		ofc_sema_intrinsic__char_rt, NULL },
	{ "IChar", 1, 2, IP_CALLBACK, { IP_CHARACTER_1, IP_INTEGER },
		ofc_sema_intrinsic__ichar_rt, NULL },

	{ "Transfer", 2, 3, IP_CALLBACK, { IP_ANY, IP_ANY, IP_INTEGER },
		ofc_sema_intrinsic__transfer_rt,
		ofc_sema_intrinsic__transfer_tv },

	{ NULL, 0, 0, 0, { 0 }, NULL, NULL }
};

static const ofc_sema_intrinsic_func_t ofc_sema_intrinsic__subr_list[] =
{
	{ "ITime",  1, 1, 0, { IP_INTEGER_A3_OUT }, NULL, NULL },
	{ "FDate",  1, 1, 0, { IP_CHARACTER_OUT  }, NULL, NULL },
	{ "Second", 1, 1, 0, { IP_REAL_OUT       }, NULL, NULL },

	{ "ChDir",  1, 2, 0, { IP_CHARACTER      , IP_INTEGER_OUT   }, NULL, NULL },
	{ "LTime",  2, 2, 0, { IP_INTEGER        , IP_CHARACTER_OUT }, NULL, NULL },
	{ "CTime",  2, 2, 0, { IP_INTEGER        , IP_CHARACTER_OUT }, NULL, NULL },
	{ "DTime",  2, 2, 0, { IP_DEF_REAL_A2_OUT, IP_REAL_OUT      }, NULL, NULL },
	{ "ETime",  2, 2, 0, { IP_DEF_REAL_A2_OUT, IP_REAL_OUT      }, NULL, NULL },
	{ "FGet",   1, 2, 0, { IP_CHARACTER_OUT  , IP_INTEGER_OUT   }, NULL, NULL },
	{ "FPut",   1, 2, 0, { IP_CHARACTER      , IP_INTEGER_OUT   }, NULL, NULL },
	{ "FTell",  2, 2, 0, { IP_INTEGER        , IP_INTEGER_OUT   }, NULL, NULL },
	{ "GetCWD", 1, 2, 0, { IP_CHARACTER_OUT  , IP_INTEGER_OUT   }, NULL, NULL },
	{ "HostNm", 1, 2, 0, { IP_CHARACTER_OUT  , IP_INTEGER_OUT   }, NULL, NULL },
	{ "System", 1, 2, 0, { IP_CHARACTER      , IP_INTEGER_OUT   }, NULL, NULL },
	{ "TtyNam", 2, 2, 0, { IP_INTEGER        , IP_CHARACTER_OUT }, NULL, NULL },
	{ "UMask",  1, 2, 0, { IP_INTEGER        , IP_INTEGER_OUT   }, NULL, NULL },
	{ "Unlink", 1, 2, 0, { IP_CHARACTER      , IP_INTEGER_OUT   }, NULL, NULL },

	{ "ChMod",  2, 3, 0, { IP_CHARACTER, IP_CHARACTER      , IP_INTEGER_OUT }, NULL, NULL },
	{ "SymLnk", 2, 3, 0, { IP_CHARACTER, IP_CHARACTER      , IP_INTEGER_OUT }, NULL, NULL },
	{ "Kill",   2, 3, 0, { IP_INTEGER  , IP_INTEGER        , IP_INTEGER_OUT }, NULL, NULL },
	{ "Stat",   2, 3, 0, { IP_CHARACTER, IP_INTEGER_A13_OUT, IP_INTEGER_OUT }, NULL, NULL },
	{ "FStat",  2, 3, 0, { IP_INTEGER  , IP_INTEGER_A13_OUT, IP_INTEGER_OUT }, NULL, NULL },
	{ "LStat",  2, 3, 0, { IP_CHARACTER, IP_INTEGER_A13_OUT, IP_INTEGER_OUT }, NULL, NULL },
	{ "Alarm",  2, 3, 0, { IP_INTEGER  , IP_INTEGER_A13    , IP_INTEGER_OUT }, NULL, NULL },
	{ "FGetC",  2, 3, 0, { IP_INTEGER  , IP_CHARACTER_OUT  , IP_INTEGER_OUT }, NULL, NULL },
	{ "FPutC",  2, 3, 0, { IP_INTEGER  , IP_CHARACTER      , IP_INTEGER_OUT }, NULL, NULL },
	{ "Link",   2, 3, 0, { IP_CHARACTER, IP_CHARACTER      , IP_INTEGER_OUT }, NULL, NULL },
	{ "Rename", 2, 3, 0, { IP_CHARACTER, IP_CHARACTER      , IP_INTEGER_OUT }, NULL, NULL },

	{ NULL, 0, 0, 0, { 0 }, NULL, NULL }
};


static const ofc_sema_type_t* ofc_sema_intrinsic__param_rtype(
	ofc_sema_intrinsic__param_e param,
	const ofc_sema_expr_list_t* args,
	const ofc_sema_type_t* (*callback)(const ofc_sema_expr_list_t*))
{
	if (param >= IP_COUNT)
		return NULL;

	const ofc_sema_intrinsic__param_t p
		= ofc_sema_intrinsic__param[param];

	const ofc_sema_type_t* stype = NULL;
	if (args && (args->count > 0))
		stype = ofc_sema_expr_type(args->expr[0]);

	const ofc_sema_type_t* rtype = NULL;
	switch (p.type_type)
	{
		case OFC_SEMA_INTRINSIC__TYPE_NORMAL:
			break;

		case OFC_SEMA_INTRINSIC__TYPE_ANY:
			/* ANY is not a valid as a return type. */
			return NULL;

		case OFC_SEMA_INTRINSIC__TYPE_SAME:
			rtype = stype;
			if (!rtype) return NULL;
			break;

		case OFC_SEMA_INTRINSIC__TYPE_SCALAR:
			rtype = ofc_sema_type_scalar(stype);
			if (!rtype) return NULL;
			break;

		case OFC_SEMA_INTRINSIC__TYPE_CALLBACK:
			if (!callback) return NULL;
			return callback(args);

		default:
			return NULL;
	}

	/* Return value can never be an array. */
	if ((p.type != OFC_SEMA_TYPE_CHARACTER)
		&& (p.size != 0))
		return NULL;

	if (!rtype && stype
		&& (p.kind == OFC_SEMA_KIND_NONE))
	{
		if (p.type == stype->type)
		{
			switch (p.type)
			{
				case OFC_SEMA_TYPE_LOGICAL:
				case OFC_SEMA_TYPE_BYTE:
				case OFC_SEMA_TYPE_INTEGER:
				case OFC_SEMA_TYPE_REAL:
				case OFC_SEMA_TYPE_COMPLEX:
					rtype = stype;
					break;
				default:
					break;
			}
		}
		else if (((p.type == OFC_SEMA_TYPE_REAL)
				|| (p.type == OFC_SEMA_TYPE_COMPLEX))
			&& ((stype->type == OFC_SEMA_TYPE_REAL)
				|| (stype->type == OFC_SEMA_TYPE_COMPLEX)))
		{
			rtype = ofc_sema_type_create_primitive(
				p.type, stype->kind);
			if (!rtype) return NULL;
		}
	}

	if (!rtype)
	{
		if (p.type == OFC_SEMA_TYPE_CHARACTER)
		{
			unsigned kind = p.kind;
			if ((kind == OFC_SEMA_KIND_NONE)
				&& ofc_sema_type_is_character(stype))
				kind = stype->kind;
			if (kind == OFC_SEMA_KIND_NONE)
				kind = OFC_SEMA_KIND_DEFAULT;

			rtype = ofc_sema_type_create_character(
				kind, p.size, (p.size == 0));
		}
		else
		{
			switch (p.type)
			{
				case OFC_SEMA_TYPE_LOGICAL:
				case OFC_SEMA_TYPE_BYTE:
				case OFC_SEMA_TYPE_INTEGER:
				case OFC_SEMA_TYPE_REAL:
				case OFC_SEMA_TYPE_COMPLEX:
					break;
				default:
					return false;
			}

			unsigned kind = p.kind;
			if (kind == OFC_SEMA_KIND_NONE)
				kind = OFC_SEMA_KIND_DEFAULT;

			rtype = ofc_sema_type_create_primitive(
				p.type, kind);

			if (p.kind == OFC_SEMA_KIND_NONE)
			{
				rtype = ofc_sema_type_promote(
					rtype, stype);
			}
		}

		if (!rtype) return NULL;
	}

	return rtype;
}


struct ofc_sema_intrinsic_s
{
	ofc_sema_intrinsic_e type;

	ofc_str_ref_t name;

	union
	{
		const ofc_sema_intrinsic_op_t*   op;
		const ofc_sema_intrinsic_func_t* func;
	};
};

static ofc_sema_intrinsic_t* ofc_sema_intrinsic__create_op(
	const ofc_sema_intrinsic_op_t* op)
{
	if (!op)
		return NULL;

	ofc_sema_intrinsic_t* intrinsic
		= (ofc_sema_intrinsic_t*)malloc(
			sizeof(ofc_sema_intrinsic_t));
	if (!intrinsic) return NULL;

	intrinsic->name = ofc_str_ref_from_strz(op->name);
	intrinsic->type = OFC_SEMA_INTRINSIC_OP;
	intrinsic->op = op;

	return intrinsic;
}

static ofc_sema_intrinsic_t* ofc_sema_intrinsic__create_func(
	const ofc_sema_intrinsic_func_t* func)
{
	if (!func)
		return NULL;

	ofc_sema_intrinsic_t* intrinsic
		= (ofc_sema_intrinsic_t*)malloc(
			sizeof(ofc_sema_intrinsic_t));
	if (!intrinsic) return NULL;

	intrinsic->name = ofc_str_ref_from_strz(func->name);
	intrinsic->type = OFC_SEMA_INTRINSIC_FUNC;
	intrinsic->func = func;

	return intrinsic;
}

static void ofc_sema_intrinsic__delete(
	ofc_sema_intrinsic_t* intrinsic)
{
	if (!intrinsic)
		return;

	free(intrinsic);
}

static const ofc_str_ref_t* ofc_sema_intrinsic__key(
	const ofc_sema_intrinsic_t* intrinsic)
{
	if (!intrinsic)
		return NULL;
	return &intrinsic->name;
}

static ofc_hashmap_t* ofc_sema_intrinsic__op_map   = NULL;
static ofc_hashmap_t* ofc_sema_intrinsic__func_map = NULL;
static ofc_hashmap_t* ofc_sema_intrinsic__subr_map = NULL;

static void ofc_sema_intrinsic__term(void)
{
	ofc_hashmap_delete(ofc_sema_intrinsic__op_map);
	ofc_hashmap_delete(ofc_sema_intrinsic__func_map);
	ofc_hashmap_delete(ofc_sema_intrinsic__subr_map);
}

static bool ofc_sema_intrinsic__op_map_init(void)
{
	ofc_sema_intrinsic__op_map = ofc_hashmap_create(
		(void*)ofc_str_ref_ptr_hash_ci,
		(void*)ofc_str_ref_ptr_equal_ci,
		(void*)ofc_sema_intrinsic__key,
		(void*)ofc_sema_intrinsic__delete);
	if (!ofc_sema_intrinsic__op_map)
		return false;

	unsigned i;
	for (i = 0; ofc_sema_intrinsic__op_list[i].name; i++)
	{
		ofc_sema_intrinsic_t* intrinsic
			= ofc_sema_intrinsic__create_op(
				&ofc_sema_intrinsic__op_list[i]);
		if (!intrinsic)
		{
			ofc_hashmap_delete(
				ofc_sema_intrinsic__op_map);
			ofc_sema_intrinsic__op_map = NULL;
			return false;
		}

		if (!ofc_hashmap_add(
			ofc_sema_intrinsic__op_map,
			intrinsic))
		{
			ofc_sema_intrinsic__delete(intrinsic);
			ofc_hashmap_delete(
				ofc_sema_intrinsic__op_map);
			ofc_sema_intrinsic__op_map = NULL;
			return false;
		}
	}

	return true;
}

static bool ofc_sema_intrinsic__func_map_init(void)
{
	ofc_sema_intrinsic__func_map = ofc_hashmap_create(
		(void*)ofc_str_ref_ptr_hash_ci,
		(void*)ofc_str_ref_ptr_equal_ci,
		(void*)ofc_sema_intrinsic__key,
		(void*)ofc_sema_intrinsic__delete);
	if (!ofc_sema_intrinsic__func_map)
		return false;

	unsigned i;
	for (i = 0; ofc_sema_intrinsic__func_list[i].name; i++)
	{
		ofc_sema_intrinsic_t* intrinsic
			= ofc_sema_intrinsic__create_func(
				&ofc_sema_intrinsic__func_list[i]);
		if (!intrinsic)
		{
			ofc_hashmap_delete(
				ofc_sema_intrinsic__func_map);
			ofc_sema_intrinsic__func_map = NULL;
			return false;
		}

		if (!ofc_hashmap_add(
			ofc_sema_intrinsic__func_map,
			intrinsic))
		{
			ofc_sema_intrinsic__delete(intrinsic);
			ofc_hashmap_delete(
				ofc_sema_intrinsic__func_map);
			ofc_sema_intrinsic__func_map = NULL;
			return false;
		}
	}

	return true;
}

static bool ofc_sema_intrinsic__subr_map_init(void)
{
	ofc_sema_intrinsic__subr_map = ofc_hashmap_create(
		(void*)ofc_str_ref_ptr_hash_ci,
		(void*)ofc_str_ref_ptr_equal_ci,
		(void*)ofc_sema_intrinsic__key,
		(void*)ofc_sema_intrinsic__delete);
	if (!ofc_sema_intrinsic__subr_map)
		return false;

	unsigned i;
	for (i = 0; ofc_sema_intrinsic__subr_list[i].name; i++)
	{
		ofc_sema_intrinsic_t* intrinsic
			= ofc_sema_intrinsic__create_func(
				&ofc_sema_intrinsic__subr_list[i]);
		if (!intrinsic)
		{
			ofc_hashmap_delete(
				ofc_sema_intrinsic__subr_map);
			ofc_sema_intrinsic__subr_map = NULL;
			return false;
		}

		if (!ofc_hashmap_add(
			ofc_sema_intrinsic__subr_map,
			intrinsic))
		{
			ofc_sema_intrinsic__delete(intrinsic);
			ofc_hashmap_delete(
				ofc_sema_intrinsic__subr_map);
			ofc_sema_intrinsic__subr_map = NULL;
			return false;
		}
	}

	return true;
}

static bool ofc_sema_intrinsic__init(void)
{
	if (ofc_sema_intrinsic__op_map
		&& ofc_sema_intrinsic__func_map
		&& ofc_sema_intrinsic__subr_map)
		return true;

	/* TODO - Set case sensitivity based on lang_opts? */
	if (!ofc_sema_intrinsic__op_map_init()
		|| !ofc_sema_intrinsic__func_map_init()
		|| !ofc_sema_intrinsic__subr_map_init())
		return false;

	atexit(ofc_sema_intrinsic__term);
	return true;
}


const ofc_sema_intrinsic_t* ofc_sema_intrinsic(
	ofc_str_ref_t name, bool case_sensitive)
{
	if (!ofc_sema_intrinsic__init())
		return NULL;

	const ofc_sema_intrinsic_t* func = ofc_hashmap_find(
		ofc_sema_intrinsic__op_map, &name);
	if (!func)
	{
		func = ofc_hashmap_find(
			ofc_sema_intrinsic__func_map, &name);
		if (!func) return NULL;
	}

	if (case_sensitive
		&& !ofc_str_ref_equal(
			name, func->name))
		return NULL;

	return func;
}

static const ofc_sema_type_t* ofc_sema_intrinsic__param_type(
	const ofc_sema_expr_t* expr,
	ofc_sema_intrinsic__param_e param,
	bool* valid)
{
	if (!expr || (param >= IP_COUNT))
		return NULL;

	const ofc_sema_type_t* type
		= ofc_sema_expr_type(expr);

	ofc_sema_intrinsic__param_t p
		= ofc_sema_intrinsic__param[param];
	switch (p.type_type)
	{
		case OFC_SEMA_INTRINSIC__TYPE_NORMAL:
			break;

		case OFC_SEMA_INTRINSIC__TYPE_ANY:
			if (valid) *valid = true;
			return type;

		case OFC_SEMA_INTRINSIC__TYPE_SCALAR:
			if (valid) *valid = ofc_sema_type_is_scalar(type);
			return ofc_sema_type_scalar(type);

		/* Arguments can't be SAME or CALLBACK. */
		default:
			return NULL;
	}

	if (!type) return NULL;

	if ((p.type == OFC_SEMA_TYPE_CHARACTER)
		&& (type->type == OFC_SEMA_TYPE_CHARACTER)
		&& ((p.kind == OFC_SEMA_KIND_NONE) || (type->kind == p.kind))
		&& ((p.size == 0) || type->len_var || (p.size == type->len)))
	{
		if (valid) *valid = true;
		return type;
	}

	if ((p.type == type->type)
		&& (p.kind == 0))
	{
		switch (p.type)
		{
			case OFC_SEMA_TYPE_LOGICAL:
			case OFC_SEMA_TYPE_BYTE:
			case OFC_SEMA_TYPE_INTEGER:
			case OFC_SEMA_TYPE_REAL:
			case OFC_SEMA_TYPE_COMPLEX:
				if (valid) *valid = true;
				return type;
			default:
				break;
		}
	}

	const ofc_sema_type_t* ctype = NULL;
	switch (p.type)
	{
		case OFC_SEMA_TYPE_CHARACTER:
			ctype = ofc_sema_type_create_character(
				(p.kind != OFC_SEMA_KIND_NONE
					? p.kind : OFC_SEMA_KIND_DEFAULT),
				p.size, (p.size == 0));
			break;

		case OFC_SEMA_TYPE_LOGICAL:
		case OFC_SEMA_TYPE_BYTE:
		case OFC_SEMA_TYPE_INTEGER:
		case OFC_SEMA_TYPE_REAL:
		case OFC_SEMA_TYPE_COMPLEX:
			ctype = ofc_sema_type_create_primitive(
				p.type, (p.kind != OFC_SEMA_KIND_NONE
					? p.kind : OFC_SEMA_KIND_DEFAULT));
			break;

		default:
			return NULL;
	}

	/* TODO - INTRINSIC - Promote when KIND isn't specified and types differ. */

	if (valid) *valid = ofc_sema_type_compatible(ctype, type);
	return ctype;
}

static ofc_sema_expr_t* ofc_sema_intrinsic__param_cast(
	const ofc_sema_expr_t* expr,
	ofc_sema_intrinsic__param_e param,
	bool* valid)
{
	if (!expr || (param >= IP_COUNT))
		return NULL;

	const ofc_sema_type_t* type
		= ofc_sema_expr_type(expr);
	if (!type) return NULL;

	const ofc_sema_type_t* ctype
		= ofc_sema_intrinsic__param_type(
			expr, param, valid);
	if (!ctype) return NULL;

	ofc_sema_expr_t* copy
		= ofc_sema_expr_copy(expr);
	if (ofc_sema_type_compatible(type, ctype))
		return copy;

	ofc_sema_expr_t* cast
		= ofc_sema_expr_cast(copy, ctype);
	if (!cast)
	{
		ofc_sema_expr_delete(copy);
		return NULL;
	}
	return cast;
}

static ofc_sema_expr_list_t* ofc_sema_intrinsic_cast__op(
	ofc_sparse_ref_t src,
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	if (!intrinsic || !args
		|| (intrinsic->type != OFC_SEMA_INTRINSIC_OP)
		|| !intrinsic->op)
		return NULL;

	if (args->count < intrinsic->op->arg_min)
	{
		ofc_sparse_ref_error(src,
			"Not enough arguments for intrinsic function.");
		return NULL;
	}
	if ((intrinsic->op->arg_max != 0)
		&& (args->count > intrinsic->op->arg_max))
	{
		ofc_sparse_ref_error(src,
			"Too many arguments for intrinsic function.");
		return NULL;
	}

	const ofc_sema_type_t* ctype = NULL;
	unsigned i;
	for (i = 0; i < args->count; i++)
	{
		bool valid = true;
		const ofc_sema_type_t* atype
			= ofc_sema_intrinsic__param_type(
				args->expr[i], intrinsic->op->arg_type, &valid);

		if (!valid)
		{
			ofc_sparse_ref_warning(args->expr[i]->src,
				"Incorrect argument type for intrinsic.");
		}

		ctype = (ctype
			? ofc_sema_type_promote(ctype, atype)
			: atype);
		if (!ctype) return NULL;
	}

	ofc_sema_expr_list_t* cargs
		= ofc_sema_expr_list_create();
	if (!cargs) return NULL;

	for (i = 0; i < args->count; i++)
	{
		ofc_sema_expr_t* carg
			= ofc_sema_expr_copy(args->expr[i]);
		if (!carg)
		{
			ofc_sema_expr_list_delete(cargs);
			return NULL;
		}

		const ofc_sema_type_t* atype
			= ofc_sema_expr_type(carg);

		if (!ofc_sema_type_compatible(
			atype, ctype))
		{
			ofc_sema_expr_t* cast
				= ofc_sema_expr_cast(carg, ctype);
			if (!cast)
			{
				ofc_sparse_ref_error(carg->src,
					"Incompatible argument type for intrinsic.");
				ofc_sema_expr_delete(carg);
				ofc_sema_expr_list_delete(cargs);
				return NULL;
			}
			carg = cast;
		}

		if (!ofc_sema_expr_list_add(cargs, carg))
		{
			ofc_sema_expr_delete(carg);
			ofc_sema_expr_list_delete(cargs);
			return NULL;
		}
	}

	return cargs;
}

static ofc_sema_expr_list_t* ofc_sema_intrinsic_cast__func(
	ofc_sparse_ref_t src,
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	if (!intrinsic || !args)
		return NULL;

	if ((intrinsic->type != OFC_SEMA_INTRINSIC_FUNC)
		&& (intrinsic->type != OFC_SEMA_INTRINSIC_SUBR))
		return NULL;

	if (!intrinsic->func)
		return NULL;

	if (args->count < intrinsic->func->arg_min)
	{
		ofc_sparse_ref_error(src,
			"Not enough arguments for intrinsic function.");
		return NULL;
	}
	if ((intrinsic->func->arg_max != 0)
		&& (args->count > intrinsic->func->arg_max))
	{
		ofc_sparse_ref_error(src,
			"Too many arguments for intrinsic function.");
		return NULL;
	}

	/* TODO - Handle array arguments. */

	ofc_sema_expr_list_t* cargs
		= ofc_sema_expr_list_create();
	if (!cargs) return NULL;

	unsigned i;
	for (i = 0; i < args->count; i++)
	{
		bool valid = true;
		ofc_sema_expr_t* carg
			= ofc_sema_intrinsic__param_cast(
				args->expr[i], intrinsic->func->arg_type[i], &valid);
		if (!carg)
		{
			ofc_sema_expr_list_delete(cargs);
			return NULL;
		}

		if (!valid)
		{
			ofc_sparse_ref_warning(carg->src,
				"Incorrect argument type for intrinsic.");
		}

		if (!ofc_sema_expr_list_add(cargs, carg))
		{
			ofc_sema_expr_delete(carg);
			ofc_sema_expr_list_delete(cargs);
			return NULL;
		}
	}

	return cargs;
}

ofc_sema_expr_list_t* ofc_sema_intrinsic_cast(
	ofc_sparse_ref_t src,
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	switch (intrinsic->type)
	{
		case OFC_SEMA_INTRINSIC_OP:
			return ofc_sema_intrinsic_cast__op(src, intrinsic, args);

		case OFC_SEMA_INTRINSIC_FUNC:
		case OFC_SEMA_INTRINSIC_SUBR:
			return ofc_sema_intrinsic_cast__func(src, intrinsic, args);

		default:
			break;
	}

	return NULL;
}

ofc_sema_typeval_t* ofc_sema_intrinsic_constant(
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	if (!intrinsic)
		return NULL;

	if (intrinsic->type == OFC_SEMA_INTRINSIC_OP)
	{
		if (!intrinsic->op
			|| !intrinsic->op->constant)
			return NULL;
		return intrinsic->op->constant(
			intrinsic, args);

	}
	else if (intrinsic->type == OFC_SEMA_INTRINSIC_FUNC)
	{
		if (!intrinsic->func
			|| !intrinsic->func->constant)
			return NULL;
		return intrinsic->func->constant(
			intrinsic, args);
	}

	return NULL;
}


const ofc_sema_type_t* ofc_sema_intrinsic_type(
	const ofc_sema_intrinsic_t* intrinsic,
	const ofc_sema_expr_list_t* args)
{
	switch (intrinsic->type)
	{
		case OFC_SEMA_INTRINSIC_OP:
			if (!intrinsic->op) return NULL;
			return ofc_sema_intrinsic__param_rtype(
				intrinsic->func->return_type, args, NULL);

		case OFC_SEMA_INTRINSIC_FUNC:
			if (!intrinsic->func) return NULL;
			return ofc_sema_intrinsic__param_rtype(
				intrinsic->func->return_type, args,
				intrinsic->func->return_type_callback);

		/* Intrinsic subroutines have no return type*/
		case OFC_SEMA_INTRINSIC_SUBR:
			return NULL;

		default:
			break;
	}

	return NULL;
}

const ofc_sema_intrinsic_t* ofc_sema_intrinsic_cast_func(
	const ofc_sema_type_t* type)
{
	if (!type)
		return NULL;

	unsigned i;
	for (i = 0; ofc_sema_intrinsic__op_list[i].name; i++)
	{
		if (ofc_sema_intrinsic__op_list[i].constant
			!= ofc_sema_intrinsic_op__constant_cast)
			continue;

		const ofc_sema_type_t* ctype
			= ofc_sema_intrinsic__param_rtype(
				ofc_sema_intrinsic__op_list[i].return_type, NULL, NULL);

		if (ofc_sema_type_compare(ctype, type))
		{
			/* TODO - INTRINSIC - Find a neater way to do this lookup. */
			const ofc_sema_intrinsic_t* intrinsic
				= ofc_sema_intrinsic(ofc_str_ref_from_strz(
					ofc_sema_intrinsic__op_list[i].name), false);
			if (intrinsic) return intrinsic;
		}
	}

	return NULL;
}

bool ofc_sema_intrinsic_print(
	ofc_colstr_t* cs,
	const ofc_sema_intrinsic_t* intrinsic)
{
	if (!cs || !intrinsic) return false;

	if (!ofc_str_ref_print(cs, intrinsic->name))
		return false;

	return true;
}
