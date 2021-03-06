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

#include "ofc/parse.h"



unsigned ofc_parse_expr_precedence(
	ofc_parse_expr_t* expr)
{
	if (!expr)
		return 0;

	switch (expr->type)
	{
		case OFC_PARSE_EXPR_UNARY:
			return ofc_parse_operator_precedence_unary(expr->unary.operator);
		case OFC_PARSE_EXPR_BINARY:
			return ofc_parse_operator_precedence_binary(expr->binary.operator);
		default:
			break;
	}

	return 0;
}

static ofc_parse_expr_t* ofc_parse_expr__literal(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug, unsigned* len)
{
	unsigned dpos = ofc_parse_debug_position(debug);

	ofc_parse_literal_t literal;
	unsigned l = ofc_parse_literal(
		src, ptr, debug, &literal);
	if (l == 0)
	{
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	ofc_parse_expr_t* expr
		= (ofc_parse_expr_t*)malloc(
			sizeof(ofc_parse_expr_t));
	if (!expr)
	{
		ofc_parse_literal_cleanup(literal);
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	expr->type    = OFC_PARSE_EXPR_CONSTANT;
	expr->src     = ofc_sparse_ref(src, ptr, l);
	expr->literal = literal;

	if (len) *len = l;
	return expr;
}

ofc_parse_expr_t* ofc_parse_expr_integer(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug, unsigned* len)
{
	unsigned dpos = ofc_parse_debug_position(debug);

	ofc_parse_literal_t literal;
	unsigned l = ofc_parse_literal_integer(
		src, ptr, debug, &literal);
	if (l == 0)
	{
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	ofc_parse_expr_t* expr
		= (ofc_parse_expr_t*)malloc(
			sizeof(ofc_parse_expr_t));
	if (!expr)
	{
		ofc_parse_literal_cleanup(literal);
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	expr->type    = OFC_PARSE_EXPR_CONSTANT;
	expr->src     = ofc_sparse_ref(src, ptr, l);
	expr->literal = literal;

	if (len) *len = l;
	return expr;
}

ofc_parse_expr_t* ofc_parse_expr_integer_variable(
       const ofc_sparse_t* src, const char* ptr,
       ofc_parse_debug_t* debug, unsigned* len)
{
	ofc_parse_expr_t* expr
		= ofc_parse_expr_integer(
			src, ptr, debug, len);
	if (expr) return expr;

	unsigned dpos = ofc_parse_debug_position(debug);

	unsigned l;
	ofc_parse_lhs_t* variable
		= ofc_parse_lhs_variable(src, ptr, debug, &l);
	if (!variable) return NULL;

	expr = (ofc_parse_expr_t*)malloc(
		sizeof(ofc_parse_expr_t));
	if (!expr)
	{
		ofc_parse_lhs_delete(variable);
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	expr->type = OFC_PARSE_EXPR_VARIABLE;
	expr->src  = ofc_sparse_ref(src, ptr, l);
	expr->variable = variable;

	if (len) *len = l;
	return expr;
}

static ofc_parse_expr_t* ofc_parse_expr__primary(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug, unsigned* len)
{
	ofc_parse_expr_t* expr
		= ofc_parse_expr__literal(
			src, ptr, debug, len);
	if (expr) return expr;

	unsigned dpos = ofc_parse_debug_position(debug);

	unsigned l;
	ofc_parse_lhs_t* variable
		= ofc_parse_lhs(src, ptr, debug, &l);
	if (variable)
	{
		expr = (ofc_parse_expr_t*)malloc(
			sizeof(ofc_parse_expr_t));
		if (!expr)
		{
			ofc_parse_lhs_delete(variable);
			ofc_parse_debug_rewind(debug, dpos);
			return NULL;
		}

		expr->type = OFC_PARSE_EXPR_VARIABLE;
		expr->src  = ofc_sparse_ref(src, ptr, l);
		expr->variable = variable;

		if (len) *len = l;
		return expr;
	}

	if (ptr[0] != '(')
		return NULL;

	ofc_parse_expr_t* expr_brackets
		= ofc_parse_expr(
			src, &ptr[1], debug, &l);
	if (!expr_brackets) return NULL;

	if  (ptr[1 + l] != ')')
	{
		ofc_parse_expr_delete(expr_brackets);
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	l += 2;

	expr = (ofc_parse_expr_t*)malloc(
		sizeof(ofc_parse_expr_t));
	if (!expr)
	{
		ofc_parse_expr_delete(expr_brackets);
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	expr->type = OFC_PARSE_EXPR_BRACKETS;
	expr->src  = ofc_sparse_ref(src, ptr, l);
	expr->brackets.expr = expr_brackets;

	if (len) *len = l;
	return expr;
}

static ofc_parse_expr_t* ofc_parse_expr__unary(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug, unsigned *len)
{
	/* TODO - Defined unary operators. */

	unsigned dpos = ofc_parse_debug_position(debug);

	ofc_parse_operator_e op;
	unsigned op_len = ofc_parse_operator(
		src, ptr, debug, &op);
	if ((op_len == 0)
		|| !ofc_parse_operator_unary(op))
	{
		ofc_parse_debug_rewind(debug, dpos);
		return ofc_parse_expr__primary(
			src, ptr, debug, len);
	}

	unsigned l;
	ofc_parse_expr_t* expr_unary
		= ofc_parse_expr__primary(src, &ptr[op_len], debug, &l);
	if (!expr_unary)
	{
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}
	l += op_len;

	ofc_parse_expr_t* expr
		= (ofc_parse_expr_t*)malloc(
			sizeof(ofc_parse_expr_t));
	if (!expr)
	{
		ofc_parse_expr_delete(expr_unary);
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	expr->type = OFC_PARSE_EXPR_UNARY;
	expr->src  = ofc_sparse_ref(src, ptr, l);
	expr->unary.operator = op;
	expr->unary.a = expr_unary;

	if (len) *len = l;
	return expr;
}

static bool ofc_parse_expr__has_right_ambig_point(
	ofc_parse_expr_t* expr)
{
	if (!expr)
		return false;

	switch (expr->type)
	{
		case OFC_PARSE_EXPR_CONSTANT:
			return ((expr->literal.type == OFC_PARSE_LITERAL_NUMBER)
				&& (expr->literal.number.base[expr->literal.number.size - 1] == '.'));
		case OFC_PARSE_EXPR_UNARY:
			return ofc_parse_expr__has_right_ambig_point(expr->unary.a);
		case OFC_PARSE_EXPR_BINARY:
			return ofc_parse_expr__has_right_ambig_point(expr->binary.b);
		default:
			break;
	}
	return false;
}

static bool ofc_parse_expr__cull_right_ambig_point(
	ofc_parse_expr_t* expr)
{
	if (!expr)
		return false;

	switch (expr->type)
	{
		case OFC_PARSE_EXPR_CONSTANT:
			if (expr->literal.type == OFC_PARSE_LITERAL_NUMBER)
			{
				expr->literal.number.size -= 1;
				expr->src.string.size     -= 1;
			}
			return true;

		case OFC_PARSE_EXPR_UNARY:
			if (ofc_parse_expr__cull_right_ambig_point(expr->unary.a))
			{
				expr->src.string.size -= 1;
				return true;
			}
			break;
		case OFC_PARSE_EXPR_BINARY:
			if (ofc_parse_expr__cull_right_ambig_point(expr->binary.b))
			{
				expr->src.string.size -= 1;
				return true;
			}
			break;
		default:
			break;
	}

	return false;
}

static ofc_parse_expr_t* ofc_parse_expr__binary_insert(
	ofc_parse_operator_e op, unsigned op_prec,
	ofc_parse_expr_t* a, ofc_parse_expr_t* b)
{
	if (!a || !b)
		return NULL;

	unsigned a_prec = ofc_parse_expr_precedence(a);
	if (a_prec <= op_prec)
	{
		ofc_parse_expr_t* expr
			= (ofc_parse_expr_t*)malloc(
				sizeof(ofc_parse_expr_t));
		if (!expr) return NULL;

		expr->type = OFC_PARSE_EXPR_BINARY;
		if (!ofc_sparse_ref_bridge(
			a->src, b->src, &expr->src))
			abort();

		expr->binary.a = a;
		expr->binary.b = b;
		expr->binary.operator = op;

		return expr;
	}


	if (a->type == OFC_PARSE_EXPR_UNARY)
	{
		ofc_parse_expr_t* c
			= ofc_parse_expr__binary_insert(op, op_prec, a->unary.a, b);
		if (!c) return NULL;

		a->unary.a = c;
		if (!ofc_sparse_ref_bridge(
			a->src, c->src, &a->src))
			abort();
		return a;
	}
	else if (a->type == OFC_PARSE_EXPR_BINARY)
	{
		ofc_parse_expr_t* c
			= ofc_parse_expr__binary_insert(op, op_prec, a->binary.b, b);
		if (!c) return NULL;

		a->binary.b = c;
		if (!ofc_sparse_ref_bridge(
			a->src, c->src, &a->src))
			abort();
		return a;
	}

	return NULL;
}

static ofc_parse_expr_t* ofc_parse_expr__binary(
	ofc_parse_expr_t* a,
	ofc_parse_debug_t* debug,
	bool no_slash)
{
	if (!a) return NULL;

	unsigned dpos = ofc_parse_debug_position(debug);

	const ofc_sparse_t* src = a->src.sparse;
	const char* ptr = a->src.string.base;
	unsigned a_len = a->src.string.size;

	ofc_parse_operator_e op;
	unsigned op_len = 0;

	/* TODO - Defined binary operators. */

	/* Handle case where we have something like:
	   ( 3 ** 3 .EQ. 76 ) */
	if (ofc_parse_expr__has_right_ambig_point(a))
	{
		op_len = ofc_parse_operator(
			src, &ptr[a_len - 1], debug, &op);
		if ((op_len > 0)
			&& ofc_parse_operator_binary(op)
			&& (!no_slash || (op != OFC_PARSE_OPERATOR_DIVIDE)))
		{
			ofc_parse_expr__cull_right_ambig_point(a);
			a_len = a->src.string.size;
		}
		else
		{
			op_len = 0;
			ofc_parse_debug_rewind(debug, dpos);
		}
	}

	if (op_len == 0)
	{
		op_len = ofc_parse_operator(
			src, &ptr[a_len], debug, &op);
		if ((op_len == 0)
			|| !ofc_parse_operator_binary(op)
			|| (no_slash && (op == OFC_PARSE_OPERATOR_DIVIDE)))
		{
			ofc_parse_debug_rewind(debug, dpos);
			return NULL;
		}
	}

	unsigned b_len;
	ofc_parse_expr_t* b
		= ofc_parse_expr__unary(
			src, &ptr[a_len + op_len], debug, &b_len);
	if (!b)
	{
		ofc_parse_debug_rewind(debug, dpos);
		return NULL;
	}

	unsigned op_prec = ofc_parse_operator_precedence_binary(op);
	ofc_parse_expr_t* expr
		= ofc_parse_expr__binary_insert(
			op, op_prec, a, b);
	if (!expr)
	{
		ofc_parse_debug_rewind(debug, dpos);
		ofc_parse_expr_delete(b);
		return NULL;
	}

	return expr;
}

static ofc_parse_expr_t* ofc_parse__expr(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len, bool no_slash)
{
	ofc_parse_expr_t* a = ofc_parse_expr__unary(
		src, ptr, debug, NULL);
	if (!a) return NULL;

	ofc_parse_expr_t* c;
	for (c = ofc_parse_expr__binary(a, debug, no_slash); c != NULL;
		a = c, c = ofc_parse_expr__binary(a, debug, no_slash));

	if (len) *len = a->src.string.size;
	return a;
}

ofc_parse_expr_t* ofc_parse_expr(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len)
{
	return ofc_parse__expr(
		src, ptr, debug, len, false);
}

ofc_parse_expr_t* ofc_parse_expr_id(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len)
{
	unsigned dpos = ofc_parse_debug_position(debug);

	unsigned l;
	ofc_parse_expr_implicit_do_t* id
		= ofc_parse_expr_implicit_do(
			src, ptr, debug, &l);
	if (id)
	{
		ofc_parse_expr_t* expr
			= (ofc_parse_expr_t*)malloc(
				sizeof(ofc_parse_expr_t));
		if (!expr)
		{
			ofc_parse_expr_implicit_do_delete(id);
			ofc_parse_debug_rewind(debug, dpos);
			return NULL;
		}

		expr->type = OFC_PARSE_EXPR_IMPLICIT_DO;
		expr->src  = ofc_sparse_ref(src, ptr, l);
		expr->implicit_do = id;

		if (len) *len = l;
		return expr;
	}

	return ofc_parse__expr(
		src, ptr, debug, len, false);
}

ofc_parse_expr_t* ofc_parse_expr_no_slash(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len)
{
	return ofc_parse__expr(
		src, ptr, debug, len, true);
}

void ofc_parse_expr_delete(
	ofc_parse_expr_t* expr)
{
	if (!expr)
		return;

	switch (expr->type)
	{
		case OFC_PARSE_EXPR_CONSTANT:
			ofc_parse_literal_cleanup(expr->literal);
			break;

		case OFC_PARSE_EXPR_VARIABLE:
			ofc_parse_lhs_delete(expr->variable);
			break;

		case OFC_PARSE_EXPR_BRACKETS:
			ofc_parse_expr_delete(expr->brackets.expr);
			break;

		case OFC_PARSE_EXPR_UNARY:
			ofc_parse_expr_delete(expr->unary.a);
			break;

		case OFC_PARSE_EXPR_BINARY:
			ofc_parse_expr_delete(expr->binary.a);
			ofc_parse_expr_delete(expr->binary.b);
			break;
		case OFC_PARSE_EXPR_IMPLICIT_DO:
			ofc_parse_expr_implicit_do_delete(expr->implicit_do);
			break;

		default:
			break;
	}

	free(expr);
}

bool ofc_parse_expr_print(
	ofc_colstr_t* cs, const ofc_parse_expr_t* expr)
{
	if (!expr)
		return false;

	switch(expr->type)
	{
		case OFC_PARSE_EXPR_CONSTANT:
			if (!ofc_parse_literal_print(
				cs, expr->literal))
				return false;
			break;
		case OFC_PARSE_EXPR_VARIABLE:
			if (!ofc_parse_lhs_print(
				cs, expr->variable, false))
				return false;
			break;
		case OFC_PARSE_EXPR_BRACKETS:
			if (!ofc_colstr_atomic_writef(cs, "(")
				|| !ofc_parse_expr_print(
					cs, expr->brackets.expr)
				|| !ofc_colstr_atomic_writef(cs, ")"))
				return false;
			break;
		case OFC_PARSE_EXPR_UNARY:
			if (!ofc_parse_operator_print(cs, expr->unary.operator)
				|| !ofc_parse_expr_print(cs, expr->unary.a))
				return false;
			break;
		case OFC_PARSE_EXPR_BINARY:
			if (!ofc_parse_expr_print(cs, expr->binary.a)
				|| !ofc_colstr_atomic_writef(cs, " ")
				|| !ofc_parse_operator_print(cs, expr->binary.operator)
				|| !ofc_colstr_atomic_writef(cs, " ")
				|| !ofc_parse_expr_print(cs, expr->binary.b))
				return false;
			break;

		default:
			return false;
	}

	return true;
}

ofc_parse_expr_t* ofc_parse_expr_copy(
	const ofc_parse_expr_t* expr)
{
	if (!expr)
		return NULL;

	ofc_parse_expr_t* copy
		= (ofc_parse_expr_t*)malloc(
			sizeof(ofc_parse_expr_t));
	if (!copy) return NULL;
	*copy = *expr;

	bool success = true;
	switch (copy->type)
	{
		case OFC_PARSE_EXPR_CONSTANT:
			if (!ofc_parse_literal_clone(
				&copy->literal, &expr->literal))
				success = false;
			break;

		case OFC_PARSE_EXPR_VARIABLE:
			copy->variable = ofc_parse_lhs_copy(expr->variable);
			if (!copy->variable)
				success = false;
			break;

		case OFC_PARSE_EXPR_BRACKETS:
			copy->brackets.expr
				= ofc_parse_expr_copy(expr->brackets.expr);
			if (!copy->brackets.expr)
				success = false;
			break;

		case OFC_PARSE_EXPR_UNARY:
			copy->unary.operator = expr->unary.operator;

			copy->unary.a
				= ofc_parse_expr_copy(expr->unary.a);
			if (!copy->unary.a)
				success = false;
			break;

		case OFC_PARSE_EXPR_BINARY:
			copy->binary.operator = expr->binary.operator;

			copy->binary.a
				= ofc_parse_expr_copy(expr->binary.a);
			if (!copy->binary.a)
				success = false;

			copy->binary.b
				= ofc_parse_expr_copy(expr->binary.b);
			if (!copy->binary.b)
				success = false;
			break;

		default:
			success = false;
			break;
	}

	if (!success)
	{
		ofc_parse_expr_delete(copy);
		copy = NULL;
	}

	return copy;
}



static ofc_parse_expr_list_t* ofc_parse_expr__list(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len, bool no_slash)
{
	ofc_parse_expr_list_t* list
		= (ofc_parse_expr_list_t*)malloc(
			sizeof(ofc_parse_expr_list_t));
	if (!list) return NULL;

	list->count = 0;
	list->expr = NULL;

	unsigned l = ofc_parse_list(
		src, ptr, debug,
		',', &list->count, (void***)&list->expr,
		(void*)(no_slash
			? ofc_parse_expr_no_slash
			: ofc_parse_expr_id),
		(void*)ofc_parse_expr_delete);
	if (l == 0)
	{
		free(list);
		return NULL;
	}

	if (len) *len = l;
	return list;
}

ofc_parse_expr_list_t* ofc_parse_expr_list(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len)
{
	return ofc_parse_expr__list(
		src, ptr, debug, len, false);
}

ofc_parse_expr_list_t* ofc_parse_expr_clist(
	const ofc_sparse_t* src, const char* ptr,
	ofc_parse_debug_t* debug,
	unsigned* len)
{
	unsigned i = 0;
	if (ptr[i++] != '/')
		return NULL;

	unsigned l;
	ofc_parse_expr_list_t* clist
		= ofc_parse_expr__list(
			src, &ptr[i], debug, &l, true);
	if (!clist) return NULL;
	i += l;

	if (ptr[i++] != '/')
	{
		ofc_parse_expr_list_delete(clist);
		return NULL;
	}

	if (len) *len = i;
	return clist;
}

ofc_parse_expr_list_t* ofc_parse_expr_list_copy(
	const ofc_parse_expr_list_t* list)
{
	if (!list)
		return NULL;

	ofc_parse_expr_list_t* copy
		= (ofc_parse_expr_list_t*)malloc(
			sizeof(ofc_parse_expr_list_t));
	if (!copy) return NULL;

	copy->count = 0;
	copy->expr = NULL;

	if (!ofc_parse_list_copy(
		&copy->count, (void***)&copy->expr,
		list->count, (const void**)list->expr,
		(void*)ofc_parse_expr_copy,
		(void*)ofc_parse_expr_delete))
	{
		free(copy);
		return NULL;
	}

	return copy;
}

void ofc_parse_expr_list_delete(
	ofc_parse_expr_list_t* list)
{
	if (!list)
		return;

	ofc_parse_list_delete(
		list->count, (void**)list->expr,
		(void*)ofc_parse_expr_delete);
	free(list);
}

bool ofc_parse_expr_list_print(
	ofc_colstr_t* cs, const ofc_parse_expr_list_t* list)
{
	if (!list)
		return false;

	return ofc_parse_list_print(
		cs, list->count, (const void**)list->expr,
		(void*)ofc_parse_expr_print);
}
