/* scp_expr.c: SCP expression evaluation support

   This file holds the expression parser and evaluator used by SCP.
   It translates infix expressions into postfix form and then evaluates
   them, including register lookups, environment substitutions, and the
   shared operator table.
*/
// SPDX-FileCopyrightText: 1993-2022 Robert M Supnik
// SPDX-License-Identifier: MIT

#include "sim_defs.h"
#include "scp.h"

extern DEVICE sim_scp_dev;
REG *find_reg_glob(const char *ptr, const char **optr, DEVICE **gdptr);

typedef t_svalue (*Operator_Function)(t_svalue, t_svalue);
typedef t_svalue (*Operator_String_Function)(const char *, const char *);

typedef struct Operator {
    const char *string;
    int precedence;
    int unary;
    Operator_Function function;
    Operator_String_Function string_function;
    const char *description;
} Operator;

typedef struct Stack_Element {
    Operator *op;
    char data[72];
} Stack_Element;

typedef struct Stack {
    int size;
    int pointer;
    int id;
    Stack_Element *elements;
} Stack;

#define STACK_GROW_AMOUNT 5

static int stack_counter = 0;
static char **sim_exp_argv = NULL;

/* Delete the given stack and release all memory associated with it. */
static void delete_Stack(Stack *sp)
{
    if (sp == NULL)
        return;

    sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev,
              "[Stack %d has been deallocated]\n", sp->id);
    free(sp->elements);
    free(sp);
    stack_counter--;
}

/* Check whether the given expression stack currently holds any values. */
static t_bool isempty_Stack(Stack *this_Stack)
{
    return (this_Stack->pointer == 0);
}

/* Allocate and initialize a new expression stack. */
static Stack *new_Stack(void)
{
    Stack *this_Stack = (Stack *)calloc(1, sizeof(*this_Stack));

    this_Stack->id = ++stack_counter;
    sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev,
              "[Stack %d has been allocated]\n", this_Stack->id);
    return this_Stack;
}

/* Pop the top stack element into the provided data and operator slots. */
static t_bool pop_Stack(Stack *this_Stack, char *data, Operator **op)
{
    *op = NULL;
    *data = '\0';

    if (isempty_Stack(this_Stack))
        return FALSE;

    strcpy(data, this_Stack->elements[this_Stack->pointer - 1].data);
    *op = this_Stack->elements[this_Stack->pointer - 1].op;
    --this_Stack->pointer;

    if (*op)
        sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev,
                  "[Stack %d - Popping '%s'(precedence %d)]\n", this_Stack->id,
                  (*op)->string, (*op)->precedence);
    else
        sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev, "[Stack %d - Popping %s]\n",
                  this_Stack->id, data);

    return TRUE;
}

/* Push a value or operator onto the specified stack. */
static t_bool push_Stack(Stack *this_Stack, char *data, Operator *op)
{
    if (this_Stack == NULL)
        return FALSE;

    if (this_Stack->pointer == this_Stack->size) {
        this_Stack->size += STACK_GROW_AMOUNT;
        this_Stack->elements = (Stack_Element *)realloc(
            this_Stack->elements,
            this_Stack->size * sizeof(*this_Stack->elements));
        memset(this_Stack->elements + this_Stack->size - STACK_GROW_AMOUNT, 0,
               STACK_GROW_AMOUNT * sizeof(*this_Stack->elements));
    }

    this_Stack->elements[this_Stack->pointer].op = op;
    strlcpy(this_Stack->elements[this_Stack->pointer].data, data,
            sizeof(this_Stack->elements[this_Stack->pointer].data));
    ++this_Stack->pointer;

    if (op)
        sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev,
                  "[Stack %d - Pushing '%s'(precedence %d)]\n", this_Stack->id,
                  op->string, op->precedence);
    else
        sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev, "[Stack %d - Pushing %s]\n",
                  this_Stack->id, data);

    return TRUE;
}

/* Peek at the top element of a stack without removing it. */
static t_bool top_Stack(Stack *this_Stack, char *data, Operator **op)
{
    if (isempty_Stack(this_Stack))
        return FALSE;

    strcpy(data, this_Stack->elements[this_Stack->pointer - 1].data);
    *op = this_Stack->elements[this_Stack->pointer - 1].op;

    if (*op)
        sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev,
                  "[Stack %d - Topping '%s'(precedence %d)]\n", this_Stack->id,
                  (*op)->string, (*op)->precedence);
    else
        sim_debug(SIM_DBG_EXP_STACK, &sim_scp_dev, "[Stack %d - Topping %s]\n",
                  this_Stack->id, data);

    return TRUE;
}

