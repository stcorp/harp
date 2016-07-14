/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "harp-internal.h"

#include "hashtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** \defgroup harp_product HARP Products
 * The HARP Products module contains everything related to HARP products.
 */

static int get_arguments(int argc, char *argv[], char **new_arguments)
{
    char *arguments = NULL;
    size_t length = 0;
    int add_quotes = 0;
    int i;

    /* Determine the stringlength */
    for (i = 1; i < argc; i++)
    {
        length += strlen(argv[i]);

        /* Add quotes for arguments that contain a whitespace, a semi-colon, an expression or a [unit] */
        add_quotes = (strstr(argv[i], " ") != NULL || strstr(argv[i], ";") != NULL || strstr(argv[i], "[") != NULL ||
                      strstr(argv[i], "]") != NULL || strstr(argv[i], "<") != NULL || strstr(argv[i], "!") != NULL ||
                      strstr(argv[i], "=") != NULL || strstr(argv[i], ">") != NULL);

        if (add_quotes)
        {
            length += 2;
        }
        if (i < argc - 1)
        {
            /* Add an extra whitespace */
            length++;
        }
    }
    /* Add an extra string termination character */
    length++;

    /* Combine the arguments (while skipping argv[0]) */
    arguments = calloc(length, sizeof(char));
    if (arguments == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       length * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    for (i = 1; i < argc; i++)
    {
        /* Add quotes for arguments that contain a whitespace, a semi-colon, an expression or a [unit] */
        add_quotes = (strstr(argv[i], " ") != NULL || strstr(argv[i], ";") != NULL || strstr(argv[i], "[") != NULL ||
                      strstr(argv[i], "]") != NULL || strstr(argv[i], "<") != NULL || strstr(argv[i], "!") != NULL ||
                      strstr(argv[i], "=") != NULL || strstr(argv[i], ">") != NULL);

        if (add_quotes)
        {
            strcat(arguments, "'");
            strcat(arguments, argv[i]);
            strcat(arguments, "'");
        }
        else
        {
            strcat(arguments, argv[i]);
        }
        if (i < argc - 1)
        {
            strcat(arguments, " ");
        }
    }

    *new_arguments = arguments;
    return 0;
}

static void sync_product_dimensions_on_variable_add(harp_product *product, const harp_variable *variable)
{
    int i;

    for (i = 0; i < variable->num_dimensions; ++i)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type != harp_dimension_independent && product->dimension[dimension_type] == 0)
        {
            product->dimension[dimension_type] = variable->dimension[i];
        }
    }
}

static void sync_product_dimensions_on_variable_remove(harp_product *product, const harp_variable *variable)
{
    int inactive_dimension_mask[HARP_NUM_DIM_TYPES] = { 0 };
    int num_inactive_dimensions;
    int i;
    int j;

    /* Update product dimensions. Dimensions that only the variable to be removed depends upon are set to zero. Other
     * dimensions are left untouched.
     */
    num_inactive_dimensions = 0;
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type != harp_dimension_independent && !inactive_dimension_mask[dimension_type])
        {
            /* For each dimension the variable to be removed depends upon, assume it is the only variable that depends
             * on that dimension. Mark such dimension as inactive.
             */
            assert(product->dimension[dimension_type] > 0);
            inactive_dimension_mask[dimension_type] = 1;
            num_inactive_dimensions++;
        }
    }

    if (num_inactive_dimensions == 0)
    {
        /* Removing the variable will not affect product dimensions. */
        return;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *other_variable = product->variable[i];

        if (other_variable == variable)
        {
            continue;
        }

        for (j = 0; j < other_variable->num_dimensions; j++)
        {
            harp_dimension_type dimension_type = other_variable->dimension_type[j];

            if (dimension_type != harp_dimension_independent && inactive_dimension_mask[dimension_type])
            {
                /* If the product contains a variable (other than the variable to be removed) that depends on a
                 * dimension marked as inactive, it follows that this dimension is in fact active.
                 */
                assert(other_variable->dimension[j] > 0);
                inactive_dimension_mask[dimension_type] = 0;
                num_inactive_dimensions--;
            }
        }

        if (num_inactive_dimensions == 0)
        {
            /* For all dimension the variable to be removed depends upon, another variable has been found that depends
             * on this dimension as well. Removing the variable therefore will not affect product dimensions.
             */
            break;
        }
    }

    /* Set each product dimension to zero for which no variable (other than the variable to be removed) was found that
     * depends on this dimension.
     */
    if (num_inactive_dimensions > 0)
    {
        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            if (inactive_dimension_mask[i])
            {
                product->dimension[i] = 0;
            }
        }
    }
}

