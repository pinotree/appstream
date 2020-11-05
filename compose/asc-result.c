/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:asc-result
 * @short_description: A composer result for a single unit.
 * @include: appstream-compose.h
 */

#include "config.h"
#include "asc-result.h"

#include "as-utils-private.h"
#include "asc-utils.h"
#include "asc-globals.h"

typedef struct
{
	AsBundleKind	bundle_kind;
	gchar		*bundle_id;

	GHashTable	*cpts; /* utf8->AsComponent */
	GHashTable	*mdata_hashes; /* AsComponent->utf8 */
	GHashTable	*hints; /* utf8->GPtrArray */
} AscResultPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (AscResult, asc_result, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (asc_result_get_instance_private (o))

static void
asc_result_finalize (GObject *object)
{
	AscResult *result = ASC_RESULT (object);
	AscResultPrivate *priv = GET_PRIVATE (result);

	priv->bundle_kind = AS_BUNDLE_KIND_UNKNOWN;

	priv->cpts = g_hash_table_new_full (g_str_hash,
					    g_str_equal,
					    g_free,
					    g_object_unref);
	priv->mdata_hashes = g_hash_table_new_full (g_direct_hash,
						g_direct_equal,
						NULL,
						g_free);
	priv->hints = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     g_free,
					     (GDestroyNotify) g_ptr_array_unref);


	G_OBJECT_CLASS (asc_result_parent_class)->finalize (object);
}

static void
asc_result_init (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);

	g_free (priv->bundle_id);

	g_hash_table_unref (priv->cpts);
	g_hash_table_unref (priv->mdata_hashes);
	g_hash_table_unref (priv->hints);
}

static void
asc_result_class_init (AscResultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = asc_result_finalize;
}

/**
 * asc_result_unit_ignored:
 * @result: an #AscResult instance.
 *
 * Returns: %TRUE if this result means the analyzed unit was ignored entirely..
 **/
gboolean
asc_result_unit_ignored (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return (g_hash_table_size (priv->cpts) == 0) && (g_hash_table_size (priv->hints) == 0);
}

/**
 * asc_result_components_count:
 * @result: an #AscResult instance.
 *
 * Returns: The amount of components found for this unit.
 **/
guint
asc_result_components_count (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_size (priv->cpts);
}

/**
 * asc_result_hints_count:
 * @result: an #AscResult instance.
 *
 * Returns: The amount of hints emitted for this unit.
 **/
guint
asc_result_hints_count (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_size (priv->hints);
}

/**
 * asc_result_get_bundle_kind:
 * @result: an #AscResult instance.
 *
 * Gets the bundle kind these results are for.
 **/
AsBundleKind
asc_result_get_bundle_kind (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return priv->bundle_kind;
}

/**
 * asc_result_set_bundle_kind:
 * @result: an #AscResult instance.
 *
 * Sets the kind of the bundle these results are for.
 **/
void
asc_result_set_bundle_kind (AscResult *result, AsBundleKind kind)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	priv->bundle_kind = kind;
}

/**
 * asc_result_get_bundle_id:
 * @result: an #AscResult instance.
 *
 * Gets the ID name of the bundle (a package / Flatpak / any entity containing metadata)
 * that these these results are generated for.
 **/
const gchar*
asc_result_get_bundle_id (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return priv->bundle_id;
}

/**
 * asc_result_set_bundle_id:
 * @result: an #AscResult instance.
 * @id: The new ID.
 *
 * Sets the name of the bundle these results are for.
 **/
void
asc_result_set_bundle_id (AscResult *result, const gchar *id)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	g_free (priv->bundle_id);
	priv->bundle_id = g_strdup (id);
}

/**
 * asc_result_get_component:
 * @result: an #AscResult instance.
 * @cid: Component ID to look for.
 *
 * Gets the component by its component-id-
 *
 * returns: (transfer none): An #AsComponent
 **/
AsComponent*
asc_result_get_component (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_lookup (priv->cpts, cid);
}

/**
 * asc_result_fetch_components:
 * @result: an #AscResult instance.
 *
 * Gets all components this #AsResult instance contains.
 *
 * Returns: (transfer full) (element-type AsComponent) : An array of #AsComponent
 **/
GPtrArray*
asc_result_fetch_components (AscResult *result)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	GPtrArray *res;
	GHashTableIter iter;
	gpointer value;

	res = g_ptr_array_new_full (g_hash_table_size (priv->cpts),
				    g_object_unref);

	g_hash_table_iter_init (&iter, priv->cpts);
	while (g_hash_table_iter_next (&iter, NULL, &value))
		g_ptr_array_add (res, g_object_ref (AS_COMPONENT (value)));
	return res;
}