static t_svalue _op_add(t_svalue augend, t_svalue addend)
{
    return augend + addend;
}

static t_svalue _op_sub(t_svalue subtrahend, t_svalue minuend)
{
    return minuend - subtrahend;
}

static t_svalue _op_mult(t_svalue factorx, t_svalue factory)
{
    return factorx * factory;
}

static t_svalue _op_div(t_svalue divisor, t_svalue dividend)
{
    if (divisor != 0)
        return dividend / divisor;
    return T_SVALUE_MAX;
}

static t_svalue _op_mod(t_svalue divisor, t_svalue dividend)
{
    if (divisor != 0)
        return dividend % divisor;
    return 0;
}

static t_svalue _op_comp(t_svalue data, t_svalue unused)
{
    (void)unused;
    return ~data;
}

static t_svalue _op_log_not(t_svalue data, t_svalue unused)
{
    (void)unused;
    return !data;
}

static t_svalue _op_log_and(t_svalue data1, t_svalue data2)
{
    return data2 && data1;
}

static t_svalue _op_log_or(t_svalue data1, t_svalue data2)
{
    return data2 || data1;
}

static t_svalue _op_bit_and(t_svalue data1, t_svalue data2)
{
    return data2 & data1;
}

static t_svalue _op_bit_rsh(t_svalue shift, t_svalue data)
{
    return data >> shift;
}

static t_svalue _op_bit_lsh(t_svalue shift, t_svalue data)
{
    return data << shift;
}

static t_svalue _op_bit_or(t_svalue data1, t_svalue data2)
{
    return data2 | data1;
}

static t_svalue _op_bit_xor(t_svalue data1, t_svalue data2)
{
    return data2 ^ data1;
}

static t_svalue _op_eq(t_svalue data1, t_svalue data2)
{
    return data2 == data1;
}

static t_svalue _op_ne(t_svalue data1, t_svalue data2)
{
    return data2 != data1;
}

static t_svalue _op_le(t_svalue data1, t_svalue data2)
{
    return data2 <= data1;
}

static t_svalue _op_lt(t_svalue data1, t_svalue data2)
{
    return data2 < data1;
}

static t_svalue _op_ge(t_svalue data1, t_svalue data2)
{
    return data2 >= data1;
}

static t_svalue _op_gt(t_svalue data1, t_svalue data2)
{
    return data2 > data1;
}

/* Compare strings using SCP's current case and whitespace rules. */
static int _i_strcmp(const char *s1, const char *s2)
{
    if (sim_switches & SWMASK('W'))
        return sim_strwhitecasecmp(s1, s2,
                                   (sim_switches & SWMASK('I')) != 0);
    return ((sim_switches & SWMASK('I')) ? strcasecmp(s2, s1) : strcmp(s2, s1));
}

static t_svalue _op_str_eq(const char *str1, const char *str2)
{
    return (0 == _i_strcmp(str2, str1));
}

static t_svalue _op_str_ne(const char *str1, const char *str2)
{
    return (0 != _i_strcmp(str2, str1));
}

static t_svalue _op_str_le(const char *str1, const char *str2)
{
    return (0 <= _i_strcmp(str2, str1));
}

static t_svalue _op_str_lt(const char *str1, const char *str2)
{
    return (0 < _i_strcmp(str2, str1));
}

static t_svalue _op_str_ge(const char *str1, const char *str2)
{
    return (0 >= _i_strcmp(str2, str1));
}

static t_svalue _op_str_gt(const char *str1, const char *str2)
{
    return (0 > _i_strcmp(str2, str1));
}

static Operator operators[] = {
    {"(", 99, 0, NULL, NULL, "Open Parenthesis"},
    {")", 99, 0, NULL, NULL, "Close Parenthesis"},
    {"+", 4, 0, _op_add, NULL, "Addition"},
    {"-", 4, 0, _op_sub, NULL, "Subtraction"},
    {"*", 3, 0, _op_mult, NULL, "Multiplication"},
    {"/", 3, 0, _op_div, NULL, "Division"},
    {"%", 3, 0, _op_mod, NULL, "Modulus"},
    {"&&", 11, 0, _op_log_and, NULL, "Logical AND"},
    {"||", 12, 0, _op_log_or, NULL, "Logical OR"},
    {"&", 8, 0, _op_bit_and, NULL, "Bitwise AND"},
    {">>", 5, 0, _op_bit_rsh, NULL, "Bitwise Right Shift"},
    {"<<", 5, 0, _op_bit_lsh, NULL, "Bitwise Left Shift"},
    {"|", 10, 0, _op_bit_or, NULL, "Bitwise Inclusive OR"},
    {"^", 9, 0, _op_bit_xor, NULL, "Bitwise Exclusive OR"},
    {"==", 7, 0, _op_eq, _op_str_eq, "Equality"},
    {"!=", 7, 0, _op_ne, _op_str_ne, "Inequality"},
    {"<=", 6, 0, _op_le, _op_str_le, "Less than or Equal"},
    {"<", 6, 0, _op_lt, _op_str_lt, "Less than"},
    {">=", 6, 0, _op_ge, _op_str_ge, "Greater than or Equal"},
    {">", 6, 0, _op_gt, _op_str_gt, "Greater than"},
    {"!", 2, 1, _op_log_not, NULL, "Logical Negation"},
    {"~", 2, 1, _op_comp, NULL, "Bitwise Compliment"},
    {NULL}};

