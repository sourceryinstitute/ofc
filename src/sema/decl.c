#include <ofc/sema.h>


static ofc_sema_decl_t* ofc_sema_decl__create(
	const ofc_sema_type_t* type,
	ofc_str_ref_t name, bool is_implicit)
{
	ofc_sema_decl_t* decl
		= (ofc_sema_decl_t*)malloc(
			sizeof(ofc_sema_decl_t));
	if (!decl) return NULL;

	decl->type = type;
	decl->name = name;
	decl->func = NULL;

	if (ofc_sema_type_is_composite(type))
	{
		decl->init_array = NULL;
	}
	else
	{
		decl->init = NULL;
	}

	decl->equiv = NULL;

	decl->is_implicit  = is_implicit;
	decl->is_static    = (type ? type->is_static    : false);
	decl->is_volatile  = (type ? type->is_volatile  : false);
	decl->is_automatic = (type ? type->is_automatic : false);
	decl->is_target    = false;

	decl->lock = false;
	return decl;
}

ofc_sema_decl_t* ofc_sema_decl_create(
	const ofc_sema_type_t* type,
	ofc_str_ref_t name)
{
	return ofc_sema_decl__create(type, name, false);
}

ofc_sema_decl_t* ofc_sema_decl_implicit_untyped(
	ofc_str_ref_t name)
{
	return ofc_sema_decl__create(NULL, name, true);
}

static ofc_sema_decl_t* ofc_sema_decl_implicit__name(
	const ofc_sema_scope_t* scope,
	ofc_str_ref_t name, const ofc_sema_array_t* array)
{
	if (!scope)
		return NULL;

	if (ofc_str_ref_empty(name))
		return NULL;

	const ofc_sema_type_t* type
		= ofc_sema_implicit_get(
			scope->implicit, name.base[0]);

	if (array)
	{
		type = ofc_sema_type_create_array(
			type, array,
			type->is_static,
			type->is_automatic,
			type->is_volatile);
	}

	return ofc_sema_decl__create(
		type, name, true);
}

ofc_sema_decl_t* ofc_sema_decl_implicit_name(
	const ofc_sema_scope_t* scope,
	ofc_str_ref_t name)
{
	return ofc_sema_decl_implicit__name(
		scope, name, NULL);
}

ofc_sema_decl_t* ofc_sema_decl_implicit_lhs(
	ofc_sema_scope_t*      scope,
	const ofc_parse_lhs_t* lhs)
{
	if (!scope || !lhs)
		return NULL;

	ofc_sema_array_t* array = NULL;
	ofc_str_ref_t base_name;
	if (!ofc_parse_lhs_base_name(
		*lhs, &base_name))
		return NULL;

    switch (lhs->type)
	{
		case OFC_PARSE_LHS_VARIABLE:
			break;
		case OFC_PARSE_LHS_ARRAY:
			array = ofc_sema_array(
				scope, lhs->array.index);
			if (!array) return NULL;
			break;
		default:
			break;
	}

	ofc_sema_decl_t* decl
		= ofc_sema_decl_implicit__name(
			scope, base_name, array);
	ofc_sema_array_delete(array);
	return decl;
}