/** \addtogroup harp_product
 * @{
 */

/** Create new product.
 * The product will be intialized with 0 variables and 0 attributes.
 * \param new_product Pointer to the C variable where the new HARP product will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_new(harp_product **new_product)
{
    harp_product *product;

    product = (harp_product *)malloc(sizeof(harp_product));
    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_product), __FILE__, __LINE__);
        return -1;
    }

    memset(product->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    product->num_variables = 0;
    product->variable = NULL;
    product->source_product = NULL;
    product->history = NULL;

    *new_product = product;
    return 0;
}

/** Delete product.
 * Remove product and all attached variables and attributes.
 * \param product HARP product.
 */
LIBHARP_API void harp_product_delete(harp_product *product)
{
    if (product != NULL)
    {
        if (product->variable != NULL)
        {
            int i;

            for (i = 0; i < product->num_variables; i++)
            {
                harp_variable_delete(product->variable[i]);
            }

            free(product->variable);
        }

        if (product->source_product != NULL)
        {
            free(product->source_product);
        }

        if (product->history != NULL)
        {
            free(product->history);
        }

        free(product);
    }
}

/** Create a copy of a product.
 * The function will create a deep-copy of the given product, also creating copyies of all attributes and variables.
 * \param other_product Product that should be copied.
 * \param new_product Pointer to the variable where the new HARP product will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_copy(const harp_product *other_product, harp_product **new_product)
{
    harp_product *product;
    int i;

    if (harp_product_new(&product) != 0)
    {
        return -1;
    }

    for (i = 0; i < other_product->num_variables; i++)
    {
        harp_variable *variable;

        if (harp_variable_copy(other_product->variable[i], &variable) != 0)
        {
            harp_product_delete(product);
            return -1;
        }

        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            harp_product_delete(product);
            return -1;
        }
    }

    if (other_product->source_product != NULL)
    {
        product->source_product = strdup(other_product->source_product);
        if (product->source_product == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_product_delete(product);
            return -1;
        }
    }

    if (other_product->history != NULL)
    {
        product->history = strdup(other_product->history);
        if (product->history == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_product_delete(product);
            return -1;
        }
    }

    *new_product = product;
    return 0;
}

/** Set the source product attribute of the specified product.
 * Store a copy of \a source_product as the value of the source product attribute of the specified product. The previous
 * value (if any) will be freed.
 * \param product Product for which to set the source product attribute.
 * \param source_product New value for the source product attribute.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_set_source_product(harp_product *product, const char *source_product)
{
    char *source_product_copy;

    if (source_product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source_product is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    source_product_copy = strdup(source_product);

    if (source_product_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (product->source_product != NULL)
    {
        free(product->source_product);
    }

    product->source_product = source_product_copy;

    return 0;
}

/** Set the history attribute of the specified product.
 * Store a copy of \a history as the value of the history attribute of the specified product. The previous value (if
 * any) will be freed.
 * \param product Product for which to set the history attribute.
 * \param history New value for the history attribute.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_set_history(harp_product *product, const char *history)
{
    char *history_copy;

    if (history == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "history is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    history_copy = strdup(history);

    if (history_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (product->history != NULL)
    {
        free(product->history);
    }

    product->history = history_copy;

    return 0;
}

/** Add a variable to a product.
 * \note The memory management of the variable will be handled via the product after you have added the variable.
 * \param product Product to which the variable should be added.
 * \param variable Variable that should be added to the product.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_add_variable(harp_product *product, harp_variable *variable)
{
    int i;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_product_has_variable(product, variable->name))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' exists (%s:%u)", variable->name, __FILE__, __LINE__);
        return -1;
    }

    /* Verify that variable and product dimensions are compatible. */
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type == harp_dimension_independent || product->dimension[dimension_type] == 0)
        {
            continue;
        }

        if (variable->dimension[i] != product->dimension[dimension_type])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension %d (of type '%s') of variable '%s' is incompatible"
                           " with product; variable = %ld, product = %ld (%s:%u)", i,
                           harp_get_dimension_type_name(dimension_type), variable->name, variable->dimension[i],
                           product->dimension[dimension_type], __FILE__, __LINE__);
            return -1;
        }
    }

    /* Add the variable to the product. */
    if (product->num_variables % BLOCK_SIZE == 0)
    {
        harp_variable **variable;

        variable = realloc(product->variable, (product->num_variables + BLOCK_SIZE) * sizeof(harp_variable *));
        if (variable == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (product->num_variables + BLOCK_SIZE) * sizeof(harp_variable *), __FILE__, __LINE__);
            return -1;
        }

        product->variable = variable;
    }
    product->variable[product->num_variables] = variable;
    product->num_variables++;

    /* Update product dimensions. */
    sync_product_dimensions_on_variable_add(product, variable);

    return 0;
}