/* Decode one expression token, including literals and operators. */
static const char *get_glyph_exp(const char *cptr, char *buf, Operator **oper,
                                 t_stat *stat)
{
    static const char HexDigits[] = "0123456789abcdefABCDEF";
    static const char OctalDigits[] = "01234567";
    static const char BinaryDigits[] = "01";

    *stat = SCPE_OK;
    *buf = '\0';
    *oper = NULL;
    while (sim_isspace(*cptr))
        ++cptr;
    if (sim_isalpha(*cptr) || (*cptr == '_')) {
        while (sim_isalnum(*cptr) || (*cptr == '.') || (*cptr == '_'))
            *buf++ = *cptr++;
        *buf = '\0';
    } else if (isdigit(*cptr)) {
        if ((!memcmp(cptr, "0x", 2)) || (!memcmp(cptr, "0X", 2))) {
            memcpy(buf, cptr, 2);
            cptr += 2;
            buf += 2;
            while (*cptr && strchr(HexDigits, *cptr))
                *buf++ = *cptr++;
            *buf = '\0';
        } else if ((!memcmp(cptr, "0b", 2)) || (!memcmp(cptr, "0B", 2))) {
            memcpy(buf, cptr, 2);
            cptr += 2;
            buf += 2;
            while (*cptr && strchr(BinaryDigits, *cptr))
                *buf++ = *cptr++;
            *buf = '\0';
        } else if (*cptr == '0') {
            while (*cptr && strchr(OctalDigits, *cptr))
                *buf++ = *cptr++;
            *buf = '\0';
        } else {
            int digits = 0;
            int commas = 0;
            const char *cp = cptr;

            while (isdigit(*cp) || (*cp == ',')) {
                if (*cp == ',')
                    ++commas;
                else
                    ++digits;
                ++cp;
            }
            if ((commas > 0) && (commas != (digits - 1) / 3)) {
                *stat = SCPE_INVEXPR;
                return cptr;
            }
            while (commas--) {
                cp -= 4;
                if (*cp != ',') {
                    *stat = SCPE_INVEXPR;
                    return cptr;
                }
            }
            while (isdigit(*cptr) || (*cptr == ',')) {
                if (*cptr != ',')
                    *buf++ = *cptr;
                ++cptr;
            }
            *buf = '\0';
        }
        if (sim_isalpha(*cptr)) {
            *stat = SCPE_INVEXPR;
            return cptr;
        }
    } else if (((cptr[0] == '-') || (cptr[0] == '+')) && isdigit(cptr[1])) {
        *buf++ = *cptr++;
        while (isdigit(*cptr))
            *buf++ = *cptr++;
        *buf = '\0';
        if (sim_isalpha(*cptr)) {
            *stat = SCPE_INVEXPR;
            return cptr;
        }
    } else {
        if ((*cptr == '"') || (*cptr == '\'')) {
            cptr = (const char *)get_glyph_gen(
                cptr, buf, 0, ((sim_switches & SWMASK('I')) != 0), TRUE, '\\');
        } else {
            Operator *op;

            for (op = operators; op->string; op++) {
                if (!memcmp(cptr, op->string, strlen(op->string))) {
                    strcpy(buf, op->string);
                    cptr += strlen(op->string);
                    *oper = op;
                    break;
                }
            }
            if (!op->string) {
                *stat = SCPE_INVEXPR;
                return cptr;
            }
        }
    }
    while (sim_isspace(*cptr))
        ++cptr;
    return cptr;
}