static bool ofc_sema_decl__decl(
	ofc_sema_scope_t* scope,
	const ofc_sema_type_t*  type,
	const ofc_parse_decl_t* decl)
{
	if (!decl || !type
		|| !decl->lhs)
		return false;

	const ofc_parse_lhs_t* lhs = decl->lhs;
	if (lhs->type == OFC_PARSE_LHS_STAR_LEN)
	{
		if (lhs->star_len.var)
		{
			/* TODO - Support this? */
			ofc_sema_scope_error(scope, lhs->src,
				"Variable length star length in decl list");
			return false;
		}

		ofc_sema_expr_t* expr
			= ofc_sema_expr(scope, lhs->star_len.len);
		if (!expr) return false;

		unsigned star_len;
		bool resolved = ofc_sema_expr_resolve_uint(expr, &star_len);
		ofc_sema_expr_delete(expr);

		if (!resolved)
		{
			ofc_sema_scope_error(scope, lhs->src,
				"Star length must be a positive whole integer");
			return false;
		}

		type = ofc_sema_type_star_len(
			type, star_len);
		if (!type) return false;

		lhs = lhs->parent;
		if (!lhs) return false;
	}

	if (lhs->type == OFC_PARSE_LHS_ARRAY)
	{
		ofc_sema_array_t* darray
			= ofc_sema_array(scope, lhs->array.index);
		if (!darray) return false;

		if (type->array)
		{
			if (!ofc_sema_array_compare(
				type->array, darray))
			{
				ofc_sema_scope_error(scope, lhs->src,
					"Conflicting array definitions in declaration");
				ofc_sema_array_delete(darray);
				return false;
			}

			ofc_sema_scope_warning(scope, lhs->src,
				"Multiple array definitions in declaration");
		}

		type = ofc_sema_type_create_array(type, darray,
			type->is_static, type->is_automatic, type->is_volatile);
		ofc_sema_array_delete(darray);
		if (!type) return false;

		lhs = lhs->parent;
		if (!lhs) return false;
	}

	if (lhs->type != OFC_PARSE_LHS_VARIABLE)
		return false;

	ofc_str_ref_t base_name;
	if (!ofc_parse_lhs_base_name(
		*lhs, &base_name))
		return false;

	ofc_sema_decl_t* sdecl
		= ofc_sema_scope_decl_find_modify(
			scope, base_name, true);
	bool exist = (sdecl != NULL);

	if (exist)
	{
		const ofc_sema_type_t* ebase_type
			= ofc_sema_decl_base_type(sdecl);
		if (!ofc_sema_type_compare(
			ebase_type, type))
		{
			if (sdecl->lock)
			{
				/* TODO - Support this where possible. */
				ofc_sema_scope_error(scope, lhs->src,
					"Redefining type after initialization");
				return false;
			}
			else if (!sdecl->is_implicit)
			{
				ofc_sema_scope_error(scope, lhs->src,
					"Redeclaration with different type");
				return false;
			}
		}

		ofc_sema_array_t* array = type->array;
		if (array)
		{
			if (ofc_sema_decl_is_array(sdecl)
				&& !ofc_sema_array_compare(
					type->array, sdecl->type->array))
			{
				/* TODO - Support adding dimensions together? */
				ofc_sema_scope_error(scope, lhs->src,
					"Redefining array dimensions");
				return false;
			}
		}
		else if (ofc_sema_decl_is_array(sdecl))
		{
			array = sdecl->type->array;
		}

		const ofc_sema_type_t* atype = type;
		if (array)
		{
			atype = ofc_sema_type_create_array(
				type, array, false, false ,false);
			if (!atype) return false;
		}

		sdecl->type = atype;
		sdecl->is_implicit = false;

		sdecl->is_static    |= type->is_static;
		sdecl->is_automatic |= type->is_automatic;
		sdecl->is_volatile  |= type->is_volatile;
	}
	else
	{
		sdecl = ofc_sema_decl_create(
			type, base_name);
		if (!sdecl) return false;
	}

	if (decl->init_expr)
	{
		ofc_sema_expr_t* init_expr
			= ofc_sema_expr(scope, decl->init_expr);
		if (!init_expr)
		{
			ofc_sema_decl_delete(sdecl);
			return false;
		}

		bool initialized = ofc_sema_decl_init(
			scope, sdecl, init_expr);
		ofc_sema_expr_delete(init_expr);

		if (!initialized)
		{
			ofc_sema_decl_delete(sdecl);
			return false;
		}
	}
	else if (decl->init_clist)
	{
		/* TODO - CList initializer resolution. */
		ofc_sema_decl_delete(sdecl);
		return false;
	}

	if (!exist && !ofc_sema_decl_list_add(
		scope->decl, sdecl))
	{
		ofc_sema_decl_delete(sdecl);
		return false;
	}

	return true;
}

bool ofc_sema_decl(
	ofc_sema_scope_t* scope,
	const ofc_parse_stmt_t* stmt)
{
	if (!stmt || !scope || !scope->decl
		|| !stmt->decl.type || !stmt->decl.decl
		|| (stmt->type != OFC_PARSE_STMT_DECL))
		return false;

	const ofc_sema_type_t* type = ofc_sema_type(
		scope, stmt->decl.type);
	if (!type) return false;

	unsigned count = stmt->decl.decl->count;
	if (count == 0) return false;

	unsigned i;
	for (i = 0; i < count; i++)
	{
		if (!ofc_sema_decl__decl(
			scope, type, stmt->decl.decl->decl[i]))
			return false;
	}

	return true;
}

