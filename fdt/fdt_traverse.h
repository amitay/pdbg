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

#ifndef __FDT_TRAVERSE_H__
#define __FDT_TRAVERSE_H__

#include <stdbool.h>

/**
 * @brief Callback function for device tree node
 *
 * @param[in] name  Name of the device tree node
 * @param[in] parent  The parent abstract object
 * @param[in] priv  Private data for callback
 * @return new abstract object for node or NULL on error
 */
typedef void * (*fdt_traverse_node_add_fn)(const char *name,
					   void *parent,
					   void *priv);

/**
 * @brief Callback function for device tree node property
 *
 * @param[in] node  Node abstract object
 * @param[in] name  Name of the device tree node property
 * @param[in] value  The value of the property
 * @param[in] valuelen  Length of the value
 * @return 0 on success, non-zero on error
 */
typedef int (*fdt_traverse_prop_add_fn)(void *node,
					const char *name,
					void *value,
					int valuelen,
					void *priv);

/**
 * @brief Callback function for iterating device tree nodes
 *
 * The function iterates over all the children of a given parent node.  To
 * start the iteration, set child = NULL.  The iterator must update the child
 * node to next node, every time the function is called.
 *
 * @param[in] parent  Abstract object for parent node
 * @param[in,out] child  Abstract object for next node
 * @return name of the next node, NULL if there is no next node
 */
typedef const char * (*fdt_traverse_node_iter_fn)(void *parent,
						  void **child);

/**
 * @brief Callback function for iterating node properties
 *
 * The function iterates over all the properties of a given node.  To start
 * the iteration, set next = NULL.  The iterator must update the next_prop
 * to next property, every time the function is called.
 *
 * @param[in] node  Abstract object for a node
 * @param[in,out] next_prop  Abstract object for next property
 * @param[out] name  Name of the property
 * @param[out] value_len  Length of the value of the property
 * @return value of the property, NULL if there is no next property
 */
typedef const void * (*fdt_traverse_prop_iter_fn)(void *node,
						  void **next_prop,
						  const char **name,
						  int *value_len);

/**
 * @brief Parse a FDT blob
 *
 * This provides a convenient way to read FDT blob.  The actual processing of
 * device tree nodes and properties is done via callbacks.  For each device
 * tree node, node_add callback is called.  For each property of a node,
 * prop_add callback is called.  If any of the callbacks return failure, the
 * traverse stops and returns false.
 *
 * @param[in] fdt  FDT blob in memory
 * @param[in] root  Abstract object representing root node
 * @param[in] node_add  Callback for new node
 * @param[in] prop_add  Callback for new property
 * @param[in] priv  Private data for callbacks
 * @return true on success, false otherwise
 */
bool fdt_traverse_read(const void *fdt,
		       void *root,
		       fdt_traverse_node_add_fn node_add,
		       fdt_traverse_prop_add_fn prop_add,
		       void *priv);

/**
 * @brief Write a FDT blob
 *
 * This provides a convenient way to create FDT blob from a tree structure.
 * The actual traversing of the tree structure and node properties is done
 * via callbacks.
 *
 * @param[in] root  Abstract object representing root node
 * @param[in] node_iter  Callback for iterating over child nodes of a parent
 * @param[in] prop_iter  Callback for iterating over properties of a node
 * @param[out] size  Size of the device tree
 * @return pointer to FDT blob, NULL on error
 */
void * fdt_traverse_write(void *root,
			  fdt_traverse_node_iter_fn node_add,
			  fdt_traverse_prop_iter_fn prop_add,
			  int *size);

#endif /* __FDT_TRAVERSE_H__ */