/* Convert one infix expression string into postfix form on stack1. */
static const char *sim_into_postfix(Stack *stack1, const char *cptr,
                                    t_stat *stat, t_bool parens_required)
{
    const char *last_cptr;
    int parens = 0;
    Operator *op = NULL;
    Operator *last_op;
    Stack *stack2 = new_Stack();
    char gbuf[CBUFSIZE];

    if (stack2 == NULL) {
        *stat = SCPE_MEM;
        return cptr;
    }
    *stat = SCPE_OK;
    while (sim_isspace(*cptr))
        ++cptr;
    if (parens_required && (*cptr != '(')) {
        delete_Stack(stack2);
        *stat = SCPE_INVEXPR;
        return cptr;
    }
    while (*cptr) {
        last_cptr = cptr;
        last_op = op;
        cptr = get_glyph_exp(cptr, gbuf, &op, stat);
        if (*stat != SCPE_OK) {
            delete_Stack(stack2);
            return cptr;
        }
        if (!last_op && !op && ((gbuf[0] == '-') || (gbuf[0] == '+'))) {
            for (op = operators; gbuf[0] != op->string[0]; op++)
                ;
            gbuf[0] = '0';
            cptr = last_cptr + 1;
        }
        sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev, "[Glyph: %s]\n",
                  op ? op->string : gbuf);
        if (!op) {
            push_Stack(stack1, gbuf, op);
            continue;
        }
        if (0 == strcmp(op->string, "(")) {
            ++parens;
            push_Stack(stack2, gbuf, op);
            continue;
        }
        if (0 == strcmp(op->string, ")")) {
            char temp_buf[CBUFSIZE];
            Operator *temp_op;

            --parens;
            if ((!pop_Stack(stack2, temp_buf, &temp_op)) || (parens < 0)) {
                *stat =
                    sim_messagef(SCPE_INVEXPR, "Invalid Parenthesis nesting\n");
                delete_Stack(stack2);
                return cptr;
            }
            while (0 != strcmp(temp_op->string, "(")) {
                push_Stack(stack1, temp_buf, temp_op);
                if (!pop_Stack(stack2, temp_buf, &temp_op))
                    break;
            }
            if (parens_required && (parens == 0)) {
                delete_Stack(stack2);
                return cptr;
            }
            continue;
        }
        while (!isempty_Stack(stack2)) {
            char top_buf[CBUFSIZE];
            Operator *top_op;

            top_Stack(stack2, top_buf, &top_op);
            if (top_op->precedence > op->precedence)
                break;
            pop_Stack(stack2, top_buf, &top_op);
            push_Stack(stack1, top_buf, top_op);
        }
        push_Stack(stack2, gbuf, op);
    }
    if (parens != 0)
        *stat = sim_messagef(SCPE_INVEXPR, "Invalid Parenthesis nesting\n");
    while (!isempty_Stack(stack2)) {
        pop_Stack(stack2, gbuf, &op);
        push_Stack(stack1, gbuf, op);
    }

    delete_Stack(stack2);
    return cptr;
}

/* Resolve one token into either a numeric value or a string representation. */
static t_bool _value_of(const char *data, t_svalue *svalue, char *string,
                        size_t string_size)
{
    const char *gptr;
    size_t data_size = strlen(data);

    if (sim_isalpha(*data) || (*data == '_')) {
        REG *rptr = NULL;
        DEVICE *dptr = sim_dfdev;
        const char *dot;

        dot = strchr(data, '.');
        if (dot) {
            char devnam[CBUFSIZE];

            memcpy(devnam, data, dot - data);
            devnam[dot - data] = '\0';
            if (find_dev(devnam)) {
                dptr = find_dev(devnam);
                data = dot + 1;
                rptr = find_reg(data, &gptr, dptr);
            }
        } else {
            rptr = find_reg_glob(data, &gptr, &dptr);
            if (!rptr) {
                char tbuf[CBUFSIZE];

                get_glyph(data, tbuf, 0);
                if (strcmp(data, tbuf))
                    rptr = find_reg_glob(tbuf, &gptr, &dptr);
            }
        }
        if (rptr) {
            *svalue = (t_svalue)get_rval(rptr, 0);
            sprint_val(string, *svalue, 10, string_size - 1, PV_LEFTSIGN);
            sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev, "[Value: %s=%s]\n", data,
                      string);
            return TRUE;
        }
        gptr = _sim_get_env_special(data, string, string_size - 1);
        if (NULL != gptr) {
            const char *endptr;

            *svalue = strtotsv(gptr, &endptr, 0);
            sprint_val(string, *svalue, 10, string_size - 1, PV_LEFTSIGN);
            sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev, "[Value: %s=%s]\n", data,
                      string);
            return ((*endptr == '\0') && (*string));
        }
        data = "";
        data_size = 0;
    }
    string[0] = '\0';
    if ((data[0] == '\'') && (data_size > 1) && (data[data_size - 1] == '\''))
        snprintf(string, string_size - 1, "\"%*.*s\"", (int)(data_size - 2),
                 (int)(data_size - 2), data + 1);
    if ((data[0] == '"') && (data_size > 1) && (data[data_size - 1] == '"'))
        strlcpy(string, data, string_size);
    if (string[0] == '\0') {
        *svalue = strtotsv(data, &gptr, 0);
        strlcpy(string, data, string_size);
        sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev, "[Value: %s=%s]\n", data,
                  string);
        return ((*gptr == '\0') && (*data));
    }
    sim_sub_args(string, string_size, sim_exp_argv);
    *svalue = strtotsv(string, &gptr, 0);
    sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev, "[Value: %s=%s]\n", data, string);
    return ((*gptr == '\0') && (*string));
}

