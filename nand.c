// Autor: Filip Koska, nr albumu 459181

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>

#include "nand.h"

#define THROW_FUNCTION_ERROR(errno_val, ret_val)   \
    do {                                           \
        errno = errno_val;                         \
        return ret_val;                            \
    } while (false)

// dynamic vector with amortised constant operation cost
typedef struct {
    size_t capacity, size;
    nand_t **data;
} output_vector;

// will hold pointer to appropriate type based on the value of enum type
typedef union {
    nand_t *nand_t_input;
    bool *signal_input;
} input;

enum type{nand, signal, null};

// t holds information about type of input
typedef struct {
    input in;
    enum type t;
} input_elem;

struct nand {
    size_t n_inputs;
    input_elem *inputs;
    bool val;
    bool visited, finished;
    ssize_t path_len;
    output_vector *outputs;
};

// the value -1 at either x or y indicates a nand_evaluate() error; otherwise, return max(x, y)
ssize_t eval_dfs(ssize_t x, ssize_t y) {
    if (x == -1 || y == -1)
        return -1;
    return y > x ? y : x;
}

output_vector* make_vector() {
    output_vector *v = (output_vector*)malloc(sizeof(output_vector));
    if (!v)
        THROW_FUNCTION_ERROR(ENOMEM, NULL);

    v->capacity = 1;
    v->size = 0;
    v->data = (nand_t**)malloc(sizeof(nand_t*));
    if (!v->data) {
        free(v);
        THROW_FUNCTION_ERROR(ENOMEM, NULL);
    }
    v->data[0] = NULL;
    return v;
}

// returns size of vector on success, -1 on failure
ssize_t push_back(output_vector *v, nand_t *gate) {
    if (!v || !gate)
        return -1;
    if (v->size == v->capacity) {
        v->capacity *= 2;
        nand_t **tmp = v->data;
        tmp = (nand_t**)realloc(tmp, v->capacity * sizeof(nand_t*));
        if (!tmp) {
            v->capacity /= 2;
            THROW_FUNCTION_ERROR(ENOMEM, -1);
        }
        v->data = tmp;
    }
    v->data[(v->size)++] = gate;
    return v->size;
}

// returns size of vector on success, -1 on failure
ssize_t pop_back(output_vector *v) {
    if (!v || !(v->data))
        return -1;

    if (v->size > 0)
        v->data[--(v->size)] = NULL;
    return v->size;
}

// returns size of vector on success, -1 on failure
ssize_t delete_vector_element(output_vector *v, nand_t *g) {
    if (!v || !g || !v->data)
        return -1;

    unsigned i = 0;
    while (i < v->size && v->data[i] != g)
        ++i;
    // element to be deleted is present in v
    if (i < v->size) {
        nand_t *tmp = v->data[v->size - 1];
        v->data[v->size - 1] = v->data[i];
        v->data[i] = tmp;
        pop_back(v);
    }
    return v->size;
}

void delete_vector(output_vector *v) {
    if (v) {
        if (v->data)
            free(v->data);
        free(v);
    }
}

void set_nand_flags(nand_t *g, bool w, bool x, bool y, ssize_t z) {
    if (!g)
        return;
    g->val = w;
    g->visited = x;
    g->finished = y;
    g->path_len = z;
}

ssize_t dfs(nand_t *g) {
    if (!g)
        THROW_FUNCTION_ERROR(EINVAL, -1);

    g->visited = true;
    bool sig = false;
    ssize_t res = 0;
    if (g->n_inputs == 0) {
        set_nand_flags(g, false, true, true, 0);
        return 0;
    }

    for (unsigned i = 0; i < g->n_inputs; ++i) {
        // if gate graph contains a cycle or one of the inputs is NULL, critical path computation fails
        if (g->inputs[i].t == null || (g->inputs[i].t == nand && g->inputs[i].in.nand_t_input->visited && !g->inputs[i].in.nand_t_input->finished))
            THROW_FUNCTION_ERROR(ECANCELED, -1);

        if (g->inputs[i].t == signal) {
            res = eval_dfs(res, 0);
            sig |= (!(*(g->inputs[i].in.signal_input)));
        }
        else {
            if (g->inputs[i].in.nand_t_input->finished) {
                res = eval_dfs(res, g->inputs[i].in.nand_t_input->path_len);
            }
            else {
                res = eval_dfs(res, dfs(g->inputs[i].in.nand_t_input));
            }
            // if any input value is false, value in g is true
            sig |= (!g->inputs[i].in.nand_t_input->val);
        }
    }
    if (res == -1)
        return -1;

    set_nand_flags(g, sig, true, true, res + 1);
    return g->path_len;
}

// resets the flags and prepares g for next call of nand_evaluate()
void clear(nand_t *g) {
    if (!g)
        return;
    g->visited = g->finished = false;
    if (!g->inputs)
        return;
    for (unsigned i = 0; i < g->n_inputs; ++i) {
        if (g->inputs[i].t == nand && g->inputs[i].in.nand_t_input->visited)
            clear(g->inputs[i].in.nand_t_input);
    }
}