void ofc_sema_decl_delete(
	ofc_sema_decl_t* decl)
{
	if (!decl)
		return;

	if (ofc_sema_decl_is_composite(decl)
		&& decl->init_array)
	{
		unsigned count
			= ofc_sema_decl_elem_count(decl);

		unsigned i;
		for (i = 0; i < count; i++)
			ofc_sema_typeval_delete(decl->init_array[i]);

		free(decl->init_array);
	}
	else
	{
		ofc_sema_typeval_delete(decl->init);
	}

	ofc_sema_scope_delete(decl->func);
	ofc_sema_equiv_delete(decl->equiv);
	free(decl);
}


bool ofc_sema_decl_init(
	const ofc_sema_scope_t* scope,
	ofc_sema_decl_t* decl,
	const ofc_sema_expr_t* init)
{
	if (!decl || !init || !decl->type
		|| ofc_sema_decl_is_procedure(decl))
		return false;

	if (decl->lock)
	{
		ofc_sema_scope_error(scope, init->src,
			"Can't initialize finalized declaration.");
		return false;
	}

    const ofc_sema_typeval_t* ctv
		= ofc_sema_expr_constant(init);
	if (!ctv)
	{
		ofc_sema_scope_error(scope, init->src,
			"Initializer element not constant.");
		return false;
	}

	if (ofc_sema_decl_is_composite(decl))
	{
		/* TODO - Support F90 "(/ 0, 1 /)" array initializers. */
		ofc_sema_scope_error(scope, init->src,
			"Can't initialize non-scalar declaration with expression.");
		return false;
	}

	ofc_sema_typeval_t* tv
		= ofc_sema_typeval_cast(
			scope, ctv, decl->type);
	if (!tv) return false;

	if (ofc_sema_decl_is_locked(decl))
	{
		bool redecl = ofc_sema_typeval_compare(
			tv, decl->init);
		ofc_sema_typeval_delete(tv);

		if (redecl)
		{
			ofc_sema_scope_warning(scope, init->src,
				"Duplicate initialization.");
		}
		else
		{
			/* TODO - Print where declaration was first used. */
			ofc_sema_scope_error(scope, init->src,
				"Can't initialize declaration after use.");
			return false;
		}
	}

	decl->init = tv;
	decl->lock = true;
	return true;
}

bool ofc_sema_decl_init_offset(
	const ofc_sema_scope_t* scope,
	ofc_sema_decl_t* decl,
	unsigned offset,
	const ofc_sema_expr_t* init)
{
	if (!decl || !init || !decl->type
		|| ofc_sema_decl_is_procedure(decl))
		return false;

	if (decl->lock)
	{
		ofc_sema_scope_warning(scope, init->src,
			"Initializing array in multiple statements");
	}

	if (!decl->type)
		return NULL;

	if (!ofc_sema_type_is_array(decl->type))
	{
		if (offset == 0)
			return ofc_sema_decl_init(
				scope, decl, init);
		return false;
	}

	unsigned elem_count
		= ofc_sema_decl_elem_count(decl);
	if (offset >= elem_count)
	{
		ofc_sema_scope_warning(scope, init->src,
			"Initializer destination out-of-bounds");
		return false;
	}

	if (!decl->init_array)
	{
		decl->init_array = (ofc_sema_typeval_t**)malloc(
			sizeof(ofc_sema_typeval_t*) * elem_count);
		if (!decl->init_array) return false;

		unsigned i;
		for (i = 0; i < elem_count; i++)
			decl->init_array[i] = NULL;
	}

	const ofc_sema_typeval_t* ctv
		= ofc_sema_expr_constant(init);
	if (!ctv)
	{
		ofc_sema_scope_error(scope, init->src,
			"Array initializer element not constant.");
		return false;
	}

	ofc_sema_typeval_t* tv = ofc_sema_typeval_cast(
		scope, ctv, ofc_sema_type_base(decl->type));
	if (!tv) return false;

	if (decl->init_array[offset])
	{
		bool equal = ofc_sema_typeval_compare(
			decl->init_array[offset], tv);
		ofc_sema_typeval_delete(tv);

		if (!equal)
		{
			ofc_sema_scope_error(scope, init->src,
				"Re-initialization of array element"
				" with different value");
			return false;
		}

		ofc_sema_scope_warning(scope, init->src,
			"Re-initialization of array element");
	}
	else
	{
		decl->init_array[offset] = tv;
	}

	decl->lock = true;
	return true;
}