/* Evaluate the postfix expression currently stored in stack1. */
static t_svalue sim_eval_postfix(Stack *stack1, t_stat *stat)
{
    Stack *stack2 = new_Stack();
    char temp_data[CBUFSIZE + 2];
    Operator *temp_op;
    t_svalue temp_val;
    char temp_string[CBUFSIZE + 2];

    *stat = SCPE_OK;
    while (!isempty_Stack(stack1)) {
        pop_Stack(stack1, temp_data, &temp_op);
        if (temp_op)
            sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev,
                      "[Expression element: %s (%d)\n", temp_op->string,
                      temp_op->precedence);
        else
            sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev,
                      "[Expression element: %s\n", temp_data);
        push_Stack(stack2, temp_data, temp_op);
    }

    while (!isempty_Stack(stack2)) {
        pop_Stack(stack2, temp_data, &temp_op);
        if (temp_op) {
            t_bool num1;
            t_svalue val1;
            char item1[CBUFSIZE];
            char string1[CBUFSIZE + 2];
            Operator *op1;
            t_bool num2;
            t_svalue val2;
            char item2[CBUFSIZE];
            char string2[CBUFSIZE + 2];
            Operator *op2;

            if (!pop_Stack(stack1, item1, &op1)) {
                *stat = SCPE_INVEXPR;
                delete_Stack(stack2);
                return 0;
            }
            if (temp_op->unary)
                strlcpy(item2, "0", sizeof(item2));
            else if ((!pop_Stack(stack1, item2, &op2)) &&
                     (temp_op->string[0] != '-') &&
                     (temp_op->string[0] != '+')) {
                *stat = SCPE_INVEXPR;
                delete_Stack(stack2);
                return 0;
            }
            num1 = _value_of(item1, &val1, string1, sizeof(string1));
            num2 = _value_of(item2, &val2, string2, sizeof(string2));
            if ((!(num1 && num2)) && temp_op->string_function)
                sprint_val(temp_data,
                           (t_value)temp_op->string_function(string1, string2),
                           10, sizeof(temp_data) - 1, PV_LEFTSIGN);
            else
                sprint_val(temp_data, (t_value)temp_op->function(val1, val2),
                           10, sizeof(temp_data) - 1, PV_LEFTSIGN);
            push_Stack(stack1, temp_data, NULL);
        } else
            push_Stack(stack1, temp_data, temp_op);
    }
    if (!pop_Stack(stack1, temp_data, &temp_op)) {
        *stat = SCPE_INVEXPR;
        delete_Stack(stack2);
        return 0;
    }
    delete_Stack(stack2);
    if (_value_of(temp_data, &temp_val, temp_string, sizeof(temp_string)))
        return temp_val;
    return (t_svalue)(strlen(temp_string) > 2);
}

/* Install the active argument vector used for expression substitutions. */
void scp_set_exp_argv(char **argv)
{
    sim_exp_argv = argv;
}

/* Evaluate one SCP expression and return the remaining unparsed text. */
const char *sim_eval_expression(const char *cptr, t_svalue *value,
                                t_bool parens_required, t_stat *stat)
{
    const char *iptr = cptr;
    Stack *postfix = new_Stack();

    sim_debug(SIM_DBG_EXP_EVAL, &sim_scp_dev, "[Evaluate Expression: %s\n",
              cptr);
    *value = 0;
    cptr = sim_into_postfix(postfix, cptr, stat, parens_required);
    if (*stat != SCPE_OK) {
        delete_Stack(postfix);
        *stat =
            sim_messagef(SCPE_ARG, "Invalid Expression Element:\n%s\n%*s^\n",
                         iptr, (int)(cptr - iptr), "");
        return cptr;
    }

    *value = sim_eval_postfix(postfix, stat);
    delete_Stack(postfix);
    return cptr;
}