/** Detach a variable from a product.
 * Removes a variable from a product without deleting the variable itself. After detaching, the caller of the function
 * will be responsible for the further memory management of the variable.
 * \param product Product from which the variable should be detached.
 * \param variable Variable that should be detached.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_detach_variable(harp_product *product, const harp_variable *variable)
{
    int i;
    int j;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (product->variable[i] == variable)
        {
            /* Update product dimensions. */
            sync_product_dimensions_on_variable_remove(product, variable);

            /* Remove the variable from the product. */
            for (j = i + 1; j < product->num_variables; j++)
            {
                product->variable[j - 1] = product->variable[j];
            }
            product->num_variables--;

            return 0;
        }
    }

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "could not find variable '%s'", variable->name);
    return -1;
}

/** Remove a variable from a product.
 * This function removes the specified variable from the product and then deletes the variable itself.
 * \param product Product from which the variable should be removed.
 * \param variable Variable that should be removed.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_remove_variable(harp_product *product, harp_variable *variable)
{
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* NB. Product dimensions will be updated by harp_product_detach_variable(). */
    if (harp_product_detach_variable(product, variable) != 0)
    {
        return -1;
    }
    harp_variable_delete(variable);

    return 0;
}

/** Replaces an existing variable with the one provided.
 * The product should already contain a variable with the same name as \a variable. This function searches in the list
 * of variables in the product for one with the same name, removes this variable and then adds the given \a variable in
 * its place. Note that if you try to replace a variable with itself the function does nothing (and returns success).
 * \param product Product in which the variable should be replaced.
 * \param variable Variable that should be used to replace an existing variable.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_replace_variable(harp_product *product, harp_variable *variable)
{
    int variable_id;
    int i;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_product_get_variable_id_by_name(product, variable->name, &variable_id) != 0)
    {
        harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "variable '%s' does not exist (%s:%u)", variable->name, __FILE__,
                       __LINE__);
        return -1;
    }

    if (product->variable[variable_id] != variable)
    {
        /* Attempt to replace variable by itself. */
        return 0;
    }

    /* Verify that variable and product dimensions are compatible. */
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type == harp_dimension_independent || product->dimension[dimension_type] == 0)
        {
            continue;
        }

        if (variable->dimension[i] != product->dimension[dimension_type])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension %d (of type '%s') of variable '%s' is incompatible"
                           " with product; variable = %ld, product = %ld (%s:%u)", i,
                           harp_get_dimension_type_name(dimension_type), variable->name, variable->dimension[i],
                           product->dimension[dimension_type], __FILE__, __LINE__);
            return -1;
        }
    }

    /* Replace variable. */
    sync_product_dimensions_on_variable_remove(product, product->variable[variable_id]);
    harp_variable_delete(product->variable[variable_id]);

    product->variable[variable_id] = variable;
    sync_product_dimensions_on_variable_add(product, product->variable[variable_id]);

    return 0;
}