bool ofc_sema_decl_init_array(
	const ofc_sema_scope_t* scope,
	ofc_sema_decl_t* decl,
	const ofc_sema_array_t* array,
	unsigned count,
	const ofc_sema_expr_t** init)
{
	if (!decl || !init
		|| ofc_sema_decl_is_procedure(decl))
		return false;

	if (count == 0)
		return true;

	if (decl->lock)
	{
		ofc_sema_scope_warning(scope, init[0]->src,
			"Initializing arrays in multiple statements.");
	}

	if (!decl->type)
		return false;

	if (!ofc_sema_type_is_array(decl->type))
	{
		if (!array && (count == 1))
			return ofc_sema_decl_init(
				scope, decl, init[0]);
		return false;
	}

	unsigned elem_count
		= ofc_sema_type_elem_count(decl->type);
	if (elem_count == 0) return true;

	if (!decl->init_array)
	{
		decl->init_array = (ofc_sema_typeval_t**)malloc(
			sizeof(ofc_sema_typeval_t*) * elem_count);
		if (!decl->init_array) return false;

		unsigned i;
		for (i = 0; i < elem_count; i++)
			decl->init_array[i] = NULL;
	}

	if (!array)
	{
		if (count > elem_count)
		{
			ofc_sema_scope_warning(scope, init[0]->src,
				"Array initializer too large, truncating.");
			count = elem_count;
		}

		unsigned i;
		for (i = 0; i < count; i++)
		{
			const ofc_sema_typeval_t* ctv
				= ofc_sema_expr_constant(init[i]);
			if (!ctv)
			{
				ofc_sema_scope_error(scope, init[i]->src,
					"Array initializer element not constant.");
				return false;
			}

			ofc_sema_typeval_t* tv = ofc_sema_typeval_cast(
				scope, ctv, ofc_sema_type_base(decl->type));
			if (!tv) return false;

			if (decl->init_array[i])
			{
				bool equal = ofc_sema_typeval_compare(
					decl->init_array[i], tv);
				ofc_sema_typeval_delete(tv);

				if (!equal)
				{
					ofc_sema_scope_error(scope, init[i]->src,
						"Re-initialization of array element"
						" with different value");
					return false;
				}

				ofc_sema_scope_warning(scope, init[i]->src,
					"Re-initialization of array element");
			}
			else
			{
				decl->init_array[i] = tv;
			}
		}
	}
	else
	{
		/* TODO - Initialize array slice. */
		return false;
	}

	decl->lock = true;
	return true;
}

bool ofc_sema_decl_init_func(
	ofc_sema_decl_t* decl,
	ofc_sema_scope_t* func)
{
	if (!ofc_sema_decl_is_procedure(decl))
		return false;

	if (decl->func)
		return (decl->func == func);

	if (decl->lock || decl->equiv)
		return false;

	decl->func = func;
	decl->lock = true;
	return true;
}


unsigned ofc_sema_decl_size(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return 0;
	return ofc_sema_type_size(
		decl->type);
}

unsigned ofc_sema_decl_elem_count(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return 0;
	return ofc_sema_type_elem_count(
		decl->type);
}

bool ofc_sema_decl_is_array(
	const ofc_sema_decl_t* decl)
{
	return (decl && ofc_sema_type_is_array(decl->type));
}

bool ofc_sema_decl_is_composite(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return false;
	return ofc_sema_type_is_composite(decl->type);
}

bool ofc_sema_decl_is_locked(
	const ofc_sema_decl_t* decl)
{
	return (decl && decl->lock);
}


bool ofc_sema_decl_is_subroutine(
	const ofc_sema_decl_t* decl)
{
	return (decl && ofc_sema_type_is_subroutine(decl->type));
}

bool ofc_sema_decl_is_function(
	const ofc_sema_decl_t* decl)
{
	return (decl && ofc_sema_type_is_function(decl->type));
}

bool ofc_sema_decl_is_procedure(
	const ofc_sema_decl_t* decl)
{
	return (ofc_sema_decl_is_subroutine(decl)
		|| ofc_sema_decl_is_function(decl));
}


const ofc_sema_type_t* ofc_sema_decl_type(
	const ofc_sema_decl_t* decl)
{
	return (decl ? decl->type : NULL);
}

