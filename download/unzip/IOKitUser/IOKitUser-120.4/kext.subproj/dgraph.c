#include <libc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dgraph.h"
#include "KXKext.h"
#include "load.h"
#include "vers_rsrc.h"


// FIXME: Put these into a header file!
extern void (*kload_log_func)(const char * format, ...);
extern void (*kload_err_log_func)(const char * format, ...);
extern int (*kload_approve_func)(int default_answer,
    const char * format, ...);

static void __dgraph_entry_free(dgraph_entry_t * entry);

/*******************************************************************************
*
*******************************************************************************/
dgraph_error_t dgraph_init(dgraph_t * dgraph)
{
    dgraph->capacity = (5);  // pulled from a hat
    dgraph->length = 0;
    dgraph->load_order = NULL;
    dgraph->root = NULL;

   /* Make sure list is big enough & graph has a good start size.
    */
    dgraph->graph = (dgraph_entry_t **)malloc(
        dgraph->capacity * sizeof(dgraph_entry_t *));

    if (!dgraph->graph) {
        return dgraph_error;
    }

    return dgraph_valid;
}

/*******************************************************************************
*
*******************************************************************************/
dgraph_error_t dgraph_init_with_arglist(
    dgraph_t * dgraph,
    int expect_addresses,
    const char * dependency_delimiter,
    const char * kernel_dependency_delimiter,
    int argc,
    char * argv[])
{
    dgraph_error_t result = dgraph_valid;
    unsigned int i;
    int found_zero_load_address = 0;
    int found_nonzero_load_address = 0;
    dgraph_entry_t * current_dependent = NULL;
    char kernel_dependencies = 0;

    result = dgraph_init(dgraph);
    if (result != dgraph_valid) {
        return result;
    }

    for (i = 0; i < argc; i++) {
        vm_address_t load_address = 0;

        if (0 == strcmp(argv[i], dependency_delimiter)) {
            kernel_dependencies = 0;
            current_dependent = NULL;
            continue;
        } else if (0 == strcmp(argv[i], kernel_dependency_delimiter)) {
            kernel_dependencies = 1;
            current_dependent = NULL;
            continue;
        }

        if (expect_addresses) {
            char * address = rindex(argv[i], '@');
            if (address) {
                *address++ = 0;  // snip the address from the filename
                load_address = strtoul(address, NULL, 0);
            }
        }

        if (!current_dependent) {
           current_dependent = dgraph_add_dependent(dgraph, argv[i],
               /* expected kmod name */ NULL, /* expected vers */ 0,
               load_address, 0);
           if (!current_dependent) {
               return dgraph_error;
           }
        } else {
            if (!dgraph_add_dependency(dgraph, current_dependent, argv[i],
               /* expected kmod name */ NULL, /* expected vers */ 0,
               load_address, kernel_dependencies)) {

               return dgraph_error;
            }
        }
    }

    dgraph->root = dgraph_find_root(dgraph);
    dgraph_establish_load_order(dgraph);

    if (!dgraph->root) {
        (*kload_err_log_func)("dependency graph has no root");
        return dgraph_invalid;
    }

    if (dgraph->root->is_kernel_component) {
        (*kload_err_log_func)("dependency graph root is a kernel component");
        return dgraph_invalid;
    }

    for (i = 0; i < dgraph->length; i++) {
        if (dgraph->graph[i]->loaded_address == 0) {
            found_zero_load_address = 1;
        } else {
            found_nonzero_load_address = 1;
        }
        if ( (i > 0) &&
             (found_zero_load_address && found_nonzero_load_address)) {

            (*kload_err_log_func)(
                "load addresses must be specified for all module files");
            return dgraph_invalid;
        }
    }

    return dgraph_valid;
}