/**
 * Test if product contains a variable with the specified name.
 * \param  product Product to search.
 * \param  name    Name of the variable to search for.
 * \return
 *   \arg \c 0, Product does not contain a variable of the specified name.
 *   \arg \c 1, Product contains a variable of the specified name.
 */
LIBHARP_API int harp_product_has_variable(const harp_product *product, const char *name)
{
    int i;

    if (name == NULL)
    {
        return 0;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (strcmp(product->variable[i]->name, name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/** Find variable with a given name for a product.
 * If no variable with the given name can be found an error is returned.
 * \param product Product in which the find the variable.
 * \param name Name of the variable.
 * \param variable Pointer to the C variable where the found HARP variable will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_variable_by_name(const harp_product *product, const char *name,
                                                  harp_variable **variable)
{
    int i;

    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (strcmp(product->variable[i]->name, name) == 0)
        {
            *variable = product->variable[i];
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "variable '%s' does not exist", name);
    return -1;
}

/** Find index of variable with a given name for a product.
 * If no variable with the given name can be found an error is returned.
 * \param product Product in which the find the variable.
 * \param name Name of the variable.
 * \param variable_id Pointer to the C variable where the index in the HARP variables list for the product is returned.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_variable_id_by_name(const harp_product *product, const char *name, int *variable_id)
{
    int i;

    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (variable_id == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable_id is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (strcmp(product->variable[i]->name, name) == 0)
        {
            *variable_id = i;
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "variable '%s' does not exist", name);
    return -1;
}

/** Determine whether all variables in a product have at least one element.
 * If at least one variable has 0 elements or if the product has 0 variables the function returns 1, and 0 otherwise.
 * \param product Product to check for empty data.
 * \return
 *   \arg \c 0, The product does not contain empty data.
 *   \arg \c 1, The product contains 0 variables or at least one variable has 0 elements.
 */
LIBHARP_API int harp_product_is_empty(const harp_product *product)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        if (product->variable[i]->num_elements == 0)
        {
            /* If at least one variable has no data, return true. */
            return 1;
        }
    }

    /* Do we have any variables at all? */
    return (product->num_variables == 0);
}

/** Update the history attribute in the product based on the command line parameters.
 * This function will extend the existing product history metadata element with a line containing the call that was
 * used to run this program. This command line execution call is constructed based on the \a argc and \a argv arguments.
 * \param product Product for which the history metada should be extended.
 * \param executable Name of the command line executable (this value is used instead of argv[0]).
 * \param argc Variable as passed by main().
 * \param argv Variable as passed by main().
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_update_history(harp_product *product, const char *executable, int argc, char *argv[])
{
    char *arguments = NULL;
    char *buffer = NULL;
    size_t length;

    if (executable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "executable is NULL");
        return -1;
    }

    /* Derive the arguments as a string */
    if (get_arguments(argc, argv, &arguments) != 0)
    {
        return -1;
    }

    /* Update the history attribute */
    /* Add the length of the executable and the arguments, a whitespace and a string termination character */
    length = strlen(executable) + strlen(arguments) + 2;
    if (product->history != NULL)
    {
        length += strlen(product->history) + 1;
    }

    /* Create the new history string */
    buffer = malloc(length * sizeof(char));
    if (buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       length * sizeof(char), __FILE__, __LINE__);
        free(arguments);
        return -1;
    }
    buffer[0] = '\0';
    if (product->history != NULL)
    {
        strcat(buffer, product->history);
        strcat(buffer, "\n");
        free(product->history);
        product->history = NULL;
    }
    strcat(buffer, executable);
    strcat(buffer, " ");
    strcat(buffer, arguments);

    product->history = buffer;

    free(arguments);

    return 0;
}