const ofc_sema_type_t* ofc_sema_decl_base_type(
	const ofc_sema_decl_t* decl)
{
	if (!decl)
		return NULL;

	return ofc_sema_type_base(decl->type);
}



static const ofc_str_ref_t* ofc_sema_decl__key(
	const ofc_sema_decl_t* decl)
{
	return (decl ? &decl->name : NULL);
}

bool ofc_sema_decl_list__remap(
	ofc_sema_decl_list_t* list)
{
    if (!list)
		return false;

	if (list->map)
		ofc_hashmap_delete(list->map);



	return (list->map != NULL);
}

ofc_sema_decl_list_t* ofc_sema_decl_list__create(
	bool case_sensitive, bool is_ref)
{
	ofc_sema_decl_list_t* list
		= (ofc_sema_decl_list_t*)malloc(
			sizeof(ofc_sema_decl_list_t));
	if (!list) return NULL;

	list->case_sensitive = case_sensitive;

	list->count  = 0;
	list->decl   = NULL;
	list->is_ref = is_ref;

	list->map = ofc_hashmap_create(
		(void*)(list->case_sensitive
			? ofc_str_ref_ptr_hash
			: ofc_str_ref_ptr_hash_ci),
		(void*)(list->case_sensitive
			? ofc_str_ref_ptr_equal
			: ofc_str_ref_ptr_equal_ci),
		(void*)ofc_sema_decl__key, NULL);
	if (!list->map)
	{
		free(list);
		return NULL;
	}

	return list;
}

ofc_sema_decl_list_t* ofc_sema_decl_list_create(
	bool case_sensitive)
{
	return ofc_sema_decl_list__create(
		case_sensitive, false);
}

ofc_sema_decl_list_t* ofc_sema_decl_list_create_ref(
	bool case_sensitive)
{
	return ofc_sema_decl_list__create(
		case_sensitive, true);
}

void ofc_sema_decl_list_delete(
	ofc_sema_decl_list_t* list)
{
	if (!list)
		return;

	ofc_hashmap_delete(list->map);

	if (!list->is_ref)
	{
		unsigned i;
		for (i = 0; i < list->count; i++)
			ofc_sema_decl_delete(list->decl[i]);
	}

	free(list->decl);

	free(list);
}

bool ofc_sema_decl_list_add(
	ofc_sema_decl_list_t* list,
	ofc_sema_decl_t* decl)
{
	if (!list || !decl
		|| list->is_ref)
		return false;

	/* Check for duplicate definitions. */
	if (ofc_sema_decl_list_find(
		list, decl->name))
		return false;

    ofc_sema_decl_t** ndecl
		= (ofc_sema_decl_t**)realloc(list->decl,
			(sizeof(ofc_sema_decl_t*) * (list->count + 1)));
	if (!ndecl) return false;
	list->decl = ndecl;

	if (!ofc_hashmap_add(
		list->map, decl))
		return false;

	list->decl[list->count++] = decl;
	return true;
}

bool ofc_sema_decl_list_add_ref(
	ofc_sema_decl_list_t* list,
	const ofc_sema_decl_t* decl)
{
	if (!list || !decl
		|| !list->is_ref)
		return false;

	/* Check for duplicate definitions. */
	if (ofc_sema_decl_list_find(
		list, decl->name))
		return false;

    const ofc_sema_decl_t** ndecl
		= (const ofc_sema_decl_t**)realloc(list->decl_ref,
			(sizeof(const ofc_sema_decl_t*) * (list->count + 1)));
	if (!ndecl) return false;
	list->decl_ref = ndecl;

	if (!ofc_hashmap_add(
		list->map, (void*)decl))
		return false;

	list->decl_ref[list->count++] = decl;
	return true;
}

const ofc_sema_decl_t* ofc_sema_decl_list_find(
	const ofc_sema_decl_list_t* list, ofc_str_ref_t name)
{
	if (!list)
		return NULL;

	return ofc_hashmap_find(
		list->map, &name);
}

ofc_sema_decl_t* ofc_sema_decl_list_find_modify(
	ofc_sema_decl_list_t* list, ofc_str_ref_t name)
{
	if (!list)
		return NULL;

	return ofc_hashmap_find_modify(
		list->map, &name);
}

const ofc_hashmap_t* ofc_sema_decl_list_map(
	const ofc_sema_decl_list_t* list)
{
	return (list ? list->map : NULL);
}