/*******************************************************************************
*
*******************************************************************************/
static void __dgraph_entry_free(dgraph_entry_t * entry)
{
    if (entry->filename) {
        free(entry->filename);
        entry->filename = NULL;
    }
    if (entry->expected_kmod_name) {
        free(entry->expected_kmod_name);
        entry->expected_kmod_name = NULL;
    }
    if (entry->dependencies) {
        free(entry->dependencies);
        entry->dependencies = NULL;
    }
    free(entry);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void dgraph_free(
    dgraph_t * dgraph,
    int free_graph)
{
    unsigned int entry_index;

    if (!dgraph) {
        return;
    }

    for (entry_index = 0; entry_index < dgraph->length; entry_index++) {
        dgraph_entry_t * current = dgraph->graph[entry_index];
        __dgraph_entry_free(current);
    }

    if (dgraph->graph) {
        free(dgraph->graph);
        dgraph->graph = NULL;
    }

    if (dgraph->load_order) {
        free(dgraph->load_order);
        dgraph->load_order = NULL;
    }

    if (free_graph && dgraph) {
        free(dgraph);
    }

    return;
}


/*******************************************************************************
*
*******************************************************************************/
dgraph_entry_t * dgraph_find_root(dgraph_t * dgraph) {
    dgraph_entry_t * root = NULL;
    dgraph_entry_t * candidate = NULL;
    unsigned int candidate_index;
    unsigned int scan_index;
    unsigned int dep_index;


   /* Scan each entry in the graph for one that isn't in any other entry's
    * dependencies.
    */
    for (candidate_index = 0; candidate_index < dgraph->length;
         candidate_index++) {

        candidate = dgraph->graph[candidate_index];

        for (scan_index = 0; scan_index < dgraph->length; scan_index++) {

            dgraph_entry_t * scan_entry = dgraph->graph[scan_index];
            if (candidate == scan_entry) {
                // don't check yourself
                continue;
            }
            for (dep_index = 0; dep_index < scan_entry->num_dependencies;
                 dep_index++) {

               /* If the dependency being checked is the candidate,
                *  then the candidate can't be the root.
                */
                dgraph_entry_t * check = scan_entry->dependencies[dep_index];

                if (check == candidate) {
                    candidate = NULL;
                    break;
                }
            }

           /* If the candidate was rejected, then hop out of this loop.
            */
            if (!candidate) {
                break;
            }
        }

       /* If we got here, the candidate is a valid one. However, if we already
        * found another, that means we have two possible roots (or more), which
        * is NOT ALLOWED.
        */
        if (candidate) {
            if (root) {
                (*kload_err_log_func)("dependency graph has multiple roots "
                    "(%s and %s)", root->filename, candidate->filename);
                return NULL;  // two valid roots, illegal
            } else {
                root = candidate;
            }
        }
    }

    if (!root) {
        (*kload_err_log_func)("dependency graph has no root node");
    }

    return root;
}

/*******************************************************************************
*
*******************************************************************************/
dgraph_entry_t ** fill_backward_load_order(
    dgraph_entry_t ** backward_load_order,
    unsigned int * list_length,
    int * list_index,
    dgraph_entry_t * entry)
{
    int i;
    dgraph_entry_t * curdep;

    for (i = 0; i < entry->num_dependencies; i++) {
        if ( (*list_index) >= (*list_length) ) {
            (*list_length) *= 2;
            backward_load_order = (dgraph_entry_t **)realloc(backward_load_order,
                (*list_length) * sizeof(dgraph_entry_t *));
            if (!backward_load_order) {
                goto finish;
            }
        }
        curdep = entry->dependencies[i];
        backward_load_order[(*list_index)++] = curdep;
    }

    for (i = 0; i < entry->num_dependencies; i++) {
        curdep = entry->dependencies[i];
        backward_load_order = fill_backward_load_order(backward_load_order,
             list_length, list_index, curdep);
        if (!backward_load_order) {
            goto finish;
        }
    }

finish:
    return backward_load_order;
}

/*******************************************************************************
*
*******************************************************************************/
int dgraph_establish_load_order(dgraph_t * dgraph) {
    unsigned int total_dependencies;
    unsigned int entry_index;
    unsigned int list_index;
    unsigned int backward_index;
    unsigned int forward_index;
    size_t load_order_size;
    size_t backward_load_order_size;
    dgraph_entry_t ** backward_load_order;

   /* Lose the old load_order list. Size can change, so it's easier to just
    * recreate from scratch.
    */
    if (dgraph->load_order) {
        free(dgraph->load_order);
        dgraph->load_order = NULL;
    }

   /* Figure how long the list needs to be to accommodate the max possible
    * entries from the graph. Duplicates get weeded out, but the list
    * initially has to accommodate them all.
    */
    total_dependencies = dgraph->length;

    for (entry_index = 0; entry_index < dgraph->length; entry_index ++) {
        dgraph_entry_t * curdep = dgraph->graph[entry_index];
        total_dependencies += curdep->num_dependencies;
    }

   /* Hmm, nothing to do!
    */
    if (!total_dependencies) {
        return 1;
    }

    backward_load_order_size = total_dependencies * sizeof(dgraph_entry_t *);

    backward_load_order = (dgraph_entry_t **)malloc(backward_load_order_size);
    if (!backward_load_order) {
        (*kload_err_log_func)("malloc failure");
        return 0;
    }
    bzero(backward_load_order, backward_load_order_size);

    list_index = 0;
    backward_load_order[list_index++] = dgraph->root;
    backward_load_order = fill_backward_load_order(backward_load_order,
        &total_dependencies, &list_index, dgraph->root);
    if (!backward_load_order) {
        (*kload_err_log_func)("error establishing load order");
        return 0;
    }

    load_order_size = dgraph->length * sizeof(dgraph_entry_t *);
    dgraph->load_order = (dgraph_entry_t **)malloc(load_order_size);
    if (!dgraph->load_order) {
        (*kload_err_log_func)("malloc failure");
        return 0;
    }
    bzero(dgraph->load_order, load_order_size);


   /* Reverse the list into the dgraph's load_order list,
    * removing any duplicates.
    */
    backward_index = list_index;
    //
    // the required 1 is taken off in loop below!

    forward_index = 0;
    do {
        dgraph_entry_t * current_entry;
        unsigned int already_got_it = 0;

        backward_index--;

       /* Get the entry to check.
        */
        current_entry = backward_load_order[backward_index];

       /* Did we already get it?
        */
        for (list_index = 0; list_index < forward_index; list_index++) {
            if (current_entry == dgraph->load_order[list_index]) {
                already_got_it = 1;
                break;
            }
        }

        if (already_got_it) {
            continue;
        }

       /* Haven't seen it before; tack it onto the load-order list.
        */
        dgraph->load_order[forward_index++] = current_entry;

    } while (backward_index > 0);

    free(backward_load_order);

    return 1;
}

/*******************************************************************************
*
*******************************************************************************/
void dgraph_verify(char * tag, dgraph_t * depgraph)
{
    unsigned int i;

     for (i = 0; i < depgraph->length; i++) {
        dgraph_entry_t * current = depgraph->graph[i];
        char vbuffer[124];

        if (!VERS_string(vbuffer, sizeof(vbuffer), current->expected_kmod_vers)) {
            goto fail;
        }
    }

    goto pass;
fail:
    kload_err_log_func("Dgraph is corrupt");
pass:
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void dgraph_print(dgraph_t * depgraph)
{
    void (*save_log_func)(const char * format, ...);

    save_log_func = kload_log_func;
    set_log_function((void(*)(const char *,...))&printf);
    dgraph_log(depgraph);
    set_log_function(save_log_func);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void dgraph_log(dgraph_t * depgraph)
{
    unsigned int i, j;

    kload_log_func("flattened dependency list: ");
    for (i = 0; i < depgraph->length; i++) {
        dgraph_entry_t * current = depgraph->graph[i];
        char vbuffer[24];

        kload_log_func("    %s", current->filename);
#if 1
        kload_log_func("      is kernel component: %s",
            current->is_kernel_component ? "yes" : "no");
        kload_log_func("      expected kmod name: [%s]",
            current->expected_kmod_name);
        VERS_string(vbuffer, sizeof(vbuffer), current->expected_kmod_vers);
        kload_log_func("      expected kmod vers: [%s]", vbuffer);
#endif 0
    }
    kload_log_func("");

    kload_log_func("load order dependency list: ");
    for (i = 0; i < depgraph->length; i++) {
        dgraph_entry_t * current = depgraph->load_order[i];
        kload_log_func("    %s", current->filename);
    }
    kload_log_func("");

    kload_log_func("dependency graph: ");
    for (i = 0; i < depgraph->length; i++) {
        dgraph_entry_t * current = depgraph->graph[i];
        for (j = 0; j < current->num_dependencies; j++) {
            dgraph_entry_t * cdep = current->dependencies[j];
            kload_log_func("  %s -> %s", current->filename, cdep->filename);
        }
    }
    kload_log_func("");

    return;
}

/*******************************************************************************
*
*******************************************************************************/
dgraph_entry_t * dgraph_find_dependent(dgraph_t * dgraph, const char * filename)
{
    unsigned int i;

    for (i = 0; i < dgraph->length; i++) {
        dgraph_entry_t * current_entry = dgraph->graph[i];
        if (0 == strcmp(filename, current_entry->filename)) {
            return current_entry;
        }
    }

    return NULL;
}

/*******************************************************************************
*
*******************************************************************************/
dgraph_entry_t * dgraph_add_dependent(
    dgraph_t * dgraph,
    const char * filename,
    const char * expected_kmod_name,
    UInt32 expected_kmod_vers,
    vm_address_t load_address,
    char is_kernel_component)
{
    int error = 0;
    dgraph_entry_t * found_entry = NULL;
    dgraph_entry_t * new_entry = NULL;    // free on error
    dgraph_entry_t * the_entry = NULL;    // returned

   /* Already got it? Great!
    */
    found_entry = dgraph_find_dependent(dgraph, filename);
    if (found_entry) {
        if (found_entry->is_kernel_component != is_kernel_component) {
            (*kload_err_log_func)(
                "%s is already defined as a %skernel component",
                filename, found_entry->is_kernel_component ? "" : "non-");
            error = 1;
            goto finish;
        }

        if (load_address != 0) {
            if (found_entry->loaded_address == 0) {
                found_entry->do_load = 0;
                found_entry->loaded_address = load_address;
            } else if (found_entry->loaded_address != load_address) {
                (*kload_err_log_func)(
                   "%s has been assigned two different addresses (0x%x, 0x%x)",
                    found_entry->filename,
                    found_entry->loaded_address,
                    load_address);
                error = 1;
                goto finish;
            }
        }
        the_entry = found_entry;
        goto finish;
    }

   /* If the graph is full, make it bigger.
    */
    if (dgraph->length == dgraph->capacity) {
        unsigned int old_capacity = dgraph->capacity;
        dgraph_entry_t ** newgraph;

        dgraph->capacity *= 2;
        newgraph = (dgraph_entry_t **)malloc(dgraph->capacity *
            sizeof(dgraph_entry_t));
        if (!newgraph) {
            return NULL;
        }
        memcpy(newgraph, dgraph->graph, old_capacity * sizeof(dgraph_entry_t));
        free(dgraph->graph);
        dgraph->graph = newgraph;
    }

    if (strlen(expected_kmod_name) > KMOD_MAX_NAME - 1) {
        (*kload_err_log_func)("expected kmod name \"%s\" is too long",
            expected_kmod_name);
        error = 1;
        goto finish;
    }

   /* Fill it.
    */
    new_entry = (dgraph_entry_t *)malloc(sizeof(dgraph_entry_t));
    if (!new_entry) {
        error = 1;
        goto finish;
    }
    bzero(new_entry, sizeof(dgraph_entry_t));
    new_entry->expected_kmod_name = strdup(expected_kmod_name);
    if (!new_entry->expected_kmod_name) {
        error = 1;
        goto finish;
    }
    new_entry->expected_kmod_vers = expected_kmod_vers;
    new_entry->is_kernel_component = is_kernel_component;
    new_entry->do_load = !is_kernel_component;
    new_entry->filename = strdup(filename);
    if (!new_entry->filename) {
        error = 1;
        goto finish;
    }
    dgraph->graph[dgraph->length++] = new_entry;


   /* Create a dependency list for the entry. Start with 5 slots.
    */
    new_entry->dependencies_capacity = 5;
    new_entry->num_dependencies = 0;
    new_entry->dependencies = (dgraph_entry_t **)malloc(
        new_entry->dependencies_capacity * sizeof(dgraph_entry_t *));
    if (!new_entry->dependencies) {
        error = 1;
        goto finish;
    }

    if (new_entry->loaded_address == 0) {
        new_entry->loaded_address = load_address;
        if (load_address != 0) {
            new_entry->do_load = 0;
        }
    }

    the_entry = new_entry;

finish:
    if (error) {
        if (new_entry) __dgraph_entry_free(new_entry);
        the_entry = new_entry = NULL;
    }
    return the_entry;
}

/*******************************************************************************
*
*******************************************************************************/
int dgraph_add_dependency(
    dgraph_t * dgraph,
    dgraph_entry_t * current_dependent,
    const char * filename,
    const char * expected_kmod_name,
    UInt32 expected_kmod_vers,
    vm_address_t load_address,
    char is_kernel_component)
{
    dgraph_entry_t * dependency = NULL;
    unsigned int i = 0;

   /* If the dependent's dependency list is full, make it bigger.
    */
    if (current_dependent->num_dependencies ==
        current_dependent->dependencies_capacity) {

        unsigned int old_capacity = current_dependent->dependencies_capacity;
        dgraph_entry_t ** newlist;

        current_dependent->dependencies_capacity *= 2;
        newlist = (dgraph_entry_t **)malloc(
            (current_dependent->dependencies_capacity *
             sizeof(dgraph_entry_t *)) );

        if (!newlist) {
            return 0;
        }
        memcpy(newlist, current_dependent->dependencies,
            old_capacity * sizeof(dgraph_entry_t *));
        free(current_dependent->dependencies);
        current_dependent->dependencies = newlist;
    }


   /* Find or add the entry for the new dependency.
    */
    dependency = dgraph_add_dependent(dgraph, filename,
         expected_kmod_name, expected_kmod_vers, load_address,
         is_kernel_component);
    if (!dependency) {
       return 0;
    }

    if (dependency == current_dependent) {
        (*kload_err_log_func)("attempt to set dependency on itself: "
            "%s", current_dependent->filename);
        return 0;
    }

    for (i = 0; i < current_dependent->num_dependencies; i++) {
        dgraph_entry_t * this_dependency = current_dependent->dependencies[i];
        if (this_dependency == dependency) {
            return 1;
        }
    }

   /* Fill in the dependency.
    */
    current_dependent->dependencies[current_dependent->num_dependencies] =
        dependency;
    current_dependent->num_dependencies++;

    return 1;
}