/**
 * asc_result_get_hints:
 * @result: an #AscResult instance.
 * @cid: Component ID to look for.
 *
 * Gets hints for a component with the given component-id.
 *
 * Returns: (transfer none) (element-type AscHint) : An array of #AscHint or %NULL
 **/
GPtrArray*
asc_result_get_hints (AscResult *result, const gchar *cid)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	return g_hash_table_lookup (priv->hints, cid);
}


/**
 * asc_result_update_component_gcid:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to edit.
 * @data: The data to include in the global component ID.
 *
 * Update the global component ID for the given component.
 *
 * Returns: %TRUE if the component existed and was updated.
 **/
gboolean
asc_result_update_component_gcid (AscResult *result, AsComponent *cpt, const gchar *data)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	g_autofree gchar *gcid = NULL;
	gchar *hash = NULL;
	const gchar *old_hash;
	const gchar *cid = as_component_get_id (cpt);

	if (as_is_empty (cid)) {
		gcid = asc_build_component_global_id (cid, NULL);
		as_component_set_data_id (cpt, gcid);
		return TRUE;
	}
	if (!g_hash_table_contains (priv->cpts, cid))
		return FALSE;

	old_hash = g_hash_table_lookup (priv->mdata_hashes, cpt);
	if (old_hash == NULL) {
		hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, data, -1);
	} else {
		g_autofree gchar *tmp = g_strconcat (old_hash, data, NULL);
		hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, tmp, -1);
	}

	g_hash_table_insert (priv->mdata_hashes, cpt, hash);
	gcid = asc_build_component_global_id (cid, hash);
	as_component_set_data_id (cpt, gcid);

	return TRUE;
}

/**
 * asc_result_add_component:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to add.
 * @data: Source data used to generate the GCID hash.
 * @error: A #GError or %NULL
 *
 * Add component to the results set.
 *
 * Returns: %TRUE on success.
 **/
gboolean
asc_result_add_component (AscResult *result, AsComponent *cpt, const gchar *data, GError **error)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	AsComponentKind ckind;
	const gchar *cid = as_component_get_id (cpt);

	if (as_is_empty (cid)) {
		g_set_error_literal (error,
				     ASC_COMPOSE_ERROR,
				     ASC_COMPOSE_ERROR_FAILED,
				     "Can not add component with empty ID to results set.");
		return FALSE;
	}


	/* web applications, operating systems, repositories
	 * and component-removal merges don't (need to) have a package/bundle name set */
	ckind = as_component_get_kind (cpt);
	if ((ckind != AS_COMPONENT_KIND_WEB_APP) &&
	    (ckind != AS_COMPONENT_KIND_OPERATING_SYSTEM) &&
	    (as_component_get_merge_kind (cpt) != AS_MERGE_KIND_REMOVE_COMPONENT)) {
		if (priv->bundle_kind == AS_BUNDLE_KIND_PACKAGE) {
			gchar *pkgnames[2] = {priv->bundle_id, NULL};
			as_component_set_pkgnames (cpt, pkgnames);
		} else if ((priv->bundle_kind != AS_BUNDLE_KIND_UNKNOWN) && (priv->bundle_kind >= AS_BUNDLE_KIND_LAST)) {
			g_autoptr(AsBundle) bundle = as_bundle_new ();
			as_bundle_set_kind (bundle, priv->bundle_kind);
			as_bundle_set_id (bundle, priv->bundle_id);
			as_component_add_bundle (cpt, bundle);
		}
        }

        g_hash_table_insert (priv->cpts,
			     g_strdup (cid),
			     g_object_ref (cpt));
	asc_result_update_component_gcid (result, cpt, data);
	return TRUE;
}

/**
 * asc_result_remove_component:
 * @result: an #AscResult instance.
 * @cpt: The #AsComponent to remove.
 *
 * Remove a component from the results set.
 *
 * Returns: %TRUE if the component was found and removed.
 **/
gboolean
asc_result_remove_component (AscResult *result, AsComponent *cpt)
{
	AscResultPrivate *priv = GET_PRIVATE (result);
	gboolean ret;

	ret = g_hash_table_remove (priv->cpts,
				   as_component_get_id (cpt));
	if (ret)
		as_component_set_data_id (cpt, NULL);
	g_hash_table_remove (priv->mdata_hashes, cpt);

	return ret;
}

/**
 * asc_result_new:
 *
 * Creates a new #AscResult.
 **/
AscResult*
asc_result_new (void)
{
	AscResult *result;
	result = g_object_new (ASC_TYPE_RESULT, NULL);
	return ASC_RESULT (result);
}