nand_t* nand_new(unsigned n) {
    nand_t *g = (nand_t*)malloc(sizeof(nand_t));
    if (!g)
        THROW_FUNCTION_ERROR(ENOMEM, NULL);

    g->n_inputs = n;
    if (n == 0) {
        g->inputs = NULL;
    }
    else {
        g->inputs = (input_elem*)malloc(n * sizeof(input_elem));
        if (!g->inputs) {
           free(g);
           THROW_FUNCTION_ERROR(ENOMEM, NULL);
        }
    }
    
    for (unsigned i = 0; i < g->n_inputs; ++i) {
        g->inputs[i].t = null;
        g->inputs[i].in.nand_t_input = NULL;
    }
    g->outputs = make_vector();
    set_nand_flags(g, (bool)n, false, false, 0);
    if (!g->outputs) {
        if (n > 0)
            free(g->inputs);
        free(g);
        THROW_FUNCTION_ERROR(ENOMEM, NULL);
    }
    return g;
}

void nand_delete(nand_t *g) {
    if (g) {

        // disconnect gates that were connected to the input of g
        if (g->inputs) {
            for (unsigned i = 0; i < g->n_inputs; ++i) {
                if (g->inputs[i].t == nand) {
                    delete_vector_element(g->inputs[i].in.nand_t_input->outputs, g);
                }
            }
            free(g->inputs);
        }

        // disconnect g from the inputs of all other gates
        if (g->outputs) {
            for (unsigned i = 0; i < g->outputs->size; ++i) {
                // if the gate is connected to itself, its input array was already freed before
                if (!g->outputs->data[i] || g->outputs->data[i] == g)
                    continue;
                
                // find the input corresponding to g and disconnect it
                for (unsigned j = 0; j < g->outputs->data[i]->n_inputs; ++j) {
                   if (g->outputs->data[i]->inputs[j].t == nand && g->outputs->data[i]->inputs[j].in.nand_t_input == g) {
                      g->outputs->data[i]->inputs[j].t = null;
                      g->outputs->data[i]->inputs[j].in.nand_t_input = NULL;
                  }
                }
            }
            delete_vector(g->outputs);
        }
        free(g);
    }
}

int nand_connect_nand(nand_t *g_out, nand_t *g_in, unsigned k) {
    if (!g_out || !g_in || k >= g_in->n_inputs)
        THROW_FUNCTION_ERROR(EINVAL, -1);

    // if push_back() failed, return error code (errno will be set to ENOMEM by push_back())
    ssize_t no_err = push_back(g_out->outputs, g_in);
    if (no_err == -1)
        return -1;

    // disconnect gate that was connected to the input of g_in before
    if (g_in->inputs[k].t == nand)
        delete_vector_element(g_in->inputs[k].in.nand_t_input->outputs, g_in);

    g_in->inputs[k].t = nand;
    g_in->inputs[k].in.nand_t_input = g_out;
    return 0;
}

int nand_connect_signal(bool const *s, nand_t *g, unsigned k) {
    if (!s || !g || k >= g->n_inputs)
        THROW_FUNCTION_ERROR(EINVAL, -1);

    // disconnect gate that was connected to the input of g before
    if (g->inputs[k].t == nand)
        delete_vector_element(g->inputs[k].in.nand_t_input->outputs, g);

    g->inputs[k].t = signal;
    g->inputs[k].in.signal_input = (bool*)s;
    return 0;
}

ssize_t nand_evaluate(nand_t **g, bool *s, size_t m) {
    if (!g || !s || m == 0)
        THROW_FUNCTION_ERROR(EINVAL, -1);
    for (unsigned i = 0; i < m; ++i)
        if (!g[i])
            THROW_FUNCTION_ERROR(EINVAL, -1);

    // recursively compute critical path
    ssize_t res = 0;
    for (unsigned i = 0; i < m; ++i) {
        res = eval_dfs(res, dfs(g[i]));
    }

    for (unsigned i = 0; i < m; ++i)
        s[i] = g[i]->val;
    // prepare flags for future calls of nand_evaluate()
    for (unsigned i = 0; i < m; ++i)
        clear(g[i]);
    return res;
    
}

ssize_t nand_fan_out(nand_t const *g) {
    if (!g)
        THROW_FUNCTION_ERROR(EINVAL, -1);

    if (g->outputs)
        return g->outputs->size;
    return 0;
}

void* nand_input(nand_t const *g, unsigned k) {
    if (!g || k >= g->n_inputs)
        THROW_FUNCTION_ERROR(EINVAL, NULL);

    // no input connected is a legal situation, so don't use error macro
    if (g->inputs[k].t == null) {
        errno = 0;
        return NULL;
    }
    if (g->inputs[k].t == signal)
        return g->inputs[k].in.signal_input;
    return g->inputs[k].in.nand_t_input;
}

nand_t* nand_output(nand_t const *g, ssize_t k) {
    if (k < 0)
        return NULL;

    size_t p = (size_t)k;
    if (!g || p > g->outputs->size)
        return NULL;
    return g->outputs->data[k];
}