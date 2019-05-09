/* Copyright 2019 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdarg.h>

#include <libfdt.h>

#include "fdt_traverse.h"

#define FDT_MAX_SIZE	(1024 * 1024)

struct fdt_error_map {
	int code;
	const char *msg;
};

#define FDT_ERROR(x)	{ x, #x }

static struct fdt_error_map fdt_error_map[FDT_ERR_MAX] = {
	FDT_ERROR(FDT_ERR_NOTFOUND),
	FDT_ERROR(FDT_ERR_EXISTS),
	FDT_ERROR(FDT_ERR_NOSPACE),
	FDT_ERROR(FDT_ERR_BADOFFSET),
	FDT_ERROR(FDT_ERR_BADPATH),
	FDT_ERROR(FDT_ERR_BADPHANDLE),
	FDT_ERROR(FDT_ERR_BADSTATE),
	FDT_ERROR(FDT_ERR_TRUNCATED),
	FDT_ERROR(FDT_ERR_BADMAGIC),
	FDT_ERROR(FDT_ERR_BADVERSION),
	FDT_ERROR(FDT_ERR_BADSTRUCTURE),
	FDT_ERROR(FDT_ERR_BADLAYOUT),
	FDT_ERROR(FDT_ERR_INTERNAL),
	FDT_ERROR(FDT_ERR_BADNCELLS),
	FDT_ERROR(FDT_ERR_BADVALUE),
	FDT_ERROR(FDT_ERR_BADOVERLAY),
	FDT_ERROR(FDT_ERR_NOPHANDLES),
};

static void fdt_error(int ret, const char *fmt, ...)
{
	va_list ap;

	ret = (ret < 0) ? -ret : ret;
	if (ret < FDT_ERR_MAX)
		fprintf(stderr, "%s: ", fdt_error_map[ret].msg);
	else
		fprintf(stderr, "FDT_UNKNOWN_ERR: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static bool fdt_traverse_read_prop(const void *fdt,
				   int offset,
				   void *node,
				   fdt_traverse_prop_add_fn prop_add,
				   void *priv)
{
	const char *name;
	int poffset;

	fdt_for_each_property_offset(poffset, fdt, offset) {
		const struct fdt_property *prop;
		int len, ret;

		if (poffset < 0) {
			fdt_error(poffset, "fdt_for_each_property_offset: offset=%d\n", offset);
			return false;
		}

		prop = fdt_get_property_by_offset(fdt, poffset, &len);
		if (!prop) {
			fdt_error(len, "fdt_get_property_by_offset: offset=%d\n", poffset);
			return false;
		}

		name = fdt_string(fdt, fdt32_to_cpu(prop->nameoff));
		if (!name) {
			fdt_error(FDT_ERR_BADOFFSET, "fdt_string: %d\n", fdt32_to_cpu(prop->nameoff));
			return false;
		}

		ret = prop_add(node, name, (void *)prop->data, len, priv);
		if (ret != 0)
			return false;
	}

	return true;
}

static bool fdt_traverse_read_node(const void *fdt,
				   int offset,
				   void *parent,
				   fdt_traverse_node_add_fn node_add,
				   fdt_traverse_prop_add_fn prop_add,
				   void *priv)
{
	const char *name;
	int noffset;

	fdt_for_each_subnode(noffset, fdt, offset) {
		void *node;
		int len;

		name = fdt_get_name(fdt, noffset, &len);
		if (!name) {
			fdt_error(len, "fdt_get_name: offset=%d\n", offset);
			return false;
		}

		node = node_add(name, parent, priv);
		if (!node)
			return false;

		if (!fdt_traverse_read_prop(fdt, noffset, node, prop_add, priv))
			return false;


		if (!fdt_traverse_read_node(fdt, noffset, node, node_add, prop_add, priv))
			return false;
	}

	return true;
}

bool fdt_traverse_read(const void *fdt,
		       void *root,
		       fdt_traverse_node_add_fn node_add,
		       fdt_traverse_prop_add_fn prop_add,
		       void *priv)
{
	if (!fdt_traverse_read_prop(fdt, 0, root, prop_add, priv))
		return false;

	return fdt_traverse_read_node(fdt, 0, root, node_add, prop_add, priv);
}

static bool fdt_traverse_write_prop(void *fdt,
				    void *node,
				    fdt_traverse_prop_iter_fn prop_iter)
{
	void *prop = NULL;
	const char *name;
	const void *value;
	int value_len;
	int ret;

	for (value = prop_iter(node, &prop, &name, &value_len);
	     value;
	     value = prop_iter(node, &prop, &name, &value_len)) {
		ret = fdt_property(fdt, name, value, value_len);
		if (ret) {
			fdt_error(ret, "fdt_property: %s\n", name);
			return false;
		}
	}

	return true;
}

static bool fdt_traverse_write_node_single(void *fdt,
					   void *node,
					   const char *name,
					   fdt_traverse_prop_iter_fn prop_iter)
{
	int ret;

	ret = fdt_begin_node(fdt, name);
	if (ret) {
		fdt_error(ret, "fdt_begin_node: %s\n", name);
		return false;
	}

	if (!fdt_traverse_write_prop(fdt, node, prop_iter))
		return false;

	return true;
}

static bool fdt_traverse_write_node(void *fdt,
				    void *parent,
				    fdt_traverse_node_iter_fn node_iter,
				    fdt_traverse_prop_iter_fn prop_iter)
{
	void *node = NULL;
	const char *name;
	int ret;

	for (name = node_iter(parent, &node);
	     name;
	     name = node_iter(parent, &node)) {

		if (!fdt_traverse_write_node_single(fdt, node, name, prop_iter))
			return false;

		if (!fdt_traverse_write_node(fdt, node, node_iter, prop_iter))
			return false;

		ret = fdt_end_node(fdt);
		if (ret) {
			fdt_error(ret, "fdt_end_node: %s\n", name);
			return false;
		}
	}

	return true;
}

void *fdt_traverse_write(void *root,
			 fdt_traverse_node_iter_fn node_iter,
			 fdt_traverse_prop_iter_fn prop_iter,
			 int *size)
{
	void *fdt;
	int ret;

	fdt = malloc(FDT_MAX_SIZE);
	if (!fdt)
		return NULL;

	ret = fdt_create(fdt, FDT_MAX_SIZE);
	if (ret) {
		fdt_error(ret, "fdt_create\n");
		goto fail;
	}

	ret = fdt_finish_reservemap(fdt);
	if (ret) {
		fdt_error(ret, "fdt_finish_reservemap\n");
		goto fail;
	}

	if (!fdt_traverse_write_node_single(fdt, root, "", prop_iter))
		goto fail;

	if (!fdt_traverse_write_node(fdt, root, node_iter, prop_iter))
		goto fail;

	ret = fdt_end_node(fdt);
	if (ret) {
		fdt_error(ret, "fdt_end_node: root\n");
		goto fail;
	}

	ret = fdt_finish(fdt);
	if (ret) {
		fdt_error(ret, "fdt_finish\n");
		goto fail;
	}

	*size = fdt_totalsize(fdt);
	return fdt;

fail:
	free(fdt);
	return NULL;
}