/** Verify that a product is internally consistent and complies with conventions.
 * \param product Product to verify.
 * \return
 *   \arg \c 0, Product verified successfully.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_verify(const harp_product *product)
{
    hashtable *variable_names;
    int i;

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL");
        return -1;
    }

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (product->dimension[i] < 0)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "dimension of type '%s' has invalid length %ld",
                           harp_get_dimension_type_name((harp_dimension_type)i), product->dimension[i]);
            return -1;
        }
    }

    if (product->num_variables < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_PRODUCT, "invalid number of variables %d", product->num_variables);
        return -1;
    }

    if (product->num_variables > 0 && product->variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_PRODUCT, "number of variables is > 0, but product contains no variables");
        return -1;
    }

    /* Check variables. */
    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable = product->variable[i];

        if (variable == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "variable at index %d undefined", i);
            return -1;
        }

        if (harp_variable_verify(variable) != 0)
        {
            if (variable->name == NULL)
            {
                harp_add_error_message(" (variable at index %d)", i);
            }
            else
            {
                harp_add_error_message(" (variable '%s')", variable->name);
            }

            return -1;
        }
    }

    /* Check consistency of dimensions between product and variables. */
    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] == harp_dimension_independent)
            {
                continue;
            }

            if (variable->dimension[j] != product->dimension[variable->dimension_type[j]])
            {
                harp_set_error(HARP_ERROR_INVALID_PRODUCT, "length %ld of dimension of type '%s' at index %d of "
                               "variable '%s' does not match length %ld of product dimension of type '%s'",
                               variable->dimension[j], harp_get_dimension_type_name(variable->dimension_type[j]), j,
                               variable->name, product->dimension[variable->dimension_type[j]],
                               harp_get_dimension_type_name(variable->dimension_type[j]));
                return -1;
            }
        }
    }

    variable_names = hashtable_new(1);
    if (variable_names == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable = product->variable[i];

        if (hashtable_add_name(variable_names, variable->name) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "variable name '%s' is not unique", variable->name);
            hashtable_delete(variable_names);
            return -1;
        }
    }

    hashtable_delete(variable_names);

    return 0;
}

/** @} */

int harp_product_rearrange_dimension(harp_product *product, harp_dimension_type dimension_type, long num_dim_elements,
                                     const long *dim_element_ids)
{
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot rearrange '%s' dimension (%s:%u)",
                       harp_get_dimension_type_name(harp_dimension_independent), __FILE__, __LINE__);
        return -1;
    }

    if (product->dimension[dimension_type] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product does not depend on dimension '%s' (%s:%u)",
                       harp_get_dimension_type_name(dimension_type), __FILE__, __LINE__);
        return -1;
    }

    if (dim_element_ids == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_element_ids is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (num_dim_elements == 0)
    {
        /* If the new length of the dimension to be rearranged is zero, return an empty product. */
        harp_product_remove_all_variables(product);
        return 0;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] != dimension_type)
            {
                continue;
            }

            if (harp_variable_rearrange_dimension(variable, j, num_dim_elements, dim_element_ids) != 0)
            {
                return -1;
            }
        }
    }

    product->dimension[dimension_type] = num_dim_elements;

    return 0;
}

/* Filter data of a variable in one dimension.
 * This function removes for all variables all elements in the given dimension where \a mask is set to 0.
 * The size of \a mask should correspond to the length of the given dimension.
 * It is an error to provide a list of \a mask values that only contain zeros (i.e. filter out all elements).
 *
 * Input:
 *    product        Pointer to product for which the variables should have their data filtered.
 *    dimension_type Dimension to filter.
 *    mask           An array containing true/false (1/0) values on whether to keep an element or not.
 */
int harp_product_filter_dimension(harp_product *product, harp_dimension_type dimension_type, const uint8_t *mask)
{
    long masked_dimension_length;
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot filter '%s' dimension (%s:%u)",
                       harp_get_dimension_type_name(harp_dimension_independent), __FILE__, __LINE__);
        return -1;
    }

    if (product->dimension[dimension_type] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product does not depend on dimension '%s'",
                       harp_get_dimension_type_name(dimension_type));
        return -1;
    }

    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "mask is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    masked_dimension_length = 0;
    for (i = 0; i < product->dimension[dimension_type]; i++)
    {
        if (mask[i])
        {
            masked_dimension_length++;
        }
    }

    if (masked_dimension_length == 0)
    {
        /* If the new length of the dimension to be filtered is zero, return an empty product. */
        harp_product_remove_all_variables(product);
        return 0;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] != dimension_type)
            {
                continue;
            }

            if (harp_variable_filter_dimension(variable, j, mask) != 0)
            {
                return -1;
            }
        }
    }

    product->dimension[dimension_type] = masked_dimension_length;

    return 0;
}

/* Remove the specified dimension from the product.
 * All variables that depend on the specified dimension will be removed from the product.
 *
 * Input:
 *    product        Product to operate on.
 *    dimension_type The dimension that should be removed.
 */
int harp_product_remove_dimension(harp_product *product, harp_dimension_type dimension_type)
{
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot remove '%s' dimension (%s:%u)",
                       harp_get_dimension_type_name(harp_dimension_independent), __FILE__, __LINE__);
        return -1;
    }

    if (product->dimension[dimension_type] == 0)
    {
        /* Product does not depend on dimensions to be removed, so nothing has to be done. */
        return 0;
    }

    i = 0;
    while (i < product->num_variables)
    {
        harp_variable *variable = product->variable[i];

        if (!harp_variable_has_dimension_type(variable, dimension_type))
        {
            i++;
            continue;
        }

        if (harp_product_remove_variable(product, variable) != 0)
        {
            return -1;
        }
    }
    assert(product->dimension[dimension_type] == 0);

    return 0;
}

/** Remove all variables from a product.
 * \param product Product from which variables should be removed.
 */
void harp_product_remove_all_variables(harp_product *product)
{
    if (product->variable != NULL)
    {
        int i;

        for (i = 0; i < product->num_variables; i++)
        {
            harp_variable_delete(product->variable[i]);
        }

        free(product->variable);
    }

    memset(product->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    product->num_variables = 0;
    product->variable = NULL;
}

/**
 * Determine the datetime range covered by the product. Start and stop datetimes are returned as the (fractional) number
 * of days since 2000.
 *
 * \param  product        Product to compute the datetime range of.
 * \param  datetime_start Pointer to the location where the start datetime of the product will be stored. If NULL, the
 *   start datetime will not be stored.
 * \param  datetime_stop  Pointer to the location where the stop datetime of the product will be stored. If NULL, the
 *   stop datetime will not be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_get_datetime_range(const harp_product *product, double *datetime_start, double *datetime_stop)
{
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_variable *datetime;
    double start;
    double stop;
    long i;

    if (harp_product_get_derived_variable(product, "datetime", "days since 2000-01-01", 1, dimension_type, &datetime)
        != 0)
    {
        return -1;
    }

    if (harp_variable_convert_data_type(datetime, harp_type_double) != 0)
    {
        harp_variable_delete(datetime);
        return -1;
    }

    start = harp_plusinf();
    stop = harp_mininf();
    for (i = 0; i < datetime->num_elements; i++)
    {
        const double value = datetime->data.double_data[i];

        if (harp_isnan(value) || value < datetime->valid_min.double_data || value > datetime->valid_max.double_data)
        {
            continue;
        }

        if (value < start)
        {
            start = value;
        }

        if (value > stop)
        {
            stop = value;
        }
    }

    if (harp_isnan(start) || start < datetime->valid_min.double_data || start > datetime->valid_max.double_data
        || harp_isnan(stop) || stop < datetime->valid_min.double_data || stop > datetime->valid_max.double_data)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot determine valid datetime range");
        return -1;
    }

    harp_variable_delete(datetime);

    if (datetime_start != NULL)
    {
        *datetime_start = start;
    }

    if (datetime_stop != NULL)
    {
        *datetime_stop = stop;
    }

    return 0;
}
